#ifndef PROPERTYGRID_H
#define PROPERTYGRID_H

#include "QtCompat_p.h"
#include "PropertyEditor.h"

#include <QDebug>

// TODO: add support for Dark mode and themes of other Operating systems
//        refer to: https://www.qt.io/blog/dark-mode-on-windows-11-with-qt-6.5

namespace PM
{
class PropertyGridPrivate;

class PropertyGrid : public QWidget
{
    Q_OBJECT

    friend class PM::PropertyGridPrivate;

public:
    explicit PropertyGrid(QWidget *parent = nullptr);
    ~PropertyGrid();

    // TODO: should we return a PropertyContext?!!
    // TODO: if we returned a PropertyContext, should we return it by ref (as RVO and NRVO will not work)
    // @@ CORE
    void addProperty(const Property &property, const QVariant &value = QVariant(), void *object = nullptr);

    // @@ CONVENIENCE
    template <typename... Attributes>
    void addProperty(const QString &name, const QVariant &value, const Attributes &...attributes)
    {
        if (!value.isValid())
            qWarning() << "PropertyGrid::addProperty(): Cannot infer the type of property" << name
                       << "because the provided QVariant value is invalid.";

        addProperty(Property(name, internal::getVariantTypeId(value), attributes...), value);
    }

    // TODO: change to return false if a property editor returns subProperties list with invalid names?!!
    template <typename T, typename = internal::templateCheck_t<internal::isPropertyEditor<T>()>>
    void addPropertyEditor();

    // FIXME: find a better name
    bool showCategories() const;
    void setShowCategories(bool value);

    bool setPropertyValue(const QString &propertyName, const QVariant &value);

    PropertyContext getPropertyContext(const QString &propertyName) const;

signals:
    void propertyValueChanged(const PM::PropertyContext &context);

private: // stable internal functions
    void addPropertyEditor_impl(TypeId, std::unique_ptr<PropertyEditor> &&editor);

private:
    PropertyGridPrivate *d;
};
} // namespace PM

template <typename T, typename>
inline void PM::PropertyGrid::addPropertyEditor()
{
    addPropertyEditor_impl(internal::getTypeId<T>(), std::make_unique<T>());
}

#endif // PROPERTYGRID_H
