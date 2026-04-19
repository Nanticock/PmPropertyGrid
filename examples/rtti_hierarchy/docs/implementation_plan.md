# `rtti_hierarchy` — Implementation Plan

## Scope

Implement the three functions declared in `rtti_hierarchy.h`:

```cpp
bool supports_rtti(const void* obj) noexcept;
bool has_inheritance(const void* obj) noexcept;
std::vector<std::type_index> get_hierarchy(const void* obj) noexcept;
```

These are **internal low-level primitives**. They are never called directly from user code.
Higher-level wrappers use `static_assert(std::is_polymorphic_v<T>)` and similar guards
before forwarding to these functions. The only safety guarantee these functions provide is
that a `nullptr` input returns a safe result. All other inputs are assumed to be valid
polymorphic objects by the contract with callers.

---

## ABI Detection

All logic is gated at compile time by a single preprocessor split:

```cpp
#if __has_include(<cxxabi.h>)
    // ── Itanium path ──────────────────────────────────────────────────────
    // GCC / Clang on Linux, macOS, Android, BSD, MinGW-w64
    // Architectures: x86, x86-64, ARM32, ARM64
#elif defined(_MSC_VER) || defined(_WIN32)
    // ── MS ABI path ───────────────────────────────────────────────────────
    // MSVC (cl.exe) and clang-cl on Windows
    // Architectures: x86, x86-64, ARM32, ARM64
#else
    #error "Unsupported compiler / ABI"
#endif
```

`__has_include(<cxxabi.h>)` is the most reliable test: MinGW on Windows includes it, MSVC
does not. It is supported by GCC ≥ 5 and all versions of Clang.

---

## File Structure

```
examples/rtti_hierarchy/
├── rtti_hierarchy.h            ← public API (unchanged)
├── rtti_hierarchy.cpp          ← single implementation file, ABI-split internally
├── rtti_hierarchy_tests.cpp    ← existing test suite
└── docs/
    ├── implementation_plan.md  ← this file
    ├── itanium_abi.md          ← Itanium ABI structures reference
    └── ms_abi.md               ← MS ABI structures reference
```

---

## Common Preamble (both ABIs)

```cpp
#include "rtti_hierarchy.h"
#include <cstdint>
#include <typeindex>
#include <typeinfo>
#include <unordered_set>
#include <vector>
```

---

## ABI Path 1: Itanium

**Reference**: [docs/itanium_abi.md](itanium_abi.md) — §"Struct Layouts", §"Diamond Walk"

### Required header

```cpp
#include <cxxabi.h>
```

`<cxxabi.h>` is part of the stable public ABI contract of both libstdc++ (GCC) and
libc++abi (LLVM). It exposes:

```cpp
namespace __cxxabiv1 {
    struct __class_type_info : std::type_info { ... };
    struct __si_class_type_info : __class_type_info {
        const __class_type_info* __base_type;
    };
    struct __vmi_class_type_info : __class_type_info {
        unsigned int __flags;
        unsigned int __base_count;
        __base_class_type_info __base_info[/* __base_count */];
    };
    struct __base_class_type_info {
        const __class_type_info* __base_type;
        long __offset_flags;                   // bits 0:virtual, 1:public, 8+:offset
    };
}
```

### Reading the vptr and type_info

```cpp
// Valid for all Itanium targets (x86, x64, ARM32, ARM64).
// obj must be non-null and point to a live polymorphic object.
static const std::type_info* read_type_info(const void* obj) noexcept
{
    const void* const* vptr_slot = static_cast<const void* const*>(obj);
    // vptr points to vtable address_point; type_info* is at vtable[-1]
    const void* const* vtable    = static_cast<const void* const*>(*vptr_slot);
    return reinterpret_cast<const std::type_info*>(vtable[-1]);
}
```

### `supports_rtti`

A non-null result of `dynamic_cast<const __cxxabiv1::__class_type_info*>(ti)` means the
type_info object belongs to a class type (which is the Itanium subclass hierarchy).
`__class_type_info` is polymorphic, so the dynamic_cast is valid.

```cpp
bool supports_rtti(const void* obj) noexcept
{
    if (!obj) return false;
    const std::type_info* ti = read_type_info(obj);
    return dynamic_cast<const __cxxabiv1::__class_type_info*>(ti) != nullptr;
}
```

### `has_inheritance`

