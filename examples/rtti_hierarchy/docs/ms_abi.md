# Microsoft Visual C++ ABI — RTTI Structures Reference

## Overview

The Microsoft C++ ABI is used by:
- **MSVC** (cl.exe) on all Windows targets
- **Clang-cl** (`clang.exe --target=*-windows-msvc`) on Windows
- Targets: x86 (32-bit), x86-64, ARM32 (Thumb-2), ARM64 (AArch64)

The ABI is not formally published by Microsoft. All knowledge here is derived from:
- LLVM reverse-engineering in `clang/lib/CodeGen/MicrosoftCXXABI.cpp`
- The Clang source comment block: _"MS RTTI Overview"_ near the bottom of that file
- Practical reverse-engineering of MSVC-compiled binaries

## Primary Sources

| Source | Notes |
|--------|-------|
| LLVM `clang/lib/CodeGen/MicrosoftCXXABI.cpp` | All structure definitions and code generation logic |
| LLVM `clang/lib/AST/VTableBuilder.cpp` | VTable and VBTable context, `MethodVFTableLocation` |
| LLVM `clang/lib/AST/MicrosoftMangle.cpp` | Name mangling for RTTI symbols |
| Clang `clang/include/clang/AST/VTableBuilder.h` | `VPtrInfo`, `VPtrInfoVector` types |
| Clang `clang/include/clang/Basic/ABI.h` | `MSInheritanceModel` enum |
| Windows SDK `<typeinfo>` | Declares `type_info`; layout is ABI-compatible with `TypeDescriptor` |

The LLVM source was studied directly from:
`https://raw.githubusercontent.com/llvm/llvm-project/main/clang/lib/CodeGen/MicrosoftCXXABI.cpp`

---

## High-Level Structure Overview

The MS ABI RTTI is composed of five distinct, inter-referencing structure types:

```
Object (vfptr in it)
  │
  └─► VFTable[−1] ──────────────────────────────► CompleteObjectLocator (COL)
                                                         │
                        ┌────────────────────────────────┤
                        │                                │
                        ▼                                ▼
                  TypeDescriptor               ClassHierarchyDescriptor
                  (== std::type_info)                    │
                                                         ▼
                                                  BaseClassArray[]
                                                         │
                                              ┌──────────┴──────────┐
                                              ▼                     ▼
                                     BaseClassDescriptor   BaseClassDescriptor
                                              │
                                              └─► TypeDescriptor (for that base)
                                              └─► ClassHierarchyDescriptor (for that base)
```

All pointer-like references inside these structures are:
- **x86 / ARM32**: Absolute 32-bit pointers (same as regular pointers on those targets)
- **x64 / ARM64**: **32-bit image-relative offsets** (RVA = value − `__ImageBase`), NOT
  64-bit pointers. This halves the size of all RTTI cross-references.

---

## Pointer Model — Image-Relative Encoding (x64 / ARM64)

On 64-bit Windows, all internal RTTI cross-references are stored as `int32_t` RVAs:

```cpp
// To dereference an RVA field:
extern "C" const char __ImageBase;  // PE image base (declared in linker script)

template<typename T>
const T* rva_to_ptr(int32_t rva) {
    return reinterpret_cast<const T*>(&__ImageBase + rva);
}
```

`__ImageBase` is a symbol provided by the MSVC linker (and LLD). Its address equals the
load address of the PE image (`HMODULE`).

The `CompleteObjectLocator` on x64 also carries a `pSelf` field (its own RVA back to
itself). The runtime uses this to recover the COL address from the stored `vfptr[-1]` RVA,
without needing `__ImageBase` directly:

```
// x64 vfptr[-1] contains: RVA of COL
// COL.selfRVA contains: RVA of COL itself
// Therefore: COL address = &__ImageBase + vfptr[-1]
// Sanity: rva_to_ptr(col->selfRVA) should equal col
```

---

## Memory Layout — Object → RTTI

### x86 (32-bit), ARM32

