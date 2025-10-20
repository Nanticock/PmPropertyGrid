#ifndef PROPERTYGRID_H
#define PROPERTYGRID_H

#include "PropertyEditor.h"
#include "QtCompat_p.h"

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
    void addProperty(const QString &name, const QVariant &value, const Attributes &...attributes);

    // TODO: change to return false if a property editor returns subProperties list with invalid names?!!
    template <typename T, typename = internal::templateCheck_t<internal::isPropertyEditor<T>()>>
    void addPropertyEditor();

    // FIXME: find a better name
    bool showCategories() const;
    void setShowCategories(bool value);

    bool setPropertyValue(const QString &propertyName, const QVariant &value);

    PropertyContext getPropertyContext(const QString &propertyName) const;

public: /* EXPERIMENTAL API */
    /**/
    template <typename OldEditor, typename NewEditor,
              // clang-format off
              typename = internal::templateCheck_t<internal::isPropertyEditor<OldEditor>()>,
              typename = internal::templateCheck_t<internal::isPropertyEditor<NewEditor>()>
              // clang-format on
              >
    void replacePropertyEditor();

    void clearProperties();            // Requested: 2
    QStringList propertyNames() const; // Requested: 2

signals:
    void propertyValueChanged(const PM::PropertyContext &context);

private: // stable internal functions
    void replacePropertyEditor_impl(TypeId oldEditorTypeId, TypeId newEditorTypeId, std::unique_ptr<PropertyEditor> &&editor);

    void addPropertyEditor_impl(TypeId typeId, std::unique_ptr<PropertyEditor> &&editor);

private:
    PropertyGridPrivate *d;
};
} // namespace PM

template <typename... Attributes>
inline void PM::PropertyGrid::addProperty(const QString &name, const QVariant &value, const Attributes &...attributes)
{
    if (!value.isValid())
        qWarning() << "PropertyGrid::addProperty(): Cannot infer the type of property" << name << "because the provided QVariant value is invalid.";

    addProperty(Property(name, internal::getVariantTypeId(value), attributes...), value);
}

template <typename OldEditor, typename NewEditor, typename, typename>
inline void PM::PropertyGrid::replacePropertyEditor()
{
    replacePropertyEditor_impl(internal::getTypeId<OldEditor>(), internal::getTypeId<NewEditor>(), std::make_unique<NewEditor>());
}

template <typename T, typename>
inline void PM::PropertyGrid::addPropertyEditor()
{
    addPropertyEditor_impl(internal::getTypeId<T>(), std::make_unique<T>());
}

#endif // PROPERTYGRID_H
