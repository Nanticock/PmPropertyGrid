// MS C++ ABI implementation of rtti_hierarchy detail functions.
//
// Compiled on:  MSVC (cl.exe) and clang-cl on Windows.
// Architectures: x86, x86-64, ARM32, ARM64.
//
// References:
//   clang/lib/CodeGen/MicrosoftCXXABI.cpp  (LLVM monorepo, studied April 2026)
//   docs/ms_abi.md  (local reference document)
//   MicrosoftRTTI_p.h  (structure definitions)
//
// VFTable memory layout  (RTTI enabled):
//
//   x86 / ARM32:
//     [ COL* (4-byte absolute ptr) ]   ← sizeof(void*)=4 bytes before vfptr
//     [ func0                      ]   ← vfptr stored in the object points here
//     [ func1                      ]
//     ...
//
//   x64 / ARM64:
//     [ COL* (8-byte absolute ptr) ]   ← sizeof(void*)=8 bytes before vfptr
//     [ func0 (8 bytes)            ]   ← vfptr points here
//     [ func1 (8 bytes)            ]
//     ...
//
// In both cases the slot contains a full-width ABSOLUTE POINTER to the COL.
// Only the INTERNAL fields of the COL itself (pTypeDescriptor, pClassDescriptor,
// pSelf) use 4-byte image-relative RVAs on x64 — the vtable slot is always a
// full-pointer-width reference.

#include "rtti_hierarchy_impl_p.h"

#include "MicrosoftRTTI_p.h"

