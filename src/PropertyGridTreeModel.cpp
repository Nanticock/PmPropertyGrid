#include "PropertyGridTreeModel.h"
#include "PropertyGridTreeItem.h"
#include "PropertyGrid_p.h"

#include <QApplication>
#include <QDebug>
#include <QString>

namespace
{
const char DEFAULT_CATEGORY_NAME[] = "Misc";
}

using namespace PM;

internal::PropertyGridTreeModel::PropertyGridTreeModel(QObject *parent) :
    QAbstractItemModel(parent),
    m_showCategories(true),
    m_rootItem(new PropertyGridTreeItem())
{
}

internal::PropertyGridTreeModel::~PropertyGridTreeModel()
{
    delete m_rootItem;
}

int internal::PropertyGridTreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 2;
}

int internal::PropertyGridTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() && parent.column() > 0)
        return 0;

    const PropertyGridTreeItem *parentItem = getItem(parent);

    return parentItem ? parentItem->childrenCount(m_showCategories) : 0;
}

QVariant internal::PropertyGridTreeModel::data(const QModelIndex &index, int role) const
{
    // CRITICAL: initialy the view asks the model for some basic data like the text alignment and the font.
    // if the answer was something else that isn't the expected data type OR an empty QVariant, the item will not be displayed

    if (!index.isValid())
        return QVariant();

    PropertyGridTreeItem *item = getItem(index);

    return item->getColumnData(index.column(), Qt::ItemDataRole(role));
}

bool internal::PropertyGridTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;

    const int columnIndex = index.column();

    if (columnIndex < 0 || columnIndex >= columnCount())
        return false;

    PropertyGridTreeItem *item = getItem(index);
    item->setColumnData(index.column(), Qt::ItemDataRole(role), value);

    emit dataChanged(index, index, {role});
    return true;
}

void internal::PropertyGridTreeModel::setDataForAllColumns(const QModelIndex &index, const QVariant &value, Qt::ItemDataRole role)
{
    const int numberOfColumns = columnCount(index);

    for (int i = 0; i < numberOfColumns; ++i)
        setData(PM::internal::siblingAtColumn(index, i), value, role);
}

Qt::ItemFlags internal::PropertyGridTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return QAbstractItemModel::flags(index);

    PropertyGridTreeItem *item = getItem(index);
    return item->flags(index.column());
}

internal::PropertyGridTreeItem *internal::PropertyGridTreeModel::getItem(const QModelIndex &index) const
{
    // NOTE: this function never returns nullptr

    if (!index.isValid())
        return m_rootItem;

    PropertyGridTreeItem *item = static_cast<PropertyGridTreeItem *>(index.internalPointer());
    if (item == nullptr)
        return m_rootItem;

    return item;
}

bool internal::PropertyGridTreeModel::showCategories() const
{
    return m_showCategories;
}

void internal::PropertyGridTreeModel::setShowCategories(bool newShowCategories)
{
    if (m_showCategories == newShowCategories)
        return;

    m_showCategories = newShowCategories;

    beginResetModel();

    // Notify about the structural change
    emit layoutAboutToBeChanged();
    emit dataChanged(createIndex(0, 0), createIndex(rowCount(), columnCount()));
    emit layoutChanged();

    endResetModel();
}

internal::PropertyGridTreeItem *internal::PropertyGridTreeModel::rootItem() const
{
    return m_rootItem;
}

bool internal::PropertyGridTreeModel::isCategory(const QModelIndex &index) const
{
    PropertyGridTreeItem *item = getItem(index);

    return item->parent == m_rootItem;
}

internal::PropertyGridTreeItem *internal::PropertyGridTreeModel::getPropertyItem(const QString &propertyName) const
{
    return m_propertiesMap.value(propertyName);
}

QModelIndex internal::PropertyGridTreeModel::getCategory(const QString &category) const
{
    PropertyGridTreeItem *item = const_cast<PropertyGridTreeModel *>(this)->getCategoryItem(category);

    return getItemIndex(item);
}