Both `__si_class_type_info` and `__vmi_class_type_info` represent classes that have at
least one base. Testing for either subtype is sufficient.

```cpp
bool has_inheritance(const void* obj) noexcept
{
    if (!obj) return false;
    const std::type_info* ti = read_type_info(obj);
    if (dynamic_cast<const __cxxabiv1::__si_class_type_info*>(ti))  return true;
    if (dynamic_cast<const __cxxabiv1::__vmi_class_type_info*>(ti)) return true;
    return false;
}
```

### `get_hierarchy`

Recursive DFS over the type_info tree. A visited set (keyed on `const std::type_info*`)
prevents infinite loops in diamond-shaped hierarchies.

```cpp
static void walk_itanium(
    const __cxxabiv1::__class_type_info*          ti,
    std::vector<std::type_index>&                  out,
    std::unordered_set<const std::type_info*>&     visited) noexcept
{
    if (!visited.insert(ti).second) return;   // already seen
    out.emplace_back(*ti);

    if (auto* si = dynamic_cast<const __cxxabiv1::__si_class_type_info*>(ti)) {
        if (si->__base_type)
            walk_itanium(si->__base_type, out, visited);
    }
    else if (auto* vmi = dynamic_cast<const __cxxabiv1::__vmi_class_type_info*>(ti)) {
        for (unsigned i = 0; i < vmi->__base_count; ++i) {
            const __cxxabiv1::__base_class_type_info& base = vmi->__base_info[i];
            if (base.__base_type)
                walk_itanium(base.__base_type, out, visited);
        }
    }
    // __class_type_info (no bases) → nothing more to walk
}

std::vector<std::type_index> get_hierarchy(const void* obj) noexcept
{
    if (!obj) return {};
    const std::type_info* ti = read_type_info(obj);
    auto* class_ti = dynamic_cast<const __cxxabiv1::__class_type_info*>(ti);
    if (!class_ti) return {};

    std::vector<std::type_index> result;
    std::unordered_set<const std::type_info*> visited;
    walk_itanium(class_ti, result, visited);
    return result;
}
```

### Itanium edge cases

| Case | Behaviour |
|---|---|
| Non-polymorphic object | Caller must not reach here; `read_type_info` would read garbage |
| Diamond virtual base | Visited-set guard prevents duplicate entries |
| ARM64/Apple non-unique RTTI | Bit 63 of type name pointer is set as flag; handled transparently by `type_info::operator==` and `type_index`; no special handling needed at this level |
| Relative-vtable ABI | Not yet widely deployed; the `type_info*` at vtable[-1] is still a resolved pointer by the time the program runs (the loader resolves it); same code works |
| z/OS dual-encoded names | Fully transparent to `type_index` |

---

## ABI Path 2: MS ABI

**Reference**: [docs/ms_abi.md](ms_abi.md) — all sections

### No external headers required

All RTTI structures are defined locally in the implementation file using exact field
layouts derived from LLVM source analysis.

### Structure definitions

