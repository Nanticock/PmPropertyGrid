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

namespace rtti_hierarchy
{
namespace detail
{
namespace
{

using namespace ms_rtti;

// ─────────────────────────────────────────────────────────────────────────────
// Read the CompleteObjectLocator from the vtable slot before the address point
// ─────────────────────────────────────────────────────────────────────────────

const CompleteObjectLocator *read_col(const void *obj) noexcept
{
    // Step 1: dereference the vfptr stored as the first word of the object.
    const void *vfptr = *static_cast<const void *const *>(obj);

    // Step 2: sizeof(void*) bytes before the address_point is a full-width
    // absolute pointer to the COL — on ALL targets.
    //   x86/ARM32: 4-byte ptr (sizeof(void*) == 4)
    //   x64/ARM64: 8-byte ptr (sizeof(void*) == 8)
    // The COL's INTERNAL fields (pTypeDescriptor, pClassDescriptor, pSelf)
    // use 4-byte image-relative RVAs on x64 — resolved separately by from_rva<T>.
    const char *slot = static_cast<const char *>(vfptr) - sizeof(void *);
    return *reinterpret_cast<const CompleteObjectLocator *const *>(slot);
}

// ─────────────────────────────────────────────────────────────────────────────
// Sanity-check a COL before trusting any of its fields
// ─────────────────────────────────────────────────────────────────────────────

bool validate_col(const CompleteObjectLocator *col) noexcept
{
    if (!col)
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
