// rtti_hierarchy.cpp — public API, ABI-independent.
//
// All ABI-specific logic lives in exactly one of:
//   rtti_hierarchy_itaniumABI.cpp  (GCC / Clang on Linux, macOS, Android, …)
//   rtti_hierarchy_msABI.cpp       (MSVC / clang-cl on Windows)
//
// CMake selects the correct file at configure time based on the detected
// compiler ABI (MSVC variable).

#include "rtti_hierarchy.h"
#include "rtti_hierarchy_impl_p.h"

namespace rtti_hierarchy
{

bool supports_rtti(const void *obj) noexcept
{
    if (!obj)
        return false;
    try
    {
        return detail::abi_supports_rtti(obj);
    }
    catch (...)
    {
        return false;
    }
}

bool has_inheritance(const void *obj) noexcept
{
    if (!obj)
        return false;
    try
    {
        return detail::abi_has_inheritance(obj);
    }
    catch (...)
    {
        return false;
    }
}

std::vector<std::type_index> get_hierarchy(const void *obj) noexcept
{
    if (!obj)
        return {};
    try
    {
        return detail::abi_get_hierarchy(obj);
    }
    catch (...)
    {
        return {};
    }
}

} // namespace rtti_hierarchy
