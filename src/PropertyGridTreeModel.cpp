#include "PropertyGridTreeModel.h"
#include "PropertyGridTreeItem.h"
#include "PropertyGrid_p.h"

#include <QApplication>
#include <QDebug>

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

// TreeModel::TreeModel(const QStringList &headers, const QString &data, QObject *parent) : QAbstractItemModel(parent)
// {
//     QVector<QVariant> rootData;
//     for (const QString &header : headers)
//         rootData << header;

//     rootItem = new TreeItem(rootData);
//     setupModelData(data.split('\n'), rootItem);
// }

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
        setData(index.siblingAtColumn(i), value, role);
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

    m_propertiesMap[context.property().name] = PropertyItem;

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

// bool TreeModel::insertColumns(int position, int columns, const QModelIndex &parent)
// {
//     beginInsertColumns(parent, position, position + columns - 1);
//     const bool success = rootItem->insertColumns(position, columns);
//     endInsertColumns();

//     return success;
// }

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

// bool TreeModel::removeColumns(int position, int columns, const QModelIndex &parent)
// {
//     beginRemoveColumns(parent, position, position + columns - 1);
//     const bool success = rootItem->removeColumns(position, columns);
//     endRemoveColumns();

//     if (rootItem->columnCount() == 0)
//         removeRows(0, rowCount());

//     return success;
// }

// bool TreeModel::removeRows(int position, int rows, const QModelIndex &parent)
// {
//     TreeItem *parentItem = getItem(parent);
//     if (!parentItem)
//         return false;

//     beginRemoveRows(parent, position, position + rows - 1);
//     const bool success = parentItem->removeChildren(position, rows);
//     endRemoveRows();

//     return success;
// }

// bool TreeModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
// {
//     if (role != Qt::EditRole || orientation != Qt::Horizontal)
//         return false;

//     const bool result = rootItem->setData(section, value);

//     if (result)
//         emit headerDataChanged(orientation, section, section);

//     return result;
// }

// void TreeModel::setupModelData(const QStringList &lines, TreeItem *parent)
// {
//     QList<TreeItem *> parents;
//     QList<int> indentations;
//     parents << parent;
//     indentations << 0;

//     int number = 0;

//     while (number < lines.count())
//     {
//         int position = 0;
//         while (position < lines[number].length())
//         {
//             if (lines[number].at(position) != ' ')
//                 break;

//             ++position;
//         }

//         const QString lineData = lines[number].mid(position).trimmed();

//         if (!lineData.isEmpty())
//         {
//             // Read the column data from the rest of the line.
//             const QStringList columnStrings = lineData.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
//             QList<QVariant> columnData;
//             columnData.reserve(columnStrings.size());
//             for (const QString &columnString : columnStrings)
//                 columnData << columnString;

//             if (position > indentations.last())
//             {
//                 // The last child of the current parent is now the new parent
//                 // unless the current parent has no children.

//                 if (parents.last()->childCount() > 0)
//                 {
//                     parents << parents.last()->child(parents.last()->childCount() - 1);
//                     indentations << position;
//                 }
//             }
//             else
//             {
//                 while (position < indentations.last() && parents.count() > 0)
//                 {
//                     parents.pop_back();
//                     indentations.pop_back();
//                 }
//             }

//             // Append a new item to the current parent's list of children.
//             TreeItem *parent = parents.last();
//             parent->insertChildren(parent->childCount(), 1, rootItem->columnCount());
//             for (int column = 0; column < columnData.size(); ++column)
//                 parent->child(parent->childCount() - 1)->setData(column, columnData[column]);
//         }
//         ++number;
//     }
// }
