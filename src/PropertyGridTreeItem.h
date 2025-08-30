#ifndef PROPERTYGRIDTREEITEM_H
#define PROPERTYGRIDTREEITEM_H

#include "Property.h"
#include "PropertyEditor.h"

#include <QList>
#include <QVariant>

class TreeItem
{
public:
    explicit TreeItem(const QVector<QVariant> &data, TreeItem *parent = nullptr);
    ~TreeItem();

    TreeItem *child(int number);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    bool insertChildren(int position, int count, int columns);
    bool insertColumns(int position, int columns);
    TreeItem *parent();
    bool removeChildren(int position, int count);
    bool removeColumns(int position, int columns);
    int childNumber() const;
    bool setData(int column, const QVariant &value);

private:
    QList<TreeItem *> childItems;
    QVector<QVariant> itemData;

    TreeItem *parentItem;
};

namespace PM
{
namespace internal
{
    struct PropertyGridTreeItem
    {
        struct Column
        {
            Qt::ItemFlags flags;
            QHash<Qt::ItemDataRole, QVariant> data;

        public:
            Column();
        };

        PM::PropertyContext context;

        PropertyGridTreeItem *parent;
        std::vector<std::unique_ptr<PropertyGridTreeItem>> children;

        bool isTransient;

        // TODO: maybe add a flag to store if the node is expanded or collapsed?!!
        // TODO: maybe add an index container for the children to access them by name?!!

    public:
        int childrenCount(bool showTransientItems = true) const;
        PropertyGridTreeItem *getChild(size_t index, bool showTransientItems = true) const;

        bool insertChildren(int position, int count, int columns);
        PropertyGridTreeItem *addChild(const PM::PropertyContext &context);

        QVariant value() const;
        void setValue(const QVariant &newValue);

        QVariant getColumnData(int columnIndex, Qt::ItemDataRole role) const;
        void setColumnData(int columnIndex, Qt::ItemDataRole role, const QVariant &newValue);

        [[deprecated]] void setDataForAllColumns(Qt::ItemDataRole role, const QVariant &newValue);

        Qt::ItemFlags flags(int columnIndex) const;
        void setFlags(int columnIndex, Qt::ItemFlags value);

        int indexInParent(bool showTransientItems = true) const;

    public:
        PropertyGridTreeItem();
        PropertyGridTreeItem(const PropertyGridTreeItem &other);

    private:
        Column columns[2];
    };

    template <typename T>
    int getIndex(const std::vector<T> &vec, const T &element)
    {
        auto it = std::find(vec.begin(), vec.end(), element);
        if (it != vec.end())
            return std::distance(vec.begin(), it);
        else
            return -1; // Element not found
    }

    template <typename T>
    int getIndex(const std::vector<std::unique_ptr<T>> &vec, const T *rawPointer)
    {
        auto it = std::find_if(vec.begin(), vec.end(), [rawPointer](const std::unique_ptr<T> &ptr) { return ptr.get() == rawPointer; });
        if (it != vec.end())
            return std::distance(vec.begin(), it);
        else
            return -1; // Raw pointer not found
    }
} // namespace internal
} // namespace PM

#endif // PROPERTYGRIDTREEITEM_H
