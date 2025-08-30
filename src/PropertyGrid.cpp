#include "PropertyGrid.h"
#include "PropertyGrid_p.h"

#include "PropertyGridTreeItem.h"
#include "PropertyGridTreeModel.h"
#include "QtCompat_p.h"

#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>

namespace
{
// FIXME: make this high-DPI aware
const std::uint32_t PROPERTY_EDITOR_DECORATION_WIDTH = 20;
const std::uint32_t PROPERTY_EDITOR_DECORATION_HEIGHT = 16;

const char PROPERTY_EDITOR_WIDGET_CHILD_STYLE_SHEET[] = R"(
                margin: 0px;
                padding: 0px;
        )";
} // namespace

using namespace PM;

internal::PropertyEditorComboBox::PropertyEditorComboBox(QWidget *parent) :
    QComboBox(parent),
    m_dropDownWidget(nullptr),
    m_dropDownFrame(nullptr, Qt::Popup)
{
    setEditable(true);

    m_frameLayout.setContentsMargins(0, 0, 0, 0);
    m_dropDownFrame.setFrameStyle(QFrame::StyledPanel);
    m_dropDownFrame.setLayout(&m_frameLayout);

    // setStyleSheet(PROPERTY_EDITOR_WIDGET_CHILD_STYLE_SHEET);
}

void internal::PropertyEditorComboBox::paintEvent(QPaintEvent *event)
{
    QComboBox::paintEvent(event);
}

QWidget *internal::PropertyEditorComboBox::dropDownWidget() const
{
    return m_dropDownWidget;
}

void internal::PropertyEditorComboBox::setDropDownWidget(QWidget *value)
{
    m_frameLayout.takeAt(0);

    m_dropDownWidget = value;

    if (m_dropDownWidget == nullptr)
        return;

    m_frameLayout.addWidget(m_dropDownWidget);
}

void internal::PropertyEditorComboBox::showPopup()
{
    if (m_dropDownWidget == nullptr)
    {
        QComboBox::showPopup();
        return;
    }

    if (m_dropDownFrame.isVisible())
        return;

    const QRect rect = this->rect();
    const QPoint belowComboBox = mapToGlobal(QPoint(0, rect.height()));

    m_dropDownFrame.move(belowComboBox);
    m_dropDownFrame.setFixedWidth(rect.width());

    m_dropDownFrame.show();
}

void internal::PropertyEditorComboBox::hidePopup()
{
    if (m_dropDownWidget == nullptr)
    {
        QComboBox::hidePopup();
        return;
    }

    m_dropDownFrame.hide();
}

internal::PropertyEditorLineEdit::PropertyEditorLineEdit(QWidget *parent) : QWidget(parent), m_button(parent), m_lineEdit(parent)
{
    m_lineEdit.setFrame(false);
    QColor inactiveWindowColor = m_lineEdit.palette().color(QPalette::AlternateBase);

    // FIXME: there is an offset of -1px below the m_line edit that needs to be fixed
    static QString stylesheet = "border-bottom: 1px solid %1;\n%2";
    m_lineEdit.setStyleSheet(stylesheet.arg(inactiveWindowColor.name(), PROPERTY_EDITOR_WIDGET_CHILD_STYLE_SHEET));

    m_button.setText("...");
    m_button.setVisible(false);
    m_button.setMinimumHeight(m_lineEdit.sizeHint().height());
    m_button.setMinimumWidth(m_button.minimumHeight());

    QHBoxLayout *layout = new QHBoxLayout();
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);
    layout->addWidget(&m_lineEdit, 1);
    layout->addWidget(&m_button, 0);

    setLayout(layout);
    setMinimumHeight(m_lineEdit.sizeHint().height());
}

QToolButton *internal::PropertyEditorLineEdit::button() const
{
    return const_cast<QToolButton *>(&m_button);
}

QLineEdit *internal::PropertyEditorLineEdit::lineEdit() const
{
    return const_cast<QLineEdit *>(&m_lineEdit);
}

