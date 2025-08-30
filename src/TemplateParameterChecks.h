#ifndef TEMPLATEPARAMETERCHECKS_H
#define TEMPLATEPARAMETERCHECKS_H

#include <type_traits>

namespace PM
{
namespace internal
{
    // Implementing templateCheck compatible with C++14
    // Since fold expressions and std::bool_constant are C++17 features,
    // we'll use recursive templates and std::integral_constant instead.

    // TODO: maybe find a better more descriptive name?!!
    // Base case: When no checks are provided, the result is true.
    template <bool... Checks>
    struct templateCheck;

    template <>
    struct templateCheck<> : std::true_type
    {
    };

    // Recursive case: Evaluate each check.
    template <bool FirstCheck, bool... RemainingChecks>
    struct templateCheck<FirstCheck, RemainingChecks...> : std::conditional<FirstCheck, templateCheck<RemainingChecks...>, std::false_type>::type
    {
    };

    // Helper alias template for ease of use.
    template <bool... Checks>
    using templateCheck_t = typename templateCheck<Checks...>::type;

    // Now, rewriting the functions in the specified form.

    template <typename T>
    constexpr bool isDefaultConstructible()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isDefaultConstructibleValue = std::is_default_constructible<DecayedT>::value;

        static_assert(isDefaultConstructibleValue, "T must be default constructible");

        return isDefaultConstructibleValue;
    }

    template <typename T>
    constexpr bool isCopyable()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isCopyableValue = std::is_copy_constructible<DecayedT>::value && std::is_copy_assignable<DecayedT>::value;

        static_assert(isCopyableValue, "T must be copyable");

        return isCopyableValue;
    }

    template <typename T>
    constexpr bool isDestructible()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isDestructibleValue = std::is_destructible<DecayedT>::value;

        static_assert(isDestructibleValue, "T must be destructible");

        return isDestructibleValue;
    }

    template <typename T>
    constexpr bool isTriviallyCopyable()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isTriviallyCopyableValue = std::is_trivially_copyable<DecayedT>::value;

        static_assert(isTriviallyCopyableValue, "T must be trivially copyable");

        return isTriviallyCopyableValue;
    }

    template <typename T>
    constexpr bool isMovable()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isMovableValue = std::is_move_constructible<DecayedT>::value && std::is_move_assignable<DecayedT>::value;

        static_assert(isMovableValue, "T must be movable");

        return isMovableValue;
    }

    template <typename T>
    constexpr bool isTriviallyMovable()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isTriviallyMovableValue =
            std::is_trivially_move_constructible<DecayedT>::value && std::is_trivially_move_assignable<DecayedT>::value;

        static_assert(isTriviallyMovableValue, "T must be trivially movable");

        return isTriviallyMovableValue;
    }

    template <typename T>
    constexpr bool isMoveConstructible()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isMoveConstructibleValue = std::is_move_constructible<DecayedT>::value;

        static_assert(isMoveConstructibleValue, "T must be move constructible");

        return isMoveConstructibleValue;
    }

    template <typename T>
    constexpr bool isMoveAssignable()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isMoveAssignableValue = std::is_move_assignable<DecayedT>::value;

        static_assert(isMoveAssignableValue, "T must be move assignable");

        return isMoveAssignableValue;
    }

    template <typename T>
    constexpr bool isTriviallyMoveConstructible()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isTriviallyMoveConstructibleValue = std::is_trivially_move_constructible<DecayedT>::value;

        static_assert(isTriviallyMoveConstructibleValue, "T must be trivially move constructible");

        return isTriviallyMoveConstructibleValue;
    }

    template <typename T>
    constexpr bool isTriviallyMoveAssignable()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isTriviallyMoveAssignableValue = std::is_trivially_move_assignable<DecayedT>::value;

        static_assert(isTriviallyMoveAssignableValue, "T must be trivially move assignable");

        return isTriviallyMoveAssignableValue;
    }

    template <typename T>
    constexpr bool isTriviallyDestructible()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isTriviallyDestructibleValue = std::is_trivially_destructible<DecayedT>::value;

        static_assert(isTriviallyDestructibleValue, "T must be trivially destructible");

        return isTriviallyDestructibleValue;
    }

    template <typename T>
    constexpr bool isPolymorphic()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isPolymorphicValue = std::is_polymorphic<DecayedT>::value;

        static_assert(isPolymorphicValue, "T must have at least one virtual function");

        return isPolymorphicValue;
    }

    template <typename T>
    constexpr bool hasVirtualDestructor()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool hasVirtualDestructorValue = std::has_virtual_destructor<DecayedT>::value;

        static_assert(hasVirtualDestructorValue, "T must have a virtual destructor");

        return hasVirtualDestructorValue;
    }

    template <typename T>
    constexpr bool isEqualityComparable()
    {
        using DecayedT = typename std::decay<T>::type;

        // Check if 'operator==' is available and returns a type convertible to bool.
        constexpr bool isEqualityComparableValue = std::is_convertible<decltype(std::declval<DecayedT>() == std::declval<DecayedT>()), bool>::value;

        static_assert(isEqualityComparableValue, "T must have an equality (==) operator");

        return isEqualityComparableValue;
    }

    template <typename T>
    constexpr bool isInequalityComparable()
    {
        using DecayedT = typename std::decay<T>::type;

        // Check if 'operator!=' is available and returns a type convertible to bool.
        constexpr bool isInequalityComparableValue = std::is_convertible<decltype(std::declval<DecayedT>() != std::declval<DecayedT>()), bool>::value;

        static_assert(isInequalityComparableValue, "T must have an inequality (!=) operator");

        return isInequalityComparableValue;
    }
} // namespace internal
} // namespace PM

#endif // TEMPLATEPARAMETERCHECKS_H