```cpp
namespace ms_rtti {

// Pointer-like field type: absolute on x86/ARM32, image-relative int32 on x64/ARM64
#if defined(_WIN64)
    using rva_t = int32_t;
    extern "C" const char __ImageBase;
    template<typename T>
    static const T* from_rva(rva_t rva) noexcept {
        return reinterpret_cast<const T*>(&__ImageBase + rva);
    }
#else
    // On 32-bit targets the "RVA" fields are actually full 32-bit absolute pointers,
    // stored as int32_t because that is what the struct layout dictates.
    using rva_t = int32_t;
    template<typename T>
    static const T* from_rva(rva_t rva) noexcept {
        return reinterpret_cast<const T*>(static_cast<uintptr_t>(
            static_cast<uint32_t>(rva)));
    }
#endif

#pragma pack(push, 4)

struct TypeDescriptor {
    const void* pVFTable;   // → ??_7type_info@@6B@
    void*       spare;
    char        name[1];    // variable length; MS-decorated name starting with '.'
};

struct PMD {
    int32_t mdisp;   // member displacement; -1 if virtual base
    int32_t pdisp;   // vbptr displacement; -1 if not virtual
    int32_t vdisp;   // vbtable displacement
};

struct BaseClassDescriptor {
    rva_t    pTypeDescriptor;     // → TypeDescriptor
    uint32_t numContainedBases;   // subtree size in BaseClassArray
    PMD      where;
    uint32_t attributes;          // see flags below
    rva_t    pClassDescriptor;    // → ClassHierarchyDescriptor
};

// BaseClassDescriptor attributes
enum : uint32_t {
    BCD_IsPrivateOnPath = 0x1 | 0x8,
    BCD_IsAmbiguous     = 0x2,
    BCD_IsPrivate       = 0x4,
    BCD_IsVirtual       = 0x10,
    BCD_HasHierDesc     = 0x40,
};

struct ClassHierarchyDescriptor {
    uint32_t signature;         // always 0
    uint32_t attributes;        // HasBranchingHierarchy=0x1, HasVirtualBranching=0x2
    uint32_t numBaseClasses;    // includes entry[0] = self
    rva_t    pBaseClassArray;   // → array of rva_t, null-terminated
};

struct CompleteObjectLocator {
    uint32_t signature;         // 0 = x86 (absolute), 1 = x64 (image-relative)
    int32_t  offset;            // vfptr subobject → complete object delta
    int32_t  cdOffset;          // vtordisp region offset
    rva_t    pTypeDescriptor;   // → TypeDescriptor
    rva_t    pClassDescriptor;  // → ClassHierarchyDescriptor
#if defined(_WIN64)
    rva_t    pSelf;             // self back-reference for sanity checking
#endif
};

#pragma pack(pop)

} // namespace ms_rtti
```

### Reading the vptr and COL

```cpp
static const ms_rtti::CompleteObjectLocator*
read_col(const void* obj) noexcept
{
    // vfptr is the first word of the object
    const void* const* vptr_slot = static_cast<const void* const*>(obj);
    const void* const* vtable    = static_cast<const void* const*>(*vptr_slot);

    // vtable[-1] is one slot (sizeof(void*) bytes) before the address point.
    // On x64: that slot holds an int32_t RVA; on x86: it holds an absolute ptr.
#if defined(_WIN64)
    const int32_t rva = *reinterpret_cast<const int32_t*>(
                            static_cast<const char*>(*vptr_slot) - sizeof(void*));
    return ms_rtti::from_rva<ms_rtti::CompleteObjectLocator>(rva);
#else
    return *reinterpret_cast<const ms_rtti::CompleteObjectLocator* const*>(vtable - 1);
#endif
}
```

### Sanity validation

```cpp
static bool validate_col(const ms_rtti::CompleteObjectLocator* col) noexcept
{
#if defined(_WIN64)
    // Signature must be 1 on x64
    if (col->signature != 1) return false;
    // Self back-reference must round-trip
    if (ms_rtti::from_rva<ms_rtti::CompleteObjectLocator>(col->pSelf) != col)
        return false;
#else
    if (col->signature != 0) return false;
#endif
    // CHD signature must be 0
    const auto* chd = ms_rtti::from_rva<ms_rtti::ClassHierarchyDescriptor>(
                          col->pClassDescriptor);
    return chd->signature == 0;
}
```

### `supports_rtti`

```cpp
bool supports_rtti(const void* obj) noexcept
{
    if (!obj) return false;
    const auto* col = read_col(obj);
    if (!col) return false;
    return validate_col(col);
}
```

### `has_inheritance`

```cpp
bool has_inheritance(const void* obj) noexcept
{
    if (!obj) return false;
    const auto* col = read_col(obj);
    if (!col || !validate_col(col)) return false;
    const auto* chd = ms_rtti::from_rva<ms_rtti::ClassHierarchyDescriptor>(
                          col->pClassDescriptor);
    // numBaseClasses == 1 means only the class itself; > 1 means at least one base
    return chd->numBaseClasses > 1;
}
```

### `get_hierarchy`