internal::PropertyEditorWidget::PropertyEditorWidget(PropertyContext &context, QWidget *parent) : QWidget(parent), m_context(&context)
{
    PropertyContextPrivate::connectValueChangedSlot(propertyContext(),
                                                    [this](const QVariant &value)
                                                    {
                                                        //
                                                        setUiValue(value);
                                                    });
    connect(this, &PropertyEditorWidget::uiValueChanged, this, &PropertyEditorWidget::onUiValueChanged);

    setAutoFillBackground(true); // FIXME: we are not supposed to use this here
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(m_dropDownComboBox.sizeHint().height());

    QHBoxLayout *layout = new QHBoxLayout();
    layout->setContentsMargins(QMargins());
    layout->addWidget(&m_propertylineEdit, 1);
    layout->addWidget(&m_dropDownComboBox, 1);
    setLayout(layout);

    setEditStyle(); // FIXME: find a cleaner solution
    initializeWidgetData();

    connect(m_propertylineEdit.lineEdit(), &QLineEdit::editingFinished, this, &PropertyEditorWidget::onTextEditingFinished);
    connect(m_dropDownComboBox.lineEdit(), &QLineEdit::editingFinished, this, &PropertyEditorWidget::onTextEditingFinished);

    if (m_context->propertyGrid() != nullptr) // TODO: figure out how to write this in a cleaner way
    {
        connect(m_context->propertyGrid(), &PropertyGrid::propertyValueChanged, this,
                [this](const PropertyContext &context) { setUiValue(context.value()); });
    }

    connect(m_propertylineEdit.button(), &QToolButton::clicked, this,
            [this](bool checked)
            {
                if (m_context == nullptr || m_context->propertyGrid() == nullptr)
                    return;

                QVariant newValue = propertyEditor()->modalClicked(propertyContext());
                PropertyGridPrivate *propertyGridImpl = PropertyGridPrivate::getImpl(*propertyContext().propertyGrid());

                propertyGridImpl->setPropertyValue(propertyContext(), newValue);
            });

    connect(&m_dropDownComboBox, qOverload<int>(&PropertyEditorComboBox::currentIndexChanged), this,
            &PropertyEditorWidget::onDropDownComboBoxCurrentIndexChanged);
}

internal::PropertyEditorWidget::~PropertyEditorWidget()
{
    PropertyContextPrivate::disconnectValueChangedSlot(propertyContext());
}

QVariant internal::PropertyEditorWidget::uiValue() const
{
    return m_uiValue;
}

void internal::PropertyEditorWidget::setUiValue(const QVariant &newValue)
{
    if (m_uiValue == newValue)
        return;

    m_uiValue = newValue;
    emit uiValueChanged();
}

QString internal::PropertyEditorWidget::text() const
{
    switch (editStyle())
    {
    case PropertyEditor::None:
        Q_FALLTHROUGH();
    case PropertyEditor::Modal:
        return m_propertylineEdit.lineEdit()->text();

    case PropertyEditor::DropDown:
        return m_dropDownComboBox.lineEdit()->text();

    default:
        break;
    }

    return "";
}

void internal::PropertyEditorWidget::setText(const QString &value)
{
    if (text() == value)
        return;

    switch (editStyle())
    {
    case PropertyEditor::None:
        Q_FALLTHROUGH();
    case PropertyEditor::Modal:
        m_propertylineEdit.lineEdit()->setText(value);
        break;

    case PropertyEditor::DropDown:
    {
        m_dropDownComboBox.setCurrentText(value);

        int currentIndex = m_dropDownComboBox.findText(value);
        if (currentIndex != -1)
            m_dropDownComboBox.setCurrentIndex(currentIndex);
    }
    break;

    default:
        return;
    }

    emit textChanged();
}

PropertyContext &internal::PropertyEditorWidget::defaultContext()
{
    static PropertyContext instance = PropertyContextPrivate::invalidContext();

    return instance;
}

PropertyEditor::EditStyle internal::PropertyEditorWidget::editStyle() const
{
    return propertyEditor()->getEditStyle(propertyContext());
}

