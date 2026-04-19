# Itanium C++ ABI — RTTI Structures Reference

## Overview

The Itanium C++ ABI is the standard C++ ABI used by:
- GCC and Clang on **Linux** (x86, x86-64, ARM32, ARM64/AArch64)
- Clang on **macOS** and **iOS** (x86-64, ARM64)
- Clang on **Android** (ARM, ARM64, x86, x86-64)
- Clang on **FreeBSD**, **OpenBSD**, **NetBSD**
- MinGW-w64 on Windows (GCC/Clang targeting GNU environment)
- Clang on **Fuchsia**, **WebAssembly**

The name "Itanium" is historical — the ABI was originally designed for the IA-64 (Itanium)
architecture but has become the universal POSIX C++ ABI regardless of CPU architecture.

## Primary Sources

| Source | URL |
|--------|-----|
| Official Itanium C++ ABI specification | https://itanium-cxx-abi.github.io/cxx-abi/abi.html |
| ABI exception handling specification | https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html |
| ARM AAPCS (ARM ABI supplement) | https://developer.arm.com/documentation/ihi0041/g/ |
| LLVM implementation | `clang/lib/CodeGen/ItaniumCXXABI.cpp` in the LLVM monorepo |
| Runtime header | `<cxxabi.h>` — part of libstdc++ and libc++abi public ABI |

The LLVM source was studied directly from:
`https://raw.githubusercontent.com/llvm/llvm-project/main/clang/lib/CodeGen/ItaniumCXXABI.cpp`

---

## Memory Layout — Object → RTTI

Every polymorphic object stores a **vptr** (virtual pointer) as its first word:

```
Object layout in memory
────────────────────────
[ vptr          ] ← first sizeof(void*) bytes of the object
[ field 1       ]
[ field 2       ]
  ...

vptr points to the vtable address point:
────────────────────────────────────────

vtable[-2]  ptrdiff_t   offset-to-top   (byte delta from this vptr subobject to complete object)
vtable[-1]  type_info*  RTTI pointer    ← typeid reads here
vtable[ 0]  void*       vfunc_0         ← vptr stored in the object points here
vtable[ 1]  void*       vfunc_1
  ...
```

Reading the type_info from a void* (all Itanium targets, all architectures):

```cpp
const void*  vptr = *static_cast<const void* const*>(obj);
const auto*  ti   = reinterpret_cast<const std::type_info* const*>(vptr)[-1];
```

### Relative VTABLE ABI variant (Android, Chrome OS builds)

On targets that enable `-fexperimental-relative-vtables`:
- All vtable slots are 32-bit signed offsets relative to the slot's own address
- The RTTI slot is still at index -1 but stores a 4-byte relative offset instead of a pointer
- Access uses the `llvm.load.relative` LLVM intrinsic at IR level
- At the binary level: `rtti_ptr = slot_addr + *reinterpret_cast<const int32_t*>(slot_addr)`
- This ABI is not yet widely deployed; the standard vtable layout is the common case

---

## type_info Class Hierarchy

All RTTI classes live in namespace `__cxxabiv1`. They are publicly declared in `<cxxabi.h>`.

### Base: `std::type_info`

```cpp
// From the C++ standard library; layout is:
class type_info {
    // vptr                    ← first word (points into one of the vtables below)
    const char* __type_name;   ← second word
    // ...
};
```

The `__type_name` pointer holds a null-terminated string. Its value is the **Itanium-mangled
name of the type, with the leading `_ZTS` prefix stripped** — i.e., the mangled name starts
at index 4.

Examples:
| C++ type | `_ZTS` symbol | Stored name (what `type_info::name()` returns) |
|---|---|---|
| `int` | `_ZTSi` | `"i"` |
| `Foo` | `_ZTSFoo` (hypothetical) | `"3Foo"` |
| `ns::Bar` | `_ZTSN2ns3BarE` | `"N2ns3BarE"` |

**Standard library types**: For all fundamental types (`void`, `bool`, `char`, `int`, ...,
`long double`, `float128`, `char8_t`, `char16_t`, `char32_t`, `__int128`) and their `T*` /
`const T*` pointer variants, the `type_info` objects are defined in the runtime library
(libstdc++abi or libc++abi), **not** emitted by the compiler. The compiler emits `extern`
references to them.

### Vtable symbols → concrete type_info subclass

