#include "rtti_hierarchy.h"

#include <cstdint>
#include <typeinfo>

namespace rtti_hierarchy
{
namespace
{
    // ------------------------------------------------------------
    // Common helpers
    // ------------------------------------------------------------

    inline bool valid_pointer(const void *p) noexcept
    {
        return p != nullptr;
    }

    inline const void *try_get_vptr(const void *obj) noexcept
    {
        if (!valid_pointer(obj))
            return nullptr;

        // Best-effort read — caller assumes pointer validity
        return *reinterpret_cast<const void * const *>(obj);
    }

#if defined(_MSC_VER)

    // ------------------------------------------------------------
    // MSVC ABI structures (locally defined to avoid including
    // internal headers like <rtti.h>)
    // ------------------------------------------------------------

    struct TypeDescriptor
    {
        void *pVFTable;
        void *spare;
        char name[1];
    };

    struct BaseClassDescriptor
    {
        TypeDescriptor *pTypeDescriptor;
        std::uint32_t numContainedBases;
        void *pmd;
        std::uint32_t attributes;
    };

    struct ClassHierarchyDescriptor
    {
        std::uint32_t signature;
        std::uint32_t attributes;
        std::uint32_t numBaseClasses;
        BaseClassDescriptor **pBaseClassArray;
    };

    struct CompleteObjectLocator
    {
        std::uint32_t signature;
        std::uint32_t offset;
        std::uint32_t cdOffset;
        TypeDescriptor *pTypeDescriptor;
        ClassHierarchyDescriptor *pClassHierarchyDescriptor;
    };

    inline const CompleteObjectLocator *get_locator(const void *obj) noexcept
    {
        auto vptr = try_get_vptr(obj);
        if (!vptr)
            return nullptr;

        auto table = reinterpret_cast<const void * const *>(vptr);
        return reinterpret_cast<const CompleteObjectLocator *>(table[-1]);
    }

    inline bool valid_rtti(const void *obj) noexcept
    {
        auto locator = get_locator(obj);
        if (!locator)
            return false;

        if (!locator->pClassHierarchyDescriptor)
            return false;

        if (!locator->pTypeDescriptor)
            return false;

        return true;
    }

    inline bool internal_has_inheritance(const void *obj) noexcept
    {
        auto locator = get_locator(obj);
        if (!locator)
            return false;

        auto chd = locator->pClassHierarchyDescriptor;
        if (!chd)
            return false;

        // numBaseClasses includes the type itself
        return chd->numBaseClasses > 1;
    }

    inline std::vector<std::type_index> internal_get_hierarchy(const void *obj) noexcept
    {
        std::vector<std::type_index> result;

        auto locator = get_locator(obj);
        if (!locator)
            return result;

        auto chd = locator->pClassHierarchyDescriptor;
        if (!chd)
            return result;

        auto array = chd->pBaseClassArray;
        if (!array)
            return result;

        for (std::uint32_t i = 0; i < chd->numBaseClasses; ++i)
        {
            auto base = array[i];
            if (!base || !base->pTypeDescriptor)
                continue;

            const char *name = base->pTypeDescriptor->name;
            if (!name)
                continue;

            // Fallback: create type_index from type_info when possible
            // We cannot reconstruct full type_info safely, so we use
            // name-based proxy via typeid(*obj) for first element.
            if (i == 0)
            {
                try
                {
                    result.emplace_back(typeid(*reinterpret_cast<const std::exception *>(obj)));
                }
                catch (...)
                {
                    // ignore
                }
            }
        }

        return result;
    }

#elif defined(__GNUC__) || defined(__clang__)

    // ------------------------------------------------------------
    // Itanium ABI (GCC / Clang)
    // ------------------------------------------------------------

#    include <cxxabi.h>

    using namespace __cxxabiv1;

    inline const __class_type_info *get_typeinfo(const void *obj) noexcept
    {
        auto vptr = try_get_vptr(obj);
        if (!vptr)
            return nullptr;

        auto table = reinterpret_cast<const void * const *>(vptr);
        auto ti = reinterpret_cast<const std::type_info *>(table[-1]);

        return dynamic_cast<const __class_type_info *>(ti);
    }

    inline bool valid_rtti(const void *obj) noexcept
    {
        auto ti = get_typeinfo(obj);
        return ti != nullptr;
    }

    inline bool has_base_recursive(const __class_type_info *ti) noexcept
    {
        if (dynamic_cast<const __si_class_type_info *>(ti))
            return true;

        if (auto vmi = dynamic_cast<const __vmi_class_type_info *>(ti))
            return vmi->__base_count > 0;

        return false;
    }

    inline bool internal_has_inheritance(const void *obj) noexcept
    {
        auto ti = get_typeinfo(obj);
        if (!ti)
            return false;

        return has_base_recursive(ti);
    }

    inline void collect_recursive(const __class_type_info *ti, std::vector<std::type_index> &out)
    {
        if (!ti)
            return;

        try
        {
            out.emplace_back(typeid(*reinterpret_cast<const std::exception *>(ti)));
        }
        catch (...)
        {
            // ignore failures
        }

        if (auto si = dynamic_cast<const __si_class_type_info *>(ti))
        {
            collect_recursive(si->__base_type, out);
        }
        else if (auto vmi = dynamic_cast<const __vmi_class_type_info *>(ti))
        {
            for (int i = 0; i < vmi->__base_count; ++i)
            {
                collect_recursive(vmi->__base_info[i].__base_type, out);
            }
        }
    }

    inline std::vector<std::type_index> internal_get_hierarchy(const void *obj) noexcept
    {
        std::vector<std::type_index> result;

        auto ti = get_typeinfo(obj);
        if (!ti)
            return result;

        collect_recursive(ti, result);
        return result;
    }

#else

    // ------------------------------------------------------------
    // Unknown compiler ABI
    // ------------------------------------------------------------

    inline bool valid_rtti(const void *) noexcept
    {
        return false;
    }

    inline bool internal_has_inheritance(const void *) noexcept
    {
        return false;
    }

    inline std::vector<std::type_index> internal_get_hierarchy(const void *) noexcept
    {
        return {};
    }

#endif

} // anonymous namespace

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------

bool supports_rtti(const void *obj) noexcept
{
    if (!valid_pointer(obj))
        return false;

    return valid_rtti(obj);
}

bool has_inheritance(const void *obj) noexcept
{
    if (!supports_rtti(obj))
        return false;

    return internal_has_inheritance(obj);
}

std::vector<std::type_index> get_hierarchy(const void *obj) noexcept
{
    if (!supports_rtti(obj))
        return {};

    return internal_get_hierarchy(obj);
}
} // namespace rtti_hierarchy
