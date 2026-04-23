// Itanium C++ ABI implementation of rtti_hierarchy detail functions.
//
// Compiled on:  GCC and Clang on Linux, macOS, Android, BSD, MinGW-w64.
// Architectures: x86, x86-64, ARM32, ARM64.
//
// References:
//   https://itanium-cxx-abi.github.io/cxx-abi/abi.html  §2.9
//   clang/lib/CodeGen/ItaniumCXXABI.cpp  (LLVM monorepo, studied April 2026)
//   <cxxabi.h>  — stable public header from libstdc++ / libc++abi
//   docs/itanium_abi.md  (local reference document)
//
// Vtable layout (all Itanium targets, all architectures):
//
//   vtable[-2]  ptrdiff_t  offset-to-top
//   vtable[-1]  type_info*              ← typeid reads here
//   vtable[ 0]  vfunc_0                 ← vptr stored in object points here
//   vtable[ 1]  vfunc_1
//   ...
//
// The vptr is the first word of every polymorphic object.

#include "rtti_hierarchy_impl_p.h"

#include <cxxabi.h>
#include <typeindex>
#include <typeinfo>
#include <unordered_set>
#include <vector>

#ifdef __APPLE__
// Apple's system <cxxabi.h> exposes only demangling and exception helpers;
// the ABI type-info hierarchy is not declared there.  We redeclare the
// minimal layout needed for dynamic_cast here, matching the memory layout
// used by Apple's libc++abi (identical to upstream LLVM libc++abi).
// All destructors are declared but not defined, so the compiler emits
// external references that are resolved at link time from libc++abi.dylib.
namespace __cxxabiv1
{
    struct __class_type_info : std::type_info
    {
        ~__class_type_info() override;
    };
    struct __base_class_type_info
    {
        const __class_type_info *__base_type;
        long __offset_flags;
        enum __offset_flags_masks
        {
            __virtual_mask = 0x1,
            __public_mask  = 0x2,
            __hwm_bit      = 2,
            __offset_shift = 8
        };
    };
    struct __si_class_type_info : __class_type_info
    {
        ~__si_class_type_info() override;
        const __class_type_info *__base_type;
    };
    struct __vmi_class_type_info : __class_type_info
    {
        ~__vmi_class_type_info() override;
        unsigned int __flags;
        unsigned int __base_count;
        __base_class_type_info __base_info[1];
    };
} // namespace __cxxabiv1
#endif // __APPLE__

namespace rtti_hierarchy
{
namespace detail
{
    namespace
    {

        // ─────────────────────────────────────────────────────────────────────────────
        // Low-level vtable accessors
        // ─────────────────────────────────────────────────────────────────────────────

        // Read the type_info pointer from vtable slot -1.
        // obj must be non-null and point to a live polymorphic object.
        const std::type_info *read_type_info(const void *obj) noexcept
        {
            // obj → vptr value → vtable address_point
            const void * const *vptr = static_cast<const void * const *>(obj);
            const void * const *vtbl = static_cast<const void * const *>(*vptr);
            // vtbl[-1] is the type_info* stored one slot before the first vfunc
            return reinterpret_cast<const std::type_info *>(vtbl[-1]);
        }

        // Return the dynamic type_info cast to __class_type_info*, or nullptr if the
        // object is not of a class type (e.g., pointer or fundamental type_info).
        const __cxxabiv1::__class_type_info *as_class_ti(const void *obj) noexcept
        {
            return dynamic_cast<const __cxxabiv1::__class_type_info *>(read_type_info(obj));
        }

        // ─────────────────────────────────────────────────────────────────────────────
        // Recursive DFS hierarchy walker
        // ─────────────────────────────────────────────────────────────────────────────

        // Walk the type_info tree rooted at `ti`, appending each unique class to `out`.
        // `visited` prevents revisiting virtual bases shared by multiple paths
        // (diamond inheritance).
        //
        // May throw std::bad_alloc via emplace_back / insert; the public API catches it.
        void walk(const __cxxabiv1::__class_type_info *ti, std::vector<std::type_index> &out, std::unordered_set<const std::type_info *> &visited)
        {
            if (!ti)
                return;

            // Guard: insert returns false when the element was already present.
            if (!visited.insert(ti).second)
                return;

            out.emplace_back(*ti);

            using namespace __cxxabiv1;

            // Single-inheritance: one base via __base_type.
            if (const auto *si = dynamic_cast<const __si_class_type_info *>(ti))
            {
                walk(si->__base_type, out, visited);
                return;
            }

            // Virtual / multiple / complex inheritance: iterate __base_info[].
            if (const auto *vmi = dynamic_cast<const __vmi_class_type_info *>(ti))
            {
                for (unsigned i = 0; i < vmi->__base_count; ++i)
                    walk(vmi->__base_info[i].__base_type, out, visited);
                return;
            }

            // __class_type_info with no bases (root class) — nothing more to recurse.
        }

    } // anonymous namespace

    // ─────────────────────────────────────────────────────────────────────────────
    // Public detail functions
    // ─────────────────────────────────────────────────────────────────────────────

    bool abi_supports_rtti(const void *obj) noexcept
    {
        // as_class_ti performs dynamic_cast on the type_info itself.
        // A non-null result means: vptr is valid, vtable[-1] holds a real
        // __class_type_info*, so the object has full polymorphic RTTI.
        return as_class_ti(obj) != nullptr;
    }

    bool abi_has_inheritance(const void *obj) noexcept
    {
        using namespace __cxxabiv1;

        const auto *ti = as_class_ti(obj);
        if (!ti)
            return false;

        // __si_class_type_info → exactly one base (single inheritance).
        if (dynamic_cast<const __si_class_type_info *>(ti))
            return true;

        // __vmi_class_type_info → virtual/multiple/other; has bases if base_count > 0.
        if (const auto *vmi = dynamic_cast<const __vmi_class_type_info *>(ti))
            return vmi->__base_count > 0u;

        // __class_type_info with no bases (root class).
        return false;
    }

    std::vector<std::type_index> abi_get_hierarchy(const void *obj)
    {
        const auto *ti = as_class_ti(obj);
        if (!ti)
            return {};

        std::vector<std::type_index> result;
        std::unordered_set<const std::type_info *> visited;
        walk(ti, result, visited);
        return result;
    }

} // namespace detail
} // namespace rtti_hierarchy
