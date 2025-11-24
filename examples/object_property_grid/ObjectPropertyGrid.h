#ifndef OBJECTPROPERTYGRID_H
#define OBJECTPROPERTYGRID_H

#include <PropertyGrid.h>

class ObjectPropertyGrid : public PM::PropertyGrid
{
    Q_OBJECT
public:
    explicit ObjectPropertyGrid(QWidget *parent = nullptr);

    QList<QObject *> selectedObjects() const;
    void setSelectedObject(QObject *object);

    QObject *selectedObject() const;
    void setSelectedObjects(const QList<QObject *> &objects);

private:
    void updateProperties();

private:
    QList<QObject *> m_objects;
};

#endif // OBJECTPROPERTYGRID_H
