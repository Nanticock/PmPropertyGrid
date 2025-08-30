#include "Property.h"

using namespace PM;

// FIXME: move to a more appropriate place
std::unordered_map<TypeId, internal::AttributesFunctionHelper> Property::s_attributesRegistry;

QVariant PM::internal::createDefaultVariantForType(int type)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QVariant(QMetaType(type), nullptr);
#else
    return QVariant(QVariant::Type(type));
#endif
}

QString Property::name() const
{
    return m_name;
}

int Property::type() const
{
    return m_type;
}

Property::Property() : m_type(QMetaType::UnknownType)
{
}

Property::Property(const Property &other) : m_type(other.m_type), m_name(other.m_name)
{
    setAttributesFromOther(other);
}

Property::Property(const QString &name, int type) : m_type(type), m_name(name)
{
}

Property &Property::operator=(const Property &other)
{
    if (this == &other)
        return *this;

    m_name = other.m_name;
    m_type = other.m_type;

    setAttributesFromOther(other);

    return *this;
}

void Property::setAttributesFromOther(const Property &other)
{
    using namespace internal;

    m_attributes.clear();

    for (const auto &pair : other.m_attributes)
    {
        AttributesFunctionHelper::CopyFunc copyFunction = Property::s_attributesRegistry[pair.first].copyFunc;

        if (copyFunction == nullptr)
            continue;

        m_attributes.emplace(pair.first, copyFunction(*pair.second));
    }
}

const Attribute *Property::getAttribute_impl(TypeId typeUid) const
{
    auto it = m_attributes.find(typeUid);
    if (it != m_attributes.end())
        return it->second.get();

    return nullptr;
}