#include <typeindex>
#include <typeinfo>
#include <unordered_set>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace rtti_hierarchy
{
namespace detail
{
namespace
{

using namespace ms_rtti;

// ─────────────────────────────────────────────────────────────────────────────
// Memory safety helpers
// ─────────────────────────────────────────────────────────────────────────────

// Returns true only if [ptr, ptr+size) is in a committed, readable page.
static bool is_readable(const void *ptr, std::size_t size) noexcept
{
    if (!ptr)
        return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0)
        return false;
    if (mbi.State != MEM_COMMIT)
        return false;
    static constexpr DWORD kNoAccess = PAGE_NOACCESS | PAGE_GUARD;
    if (mbi.Protect & kNoAccess)
        return false;
    // Check the region contains the entire requested range.
    const auto region_end = reinterpret_cast<const char *>(mbi.BaseAddress) + mbi.RegionSize;
    const auto range_end  = static_cast<const char *>(ptr) + size;
    return range_end <= region_end;
}

// ─────────────────────────────────────────────────────────────────────────────
// Read the CompleteObjectLocator from the vtable slot before the address point
// ─────────────────────────────────────────────────────────────────────────────

// Forward declaration (defined below).
bool validate_col(const CompleteObjectLocator *col) noexcept;

// Try to read a COL from a single candidate vfptr value.
// Returns nullptr if the pointer is not a valid vfptr with an RTTI COL.
static const CompleteObjectLocator *try_read_col_from_vfptr(const void *vfptr) noexcept
{
    if (!vfptr)
        return nullptr;
    const char *slot = static_cast<const char *>(vfptr) - sizeof(void *);
    if (!is_readable(slot, sizeof(void *)))
        return nullptr;
    const auto *col = *reinterpret_cast<const CompleteObjectLocator *const *>(slot);
    return validate_col(col) ? col : nullptr;
}

// Returns the COL for the most-derived object.
//
// MSVC object layout for classes with virtual bases:
//   The first word of a subobject that has direct virtual bases is a
//   vbtable pointer (not a vfptr).  Its entries are int32_t offsets:
//     vbt[0] = offset from this subobject to itself (always 0)
//     vbt[1] = offset from this subobject to first virtual base
//     vbt[2] = offset from this subobject to second virtual base, etc.
//   Each virtual-base subobject then has its own vfptr at its offset 0.
//
// Strategy:
//   1. Try the normal path: treat words[0] as a vfptr.
//   2. If that fails, treat words[0] as a vbtable pointer and walk each
//      virtual-base-offset entry (indices 1..N) to find a subobject with
//      a valid COL.
const CompleteObjectLocator *read_col(const void *obj) noexcept
{
    if (!is_readable(obj, sizeof(void *)))
        return nullptr;

    const void *words0 = *static_cast<const void *const *>(obj);

    // Path 1: direct vfptr (non-virtual or pure non-virtual-base classes).
    if (const auto *col = try_read_col_from_vfptr(words0))
        return col;

    // Path 2: words[0] might be a vbtable pointer.
    // A vbtable is an array of int32_t.  Entry [0] is always 0 (self-offset).
    // Entries [1..N] are byte offsets from obj to each virtual base subobject.
    // We try to read up to kMaxVBases entries (hard cap to avoid runaway reads).
    static constexpr int kMaxVBases = 16;
    const auto *vbt = static_cast<const std::int32_t *>(words0);
    if (!is_readable(vbt, sizeof(std::int32_t) * 2))
        return nullptr;
    if (vbt[0] != 0)        // vbt[0] must be 0; if not, this isn't a vbtable
        return nullptr;

    for (int i = 1; i < kMaxVBases; ++i)
    {
        // Check if vbt[i] is readable; stop if we fall off the end.
        if (!is_readable(&vbt[i], sizeof(std::int32_t)))
            break;
        const std::int32_t vb_offset = vbt[i];
        if (vb_offset == 0)
            break; // end-of-table sentinel

        const char *vbase = static_cast<const char *>(obj) + vb_offset;
        if (!is_readable(vbase, sizeof(void *)))
            continue;

        const void *candidate_vfptr = *reinterpret_cast<const void *const *>(vbase);
        if (const auto *col = try_read_col_from_vfptr(candidate_vfptr))
            return col;
    }

    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sanity-check a COL before trusting any of its fields
// ─────────────────────────────────────────────────────────────────────────────

bool validate_col(const CompleteObjectLocator *col) noexcept
{
    if (!col)
        return false;

    // Guard: the COL pointer itself might be garbage (e.g. data read from a
    // vbtable slot).  Check readability before accessing any field.
    if (!is_readable(col, sizeof(CompleteObjectLocator)))
        return false;

#if defined(_WIN64)
    // On x64/ARM64 the signature must be 1 (image-relative layout).
    if (col->signature != 1u)
        return false;

    // The self back-reference must round-trip: from_rva(pSelf) == col.
    // This is the strongest single validation available without reading
    // the full image map.
    if (from_rva<CompleteObjectLocator>(col->pSelf) != col)
        return false;
#else
    // On x86/ARM32 the signature must be 0 (absolute-pointer layout).
    if (col->signature != 0u)
        return false;
#endif

    // The ClassHierarchyDescriptor must be reachable and carry signature 0.
    const auto *chd = from_rva<ClassHierarchyDescriptor>(col->pClassDescriptor);
    if (!chd)
        return false;
    if (chd->signature != 0u)
        return false;

    return true;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Public detail functions
// ─────────────────────────────────────────────────────────────────────────────

bool abi_supports_rtti(const void *obj) noexcept
{
    return validate_col(read_col(obj));
}

bool abi_has_inheritance(const void *obj) noexcept
{
    const auto *col = read_col(obj);
    if (!validate_col(col))
        return false;

    const auto *chd = from_rva<ClassHierarchyDescriptor>(col->pClassDescriptor);
    // numBaseClasses includes the class itself as entry [0].
    // A value greater than 1 means at least one proper base exists.
    return chd->numBaseClasses > 1u;
}

std::vector<std::type_index> abi_get_hierarchy(const void *obj)
{
    const auto *col = read_col(obj);
    if (!validate_col(col))
        return {};

    const auto *chd = from_rva<ClassHierarchyDescriptor>(col->pClassDescriptor);
    if (chd->numBaseClasses == 0u)
        return {};

    // BaseClassArray: array of rva_t values (one per BCD), null-terminated.
    // Entry [0] is always the most-derived class itself (self-entry).
    const rva_t *bca = from_rva<rva_t>(chd->pBaseClassArray);
    if (!bca)
        return {};

    std::vector<std::type_index> result;
    result.reserve(chd->numBaseClasses);

    // Virtual bases appear multiple times in the BCA (once per path through
    // the hierarchy).  Deduplicate by TypeDescriptor pointer, which is unique
    // per type within a single PE image.
    std::unordered_set<const TypeDescriptor *> visited;

    for (std::uint32_t i = 0u; i < chd->numBaseClasses; ++i)
    {
        if (bca[i] == 0)
            break; // null terminator

        const auto *bcd = from_rva<BaseClassDescriptor>(bca[i]);
        if (!bcd)
            continue;

        const auto *td = from_rva<TypeDescriptor>(bcd->pTypeDescriptor);
        if (!td)
            continue;

        if (!visited.insert(td).second)
            continue; // already added (deduplicate virtual base)

        // TypeDescriptor is layout-compatible with std::type_info:
        // both begin with a vtable pointer followed by a spare/name pointer.
        const std::type_info &ti = *reinterpret_cast<const std::type_info *>(td);
        result.emplace_back(ti);
    }

    return result;
}

} // namespace detail
} // namespace rtti_hierarchy
