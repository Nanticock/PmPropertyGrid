#include "ObjectPropertyGrid.h"

#include <QMetaProperty>

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

    const QMetaObject *meta = m_objects.first()->metaObject();

    for (int i = 0; i < meta->propertyCount(); ++i)
    {
        QMetaProperty prop = meta->property(i);
        if (!prop.isReadable())
            continue;

        const QString name = prop.name();

        // Check if property exists on all objects
        bool common = true;
        for (QObject *obj : qAsConst(m_objects))
        {
            if (obj->metaObject()->indexOfProperty(name.toUtf8().constData()) < 0)
            {
                common = false;
                break;
            }
        }
        if (!common)
            continue;

        // Determine merged value
        QVariant firstValue = m_objects.first()->property(name.toUtf8().constData());
        bool allSame = true;
        for (QObject *obj : qAsConst(m_objects))
        {
            if (obj->property(name.toUtf8().constData()) != firstValue)
            {
                allSame = false;

                break;
            }
        }

        PM::Property property(name, prop.type());
        const QVariant displayValue = allSame ? firstValue : QVariant(); // blank if different
        addProperty(property, displayValue);

        // Connect changes back to all objects
        connect(this, &PM::PropertyGrid::propertyValueChanged, this,
                [this, name](const PM::PropertyContext &context)
                {
                    for (QObject *obj : qAsConst(m_objects))
                        obj->setProperty(name.toUtf8().constData(), context.value());
                });
    }
}
