#include "ObjectPropertyGrid.h"

#include <QMetaProperty>

// Return list of all common readable properties across selected objects
static QList<QMetaProperty> getCommonProperties(const QList<QObject *> &objects)
{
    QList<QMetaProperty> result;
    if (objects.isEmpty())
        return result;

    const QMetaObject *meta = objects.first()->metaObject();
    for (int i = 0; i < meta->propertyCount(); ++i)
    {
        QMetaProperty prop = meta->property(i);
        if (!prop.isReadable())
            continue;

        const char *name = prop.name();
        bool common = true;
        for (QObject *obj : objects)
        {
            if (obj->metaObject()->indexOfProperty(name) >= 0)
                continue;

            common = false;
            break;
        }

        if (common)
            result.append(prop);
    }

    return result;
}

// Return merged value for a given property across selected objects
static QVariant getMergedPropertyValue(const QList<QObject *> &objects, const QMetaProperty &prop)
{
    if (objects.isEmpty())
        return QVariant();

    const char *name = prop.name();
    QVariant firstValue = objects.first()->property(name);

    for (QObject *obj : objects)
    {
        if (obj->property(name) == firstValue)
            continue;

        return QVariant(); // blank if different
    }

    return firstValue;
}

static PM::Property createGridProperty(const QMetaProperty &metaObjectProperty)
{
    PM::Property result(metaObjectProperty.name(), metaObjectProperty.type());

    if (!metaObjectProperty.isWritable())
        result.addAttribute(PM::ReadOnlyAttribute());

    return result;
}

ObjectPropertyGrid::ObjectPropertyGrid(QWidget *parent) : PM::PropertyGrid(parent)
{
}

void ObjectPropertyGrid::setSelectedObject(QObject *object)
{
    m_objects.clear();

    if (object != nullptr)
        m_objects.append(object);

    updateProperties();
}

void ObjectPropertyGrid::setSelectedObjects(const QList<QObject *> &objects)
{
    m_objects = objects;

    updateProperties();
}

QList<QObject *> ObjectPropertyGrid::selectedObjects() const
{
    return m_objects;
}

QObject *ObjectPropertyGrid::selectedObject() const
{
    return m_objects.isEmpty() ? nullptr : m_objects.first();
}

void ObjectPropertyGrid::updateProperties()
{
    clearProperties();

    if (m_objects.isEmpty())
        return;

    // Use helper functions
    const QList<QMetaProperty> commonProperties = getCommonProperties(m_objects);

    for (const QMetaProperty &property : commonProperties)
    {
        const QVariant propertyValue = getMergedPropertyValue(m_objects, property);

        addProperty(createGridProperty(property), propertyValue);

        // Connect changes back to all objects
        connect(this, &PM::PropertyGrid::propertyValueChanged, this,
                [this](const PM::PropertyContext &context)
                {
                    for (QObject *obj : qAsConst(m_objects))
                        obj->setProperty(context.property().name().toUtf8().constData(), context.value());
                });
    }
}
