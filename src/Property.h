#ifndef PROPERTY_H
#define PROPERTY_H

#include "TemplateParameterChecks.h"

#include <QVariant>

#include <memory>
#include <typeindex>

namespace PM
{
struct Attribute;
struct Property;

using TypeId = std::type_index;

namespace internal
{
    struct AttributesFunctionHelper
    {
        // TODO: define these as plain function pointers
        using CreateFunc = std::function<std::unique_ptr<Attribute>()>;
        using CopyFunc = std::function<std::unique_ptr<Attribute>(const Attribute &)>;

        CreateFunc createFunc;
        CopyFunc copyFunc;
    };

    template <typename T>
    constexpr bool isAttribute()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isAttribute = std::is_base_of<Attribute, DecayedT>::value;

        static_assert(isAttribute, "T must be of type PM::Attribute");

        return isAttribute;
    }

    template <typename T>
    TypeId getTypeId();

    template <typename T>
    void registerAttribute();

    template <typename T>
    bool isRegisteredAttribute();

    // NOTE: this is a convenience function that is compatible with both Qt5 and Qt6
    int getVariantTypeId(const QVariant &value);
    bool canConvert(const QVariant &value, int typeId);

    bool isReadOnly(const Property &property);

    // TODO: is this function really needed?!!
    QVariant createDefaultVariantForType(int type);
} // namespace internal

struct Attribute
{
    // TODO: maybe in the future we can add support for dynamic polymorphism
};

// TODO: should this be renamed to PropertyDescriptor?!!
struct Property
{
    QString name;
    int type; // TODO: maybe make this readonly?!!

    // TODO: add an extra read-only member to hold the typeId.
    //       this is important because the type information should be preserved even if the value was invalid.
    //       currently if the value was set to an invalid QVariant all type information will be lost.
    //
    //       The type of this variable can be int, or QMetaType.
    //       int is how Qt5 used to store the type info.
    //       the drawback is that strong type checks in tings like function parameters doesn't work out-of-the-box
    //       in Qt6 this was replaced with using QMetaType instead.
    //       this incurs additional memory and processing overhead but ensures that strong type checks work properly
    //
    //       this is an example code that demonstrates this:
    /*
     *          QVariant value;
     *
     *          if (!value.isValid())
     *          #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
     *              result.value = QVariant(value.metaType(), nullptr); // ##Qt6 value.metaType() returns QMetaType
     *          #else
     *              result.value = QVariant(value.userType(), nullptr); // ##Qt5 value.userType() returns int
     *          #endif
     */
    //       another possible solution would be to use std::type_index.
    //       this will add the ability to support data types that are not registered as in the qmetatype system
    //       like std::strings and std::vectors and the added benefit of being independant of the changes that happen to Qt.
    //       but such decision will require us to implement some reliant / lightweight translation method
    //       that is able to map both systems to each other.
    //
    //       such method can be as simple as two static maps like this:
    /*
     *       std::unordered_map<std::type_index, int> s_typeIndexToQMetaTypeId = {
     *           {PM::internal::getTypeId<QString>(), qMetaTypeId<QString>()},
     *           {PM::internal::getTypeId<QSize>(), qMetaTypeId<QSize>()},
     *           {PM::internal::getTypeId<QVector2D>(), qMetaTypeId<QVector2D>()},
     *           {PM::internal::getTypeId<QMatrix4x4>(), qMetaTypeId<QMatrix4x4>()},
     *       };
     *
     *       // use a lambda function to automatically fill the inverse map
     *       std::unordered_map<int, std::type_index> s_qMetaTypeIdToTypeIndex = [&] {
     *           std::unordered_map<int, std::type_index> map;
     *           for (const auto& pair : s_typeIndexToQMetaTypeId)
     *               map[pair.second] = pair.first;
     *
     *           return map;
     *       }();
     *
     *       std::type_index getTypeId(int metaTypeId)
     *       {
     *           if (s_qMetaTypeIdToTypeIndex.find(metaTypeId) == s_qMetaTypeIdToTypeIndex.end())
     *               return typeid(void);
     *
     *           return s_qMetaTypeIdToTypeIndex[metaTypeId];
     *       }
     *
     *       int getMetaTypeId(std::type_index typeId)
     *       {
     *           if (s_typeIndexToQMetaTypeId.find(typeId) == s_typeIndexToQMetaTypeId.end())
     *               return QMetaType::UnknownType;
     *
     *           return s_typeIndexToQMetaTypeId[typeId];
     *       }
     */

    Property();
    Property(const Property &other);
    Property(const QString &name, int type);
    // TODO: add move constructor

    // TODO: add a templated constructor when the interface is finalized
    //       the user has the option to either let the constructor set the type based on the value
    //       or explicitly provide both the type and the value
    //

    // Property(const QString &name, int typeId, const QVariant &value) : name(name), typeId(typeId), value(value)
    // {
    // }

    template <typename... Attributes>
    Property(const QString &name, int typeId, Attributes &&...attributes) : Property(name, typeId)
    {
        // NOTE: this is just a trick to execute addAttribute on every attribute in the pack, a work-around for C++17 fold expressions
        int dummy[] = {(addAttribute(std::forward<Attributes>(attributes)), 0)...};

        Q_UNUSED(dummy)
    }

