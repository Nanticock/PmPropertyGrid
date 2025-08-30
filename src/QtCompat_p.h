#ifndef QTCOMPAT_P_H
#define QTCOMPAT_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the PM::PropertyGrid API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
//

#include <QModelIndex>

namespace PM
{
namespace internal
{
    inline QModelIndex siblingAtColumn(const QModelIndex &index, int column)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
        return index.siblingAtColumn(column);
#else
        return index.sibling(index.row(), column);
#endif
    }

    inline QString getMetaTypeName(int typeId)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        return QMetaType(typeId).name();
#else
        return QMetaType::typeName(typeId);
#endif
    }

    inline int getVariantTypeId(const QVariant &value)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return value.typeId();
#else
        return value.type();
#endif
    }

    inline bool canConvert(const QVariant &value, int typeId)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QMetaType variantType = value.metaType();
        const QMetaType targetType(typeId);

        return QMetaType::canConvert(variantType, targetType);
#else
        return value.canConvert(typeId);
#endif
    }

    // TODO: is this function really needed?!!
    inline QVariant createDefaultVariantForType(int type)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return QVariant(QMetaType(type), nullptr);
#else
        return QVariant(QVariant::Type(type));
#endif
    }
} // namespace internal
} // namespace PM

#endif // QTCOMPAT_P_H