PropertyEditor *internal::PropertyEditorWidget::propertyEditor() const
{
    PropertyGrid *propertyGrid = m_context->propertyGrid();

    if (propertyGrid == nullptr)
        return &PropertyGridPrivate::defaultPropertyEditor();

    PropertyGridPrivate *propertyGridImpl = PropertyGridPrivate::getImpl(*propertyGrid);

    return propertyGridImpl->getEditorForProperty(propertyContext());
}

PropertyContext &internal::PropertyEditorWidget::propertyContext() const
{
    if (m_context == nullptr)
        return defaultContext();

    return *m_context;
}

void internal::PropertyEditorWidget::onUiValueChanged()
{
    const QString valueAsString = valueToString(m_uiValue);

    setText(valueAsString);
}

void internal::PropertyEditorWidget::onTextEditingFinished()
{
    QString errorMessage;
    QVariant newValue = propertyEditor()->fromString(text(), propertyContext(), &errorMessage);

    if (!errorMessage.isEmpty() || newValue == m_context->value())
        return;

    PropertyEditor *editor = propertyEditor();
    if (editor->toString(propertyContext()) == text())
        return;

    setUiValue(newValue);
}

void internal::PropertyEditorWidget::onDropDownComboBoxCurrentIndexChanged(int index)
{
    // If the editor provided a dropdown widget, we don't need to update the ui value
    if (m_dropDownComboBox.dropDownWidget() != nullptr)
        return;

    // FIXME: this looks like it would be better if PropertyEditor::getDropDown returned a QVariantList
    const QVariant value = propertyEditor()->fromString(m_dropDownComboBox.currentText(), propertyContext());
    setUiValue(value);
}

void internal::PropertyEditorWidget::setEditStyle()
{
    const PropertyEditor::EditStyle style = editStyle();

    m_dropDownComboBox.setVisible(style == PropertyEditor::DropDown);
    m_propertylineEdit.setVisible(style != PropertyEditor::DropDown);
    m_propertylineEdit.button()->setVisible(style == PropertyEditor::Modal);

    if (m_dropDownComboBox.isVisible())
        setFocusProxy(m_dropDownComboBox.lineEdit());
    else
        setFocusProxy(m_propertylineEdit.lineEdit());
}

void PM::internal::PropertyEditorWidget::initializeWidgetData()
{
    // TODO: modify the UI to:
    //       1. populate the values / widget of the dropdown combobox
    //       2. display the string representation for the current property context value

    const PropertyEditor *editor = propertyEditor();
    const QString valueAsString = editor->toString(propertyContext());

    if (editStyle() == PM::PropertyEditor::DropDown)
    {
        auto dropDownData = editor->getDropDown(propertyContext());

        if (std::holds_alternative<QWidget *>(dropDownData))
        {
            m_dropDownComboBox.setDropDownWidget(std::get<QWidget *>(dropDownData));
            m_dropDownComboBox.setCurrentText(valueAsString);
        }
        else if (std::holds_alternative<QVariantList>(dropDownData))
        {
            // FIXME: should we use a QStringList instead of QVariantList?!!
            const QVariantList valuesList = std::get<QVariantList>(dropDownData);
            for (const QVariant &value : valuesList)
                m_dropDownComboBox.addItem(valueToString(value), value);
        }
    }

    setUiValue(m_context->value());
}

QString internal::PropertyEditorWidget::valueToString(const QVariant &value) const
{
    const PropertyContext newContext = PropertyContextPrivate::createContext(propertyContext(), value);

    return propertyEditor()->toString(newContext);
}

PropertyContext &internal::PropertyContextPrivate::invalidContext()
{
    static PropertyContext result;

    return result;
}

PropertyContext internal::PropertyContextPrivate::createContext(const Property &property)
{
    return PropertyContext(property, QVariant(), nullptr, nullptr);
}

PropertyContext internal::PropertyContextPrivate::createContext(const QVariant &value, bool valid)
{
    PropertyContext result;
    result.m_value = value;
    result.m_isValid = valid;

    return result;
}

PropertyContext internal::PropertyContextPrivate::createContext(const PropertyContext &other, const QVariant &newValue)
{
    PropertyContext result = other;
    result.m_value = newValue;

    return result;
}