```
Object layout
──────────────
[ vfptr   ] → points to address_point in VFTable

VFTable memory layout
──────────────────────────────────────
[ COL*  ] ← absolute 4-byte pointer; vfptr[-1] reads this
[ func0 ] ← vfptr points here (address_point = slot 0)
[ func1 ]
  ...
```

Reading the COL on x86/ARM32:
```cpp
const void* vptr = *static_cast<const void* const*>(obj);
const COL*  col  = *reinterpret_cast<const COL* const*>(
                       static_cast<const char*>(vptr) - sizeof(void*));
```

### x64 (AMD64), ARM64

```
Object layout
──────────────
[ vfptr   ] → points to address_point in VFTable

VFTable memory layout
──────────────────────────────────────
[ RVA of COL ] ← 4-byte image-relative signed int; vfptr occupies 8 bytes but
                 only the first 4 bytes carry the RVA; last 4 are padding/alignment
[ func0      ] ← vfptr points here
[ func1      ]
  ...
```

Reading the COL on x64/ARM64:
```cpp
const void*   vptr   = *static_cast<const void* const*>(obj);
// vfptr[-1] is one slot before address_point; each slot is sizeof(void*)=8 bytes wide
// The RVA is stored as int32_t at the start of that 8-byte slot
const int32_t rva    = *reinterpret_cast<const int32_t*>(
                           static_cast<const char*>(vptr) - sizeof(void*));
extern "C" const char __ImageBase;
const COL*    col    = reinterpret_cast<const COL*>(&__ImageBase + rva);
```

---

## Structure 1: TypeDescriptor (= `std::type_info`)

```cpp
// Variable-size structure; the LLVM struct type is named "rtti.TypeDescriptorN"
// where N is the byte length of the name string.
struct TypeDescriptor {
    const void* pVFTable;  // → ??_7type_info@@6B@  (type_info's vtable in msvcrt.dll)
    void*       spare;     // null initially; used by the runtime for caching
    char        name[1];   // MS-decorated name, variable length, null-terminated
                           // Starts with '.' followed by the decorated name without '?'
                           // Example: Foo → ".?AUFoo@@"
                           //          ns::Bar → ".?AVBar@ns@@"
};
```

**Key fact**: `TypeDescriptor` is layout-compatible with `std::type_info`. The `pVFTable`
field makes every `TypeDescriptor` a valid `std::type_info` object at runtime. This is why
`typeid()` returns a `std::type_info&` on MSVC — the runtime simply casts the
`TypeDescriptor*` to `std::type_info*`.

The mangled symbol name for `TypeDescriptor` is `??_R0<decorated-type>@8` on MSVC.
Example for `class Foo`: `??_R0?AVFoo@@8`.

**Name string format**: The name starts with `.` and contains the MS-decorated class name.
Examples:
| C++ type | Stored name string |
|---|---|
| `struct Foo` | `.?AUFoo@@` |
| `class Foo` | `.?AVFoo@@` |
| `class ns::Bar` | `.?AVBar@ns@@` |
| `int` | `.H` |

---

## Structure 2: CompleteObjectLocator (COL)

```cpp
// LLVM struct type name: "rtti.CompleteObjectLocator"
struct RTTICompleteObjectLocator {
    uint32_t  signature;          // 0 = x86 (absolute ptrs), 1 = x64 (image-relative)
    int32_t   offset;             // byte delta from the vfptr subobject to the
                                  // complete object ("this" adjustment)
    int32_t   cdOffset;           // vtordisp offset within the complete object
                                  // (relevant only during ctor/dtor execution;
                                  //  non-zero only when the class has vbases and
                                  //  the vfptr is inside a vbase subobject)
    // On x86: following fields are absolute pointers (4 bytes each)
    // On x64: following fields are int32_t image-relative RVAs
    /*ptr*/ TypeDescriptor*               pTypeDescriptor;
    /*ptr*/ RTTIClassHierarchyDescriptor* pClassDescriptor;
    /*ptr*/ RTTICompleteObjectLocator*    pSelf;  // x64 only; self back-reference
};
```

One COL is emitted per VFTable. A class with N vfptr locations has N COLs with different
`offset` values.