    template <typename... Attributes>
    Property(const QString &name, int typeId, const Attributes &...attributes) : Property(name, typeId)
    {
        // NOTE: this is just a trick to execute addAttribute on every attribute in the pack, a work-around for C++17 fold expressions
        int dummy[] = {(addAttribute(attributes), 0)...};

        Q_UNUSED(dummy)
    }

    // template <typename... Attributes>
    // Property(const QString &name, int typeId, const QVariant &value, const Attributes &...attributes) : Property(name, typeId, value)
    // {
    //     addAttribute(attributes...);
    // }

    Property &operator=(const Property &other);

    template <typename T, typename = internal::templateCheck_t<internal::isAttribute<T>() && internal::isCopyable<T>()>>
    void addAttribute(const T &attribute);

    template <typename T, typename = internal::templateCheck_t<internal::isAttribute<T>() && internal::isMovable<T>()>>
    void addAttribute(T &&attribute);

    template <typename T, typename = internal::templateCheck_t<internal::isAttribute<T>()>>
    T getAttribute() const;

    template <typename T, typename = internal::templateCheck_t<internal::isAttribute<T>()>>
    bool hasAttribute() const;

private:
    void setAttributesFromOther(const Property &other);

    template <typename T>
    const T *getAttributeAsT() const;
    const Attribute *getAttribute_impl(TypeId typeUid) const;

private:
    template <typename T>
    friend void internal::registerAttribute();

    template <typename T>
    friend bool internal::isRegisteredAttribute();

private:
    std::unordered_map<TypeId, std::unique_ptr<Attribute>> m_attributes;

    // FIXME: move to a more appropriate place
    static std::unordered_map<TypeId, internal::AttributesFunctionHelper> s_attributesRegistry;
};

//
// Basic attributes
//

struct DescriptionAttribute : public Attribute
{
    DescriptionAttribute() = default;

    inline explicit DescriptionAttribute(const QString &value) : value(value)
    {
    }

    QString value;
};

struct DefaultValueAttribute : public Attribute
{
    DefaultValueAttribute() = default;

    inline explicit DefaultValueAttribute(const QVariant &value) : value(value)
    {
    }

    QVariant value;
};

struct CategoryAttribute : public Attribute
{
    CategoryAttribute() = default;

    inline explicit CategoryAttribute(const QString &value) : value(value)
    {
    }

    QString value;
};

struct ReadOnlyAttribute : public Attribute
{
    inline ReadOnlyAttribute() : value(true)
    {
    }

    inline explicit ReadOnlyAttribute(bool value) : value(value)
    {
    }

    bool value;
};
} // namespace PM

template <typename T, typename>
inline void PM::Property::addAttribute(const T &attribute)
{
    using DecayedT = typename std::decay<T>::type;

    if (!internal::isRegisteredAttribute<DecayedT>())
        internal::registerAttribute<DecayedT>();

    m_attributes[PM::internal::getTypeId<DecayedT>()] = std::make_unique<DecayedT>(attribute);
}

template <typename T, typename>
inline void PM::Property::addAttribute(T &&attribute)
{
    using DecayedT = typename std::decay<T>::type;

    if (!internal::isRegisteredAttribute<DecayedT>())
        internal::registerAttribute<DecayedT>();

    m_attributes[PM::internal::getTypeId<DecayedT>()] = std::make_unique<DecayedT>(std::forward<T>(attribute));
}

template <typename T, typename>
inline T PM::Property::getAttribute() const
{
    using DecayedT = typename std::decay<T>::type;

    const DecayedT *result = getAttributeAsT<DecayedT>();

    if (result == nullptr)
        return DecayedT();

    return DecayedT(*result);
}

template <typename T, typename>
inline bool PM::Property::hasAttribute() const
{
    using DecayedT = typename std::decay<T>::type;

    return getAttributeAsT<DecayedT>() != nullptr;
}

template <typename T>
inline const T *PM::Property::getAttributeAsT() const
{
    const Attribute *result = getAttribute_impl(PM::internal::getTypeId<T>());

    return static_cast<const T *>(result);
}

template <typename T>
inline PM::TypeId PM::internal::getTypeId()
{
    return std::type_index(typeid(T));
}

template <typename T>
inline void PM::internal::registerAttribute()
{
    using DecayedT = typename std::decay<T>::type;

    AttributesFunctionHelper helper;
    helper.createFunc = []() { return std::make_unique<DecayedT>(); };
    helper.copyFunc = [](const Attribute &attr) { return std::make_unique<DecayedT>(static_cast<const T &>(attr)); };

    Property::s_attributesRegistry[PM::internal::getTypeId<DecayedT>()] = helper;
}

template <typename T>
inline bool PM::internal::isRegisteredAttribute()
{
    using DecayedT = typename std::decay<T>::type;

    return Property::s_attributesRegistry.find(PM::internal::getTypeId<DecayedT>()) != Property::s_attributesRegistry.end();
}

inline int PM::internal::getVariantTypeId(const QVariant &value)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return value.typeId();
#else
    return value.type();
#endif
}

inline bool PM::internal::canConvert(const QVariant &value, int typeId)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QMetaType variantType = value.metaType();
    const QMetaType targetType(typeId);

    return QMetaType::canConvert(variantType, targetType);
#else
    return value.canConvert(typeId);
#endif
}

inline bool PM::internal::isReadOnly(const Property &property)
{
    return property.hasAttribute<PM::ReadOnlyAttribute>() && property.getAttribute<PM::ReadOnlyAttribute>().value == true;
}

#endif // PROPERTY_H