QModelIndex internal::PropertyGridTreeModel::addProperty(const PropertyContext &context)
{
    QString category = DEFAULT_CATEGORY_NAME;
    if (context.property().hasAttribute<CategoryAttribute>())
        category = context.property().getAttribute<CategoryAttribute>().value;

    internal::PropertyGridTreeItem *categoryItem = getCategoryItem(category);
    internal::PropertyGridTreeItem *PropertyItem = categoryItem->addChild(context);

    m_propertiesMap[context.property().name()] = PropertyItem;

    bool readOnly = context.property().hasAttribute<ReadOnlyAttribute>() && context.property().getAttribute<ReadOnlyAttribute>().value == true;

    Qt::ItemFlags flags = PropertyItem->flags(1);
    flags.setFlag(Qt::ItemIsEditable, !readOnly);
    PropertyItem->setFlags(1, flags);

    return getItemIndex(PropertyItem);
}

QModelIndex internal::PropertyGridTreeModel::getItemIndex(PropertyGridTreeItem *item) const
{
    return createIndex(item->indexInParent(m_showCategories), 0, item);
}

void internal::PropertyGridTreeModel::clearModel()
{
    beginResetModel();
    {
        m_categoriesMap.clear();
        m_propertiesMap.clear();
        m_rootItem->children.clear();
    }
    endResetModel();
}

internal::PropertyGridTreeItem *internal::PropertyGridTreeModel::getCategoryItem(const QString &category)
{
    if (m_categoriesMap.contains(category))
        return m_categoriesMap.value(category);

    const PropertyContext tempCategoryContext = internal::PropertyContextPrivate::createContext(PM::Property(category, QMetaType::UnknownType));
    PropertyGridTreeItem *result = m_rootItem->addChild(tempCategoryContext);
    result->isTransient = true;
    // TODO: make category item expanded by default?!!

    QFont font = QApplication::font(); // TODO: get font from parent PropertyGrid
    font.setBold(true);

    const QColor inactiveBorderColor =
        QApplication::palette().color(QPalette::Inactive, QPalette::Window); // TODO: get palette from parent PropertyGrid

    result->setDataForAllColumns(Qt::FontRole, font);
    result->setDataForAllColumns(Qt::BackgroundRole, inactiveBorderColor);

    m_categoriesMap.insert(category, result);

    return result;
}

QVariant internal::PropertyGridTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    static QStringList headerNames = {"Name", "Value"};

    if (section < 0 || section >= columnCount())
        return QVariant();

    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return headerNames[section];

    return QVariant();
}

QModelIndex internal::PropertyGridTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() && parent.column() != 0)
        return QModelIndex();

    PropertyGridTreeItem *parentItem = getItem(parent);
    if (parentItem == nullptr)
        return QModelIndex();

    PropertyGridTreeItem *childItem = parentItem->getChild(row, m_showCategories);
    if (childItem != nullptr)
        return createIndex(row, column, childItem);

    return QModelIndex();
}

bool internal::PropertyGridTreeModel::insertRows(int position, int rows, const QModelIndex &parent)
{
    PropertyGridTreeItem *parentItem = getItem(parent);
    if (!parentItem)
        return false;

    beginInsertRows(parent, position, position + rows - 1);
    const bool result = parentItem->insertChildren(position, rows, columnCount());
    endInsertRows();

    return result;
}

QModelIndex internal::PropertyGridTreeModel::parent(const QModelIndex &index) const
{
    // FIXME: handle m_showCategories

    if (!index.isValid())
        return QModelIndex();

    PropertyGridTreeItem *childItem = getItem(index);
    PropertyGridTreeItem *parentItem = childItem ? childItem->parent : nullptr;

    if (parentItem == m_rootItem || !parentItem)
        return QModelIndex();

    size_t indexInParent = parentItem->indexInParent(m_showCategories);

    if (indexInParent < 0)
        return QModelIndex();

    return createIndex(int(indexInParent), 0, parentItem);
}
