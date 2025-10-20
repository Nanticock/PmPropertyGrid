#ifndef PROPERTYCONTEXT_P_H
#define PROPERTYCONTEXT_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the PM::PropertyGrid API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
//

#include "PropertyContext.h"

namespace PM
{
class PropertyContextPrivate
{
public:
    using valueChangedSlot_t = std::function<void(const QVariant &)>;

public:
    static PropertyContext &invalidContext();

    static PropertyContext createContext(const Property &property);
    static PropertyContext createContext(const QVariant &value, bool valid = true);
    static PropertyContext createContext(const PropertyContext &other, const QVariant &newValue);
    static PropertyContext createContext(const Property &property, const QVariant &value, void *object, PropertyGrid *propertyGrid);

    static void setValue(PropertyContext &context, const QVariant &value);

    static valueChangedSlot_t defaultValueChangedSlot();

    static void disconnectValueChangedSlot(PropertyContext &context);
    static void connectValueChangedSlot(PropertyContext &context, const valueChangedSlot_t &slot);

    static void notifyValueChanged(const PropertyContext &context, const QVariant &newValue);
};
} // namespace PM

#endif // PROPERTYCONTEXT_P_H