PropertyContext internal::PropertyContextPrivate::createContext(const Property &property, const QVariant &value, void *object,
                                                                PropertyGrid *propertyGrid)
{
    return PropertyContext(property, value, object, propertyGrid);
}

void internal::PropertyContextPrivate::setValue(PropertyContext &context, const QVariant &value)
{
    context.m_value = value;
    notifyValueChanged(context, value);
}

internal::PropertyContextPrivate::valueChangedSlot_t internal::PropertyContextPrivate::defaultValueChangedSlot()
{
    static valueChangedSlot_t result = [](const QVariant &) {};

    return result;
}

void internal::PropertyContextPrivate::disconnectValueChangedSlot(PropertyContext &context)
{
    context.m_valueChangedSlot = defaultValueChangedSlot();
}

void internal::PropertyContextPrivate::connectValueChangedSlot(PropertyContext &context, const valueChangedSlot_t &slot)
{
    context.m_valueChangedSlot = slot;
}

void internal::PropertyContextPrivate::notifyValueChanged(const PropertyContext &context, const QVariant &newValue)
{
    context.m_valueChangedSlot(newValue);
}

QString internal::ModelIndexHelperFunctions::firstValueInRowAsString(const QModelIndex &index, Qt::ItemDataRole role)
{
    QVariant result = firstValueInRow(index, role);

    if (internal::getVariantTypeId(result) != qMetaTypeId<QString>())
        return "";

    return result.toString();
}

QModelIndex internal::ModelIndexHelperFunctions::firstIndexInRow(const QModelIndex &index)
{
    QModelIndex result = index;

    if (result.column() != 0)
        result = PM::internal::siblingAtColumn(index, 0);

    return result;
}

QVariant internal::ModelIndexHelperFunctions::firstValueInRow(const QModelIndex &index, Qt::ItemDataRole role)
{
    QModelIndex resultIndex = firstIndexInRow(index);

    if (!resultIndex.isValid())
        return QVariant();

    return resultIndex.data(role);
}

QTreeWidgetItem *internal::ModelIndexHelperFunctions::treeItemFromIndex(const QModelIndex &index)
{
    if (!index.isValid())
        return nullptr;

    return reinterpret_cast<QTreeWidgetItem *>(index.internalPointer());
}

void internal::PropertyGridStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    QColor inactiveBorderColor = option->palette.color(QPalette::Inactive, QPalette::Window);
    if (element == PE_IndicatorBranch && option->rect.x() == 0) // Draw Left margin indicator
    {
        painter->fillRect(option->rect, inactiveBorderColor);
    }
    else if (element == PE_PanelItemViewItem) // Draw grid lines
    {
        painter->setPen(inactiveBorderColor);

        const QRect &rect = option->rect;
        painter->drawLine(rect.bottomLeft(), rect.bottomRight()); // horizontal grid lines
        painter->drawLine(rect.topRight(), rect.bottomRight());   // vertical grid lines
    }

    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

internal::PropertyGridItemDelegate::PropertyGridItemDelegate(PropertyGrid *parent) :
    QStyledItemDelegate(parent),
    m_parentGrid(parent),
    m_widget(PropertyContextPrivate::invalidContext())
{
}

// FIXME: to be removed
void internal::PropertyGridItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // TODO: can be used to customize the shape of the items in dark mode
    // TODO: give this delegate access to the PropertiesEditor so that we can draw the separator line for the categories elements

    QStyledItemDelegate::paint(painter, option, index);
}

QWidget *internal::PropertyGridItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const PropertyGridPrivate *propertyGridPrivate = PropertyGridPrivate::getImpl(*m_parentGrid);
    PropertyContext &context = propertyGridPrivate->m_model.getItem(index)->context;

    PropertyEditorWidget *result = new PropertyEditorWidget(context, parent);

    //
    // NOTE: This is very important because sometimes the property grid gets destroyed while the editor widget is still opened
    //       This means that the editor might try to access a property context after it got destroyed.
    //       This makes sure that the editor widget will never point to an invalid property context
    //
    connect(this, &QObject::destroyed, this,
            [result]()
            {
                //
                result->m_context = nullptr;
            });

    return result;
}

