#ifndef PROPERTYEDITOR_H
#define PROPERTYEDITOR_H

#include "PropertyContext.h"

#include <QTreeWidgetItem>

#include <QPointer>
#include <variant>

namespace PM
{
class PropertyGrid;
class PropertyEditor;

namespace internal
{
    template <typename T>
    constexpr bool isPropertyEditor()
    {
        using DecayedT = typename std::decay<T>::type;
        constexpr bool isPropertyEditor = std::is_base_of<PropertyEditor, DecayedT>::value;

        static_assert(isPropertyEditor, "T must be of type PM::PropertyEditor");

        return isPropertyEditor;
    }

    using PropertyEditorsMap_t = std::unordered_map<TypeId, std::shared_ptr<PropertyEditor>>;
    const PropertyEditorsMap_t &defaultPropertyEditors();
} // namespace internal

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
