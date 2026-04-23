#pragma once

#include <typeindex>
#include <vector>

// Public API intentionally minimal and ABI-agnostic.
// All compiler / runtime specific logic is isolated in the .cpp file.

namespace rtti_hierarchy
{
// Returns true if the object appears to support RTTI traversal
// with the current compiler/runtime configuration.
bool supports_rtti(const void *obj) noexcept;

// Returns true if the dynamic type of the object has at least
// one base class in its inheritance hierarchy.
bool has_inheritance(const void *obj) noexcept;

// Returns the inheritance chain starting from the most-derived
// type down to its base classes.
//
// If RTTI is unavailable, unsupported, or validation fails,
// this function returns an empty vector.
std::vector<std::type_index> get_hierarchy(const void *obj) noexcept;
} // namespace rtti_hierarchy
