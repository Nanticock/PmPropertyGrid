#include "PropertyContext.h"
#include "PropertyContext_p.h"

#include "PropertyGrid.h"

using namespace PM;

PropertyContext &PropertyContextPrivate::invalidContext()
{
    static PropertyContext result;

    return result;
}

PropertyContext PropertyContextPrivate::createContext(const Property &property)
{
    return PropertyContext(property, QVariant(), nullptr, nullptr);
}

PropertyContext PropertyContextPrivate::createContext(const QVariant &value, bool valid)
{
    PropertyContext result;
    result.m_value = value;
    result.m_isValid = valid;

    return result;
}

PropertyContext PropertyContextPrivate::createContext(const PropertyContext &other, const QVariant &newValue)
{
    PropertyContext result = other;
    result.m_value = newValue;

    return result;
}

PropertyContext PropertyContextPrivate::createContext(const Property &property, const QVariant &value, void *object, PropertyGrid *propertyGrid)
{
    return PropertyContext(property, value, object, propertyGrid);
}

void PropertyContextPrivate::setValue(PropertyContext &context, const QVariant &value)
{
    context.m_value = value;
    notifyValueChanged(context, value);
}

PropertyContextPrivate::valueChangedSlot_t PropertyContextPrivate::defaultValueChangedSlot()
{
    static valueChangedSlot_t result = [](const QVariant &) {};

    return result;
}

void PropertyContextPrivate::disconnectValueChangedSlot(PropertyContext &context)
{
    context.m_valueChangedSlot = defaultValueChangedSlot();
}

void PropertyContextPrivate::connectValueChangedSlot(PropertyContext &context, const valueChangedSlot_t &slot)
{
    context.m_valueChangedSlot = slot;
}

void PropertyContextPrivate::notifyValueChanged(const PropertyContext &context, const QVariant &newValue)
{
    context.m_valueChangedSlot(newValue);
}

Property PropertyContext::property() const
{
    return m_property;
}

QVariant PropertyContext::value() const
{
    return m_value;
}

bool PropertyContext::isValid() const
{
    return m_isValid;
}

PropertyGrid *PropertyContext::propertyGrid() const
{
    return m_propertyGrid;
}

PropertyContext::PropertyContext() : PropertyContext(Property(), QVariant(), nullptr, nullptr)
{
}

PropertyContext::PropertyContext(const Property &property, const QVariant &value, void *object, PropertyGrid *propertyGrid) :
    m_property(property),
    m_value(value),
    m_object(object),
    m_propertyGrid(propertyGrid),
    m_isValid(!property.name().isEmpty()),
    m_valueChangedSlot(PropertyContextPrivate::defaultValueChangedSlot())
{
}
