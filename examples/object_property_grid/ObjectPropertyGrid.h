#ifndef OBJECTPROPERTYGRID_H
#define OBJECTPROPERTYGRID_H

#include <PropertyGrid.h>

class ObjectPropertyGrid : public PM::PropertyGrid
{
    Q_OBJECT
public:
    explicit ObjectPropertyGrid(QWidget *parent = nullptr);

    QObject *selectedObject() const;
    void setSelectedObject(QObject *object);

    QObjectList selectedObjects() const;
    void setSelectedObjects(const QObjectList &objects);

private:
    void updateProperties();

private:
    QObjectList m_objects;
    QVector<QMetaObject::Connection> m_objectsConnections;
};

#endif // OBJECTPROPERTYGRID_H