void internal::PropertyGridItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    Q_UNUSED(index)
    Q_UNUSED(editor)

    // NOTE: this is intentionaly left blank to remove un-necessary performance overhead
}

void internal::PropertyGridItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    PropertyEditorWidget *propertyEditorWidget = qobject_cast<PropertyEditorWidget *>(editor);

    PropertyContext &context = propertyEditorWidget->propertyContext();

    PropertyGridPrivate *parentGridPrivate = PropertyGridPrivate::getImpl(*m_parentGrid);
    if (parentGridPrivate->setPropertyValue(context, propertyEditorWidget->uiValue()))
        return;

    QMessageBox::warning(m_parentGrid, "Warning", "Couldn't set data");

    // FIXME: Display an error message if the user input was invalid
}

QSize internal::PropertyGridItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize result = QStyledItemDelegate::sizeHint(option, index);
    result.setHeight(m_widget.minimumHeight());

    return result;
}

PropertyGridPrivate::PropertyGridPrivate(PropertyGrid *q) : q(q), ui(new Ui::PropertyGrid()), tableViewItemDelegate(q)
{
}

PropertyEditor &PropertyGridPrivate::defaultPropertyEditor()
{
    static PropertyEditor result;

    return result;
}

PropertyGridPrivate *PropertyGridPrivate::getImpl(PropertyGrid &propertyGrid)
{
    return propertyGrid.d;
}
const PropertyGridPrivate *PropertyGridPrivate::getImpl(const PropertyGrid &propertyGrid)
{
    return propertyGrid.d;
}

void PropertyGridPrivate::closeEditor()
{
    QTreeView *treeView = ui->propertiesTreeView;

    treeView->closePersistentEditor(treeView->currentIndex());
}

void PropertyGridPrivate::updatePropertyValue(const QModelIndex &index, const QVariant &value)
{
    internal::PropertyGridTreeItem *item = m_model.getItem(index);

    if (item == nullptr)
        return;

    bool valueChanged = item->context.value() != value;

    PropertyContext &context = item->context;
    const PropertyEditor *editor = getEditorForProperty(context);

    QModelIndex valueIndex = PM::internal::siblingAtColumn(index, 1);

    internal::PropertyContextPrivate::setValue(context, value);

    m_model.setData(valueIndex, value, Qt::EditRole);
    m_model.setData(valueIndex, editor->toString(context), Qt::DisplayRole);
    m_model.setData(valueIndex, generateDecoration(editor->getPreviewIcon(context)), Qt::DecorationRole);

    if (!valueChanged)
        return;

    emit q->propertyValueChanged(context);
}

bool PropertyGridPrivate::setPropertyValue(const PropertyContext &context, const QVariant &value)
{
    const Property &property = context.property();
    if (internal::getVariantTypeId(value) != property.type() && !internal::canConvert(value, property.type()))
        return false;

    internal::PropertyGridTreeItem *propertyItem = m_model.getPropertyItem(property.name());

    if (propertyItem == nullptr)
        return false;

    const QModelIndex index = m_model.getItemIndex(propertyItem);

    updatePropertyValue(index, value);

    return true;
}

void PropertyGridPrivate::handleUiSelectionChange(const QModelIndex &current, const QModelIndex &previous)
{
    ui->propertyDescriptionLabel->setText("");
    ui->propertyNameLabel->setText(internal::ModelIndexHelperFunctions::firstValueInRowAsString(current, Qt::DisplayRole));

    internal::PropertyGridTreeItem *item = m_model.getItem(current);

    if (item == nullptr)
        return;

    QString propertyDescription;
    if (item->context.property().hasAttribute<DescriptionAttribute>())
        propertyDescription = item->context.property().getAttribute<DescriptionAttribute>().value;

    ui->propertyDescriptionLabel->setText(propertyDescription);
}

PropertyEditor *PropertyGridPrivate::getEditorForProperty(const PropertyContext &context) const
{
    // TODO: in the future add the ability to have a special Attribute that can override the editor

    // NOTE: this function never returns nullptr

    for (auto editor = m_propertyEditors.begin(); editor != m_propertyEditors.end(); ++editor)
    {
        if (editor->second->canHandle(context))
            return editor->second.get();
    }

    // if no editor knows how to handle this data, return the default one
    return &defaultPropertyEditor();
}