The vtable pointer at the start of every `type_info` subclass object identifies its concrete
type. These vtable symbols are defined in the C++ runtime library:

| Vtable symbol (Itanium-mangled) | C++ class | Used for |
|---|---|---|
| `_ZTVN10__cxxabiv123__fundamental_type_infoE` | `__fundamental_type_info` | `void`, `bool`, all integer/float types, vectors, complex, `nullptr_t` |
| `_ZTVN10__cxxabiv117__array_type_infoE` | `__array_type_info` | Array types (`T[]`, `T[N]`) |
| `_ZTVN10__cxxabiv120__function_type_infoE` | `__function_type_info` | Function types |
| `_ZTVN10__cxxabiv116__enum_type_infoE` | `__enum_type_info` | Enum and enum class |
| `_ZTVN10__cxxabiv117__class_type_infoE` | `__class_type_info` | Class/struct with **no bases** (root) |
| `_ZTVN10__cxxabiv120__si_class_type_infoE` | `__si_class_type_info` | **Single inheritance** (see criteria below) |
| `_ZTVN10__cxxabiv121__vmi_class_type_infoE` | `__vmi_class_type_info` | **Virtual / multiple / other inheritance** |
| `_ZTVN10__cxxabiv119__pointer_type_infoE` | `__pointer_type_info` | Pointer types (`T*`, `T* const`, etc.) |
| `_ZTVN10__cxxabiv129__pointer_to_member_type_infoE` | `__pointer_to_member_type_info` | Member pointers (`T C::*`, `R (C::*)(Args...)`) |

---

## Struct Layouts (from `<cxxabi.h>` and the ABI spec §2.9)

### `__class_type_info` (no bases)

```cpp
// ABI spec §2.9.5p4 — adds no data members to std::type_info
struct __class_type_info : public std::type_info {
    // [vtable ptr → _ZTVN10__cxxabiv117__class_type_infoE]
    // [const char* __type_name]
    // (no additional fields)
};
```

### `__si_class_type_info` (single inheritance)

```cpp
// ABI spec §2.9.5p6b
struct __si_class_type_info : public __class_type_info {
    // [vtable ptr → _ZTVN10__cxxabiv120__si_class_type_infoE]
    // [const char* __type_name]
    const __class_type_info* __base_type;   // pointer to the one base's type_info
};
```

**Criteria for use** (`CanUseSingleInheritance` in LLVM source):
1. The class has **exactly one** direct base
2. That base is **non-virtual**
3. That base is **public**
4. The class is **dynamic if and only if the base is** (non-dynamic empty base is allowed)

If any of these conditions fails, `__vmi_class_type_info` is used instead.

### `__vmi_class_type_info` (virtual / multiple / other inheritance)

```cpp
// ABI spec §2.9.5p5c
struct __vmi_class_type_info : public __class_type_info {
    // [vtable ptr → _ZTVN10__cxxabiv121__vmi_class_type_infoE]
    // [const char* __type_name]
    unsigned int __flags;        // topology flags
    unsigned int __base_count;   // number of direct proper base classes
    __base_class_type_info __base_info[__base_count];  // variable-length array
};
```

`__flags` values (can be combined):
| Flag | Value | Meaning |
|---|---|---|
| `VMI_NonDiamondRepeat` | `0x1` | Some base class appears more than once non-virtually |
| `VMI_DiamondShaped` | `0x2` | Diamond inheritance: a virtual base is shared |

### `__base_class_type_info` (element of the above array)

```cpp
// ABI spec §2.9.5p6c
struct __base_class_type_info {
    const __class_type_info* __base_type;  // pointer to base's type_info
    long                     __offset_flags;
};
```

`__offset_flags` bit layout:
| Bits | Meaning |
|---|---|
| bit 0 (`0x1`) | Base is **virtual** |
| bit 1 (`0x2`) | Base is **public** |
| bits 7:2 | reserved |
| bits (N-1):8 | **Byte offset** of base subobject (shift right by 8 to get value) |

On LLP64 (MinGW/Windows-Itanium) where `long` is 32 bits but pointers are 64 bits, the
field is promoted to `long long` to hold a 64-bit offset. LLVM detects this via
`TI.getTriple().isOSCygMing() && TI.getPointerWidth() > TI.getLongWidth()`.

### `__pointer_type_info`