**`offset` field semantics**:
- For the primary VFTable (vfptr at offset 0 in the complete object): `offset = 0`
- For secondary VFTables (in base subobjects): `offset = -(distance from complete object
  start to this subobject's start)`

Example for `struct D : A, B` where A is at offset 0, B is at offset 8:
- COL for A's VFTable: `offset = 0`
- COL for B's VFTable: `offset = -8` (or rather the LLVM source uses positive
  `FullOffsetInMDC` which is the distance from the MDC start to the subobject)

**Sanity check for validation** (x64): `rva_to_ptr<COL>(col->selfRVA) == col`

---

## Structure 3: ClassHierarchyDescriptor (CHD)

```cpp
// LLVM struct type name: "rtti.ClassHierarchyDescriptor"
struct RTTIClassHierarchyDescriptor {
    uint32_t  signature;           // always 0 (reserved by the runtime)
    uint32_t  attributes;          // flags about the hierarchy topology
    uint32_t  numBaseClasses;      // total entries in BaseClassArray (including self as [0])
    /*ptr*/   RTTIBaseClassArray*  pBaseClassArray;  // RVA on x64, absolute on x86
};
```

`attributes` flags:
| Flag | Value | Meaning |
|---|---|---|
| `HasBranchingHierarchy` | `0x1` | Multiple inheritance (class has > 1 direct base) |
| `HasVirtualBranchingHierarchy` | `0x2` | Virtual inheritance **and** multiple inheritance |
| `HasAmbiguousBases` | `0x4` | **KNOWN BUG**: cl.exe computes this incorrectly. LLVM implements the correct computation but notes "We believe the field isn't actually used" by the runtime. Do not rely on this flag. |

---

## Structure 4: BaseClassArray (BCA)

Not a named structure in LLVM — it is simply an array:

```cpp
// Conceptually: const RTTIBaseClassDescriptor* [numBaseClasses + 1]
// Each element is: int32_t RVA on x64, absolute ptr on x86
// Null-terminated (an extra null entry is appended after all descriptors)
```

**Critical properties**:
1. **First entry is the most-derived class itself** — "BaseClassArray is a misnomer" (LLVM
   source comment). Entry [0] always describes the class that owns the CHD.
2. **Pre-order depth-first** traversal of the entire hierarchy (not just direct bases).
3. **Virtual bases appear multiple times** — once for each path through which they are
   reached, unlike Itanium where they appear once.
4. **Null terminator** — one extra null entry is appended after all real entries.
5. **Tree navigation**: Each `BaseClassDescriptor` has a `numContainedBases` field giving
   the count of entries that follow it in the array as its subtree descendants. This allows
   O(1) skipping:
   ```cpp
   // Skip over a subtree rooted at entry[i]:
   i = i + 1 + entry[i]->numContainedBases;
   ```

---

## Structure 5: BaseClassDescriptor (BCD)

```cpp
// LLVM struct type name: "rtti.BaseClassDescriptor"
struct RTTIBaseClassDescriptor {
    /*ptr*/ TypeDescriptor*               pTypeDescriptor;    // RVA on x64
    uint32_t                              numContainedBases;  // subtree size below this
    struct PMD {
        int32_t mdisp;  // member displacement: byte offset of base in the containing object
                        // -1 if the base is reached via virtual inheritance (use pdisp/vdisp)
        int32_t pdisp;  // vbptr displacement: -1 if the base is not virtual,
                        // otherwise: byte offset from the derived object's start to its vbptr
        int32_t vdisp;  // vbtable displacement: byte offset inside the vbtable to the entry
                        // that gives the offset to the virtual base
    } where;            // "pointer to member data" (PMD) — describes how to find this base
    uint32_t            attributes;         // flags
    /*ptr*/ RTTIClassHierarchyDescriptor* pClassDescriptor;  // RVA on x64; CHD for this base
};
```

