#pragma once

// MS C++ ABI — RTTI structure layout definitions.
//
// Used exclusively by rtti_hierarchy_msABI.cpp.
//
// Sources:
//   clang/lib/CodeGen/MicrosoftCXXABI.cpp (LLVM monorepo)
//   https://github.com/llvm/llvm-project (studied April 2026)
//   docs/ms_abi.md  (local reference document)
//
// Pointer model
// ─────────────
//   x64 / ARM64  (_WIN64 defined):
//     Pointer-like fields inside RTTI structures are stored as int32_t
//     image-relative offsets (RVA = address − __ImageBase).
//     The COL carries a pSelf back-reference for sanity checking.
//
//   x86 / ARM32  (_WIN64 not defined):
//     Those same fields hold absolute 32-bit pointer values, typed as
//     int32_t in the struct because they occupy 4 bytes either way.
//     from_rva<T> reinterprets the bit-pattern as an address directly.

#if !defined(_WIN32)
#    error "MicrosoftRTTI_p.h is for Windows (MSVC / clang-cl) targets only"
#endif

#include <cstddef>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// PE image base — supplied by the linker for any EXE or DLL; no lib needed.
// Declared at global scope so from_rva<T> can reference it from any namespace.
// ─────────────────────────────────────────────────────────────────────────────
#if defined(_WIN64)
extern "C" const char __ImageBase;
#endif

namespace ms_rtti
{

// ─────────────────────────────────────────────────────────────────────────────
// Platform-aware pointer resolution
// ─────────────────────────────────────────────────────────────────────────────

using rva_t = std::int32_t; // field type for all cross-references inside RTTI structs

#if defined(_WIN64)

// Resolve an image-relative signed offset to a typed pointer.
template <typename T>
inline const T *from_rva(rva_t rva) noexcept
{
    return reinterpret_cast<const T *>(&__ImageBase + static_cast<std::ptrdiff_t>(rva));
}

#else // x86 / ARM32

// On 32-bit targets the stored value is an absolute 32-bit pointer.
// Cast through uint32_t to preserve the bit pattern regardless of sign.
template <typename T>
inline const T *from_rva(rva_t rva) noexcept
{
    return reinterpret_cast<const T *>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(rva)));
}

#endif

// ─────────────────────────────────────────────────────────────────────────────
// Structure definitions
//
// All cross-reference fields (typed rva_t) are int32_t in both the x86 and
// x64 layouts — they are either absolute 32-bit pointers or 32-bit RVAs.
// Every other field is uint32_t / int32_t, so no padding is inserted and
// no explicit pack pragma is required.
// ─────────────────────────────────────────────────────────────────────────────

// TypeDescriptor  (layout-compatible with std::type_info)
// ─────────────────────────────────────────────────────────
// pVFTable and spare use the natural pointer width of the target.
// name[] is a variable-length MS-decorated name starting with '.'.
struct TypeDescriptor
{
    const void *pVFTable; // → ??_7type_info@@6B@  (type_info vtable in vcruntime)
    void *spare;          // runtime cache, null on construction
    char name[1];         // e.g. ".?AVFoo@@\0"  (null-terminated, variable length)
};

// PMD — Pointer-to-Member Data: describes how to locate a base subobject.
struct PMD
{
    std::int32_t mdisp; // non-virtual byte offset of base in the derived object
                        // irrelevant when pdisp != -1
    std::int32_t pdisp; // byte offset from object start to the vbptr field;
                        // -1 for non-virtual bases
    std::int32_t vdisp; // byte offset inside the vbtable to the vbase offset entry
};

// BaseClassDescriptor (BCD)
// ─────────────────────────
// One entry per class in the BaseClassArray (DFS pre-order, most-derived first).
// The symbol name encodes all fields so the linker can COMDAT-merge duplicates.
struct BaseClassDescriptor
{
    rva_t pTypeDescriptor;           // → TypeDescriptor for this class
    std::uint32_t numContainedBases; // count of entries in BCA below this node
                                     // (not counting this node itself)
    PMD where;                       // locator for this base in the containing object
    std::uint32_t attributes;        // flags — see BcdAttr below
    rva_t pClassDescriptor;          // → ClassHierarchyDescriptor for this class
};

enum BcdAttr : std::uint32_t
{
    BCD_IsPrivateOnPath = 0x1u | 0x8u, // access is private somewhere on path from MDC
    BCD_IsAmbiguous = 0x2u,            // base is ambiguous in the MDC hierarchy
    BCD_IsPrivate = 0x4u,              // base is directly private
    BCD_IsVirtual = 0x10u,             // base is a virtual base
    BCD_HasHierarchyDesc = 0x40u,      // always set
};

// ClassHierarchyDescriptor (CHD)
// ────────────────────────────────
// Describes the full class hierarchy.  One CHD per class; shared by all COLs
// of that class.
struct ClassHierarchyDescriptor
{
    std::uint32_t signature;      // always 0 (reserved by the runtime)
    std::uint32_t attributes;     // topology flags — see ChdAttr below
    std::uint32_t numBaseClasses; // total BCA entries including self at [0]
    rva_t pBaseClassArray;        // → array of rva_t (BCDs), null-terminated
};

enum ChdAttr : std::uint32_t
{
    CHD_HasBranching = 0x1u,        // multiple inheritance
    CHD_HasVirtualBranching = 0x2u, // virtual inheritance + multiple inheritance
    // 0x4u = HasAmbiguousBases: cl.exe computes this incorrectly; do not rely on it
};

// CompleteObjectLocator (COL)
// ────────────────────────────
// One COL per vfptr location in the class.  Referenced by the vtable slot
// immediately before the address point (the first function pointer).
//
//   x86/ARM32: vtable[-1] holds an absolute 4-byte pointer to the COL.
//   x64/ARM64: vtable[-1] holds an absolute 8-byte pointer to the COL.
//
// In both cases the slot is a full-width absolute pointer.  Only the COL's
// own internal cross-reference fields (pTypeDescriptor, pClassDescriptor,
// pSelf) use 32-bit image-relative RVAs on x64/ARM64.
struct CompleteObjectLocator
{
    std::uint32_t signature; // 0 = x86/absolute, 1 = x64/image-relative
    std::int32_t offset;     // byte offset from this vfptr subobject to MDC start
    std::int32_t cdOffset;   // vtordisp region offset (0 unless vbase dtor path)
    rva_t pTypeDescriptor;   // → TypeDescriptor for the most-derived class
    rva_t pClassDescriptor;  // → ClassHierarchyDescriptor for the MDC
#if defined(_WIN64)
    rva_t pSelf; // → this COL itself; used to recover __ImageBase at runtime
                 // and as a sanity-check that we found a real COL
#endif
};

} // namespace ms_rtti
