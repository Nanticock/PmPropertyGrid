#pragma once

// Internal ABI dispatch interface.
//
// Exactly one translation unit — either rtti_hierarchy_itaniumABI.cpp or
// rtti_hierarchy_msABI.cpp — provides these three functions.  The common
// rtti_hierarchy.cpp calls them after performing the shared null-guard.
//
// Contracts:
//   - abi_supports_rtti   : obj != nullptr
//   - abi_has_inheritance : obj != nullptr
//   - abi_get_hierarchy   : obj != nullptr
//     May propagate std::bad_alloc; the public API layer catches it.

#include <typeindex>
#include <vector>

namespace rtti_hierarchy
{
namespace detail
{

bool abi_supports_rtti(const void *obj) noexcept;
bool abi_has_inheritance(const void *obj) noexcept;
std::vector<std::type_index> abi_get_hierarchy(const void *obj);

} // namespace detail
} // namespace rtti_hierarchy