QPixmap PropertyGridPrivate::generateDecoration(const QPixmap &pixmap)
{
    if (pixmap.isNull())
        return pixmap;

    QPixmap result(PROPERTY_EDITOR_DECORATION_WIDTH, PROPERTY_EDITOR_DECORATION_HEIGHT);
    result.fill(Qt::transparent);
    QPainter painter(&result);

    // Enable antialiasing for smoother scaling
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QSize scaledSize = pixmap.size();
    scaledSize.scale(result.size(), Qt::KeepAspectRatio);

    QPixmap scaledPixmap = pixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QRect targetRect(QPoint(0, 0), scaledPixmap.size());
    targetRect.moveCenter(result.rect().center());

    // Draw the scaled pixmap
    painter.drawPixmap(targetRect, scaledPixmap);

    // Draw border
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QApplication::palette().color(QPalette::WindowText));
    painter.drawRect(result.rect().adjusted(0, 0, -1, -1));

    return result;
}

PropertyGrid::PropertyGrid(QWidget *parent) : QWidget(parent), d(new PropertyGridPrivate(this))
{
    d->ui->setupUi(this);

    d->ui->propertiesTreeView->setModel(&d->m_model);
    d->ui->propertiesTreeView->setStyle(&d->tableViewStyle);
    d->ui->propertiesTreeView->setItemDelegate(&d->tableViewItemDelegate);

    d->ui->propertiesTreeView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    d->ui->propertiesTreeView->header()->setSectionResizeMode(1, QHeaderView::Stretch);

    connect(d->ui->propertiesTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current, const QModelIndex &previous)
            {
                //
                d->handleUiSelectionChange(current, previous);
            });
}

PropertyGrid::~PropertyGrid()
{
    delete d;
}

void PropertyGrid::addProperty(const Property &property, const QVariant &value, void *object)
{
    // NEVER EVER allow any properties with empty names
    if (property.name().isEmpty())
        return;

    // property name is a unique identifier. duplicates are not allowed
    if (d->m_model.getPropertyItem(property.name()) != nullptr)
    {
        qWarning() << "property" << property.name() << "alrready exists!";
        return;
    }

    PropertyContext context = internal::PropertyContextPrivate::createContext(property, value, object, this);

    QModelIndex propertyIndex = d->m_model.addProperty(context);

    bool readOnly = property.hasAttribute<ReadOnlyAttribute>() && property.getAttribute<ReadOnlyAttribute>().value == true;

    QColor disabledTextColor = palette().color(QPalette::Disabled, QPalette::Text);
    if (readOnly)
        d->m_model.setDataForAllColumns(propertyIndex, disabledTextColor, Qt::ForegroundRole);

    d->updatePropertyValue(propertyIndex, value);

    if (d->m_model.showCategories())
        d->ui->propertiesTreeView->expandToDepth(0);
}

bool PropertyGrid::showCategories() const
{
    return d->m_model.showCategories();
}

void PropertyGrid::setShowCategories(bool value)
{
    d->m_model.setShowCategories(value);
}

bool PropertyGrid::setPropertyValue(const QString &propertyName, const QVariant &value)
{
    if (propertyName.trimmed().isEmpty())
        return false;

    internal::PropertyGridTreeItem *propertyItem = d->m_model.getPropertyItem(propertyName);

    if (propertyItem == nullptr)
        return false;

    return d->setPropertyValue(propertyItem->context, value);
}

PropertyContext PropertyGrid::getPropertyContext(const QString &propertyName) const
{
    internal::PropertyGridTreeItem *treeItem = d->m_model.getPropertyItem(propertyName);

    if (treeItem != nullptr)
        return treeItem->context;

    return internal::PropertyContextPrivate::invalidContext();
}

void PropertyGrid::addPropertyEditor_impl(TypeId typeId, std::unique_ptr<PropertyEditor> &&editor)
{
    d->m_propertyEditors.emplace(typeId, std::move(editor));
}
