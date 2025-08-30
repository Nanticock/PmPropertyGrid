#ifndef PROPERTYEDITOR_H
#define PROPERTYEDITOR_H

#include "Property.h"

#include <QTreeWidgetItem>

#include <QPointer>
#include <variant>

namespace PM
{
class PropertyGrid;
class PropertyEditor;

namespace internal
{
    class PropertyContextPrivate;

    template <typename T>
    constexpr bool isPropertyEditor()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isPropertyEditor = std::is_base_of<PropertyEditor, DecayedT>::value;

        static_assert(isPropertyEditor, "T must be of type PM::PropertyEditor");

        return isPropertyEditor;
    }
} // namespace internal

class PropertyContext
{
    friend class PM::internal::PropertyContextPrivate;

public:
    Property property() const;
    QVariant value() const;

    bool isValid() const;

    template <typename T>
    T object() const // WARNING: this function would never perform any checks for type-safety
    {
        return getObjectValue<T>(std::is_pointer<T>());
    }

    PropertyGrid *propertyGrid() const;

private: // TODO: wouldn't this cause problems when writing unit tests for user property editors?
    PropertyContext();
    PropertyContext(const Property &property, const QVariant &value, void *object, PropertyGrid *propertyGrid);

    // function for pointer types
    template <typename T>
    T getObjectValue(std::true_type) const
    {
        return static_cast<T>(m_object);
    }

    // function for non-pointer types
    template <typename T>
    T getObjectValue(std::false_type) const
    {
        T *result = static_cast<T *>(m_object);
        if (result == nullptr)
            return T();

        return *result;
    }

private:
    Property m_property;
    QVariant m_value;
    void *m_object; // TODO: use std::any
    QPointer<PropertyGrid> m_propertyGrid;

    // Meta values
    bool m_isValid;
    std::function<void(const QVariant &)> m_valueChangedSlot;
};

class PropertyEditor
{
public:
    enum EditStyle
    {
        None = 0,
        Modal = 1,
        DropDown = 2
    };

    PropertyEditor() = default;
    virtual ~PropertyEditor() = default;

    virtual bool canHandle(const PropertyContext &context) const;

    virtual QString toString(const PropertyContext &context) const;
    // TODO: does this function make any sense?!!
    // TODO: use std::expected instead of `QString *errorMessage`
    //       refer to: https://github.com/TartanLlama/expected
    virtual QVariant fromString(const QString &value, const PropertyContext &context, QString *errorMessage = nullptr) const;

    virtual EditStyle getEditStyle(const PropertyContext &context) const;
    // TODO: implement an editValue() function

    virtual QVariant modalClicked(const PropertyContext &context) const; // FIXME: is this the best we can do?!!
    // TODO: should we return a QStringList or a QVariantList
    virtual std::variant<QWidget *, QVariantList> getDropDown(const PropertyContext &context) const;

    virtual QPixmap getPreviewIcon(const PropertyContext &context) const; // FIXME: find a better name

protected:
    void editValue(const PropertyContext &context, const QVariant &newValue) const;
};

//
// Basic Property Editors
//

class SizePropertyEditor : public PropertyEditor
{
public:
    bool canHandle(const PropertyContext &context) const override;
    QString toString(const PropertyContext &context) const override;
};

class RectPropertyEditor : public PropertyEditor
{
public:
    bool canHandle(const PropertyContext &context) const override;
    QString toString(const PropertyContext &context) const override;
};

class FontPropertyEditor : public PropertyEditor
{
public:
    bool canHandle(const PropertyContext &context) const override;

    QString toString(const PropertyContext &context) const override;
    QVariant fromString(const QString &value, const PropertyContext &context, QString *errorMessage = nullptr) const override;

    QVariant modalClicked(const PropertyContext &context) const override;
    EditStyle getEditStyle(const PropertyContext &context) const override;

    QPixmap getPreviewIcon(const PropertyContext &context) const override;
};

class ColorPropertyEditor : public PropertyEditor
{
public:
    bool canHandle(const PropertyContext &context) const override;
    QString toString(const PropertyContext &context) const override;

    QVariant modalClicked(const PropertyContext &context) const override;
    EditStyle getEditStyle(const PropertyContext &context) const override;
    QPixmap getPreviewIcon(const PropertyContext &context) const override;
};

class ImagesPropertyEditor : public PropertyEditor
{
public:
    ImagesPropertyEditor() = default;
    virtual ~ImagesPropertyEditor() = default;

    bool canHandle(const PropertyContext &context) const override;
    QString toString(const PropertyContext &context) const override;

    QPixmap getPreviewIcon(const PropertyContext &context) const override;
};

class CursorPropertyEditor : public PropertyEditor
{
public:
    bool canHandle(const PropertyContext &context) const override;
    QString toString(const PropertyContext &context) const override;

public:
    static QString cursorShapeName(Qt::CursorShape shape);
};

class BoolPropertyEditor : public PropertyEditor
{
public:
    bool canHandle(const PropertyContext &context) const override;
    QString toString(const PropertyContext &context) const override;

    EditStyle getEditStyle(const PropertyContext &context) const override;
    std::variant<QWidget *, QVariantList> getDropDown(const PropertyContext &context) const override;
};

} // namespace PM

#endif // PROPERTYEDITOR_H