```cpp
std::vector<std::type_index> get_hierarchy(const void* obj) noexcept
{
    if (!obj) return {};
    const auto* col = read_col(obj);
    if (!col || !validate_col(col)) return {};

    const auto* chd = ms_rtti::from_rva<ms_rtti::ClassHierarchyDescriptor>(
                          col->pClassDescriptor);
    if (chd->numBaseClasses == 0) return {};

    // BaseClassArray: array of rva_t entries, null-terminated
    const ms_rtti::rva_t* bca =
        ms_rtti::from_rva<ms_rtti::rva_t>(chd->pBaseClassArray);

    std::vector<std::type_index> result;
    result.reserve(chd->numBaseClasses);

    // Deduplication: virtual bases appear multiple times in the BCA.
    // Key on TypeDescriptor pointer (unique per type in a single image).
    std::unordered_set<const ms_rtti::TypeDescriptor*> visited;

    for (uint32_t i = 0; i < chd->numBaseClasses; ++i) {
        if (bca[i] == 0) break;   // null terminator

        const auto* bcd = ms_rtti::from_rva<ms_rtti::BaseClassDescriptor>(bca[i]);
        if (!bcd) continue;

        // Skip private-on-path or ambiguous bases
        // (keep them if the caller wants the full structural picture;
        //  they are included here since get_hierarchy is a structural query)
        // Optionally:
        // if (bcd->attributes & (ms_rtti::BCD_IsPrivateOnPath |
        //                        ms_rtti::BCD_IsAmbiguous)) continue;

        const auto* td = ms_rtti::from_rva<ms_rtti::TypeDescriptor>(
                             bcd->pTypeDescriptor);
        if (!td) continue;

        if (!visited.insert(td).second) continue;   // already added

        // TypeDescriptor is layout-compatible with std::type_info
        const std::type_info& ti = *reinterpret_cast<const std::type_info*>(td);
        result.emplace_back(ti);
    }

    return result;
}
```

### MS ABI edge cases

| Case | Behaviour |
|---|---|
| Non-polymorphic object | COL signature / self-reference sanity check returns false |
| `__declspec(novtable)` class | vfptr is uninitialized → `read_col` returns garbage; `validate_col` catches this via the signature/self-reference checks in the common case |
| Multiple VFTables | Only the primary vfptr (at offset 0 in MDC) is ever stored in the object's first word; `read_col` always reads the correct COL for `supports_rtti` and `has_inheritance` |
| Virtual bases in BCA | Appear multiple times; deduplicated via the `visited` set |
| `HasAmbiguousBases` flag bug | Not relied upon; base-level `IsAmbiguous` per-BCD attribute is used directly instead |
| x86 (`_WIN32` but not `_WIN64`) | Absolute pointers stored as int32_t; `from_rva` does a zero-extending cast back to pointer |
| ARM32 on Windows | Same code path as x86 (32-bit absolute pointers, `signature == 0`) |
| ARM64 on Windows | Same code path as x64 (image-relative int32, `signature == 1`, `pSelf` present) |

---

## noexcept Compliance

All three public functions are `noexcept`. Internally:

- `dynamic_cast` on a type_info object (Itanium path) does not throw
- No `std::bad_alloc` path: `std::vector::reserve` and `unordered_set::insert` can throw.
  These must be wrapped in `try { ... } catch (...) { return {}; }` inside
  `get_hierarchy` to honour the `noexcept` contract.
- The Itanium `walk_itanium` helper should be declared `noexcept` and have its allocation
  calls wrapped similarly, or the allocation can be done in the public function and the
  helper can be passed a pre-allocated container.

---

## Testing Matrix

| Scenario | `supports_rtti` | `has_inheritance` | `get_hierarchy` |
|---|---|---|---|
| `nullptr` | `false` | `false` | `{}` |
| Non-polymorphic struct | N/A (caller guarantees polymorphic) | N/A | N/A |
| Root polymorphic class (no bases) | `true` | `false` | `[self]` |
| Single inheritance | `true` | `true` | `[derived, base]` |
| Multiple inheritance | `true` | `true` | `[derived, base1, base2]` |
| Diamond (virtual bases) | `true` | `true` | `[D, L, R, Base]` (no duplicates) |
| Heap-allocated object | same as stack | same | same |
| Base ptr → derived object | reflects derived type | reflects derived | reflects derived |

The existing tests in `rtti_hierarchy_tests.cpp` cover all these scenarios. The
implementation must pass all of them on both ABI paths.

---

## Build Configuration

No CMake changes are required. The implementation is a single `.cpp` file that uses
`__has_include` to branch at compile time. Both `<cxxabi.h>` (Itanium) and the locally
defined MS RTTI structures (MS ABI) are header-only dependencies already available on
their respective platforms.

The `__ImageBase` symbol on Windows is provided automatically by the linker for any PE
image (EXE or DLL) — no pragma or lib specification is needed.