`attributes` flags:
| Flag | Value | Meaning |
|---|---|---|
| `IsPrivateOnPath` | `0x1 \| 0x8` | Access is private somewhere on the path from MDC to this base |
| `IsAmbiguous` | `0x2` | This base is ambiguous in the MDC hierarchy |
| `IsPrivate` | `0x4` | This base is directly private |
| `IsVirtual` | `0x10` | This base is a virtual base |
| `HasHierarchyDescriptor` | `0x40` | Always set |

**Name mangling encodes all fields**: The linker symbol name of a `BaseClassDescriptor`
encodes `OffsetInVBase`, `VBPtrOffset`, `VBTableOffset`, and `Flags`. This enables the
linker to COMDAT-merge identical descriptors across translation units aggressively.

**PMD (Pointer to Member Data) structure** — how to locate a base subobject:

For a **non-virtual base**:
- `mdisp` = byte offset from the start of the containing subobject to this base
- `pdisp` = `-1`
- `vdisp` = `0`

For a **virtual base**:
- `mdisp` = `0` (or irrelevant; the vbtable is used instead)
- `pdisp` = byte offset from the object's start to its `vbptr` (virtual base table pointer)
- `vdisp` = byte offset within the vbtable to the entry containing the offset to this vbase

---

## Virtual Base Table (VBTable)

The VBTable is a separate structure (not part of RTTI proper, but relevant for finding
virtual bases during `dynamic_cast`):

```
VBTable layout (array of int32_t):
  [0]  = offset from vbptr itself back to the start of the object that contains it
         (always negative or zero; equals -(vbptr offset within object))
  [i]  = offset from vbptr to the i-th virtual base (1-indexed)
```

A vbptr stored in an object points into this table. The VBTable for a class is a global
constant array with symbol name mangled by `mangleCXXVBTable`.

---

## Runtime Functions

These are exported from `msvcr*.dll` / `vcruntime*.dll`:

### `typeid` → `__RTtypeid`
```c
// Returns a pointer to the TypeDescriptor (= std::type_info*) of the dynamic type.
// Throws std::bad_typeid if inptr is null (for pointer typeid).
void* __RTtypeid(void* inptr);
```

### `dynamic_cast` → `__RTDynamicCast`
```c
// PVOID __RTDynamicCast(
//   PVOID inptr,       // "this" pointer, adjusted so vfptr is accessible
//   LONG  VfDelta,     // byte offset from 'inptr' to the vfptr used for lookup
//   PVOID SrcType,     // TypeDescriptor* of the static type of inptr
//   PVOID TargetType,  // TypeDescriptor* of the desired target type
//   BOOL  isReference  // true if the cast is dynamic_cast<T&>, throws on failure
// );
void* __RTDynamicCast(void* inptr, long VfDelta,
                      void* SrcType, void* TargetType, int isReference);
```

`VfDelta` is required when the vfptr used for lookup is not at the start of the object
being cast — specifically when the polymorphic vfptr lives inside a virtual base subobject.
The `performBaseAdjustment` function in LLVM handles finding the right vfptr to use.

### `dynamic_cast<void*>` → `__RTCastToVoid`
```c
void* __RTCastToVoid(void* inptr);
```

### `throw` → `_CxxThrowException`
```c
// Throws a C++ exception. Declared as __declspec(noreturn).
// On x86: stdcall calling convention.
void _CxxThrowException(void* pExceptionObject, struct _ThrowInfo* pThrowInfo);
```

---

## Exception Handling Structures (`.xdata` section)

These structures are placed in the `.xdata` PE section (not `.rdata`).

### CatchableType

Describes how the runtime can catch a thrown object as one specific type:

```cpp
// LLVM struct type name: "eh.CatchableType"
struct CatchableType {
    uint32_t  Flags;                // type property flags
    /*ptr*/   TypeDescriptor*  pType;          // RVA on x64; unqualified type
    int32_t   NonVirtualAdjustment; // byte offset from thrown object to this type's subobject
    int32_t   OffsetToVBPtr;        // -1 if not virtual; else: byte offset to vbptr
    int32_t   VBTableIndex;         // 0 if not virtual; else: vbtable entry index × 4
    uint32_t  Size;                 // sizeof(thrown type)
    /*ptr*/   void*  pCopyCtor;     // RVA on x64; copy constructor (null if trivial)
                                    // May be a "copying closure" thunk (see below)
};
```

