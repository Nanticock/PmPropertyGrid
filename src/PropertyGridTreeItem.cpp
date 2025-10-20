#include "PropertyGridTreeItem.h"

#include "PropertyContext_p.h"

using namespace PM;

TreeItem::TreeItem(const QVector<QVariant> &data, TreeItem *parent) : itemData(data), parentItem(parent)
{
}

TreeItem::~TreeItem()
{
    qDeleteAll(childItems);
}

TreeItem *TreeItem::child(int number)
{
    if (number < 0 || number >= childItems.size())
        return nullptr;

    return childItems.at(number);
}

int TreeItem::childCount() const
{
    return childItems.count();
}

int TreeItem::childNumber() const
{
    if (parentItem)
        return parentItem->childItems.indexOf(const_cast<TreeItem *>(this));

    return 0;
}

int TreeItem::columnCount() const
{
    return itemData.count();
}

QVariant TreeItem::data(int column) const
{
    if (column < 0 || column >= itemData.size())
        return QVariant();

    return itemData.at(column);
}

bool TreeItem::insertChildren(int position, int count, int columns)
{
    if (position < 0 || position > childItems.size())
        return false;

    for (int row = 0; row < count; ++row)
    {
        QVector<QVariant> data(columns);
        TreeItem *item = new TreeItem(data, this);
        childItems.insert(position, item);
    }

    return true;
}

bool TreeItem::insertColumns(int position, int columns)
{
    if (position < 0 || position > itemData.size())
        return false;

    for (int column = 0; column < columns; ++column)
        itemData.insert(position, QVariant());

    for (TreeItem *child : std::as_const(childItems))
        child->insertColumns(position, columns);

    return true;
}

TreeItem *TreeItem::parent()
{
    return parentItem;
}

bool TreeItem::removeChildren(int position, int count)
{
    if (position < 0 || position + count > childItems.size())
        return false;

    for (int row = 0; row < count; ++row)
        delete childItems.takeAt(position);

    return true;
}

bool TreeItem::removeColumns(int position, int columns)
{
    if (position < 0 || position + columns > itemData.size())
        return false;

    for (int column = 0; column < columns; ++column)
        itemData.removeOne(position);

    for (TreeItem *child : std::as_const(childItems))
        child->removeColumns(position, columns);

    return true;
}

bool TreeItem::setData(int column, const QVariant &value)
{
    if (column < 0 || column >= itemData.size())
        return false;

    itemData[column] = value;
    return true;
}

internal::PropertyGridTreeItem::Column::Column() : flags(Qt::ItemIsSelectable | Qt::ItemIsEnabled)
{
}

int internal::PropertyGridTreeItem::childrenCount(bool showTransientItems) const
{
    if (showTransientItems)
        return int(children.size());

    int result = 0;

    for (const auto &child : children)
        result += child->isTransient ? child->childrenCount(showTransientItems) : 1;

    return result;
}

bool internal::PropertyGridTreeItem::insertChildren(int position, int count, int columns)
{
    if (position < 0 || position > children.size())
        return false;

    for (int row = 0; row < count; ++row)
    {
        auto newItem = std::make_unique<PropertyGridTreeItem>();
        newItem->parent = this;
        children.insert(children.begin() + position, std::move(newItem));
    }

    return true;
}

internal::PropertyGridTreeItem *internal::PropertyGridTreeItem::getChild(size_t index, bool showTransientItems) const
{
    // TODO: implement caching using an `isDirty` flag to make this function ~O(1)

    size_t currentIndex = 0;
    for (const auto &child : children)
    {
        if (showTransientItems || !child->isTransient)
        {
            if (currentIndex == index)
                return child.get();

            currentIndex++;
        }
        else if (!showTransientItems && child->isTransient)
        {
            // Handle the children of transient items directly here
            for (const auto &grandChild : child->children)
            {
                if (currentIndex == index)
                    return grandChild.get();

                currentIndex++;
            }
        }
    }

    return nullptr; // Index out of range
}

internal::PropertyGridTreeItem *internal::PropertyGridTreeItem::addChild(const PropertyContext &context)
{
    if (!insertChildren(static_cast<int>(children.size()), 1, 2))
        return nullptr;

    auto &newChild = children.back();
    newChild->context = context;

    return newChild.get();
}

QVariant internal::PropertyGridTreeItem::value() const
{
    return columns[1].data[Qt::EditRole];
}

void internal::PropertyGridTreeItem::setValue(const QVariant &newValue)
{
    columns[1].data[Qt::EditRole] = newValue;
}

QVariant internal::PropertyGridTreeItem::getColumnData(int columnIndex, Qt::ItemDataRole role) const
{
    if (columnIndex == 0 && role == Qt::DisplayRole)
        return context.property().name();

    // in case the user didn't provide any data for the display role, we return the value of the edit role
    if (role == Qt::DisplayRole && !columns[columnIndex].data.contains(Qt::DisplayRole)) // FIXME: stop using that
        role = Qt::EditRole;

    return columns[columnIndex].data[role];
}

void internal::PropertyGridTreeItem::setColumnData(int columnIndex, Qt::ItemDataRole role, const QVariant &newValue)
{
    if (columnIndex == 0 && role == Qt::DisplayRole)
    {
        context.property().name() = newValue.toString();
        return;
    }

    if (newValue.isValid())
        columns[columnIndex].data[role] = newValue;
    else // if the new value is invalid, remove it from the data to optimize space
        columns[columnIndex].data.remove(role);
}

void internal::PropertyGridTreeItem::setDataForAllColumns(Qt::ItemDataRole role, const QVariant &newValue)
{
    setColumnData(0, role, newValue);
    setColumnData(1, role, newValue);
}

Qt::ItemFlags internal::PropertyGridTreeItem::flags(int columnIndex) const
{
    return columns[columnIndex].flags;
}

void internal::PropertyGridTreeItem::setFlags(int columnIndex, Qt::ItemFlags value)
{
    columns[columnIndex].flags = value;
}

int internal::PropertyGridTreeItem::indexInParent(bool showTransientItems) const
{
    if (parent == nullptr)
        return -1;

    int currentIndex = 0;
    for (const auto &child : parent->children)
    {
        if (showTransientItems || !child->isTransient)
        {
            if (child.get() == this)
                return currentIndex;

            currentIndex++;
        }
        else if (!showTransientItems && child->isTransient)
        {
            for (const auto &grandChild : child->children)
            {
                if (grandChild.get() == this)
                    return currentIndex;

                currentIndex++;
            }
        }
    }

    return -1;
}

internal::PropertyGridTreeItem::PropertyGridTreeItem() : context(PM::PropertyContextPrivate::invalidContext()), parent(nullptr), isTransient(false)
{
}

internal::PropertyGridTreeItem::PropertyGridTreeItem(const internal::PropertyGridTreeItem &other) :
    context(other.context),
    parent(other.parent),
    isTransient(other.isTransient)
{
    columns[0] = other.columns[0];
    columns[1] = other.columns[1];

    for (const auto &child : other.children)
        children.push_back(std::make_unique<PropertyGridTreeItem>(*child));
}
