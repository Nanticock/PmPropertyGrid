#ifndef PROPERTYCONTEXT_H
#define PROPERTYCONTEXT_H

#include "Property.h"

#include <QPointer>

namespace PM
{
class PropertyGrid;
class PropertyContextPrivate;

class PropertyContext
{
    friend class PM::PropertyContextPrivate;

public:
    Property property() const;
    QVariant value() const;

    bool isValid() const;

    template <typename T>
    T object() const // WARNING: this function doesn't perform any checks for type-safety
    {
        return getObjectValue<T>(std::is_pointer<T>());
    }

    PropertyGrid *propertyGrid() const;

private: // TODO: wouldn't this cause problems when writing unit tests for user property editors?
    PropertyContext();
    PropertyContext(const Property &property, const QVariant &value, void *object, PropertyGrid *propertyGrid);

    // function for pointer types
    template <typename T>
    T getObjectValue(std::true_type) const
    {
        return static_cast<T>(m_object);
    }

    // function for non-pointer types
    template <typename T>
    T getObjectValue(std::false_type) const
    {
        T *result = static_cast<T *>(m_object);
        if (result == nullptr)
            return T();

        return *result;
    }

private:
    Property m_property;
    QVariant m_value;
    void *m_object; // TODO: use std::any
    QPointer<PropertyGrid> m_propertyGrid;

    // Meta values
    bool m_isValid;
    std::function<void(const QVariant &)> m_valueChangedSlot;
};
} // namespace PM

#endif // PROPERTYCONTEXT_H