```cpp
// ABI spec §2.9.5p7
struct __pointer_type_info : public std::type_info {
    // [vtable ptr → _ZTVN10__cxxabiv119__pointer_type_infoE]
    // [const char* __type_name]
    unsigned int             __flags;    // cv-qualification and completeness flags
    const std::type_info*    __pointee;  // type_info of the pointee type (unqualified)
};
```

`__flags` (PTI_* values):
| Flag | Value | Meaning |
|---|---|---|
| `PTI_Const` | `0x01` | Pointee has `const` qualifier |
| `PTI_Volatile` | `0x02` | Pointee has `volatile` qualifier |
| `PTI_Restrict` | `0x04` | Pointee has `restrict` qualifier |
| `PTI_Incomplete` | `0x08` | Pointee is or contains an incomplete class type |
| `PTI_ContainingClassIncomplete` | `0x10` | For member ptrs: containing class is incomplete |
| `PTI_Noexcept` | `0x40` | Pointee is a `noexcept` function type (C++17) |

### `__pointer_to_member_type_info`

```cpp
// ABI spec §2.9.5p9
struct __pointer_to_member_type_info : public __pointer_type_info {
    // [vtable ptr → _ZTVN10__cxxabiv129__pointer_to_member_type_infoE]
    // [const char* __type_name]
    // [unsigned int __flags]   (from __pointer_type_info)
    // [const std::type_info* __pointee]
    const __class_type_info* __context;  // the class A in "int A::*"
};
```

---

## `__dynamic_cast` Signature and Offset Hint

```cpp
// Defined in the C++ runtime library (libstdc++abi / libc++abi)
void* __dynamic_cast(
    const void*                    sub,              // "this" pointer
    const abi::__class_type_info*  src,              // static type RTTI (Src)
    const abi::__class_type_info*  dst,              // target type RTTI (Dst)
    std::ptrdiff_t                 src2dst_offset    // hint (see below)
);
```

The `src2dst_offset` hint (computed by `computeOffsetHint` in LLVM):
| Value | Meaning |
|---|---|
| `>= 0` | Src is a unique public non-virtual base of Dst at this byte offset |
| `-1` | No hint — virtual base exists somewhere in the path |
| `-2` | Src is **not** a public base of Dst at all |
| `-3` | Src is a **multiple** public non-virtual base of Dst (ambiguous) |

---

## `typeid` Lowering

For a polymorphic lvalue:
```cpp
// vtable[-1] is read directly; the compiler validates the vptr first
// (null-check if the source is a pointer, not if it is a reference)
const std::type_info& ti = typeid(*ptr);
```

LLVM lowers this as:
```cpp
// vtable is loaded via GetVTablePtr
Value = GEP(StdTypeInfoPtrTy, vptr, -1);  // vtable[-1]
Load(StdTypeInfoPtrTy, Value, pointer_align);
```

---

## Linkage Rules for type_info Objects

| Condition | Linkage |
|---|---|
| Type contains an incomplete class type in a pointer chain | `internal` (TU-local, no cross-TU resolution) |
| RTTI disabled (`-fno-rtti`) | `linkonce_odr` |
| Dynamic class (has virtual functions) | Same as vtable linkage (`weak_odr` or `external`) |
| Class with `__attribute__((weak))` | `weak_odr` |
| Other external types | `linkonce_odr` |
| Internal / anonymous linkage type | `internal` |

**Uniqueness guarantee**: The ABI requires all `type_info` objects for the same type across
all TUs to be merged into one at link time (via `weak_odr` COMDAT). This means `typeid(T) ==
typeid(T)` can be implemented as pointer equality. This guarantee is explicitly listed as
**not holding** on:

- ARM64/Apple (`AppleARM64CXXABI`): `shouldRTTIBeUnique()` returns `false`. The runtime
  must use **string comparison** for type identity. The high bit (bit 63) of the `__type_name`
  pointer is set as a flag to signal this at runtime.
- Any type with hidden visibility that might exist in multiple DSOs simultaneously.

---

## Diamond Inheritance Walk Algorithm

For `__vmi_class_type_info`, virtual bases appear at most once in a well-formed hierarchy
but the same virtual base can appear in multiple `__base_class_type_info` entries across
different levels. A traversal must use a visited set:

```
visited = {}
function walk(ti):
    if ti in visited: return
    visited.add(ti)
    emit(ti)
    if ti is __si_class_type_info:
        walk(ti.__base_type)
    else if ti is __vmi_class_type_info:
        for each entry in ti.__base_info:
            walk(entry.__base_type)
```

