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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    using StringRef_t = QStringView;
#elif QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    using StringRef_t = QStringRef;
#else
    using StringRef_t = QString;
#endif

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

    inline StringRef_t stringLeft(const QString &input, int n)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return QStringView(input).left(n);
#elif QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        return input.leftRef(n);
#else
        return input.left(n);
#endif
    }

    inline StringRef_t stringMid(const QString &input, int position, int n = -1)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return QStringView(input).mid(position, n);
#elif QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        return input.midRef(position, n);
#else
        return input.mid(position, n);
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
