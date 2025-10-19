#ifndef PROPERTYGRIDTREEMODEL_H
#define PROPERTYGRIDTREEMODEL_H

#include "PropertyEditor.h"

#include <QAbstractItemModel>
#include <QModelIndex>

namespace PM
{
class PropertyGrid;

namespace internal
{
    class PropertyGridTreeItem;

    class PropertyGridTreeModel : public QAbstractItemModel
    {
        Q_OBJECT

        friend class PM::PropertyGrid;

    public:
        explicit PropertyGridTreeModel(QObject *parent = nullptr);
        ~PropertyGridTreeModel();

        QVariant data(const QModelIndex &index, int role) const override;
        bool setData(const QModelIndex &index, const QVariant &value, int role) override;
        void setDataForAllColumns(const QModelIndex &index, const QVariant &value, Qt::ItemDataRole role);

        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

        QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
        QModelIndex parent(const QModelIndex &index) const override;

        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        int columnCount(const QModelIndex &parent = QModelIndex()) const override;

        Qt::ItemFlags flags(const QModelIndex &index) const override;

        bool insertRows(int position, int rows, const QModelIndex &parent = QModelIndex()) override;

        bool showCategories() const;
        void setShowCategories(bool newShowCategories);

        PropertyGridTreeItem *rootItem() const;

        bool isCategory(const QModelIndex &index) const;

        QModelIndex getCategory(const QString &category) const;
        PropertyGridTreeItem *getPropertyItem(const QString &propertyName) const;
        [[deprecated]] PropertyGridTreeItem *getCategoryItem(const QString &category);

        QModelIndex addProperty(const PropertyContext &context);

        PropertyGridTreeItem *getItem(const QModelIndex &index) const; // FIXME: should this be public?!!
        QModelIndex getItemIndex(PropertyGridTreeItem *item) const;

        void clearModel();
        QStringList getPropertiesNames() const;

    private:
        bool m_showCategories;
        PropertyGridTreeItem *m_rootItem;

        QHash<QString, PropertyGridTreeItem *> m_propertiesMap;
        QHash<QString, PropertyGridTreeItem *> m_categoriesMap;
    };
} // namespace internal
} // namespace PM

inline QStringList PM::internal::PropertyGridTreeModel::getPropertiesNames() const
{
    return m_propertiesMap.keys();
}

#endif // PROPERTYGRIDTREEMODEL_H
