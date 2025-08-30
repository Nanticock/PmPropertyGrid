#ifndef PROPERTYGRID_P_H
#define PROPERTYGRID_P_H

#include "PropertyGrid.h"
#include "ui_PropertyGrid.h"

#include "PropertyGridTreeModel.h"

#include <QComboBox>
#include <QLineEdit>
#include <QProxyStyle>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

namespace PM
{
namespace internal
{
    class PropertyGridItemDelegate;

    class PropertyEditorComboBox : public QComboBox
    {
        Q_OBJECT

    public:
        explicit PropertyEditorComboBox(QWidget *parent = nullptr);

        void paintEvent(QPaintEvent *event) override;

        QWidget *dropDownWidget() const;
        void setDropDownWidget(QWidget *value);

    protected:
        void showPopup() override;
        void hidePopup() override;

    private:
        QWidget *m_dropDownWidget;

        QFrame m_dropDownFrame;
        QVBoxLayout m_frameLayout;
    };

    class PropertyEditorLineEdit : public QWidget
    {
        Q_OBJECT

    public:
        explicit PropertyEditorLineEdit(QWidget *parent = nullptr);

        QToolButton *button() const;
        QLineEdit *lineEdit() const;

    private:
        QToolButton m_button;
        QLineEdit m_lineEdit;
    };

    class PropertyEditorWidget : public QWidget
    {
        friend class PropertyGridItemDelegate;

        Q_OBJECT
        Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged FINAL)
        Q_PROPERTY(QVariant uiValue READ uiValue WRITE setUiValue NOTIFY uiValueChanged FINAL)

    public:
        explicit PropertyEditorWidget(PropertyContext &context, QWidget *parent = nullptr);
        ~PropertyEditorWidget();

        QVariant uiValue() const;
        void setUiValue(const QVariant &newValue);

        // FIXME: make these private
        PropertyEditor *propertyEditor() const;
        PropertyContext &propertyContext() const;
        static PropertyContext &defaultContext();

    signals:
        void textChanged();
        void uiValueChanged();

    private slots:
        void onUiValueChanged();
        void onTextEditingFinished();
        void onDropDownComboBoxCurrentIndexChanged(int index);

    private:
        void setEditStyle();
        void initializeWidgetData();

        QString text() const;
        void setText(const QString &value);

        QString valueToString(const QVariant &value) const;

        PropertyEditor::EditStyle editStyle() const;

    private:
        QVariant m_uiValue;
        //
        // WARNING: never get tempted to change this to a reference.
        //          It has to be nullable value as there is always a chance that
        //          the property grid that created the context gets destroyed before
        //          the editor widget. In that case we need to make sure we
        //          are not referencing a deleted property context
        //
        PropertyContext *m_context;

        PropertyEditorComboBox m_dropDownComboBox;
        PropertyEditorLineEdit m_propertylineEdit;
    };

    // FIXME: move to a more appropriate location
    class PropertyContextPrivate
    {
    public:
        using valueChangedSlot_t = std::function<void(const QVariant &)>;

    public:
        static PropertyContext &invalidContext();

        static PropertyContext createContext(const Property &property);
        static PropertyContext createContext(const QVariant &value, bool valid = true);
        static PropertyContext createContext(const PropertyContext &other, const QVariant &newValue);
        static PropertyContext createContext(const Property &property, const QVariant &value, void *object, PropertyGrid *propertyGrid);

        static void setValue(PropertyContext &context, const QVariant &value);

        static valueChangedSlot_t defaultValueChangedSlot();

        static void disconnectValueChangedSlot(PropertyContext &context);
        static void connectValueChangedSlot(PropertyContext &context, const valueChangedSlot_t &slot);

        static void notifyValueChanged(const PropertyContext &context, const QVariant &newValue);
    };

    // TODO: can we get rid of this altogether?!!
    struct ModelIndexHelperFunctions
    {
        static QModelIndex firstIndexInRow(const QModelIndex &index);

        static QVariant firstValueInRow(const QModelIndex &index, Qt::ItemDataRole role);
        static QString firstValueInRowAsString(const QModelIndex &index, Qt::ItemDataRole role);

        static QTreeWidgetItem *treeItemFromIndex(const QModelIndex &index);
    };

    // TODO: can we eleminate this class altogether?!!
    class PropertyGridStyle : public QProxyStyle
    {
    public:
        void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override;
    };

    class PropertyGridItemDelegate : public QStyledItemDelegate
    {
        Q_OBJECT

    public:
        PropertyGridItemDelegate(PropertyGrid *parent);

        // FIXME: to be removed
        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

        QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override; // TODO: keep this

        void setEditorData(QWidget *editor, const QModelIndex &index) const override;
        void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override; // TODO: keep this

        QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    private:
        PropertyGrid *m_parentGrid;
        PropertyEditorWidget m_widget;
    };
} // namespace internal

class PropertyGridPrivate
{
public:
    explicit PropertyGridPrivate(PropertyGrid *q);

    static PropertyEditor &defaultPropertyEditor();

    static PropertyGridPrivate *getImpl(PropertyGrid &propertyGrid);
    static const PropertyGridPrivate *getImpl(const PropertyGrid &propertyGrid);

    void closeEditor();
    void updatePropertyValue(const QModelIndex &index, const QVariant &value);

    bool setPropertyValue(const PropertyContext &context, const QVariant &value);
    // TODO: maybe change this to return a const reference?!!
    PropertyEditor *getEditorForProperty(const PropertyContext &context) const;

    static QPixmap generateDecoration(const QPixmap &pixmap);

public: // slots
    void handleUiSelectionChange(const QModelIndex &current, const QModelIndex &previous);

public:
    PropertyGrid *q;

    std::unique_ptr<Ui::PropertyGrid> ui;
    internal::PropertyGridStyle tableViewStyle;
    internal::PropertyGridItemDelegate tableViewItemDelegate;

    internal::PropertyGridTreeModel m_model;
    std::unordered_map<TypeId, std::unique_ptr<PropertyEditor>> m_propertyEditors;
};
} // namespace PM

#endif // PROPERTYGRID_P_H