---

## Non-Polymorphic Object Handling

For a non-polymorphic object (no virtual functions), there is no vptr and no vtable.
Reading `*reinterpret_cast<const void* const*>(obj)` would read whatever data sits at the
start of the object — this is not a valid type_info pointer.

**Detection strategy**: The type must be known at compile time via `std::is_polymorphic_v<T>`
or, at runtime for opaque `void*`, by attempting to validate the vptr against the known
set of ABI vtable symbols. The low-level functions in `rtti_hierarchy` are internal
primitives — callers are responsible for ensuring the object is polymorphic before calling
them. In practice this is enforced by the higher-level wrappers via `static_assert`.

---

## Platform-Specific Notes

### x86 (32-bit)
- `ptrdiff_t` = 32 bits; `__offset_flags` uses `long` = 32 bits
- Vtable pointers are 4-byte aligned 32-bit addresses
- Everything else is identical to x86-64

### x86-64
- `ptrdiff_t` = 64 bits; `long` = 64 bits (LP64)
- No image-relative encoding (that is an MS ABI concept)
- `__offset_flags` high bits can hold 64-bit offsets

### ARM32
- Uses the **ARM AAPCS** ABI supplement on top of Itanium
- Method pointer encoding differs: virtual bit is in `adj` field rather than low bit of `ptr`
  (`UseARMMethodPtrABI = true`)
- Guard variable ABI differs (`UseARMGuardVarABI = true`)
- RTTI structures themselves are **identical** to standard Itanium
- Constructor/destructor functions return `this`

### ARM64 / AArch64
- Uses `UseARMMethodPtrABI = true` and `UseARMGuardVarABI = true`
- On Apple platforms (`AppleARM64CXXABI`): RTTI may be non-unique (bit 63 flag)
- `Use32BitVTableOffsetABI = true` on Apple (vtable component offsets are 32-bit)
- RTTI structures themselves are **identical** to standard Itanium

### z/OS
- Type name stored as dual-encoded: EBCDIC bytes followed by null followed by ASCII bytes
- `type_info::name()` returns the EBCDIC portion; the ASCII portion follows after the null

---

## Implementation Notes for `rtti_hierarchy`

### `supports_rtti(const void* obj)`

1. Return `false` if `obj == nullptr`
2. Read `vptr = *static_cast<const void* const*>(obj)`
3. Attempt `dynamic_cast<const __cxxabiv1::__class_type_info*>(ti)` where ti is read from
   `vptr[-1]`
4. Return `true` if the cast succeeds (the type_info is for a class type, meaning the object
   is polymorphic)

The `dynamic_cast` on the type_info itself is safe because `__class_type_info` is a
polymorphic class (it has virtual functions). A non-class type_info (e.g., for a pointer
type) would fail the cast and return `nullptr`.

### `has_inheritance(const void* obj)`

1. Perform steps 1–3 of `supports_rtti`; return `false` if it fails
2. Check if ti is `__si_class_type_info*` or `__vmi_class_type_info*`
3. Return `true` if either cast succeeds

`__si_class_type_info` inherits from `__class_type_info`, and `__vmi_class_type_info`
likewise — both express the presence of at least one base class.

### `get_hierarchy(const void* obj)`

1. Perform the `supports_rtti` check; return `{}` on failure
2. Recursively walk using the algorithm described in the Diamond Inheritance section above
3. Each visited type_info is pushed as a `std::type_index` into the output vector
4. Use `std::unordered_set<const std::type_info*>` (by pointer, not by value) for the
   visited set — this is correct because on Itanium type_info objects are globally unique
   (or compared by string for non-unique platforms, but the pointer uniqueness within a
   single process is sufficient for deduplication here)

### Header required

```cpp
#include <cxxabi.h>
// Provides:
//   __cxxabiv1::__class_type_info
//   __cxxabiv1::__si_class_type_info   → member: __base_type
//   __cxxabiv1::__vmi_class_type_info  → members: __flags, __base_count, __base_info[]
//   __cxxabiv1::__base_class_type_info → members: __base_type, __offset_flags
```

`<cxxabi.h>` is part of the stable public ABI of both libstdc++ and libc++abi. It is
available on all POSIX platforms that use the Itanium ABI.