`Flags`:
| Bit | Value | Meaning |
|---|---|---|
| 0 | `0x1` | Scalar type (not a class, or no copy ctor needed) |
| 2 | `0x4` | Type has virtual bases |
| 4 | `0x10` | Type is `std::bad_alloc` (special treatment) |

**Copying closure thunk**: If the copy constructor has non-default calling convention or
extra parameters (e.g., a templated copy ctor), the compiler emits a `Ctor_CopyingClosure`
thunk that matches the expected signature. This is referenced via `pCopyCtor`.

### CatchableTypeArray

Lists all types a given thrown object can be matched against:

```cpp
// LLVM struct type name: "eh.CatchableTypeArrayN" (N = NumEntries)
struct CatchableTypeArray {
    uint32_t  NumEntries;
    /*ptr*/   CatchableType* [NumEntries];  // RVA on x64
};
```

Contents:
- One entry per public unambiguous base class of the thrown type (any nesting depth)
- One entry for the thrown type itself
- For pointer throws: one entry for `void*`
- For `std::nullptr_t` throws: one entry for `void*` (MSVC behavior, even though
  the standard cannot list all pointer types; this matches MSVC's behavior)
- Duplicate virtual bases are deduplicated via `llvm::SmallSetVector`

The number of entries is encoded in the mangled name, so the runtime knows the size without
a null terminator.

### ThrowInfo

Top-level exception description passed to `_CxxThrowException`:

```cpp
// LLVM struct type name: "eh.ThrowInfo"
struct ThrowInfo {
    uint32_t  Flags;                    // cv-qualifiers of the thrown expression
    /*ptr*/   void* pDestructor;        // RVA on x64; destructor of thrown object (null=trivial)
    /*ptr*/   void* pForwardCompat;     // always null (reserved for future use)
    /*ptr*/   CatchableTypeArray* pCTA; // RVA on x64
};
```

`Flags` (CV-qualifiers of the thrown expression, **separate** from the TypeDescriptor):
| Bit | Value | Meaning |
|---|---|---|
| 0 | `0x1` | `const` |
| 1 | `0x2` | `volatile` |
| 2 | `0x4` | `__unaligned` |

**Important**: The TypeDescriptor always stores the **cv-unqualified** type. Qualifiers live
here in ThrowInfo. This separation supports qualification conversions in catch matching
(e.g., `catch(const Foo&)` catches a thrown `Foo`).

---

## Multiple VFTables Per Class

Unlike Itanium (one vtable per class), the MS ABI generates **one VFTable per vfptr
location** in the class. A class with virtual bases can have vfptrs at multiple offsets.

Example:
```cpp
struct A { virtual void f(); };
struct B { virtual void g(); };
struct D : A, B { void f() override; void g() override; };
```

`D` has two vfptrs: one inherited from `A` at offset 0, one from `B` at offset `sizeof(A)`.
This produces two VFTables:
- `??_7D@@6BA@@@` — for the `A` part, COL `offset = 0`
- `??_7D@@6BB@@@` — for the `B` part, COL `offset = -sizeof(A)`

Both COLs point to the same `ClassHierarchyDescriptor`.

**For `get_hierarchy`**: Only the **primary VFTable** (vfptr at offset 0 in the complete
object) needs to be consulted. Its COL gives `offset = 0`, and its CHD describes the full
hierarchy.

---

## vtordisp

A hidden `int32_t` field inserted **4 bytes before each virtual base subobject** in the
complete object layout, but only while a constructor or destructor is executing. Its value
is:

```
vtordisp = (runtime virtual base offset from vbptr) − (compile-time virtual base offset)
```

It is always zero outside constructor/destructor execution. The `cdOffset` field in the COL
records where this vtordisp region is relative to the complete object, for use during
virtual destructor calls via virtual bases.

---

## `__declspec(novtable)` Effect on RTTI

When a class is marked `__declspec(novtable)`:
- `doStructorsInitializeVPtrs()` returns `false`
- Constructors never write vfptrs into the object
- Calling `typeid` or `dynamic_cast` through such a pointer is undefined behavior and will
  crash in practice
- Used for pure abstract base class optimization to reduce code size
- The RTTI structures (TypeDescriptor, COL, CHD, BCA) are still emitted normally

---

## Implementation Notes for `rtti_hierarchy`

### `supports_rtti(const void* obj)`

1. Return `false` if `obj == nullptr`
2. Read `vptr = *static_cast<const void* const*>(obj)` 
3. Obtain the COL using the platform-specific method (x86 vs x64, see above)
4. On x64: verify `rva_to_ptr<COL>(col->selfRVA) == col` as a sanity check
5. Check `col->signature == 0` (x86) or `col->signature == 1` (x64) as an additional check
6. Obtain the CHD: `rva_to_ptr<CHD>(col->hierDescRVA)`
7. Verify `chd->signature == 0`
8. Return `true` if all checks pass

**Why no try/catch**: Reading from arbitrary memory is undefined behavior regardless of
exception handling. The sanity checks (self-pointer round-trip on x64, signature values)
catch most garbage-pointer cases before reading deep into the structure. The higher-level
callers guarantee valid polymorphic objects, so this is defense-in-depth only.

### `has_inheritance(const void* obj)`

1. Perform `supports_rtti` check; return `false` on failure
2. Obtain CHD; return `chd->numBaseClasses > 1`
3. `numBaseClasses == 1` means only the class itself is listed (no bases)

### `get_hierarchy(const void* obj)`

1. Perform `supports_rtti` check; return `{}` on failure
2. Obtain the BCA from CHD
3. Skip entry [0] (the most-derived class itself) OR include it as the first element
   depending on desired semantics (the rtti_hierarchy API says "starting from most-derived")
4. For each entry i in [0, numBaseClasses):
   - Resolve BCD: `rva_to_ptr<BCD>(bca[i])`
   - Skip entries with `attributes & (IsPrivateOnPath | IsAmbiguous)` if only
     public/unambiguous bases are desired
   - Resolve TypeDescriptor: `rva_to_ptr<TypeDesc>(bcd->typeDescRVA)`
   - Cast to `std::type_info`: `*reinterpret_cast<const std::type_info*>(td)`
   - Push `std::type_index(ti)` to result
5. Use `std::unordered_set<const void*>` keyed on the TypeDescriptor pointer for
   deduplication — virtual bases appear multiple times in the BCA

**Attribute-based filtering**: The MS ABI places all bases in the BCA regardless of access.
Private and ambiguous bases are tagged via `attributes`. For the purposes of
`get_hierarchy`, including all bases (even private) is reasonable since the function
is about structural hierarchy, not accessibility.

### Required platform symbol

```cpp
#ifdef _WIN64
    extern "C" const char __ImageBase;
    // Declared but never defined in your code;
    // the linker provides it automatically for PE images.
#endif
```

No headers are needed for the MS RTTI structures — they are defined locally in the
implementation file.

---

## Platform-Specific Notes

### x86 (32-bit)
- All RTTI pointers are 32-bit absolute addresses
- `COL.signature == 0`; `COL.pSelf` field does not exist in the struct (dropped)
- `vfptr[-1]` is a `void* const*` → dereference gives `COL*`
- `_CxxThrowException` uses `__stdcall` on x86

### x86-64 (AMD64)
- All RTTI pointers are `int32_t` image-relative offsets
- `COL.signature == 1`; `COL.pSelf` field present and used for sanity checking
- `vfptr[-1]` slot is 8 bytes wide; the `int32_t` RVA is in the first 4 bytes

### ARM32 (Thumb-2)
- 32-bit absolute pointers (same model as x86)
- `COL.signature == 0`; no `pSelf`
- Function pointers may have thumb-bit (low bit set) — irrelevant for RTTI data pointers

### ARM64 (AArch64)
- 64-bit targets use image-relative int32 (same model as x64)
- `COL.signature == 1`; `COL.pSelf` present
- Otherwise identical to x64 from the RTTI perspective
