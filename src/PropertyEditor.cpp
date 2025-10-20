#include "PropertyEditor.h"

#include "PropertyContext_p.h"
#include "QtCompat_p.h"

#include <QApplication>
#include <QBitArray>
#include <QBitmap>
#include <QColorDialog>
#include <QDate>
#include <QDebug>
#include <QFontDialog>
#include <QMetaEnum>
#include <QPainter>
#include <QPushButton> // FIXME: remove this
#include <QUrl>

namespace
{
// FIXME: make this high-DPI aware
const std::uint32_t PROPERTY_EDITOR_DECORATION_WIDTH = 20;
const std::uint32_t PROPERTY_EDITOR_DECORATION_HEIGHT = 16;

const char PROPERTY_EDITOR_TRUE_STRING[] = "true";
const char PROPERTY_EDITOR_FALSE_STRING[] = "false";

const char PROPERTY_EDITOR_STRING_LIST_SEPARATOR = '\n';
const char PROPERTY_EDITOR_FONT_STRING_SEPARATOR[] = ", ";
} // namespace

using namespace PM;
using namespace PM::internal;

const QString &boolToString(bool value)
{
    static QString trueKey = PROPERTY_EDITOR_TRUE_STRING;
    static QString falseKey = PROPERTY_EDITOR_FALSE_STRING;

    return value ? trueKey : falseKey;
}

template <typename T>
std::pair<TypeId, std::shared_ptr<T>> createPropertyEditorMapEntry()
{
    return std::pair<TypeId, std::shared_ptr<T>>(internal::getTypeId<T>(), std::make_shared<T>());
}

const PropertyEditorsMap_t &PM::internal::defaultPropertyEditors()
{
    // TODO: should have the ability to modify this list at runtime if we ever made this API public

    // clang-format off
    static PropertyEditorsMap_t instance = {
        createPropertyEditorMapEntry<SizePropertyEditor>(),
        createPropertyEditorMapEntry<RectPropertyEditor>(),
        createPropertyEditorMapEntry<FontPropertyEditor>(),
        createPropertyEditorMapEntry<ColorPropertyEditor>(),
        createPropertyEditorMapEntry<ImagesPropertyEditor>(),
        createPropertyEditorMapEntry<CursorPropertyEditor>(),
        createPropertyEditorMapEntry<BoolPropertyEditor>(),
    };
    // clang-format on

    return instance;
}

bool PropertyEditor::canHandle(const PropertyContext &context) const
{
    return true;
}

QString PropertyEditor::toString(const PropertyContext &context) const
{
    const QVariant value = context.value();

    switch (context.property().type())
    {
    case qMetaTypeId<QString>():
        return value.toString();

    case qMetaTypeId<int>():
        return QString::number(value.toInt());

    case qMetaTypeId<double>():
        return QString::number(value.toDouble());

    case qMetaTypeId<bool>():
        return value.toBool() ? PROPERTY_EDITOR_TRUE_STRING : PROPERTY_EDITOR_FALSE_STRING;

    case qMetaTypeId<QChar>():
        return QString(value.toChar());

    case qMetaTypeId<std::uint32_t>():
        return QString::number(value.toUInt());

    case qMetaTypeId<std::int64_t>():
        return QString::number(value.toLongLong());

    case qMetaTypeId<std::uint64_t>():
        return QString::number(value.toULongLong());

    case qMetaTypeId<QDate>():
        return value.toDate().toString(Qt::ISODate);

    case qMetaTypeId<QTime>():
        return value.toTime().toString(Qt::ISODate);

    case qMetaTypeId<QDateTime>():
        return value.toDateTime().toString(Qt::ISODate);

    case qMetaTypeId<QByteArray>():
        return QString::fromUtf8(value.toByteArray());

    case qMetaTypeId<QStringList>():
        return value.toStringList().join(PROPERTY_EDITOR_STRING_LIST_SEPARATOR);

    case qMetaTypeId<QUrl>():
        return value.toUrl().toString();

    case qMetaTypeId<QBitArray>():
    {
        QByteArray byteArray;

        for (int i = 0; i < value.toBitArray().size(); ++i)
            byteArray.append(value.toBitArray().testBit(i) ? '1' : '0');

        return QString::fromUtf8(byteArray.toHex());
    }

    default:
        break;
    }

    return value.toString();
}

QVariant PropertyEditor::fromString(const QString &value, const PropertyContext &context, QString *errorMessage) const
{
    static const char errorMessageTemplate[] = "%1 is not a valid value for %2.";

    bool _ok;
    switch (context.property().type())
    {
    case qMetaTypeId<QString>():
        return value;

    case qMetaTypeId<int>():
    {
        int intValue = value.toInt(&_ok);

        if (!_ok)
            break;

        return intValue;
    }

    case qMetaTypeId<double>():
    {
        double doubleValue = value.toDouble(&_ok);

        if (!_ok)
            break;

        return doubleValue;
    }

    case qMetaTypeId<bool>():
    {
        if (value.compare(PROPERTY_EDITOR_TRUE_STRING, Qt::CaseInsensitive) == 0)
            return true;
        else if (value.compare(PROPERTY_EDITOR_FALSE_STRING, Qt::CaseInsensitive) == 0)
            return false;
    }
    break;

    case qMetaTypeId<QChar>():
        if (value.length() != 1)
            break;

        return value.at(0);

    case qMetaTypeId<std::uint32_t>():
    {
        std::uint32_t uintValue = value.toUInt(&_ok);

        if (!_ok)
            break;

        return uintValue;
    }

    case qMetaTypeId<std::int64_t>():
    {
        std::int64_t longValue = value.toLongLong(&_ok);

        if (!_ok)
            break;

        return QVariant::fromValue(longValue);
    }

    case qMetaTypeId<std::uint64_t>():
    {
        std::uint64_t ulongValue = value.toULongLong(&_ok);

        if (!_ok)
            break;

        return QVariant::fromValue(ulongValue);
    }

    case qMetaTypeId<QDate>():
    {
        QDate dateValue = QDate::fromString(value, Qt::ISODate);

        if (!dateValue.isValid())
            break;

        return dateValue;
    }

    case qMetaTypeId<QTime>():
    {
        QTime timeValue = QTime::fromString(value, Qt::ISODate);

        if (!timeValue.isValid())
            break;

        return timeValue;
    }

    case qMetaTypeId<QDateTime>():
    {
        QDateTime dateTimeValue = QDateTime::fromString(value, Qt::ISODate);

        if (!dateTimeValue.isValid())
            break;

        return dateTimeValue;
    }

    case qMetaTypeId<QByteArray>():
        return value.toUtf8();

    case qMetaTypeId<QStringList>():
        return value.split(PROPERTY_EDITOR_STRING_LIST_SEPARATOR);

    case qMetaTypeId<QUrl>():
    {
        QUrl urlValue = QUrl(value);

        if (!urlValue.isValid())
            break;

        return urlValue;
    }

    case qMetaTypeId<QBitArray>():
    {
        QByteArray byteArray = QByteArray::fromHex(value.toUtf8());

        if (byteArray.isEmpty())
            break;

        QBitArray bitArray(byteArray.size() * 8);
        for (int i = 0; i < byteArray.size(); ++i)
        {
            for (int j = 0; j < 8; ++j)
                bitArray.setBit(i * 8 + j, (byteArray.at(i) & (1 << j)) != 0);
        }

        return bitArray;
    }

    default:
        break;
    }

    if (errorMessage != nullptr)
        *errorMessage = QString(errorMessageTemplate).arg(value, PM::internal::getMetaTypeName(context.property().type()));

    return QVariant();
}

QVariant PropertyEditor::modalClicked(const PropertyContext &context) const
{
    return context.value();
}

std::variant<QWidget *, QVariantList> PropertyEditor::getDropDown(const PropertyContext &context) const
{
    return nullptr;
}

PropertyEditor::EditStyle PropertyEditor::getEditStyle(const PropertyContext &context) const
{
    return None;
}

QPixmap PropertyEditor::getPreviewIcon(const PropertyContext &context) const
{
    return QPixmap();
}

void PropertyEditor::editValue(const PropertyContext &context, const QVariant &newValue) const
{
    if (context.value() == newValue)
        return;

    PropertyContextPrivate::notifyValueChanged(context, newValue);
}

bool SizePropertyEditor::canHandle(const PropertyContext &context) const
{
    switch (context.property().type())
    {
    case qMetaTypeId<QSize>():
        return true;

    case qMetaTypeId<QSizeF>():
        return true;

    default:
        break;
    }

    return false;
}

QString SizePropertyEditor::toString(const PropertyContext &context) const
{
    static const char stringTemplate[] = "%1, %2";

    const QVariant value = context.value();
    switch (context.property().type())
    {
    case qMetaTypeId<QSize>():
    {
        QSize v = value.value<QSize>();

        return QString(stringTemplate).arg(v.width()).arg(v.height());
    }

    case qMetaTypeId<QSizeF>():
    {
        QSizeF v = value.value<QSizeF>();

        return QString(stringTemplate).arg(v.width()).arg(v.height());
    }
    default:
        break;
    }

    return QString(stringTemplate).arg(0).arg(0);
}

bool RectPropertyEditor::canHandle(const PropertyContext &context) const
{
    switch (context.property().type())
    {
    case qMetaTypeId<QRect>():
        return true;

    case qMetaTypeId<QRectF>():
        return true;

    default:
        break;
    }

    return false;
}

QString RectPropertyEditor::toString(const PropertyContext &context) const
{
    static const char stringTemplate[] = "%1, %2, %3, %4";

    const QVariant value = context.value();
    switch (context.property().type())
    {
    case qMetaTypeId<QRect>():
    {
        QRect v = value.value<QRect>();

        return QString(stringTemplate).arg(v.x()).arg(v.y()).arg(v.width()).arg(v.height());
    }

    case qMetaTypeId<QRectF>():
    {
        QRectF v = value.value<QRectF>();

        return QString(stringTemplate).arg(v.x()).arg(v.y()).arg(v.width()).arg(v.height());
    }
    default:
        break;
    }

    return QString(stringTemplate).arg(0).arg(0).arg(0).arg(0);
}

bool FontPropertyEditor::canHandle(const PropertyContext &context) const
{
    return context.property().type() == qMetaTypeId<QFont>();
}

QString FontPropertyEditor::toString(const PropertyContext &context) const
{
    QFont font = context.value().value<QFont>();

    static const QFont defaultFont; // Dynamically fetch default font

    static const QString weightKey = "weight=";
    static const QString styleKey = "style=";
    static const QString underlineKey = "underline=";
    static const QString strikeOutKey = "strikeOut=";
    static const QString kerningKey = "kerning=";

    QStringList components;

    // Mandatory properties (no key names)
    components.append(font.family());

    if (font.pointSize() > 0)
        components.append(QString("%1pt").arg(font.pointSize()));
    else
        components.append(QString("%1px").arg(font.pixelSize()));

    // Conditional properties
    if (font.weight() != defaultFont.weight())
        components.append(weightKey + QString::number(font.weight()));

    if (font.style() != defaultFont.style())
        components.append(styleKey + QString::number(font.style()));

    if (font.underline() != defaultFont.underline())
        components.append(underlineKey + boolToString(font.underline()));

    if (font.strikeOut() != defaultFont.strikeOut())
        components.append(strikeOutKey + boolToString(font.strikeOut()));

    if (font.kerning() != defaultFont.kerning()) // Respect user-defined default font
        components.append(kerningKey + boolToString(font.kerning()));

    return components.join(PROPERTY_EDITOR_FONT_STRING_SEPARATOR);
}

QVariant FontPropertyEditor::fromString(const QString &value, const PropertyContext &context, QString *errorMessage) const
{
    static const QString weightKey = "weight=";
    static const QString styleKey = "style=";
    static const QString underlineKey = "underline=";
    static const QString strikeOutKey = "strikeOut=";
    static const QString kerningKey = "kerning=";

    QStringList properties = value.split(PROPERTY_EDITOR_FONT_STRING_SEPARATOR);
    if (properties.size() < 2) // Family and size must be present
    {
        if (errorMessage)
            *errorMessage = "Invalid font format: Missing family or size.";

        return QVariant();
    }

    QString fontFamily = properties[0];

    QString sizeValue = properties[1];
    bool isPixelSize = sizeValue.endsWith("px");
    int fontSize = sizeValue.left(sizeValue.length() - (isPixelSize ? 2 : 0)).toInt();

    QFont font(fontFamily);
    if (isPixelSize)
        font.setPixelSize(fontSize);
    else
        font.setPointSize(fontSize);

    // Apply optional properties
    for (int i = 2; i < properties.size(); ++i)
    {
        const QString &prop = properties[i];

        if (prop.startsWith(weightKey))
            font.setWeight(static_cast<QFont::Weight>(prop.mid(weightKey.length()).toInt()));
        else if (prop.startsWith(styleKey))
            font.setStyle(static_cast<QFont::Style>(prop.mid(styleKey.length()).toInt()));
        else if (prop.startsWith(underlineKey))
            font.setUnderline(prop.mid(underlineKey.length()).toInt());
        else if (prop.startsWith(strikeOutKey))
            font.setStrikeOut(prop.mid(strikeOutKey.length()).toInt());
        else if (prop.startsWith(kerningKey))
            font.setKerning(prop.mid(kerningKey.length()).toInt());
    }

    return QVariant::fromValue(font);
}

QVariant FontPropertyEditor::modalClicked(const PropertyContext &context) const
{
    bool ok = false;
    QFont result = QFontDialog::getFont(&ok, context.value().value<QFont>());

    if (!ok)
        return PropertyEditor::modalClicked(context);

    return result;
}

PropertyEditor::EditStyle FontPropertyEditor::getEditStyle(const PropertyContext &context) const
{
    return Modal;
}

QPixmap FontPropertyEditor::getPreviewIcon(const PropertyContext &context) const
{
    const QFont font = context.value().value<QFont>();

    QPixmap result(PROPERTY_EDITOR_DECORATION_WIDTH, PROPERTY_EDITOR_DECORATION_HEIGHT);
    result.fill(QApplication::palette().color(QPalette::Highlight));

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);

    QFont fixedSizeFont = font;
    fixedSizeFont.setPointSize(QApplication::font().pointSize());

    static const QString previewString = "Ab";

    QFontMetrics fontMetrics(font);
    QRect textRectangle = fontMetrics.boundingRect(previewString);
    textRectangle.moveCenter(result.rect().center());

    painter.setFont(fixedSizeFont);
    painter.setPen(QApplication::palette().color(QPalette::HighlightedText));
    painter.drawText(textRectangle, previewString);

    return result;
}

bool ColorPropertyEditor::canHandle(const PropertyContext &context) const
{
    return context.property().type() == qMetaTypeId<QColor>();
}

QString ColorPropertyEditor::toString(const PropertyContext &context) const
{
    static const char stringTemplate[] = "%1, %2, %3, (%4)";

    // TODO: implement showing the name of named colors
    QColor v = context.value().value<QColor>();
    return QString(stringTemplate).arg(v.red()).arg(v.green()).arg(v.blue()).arg(v.alpha());
}

QVariant ColorPropertyEditor::modalClicked(const PropertyContext &context) const
{
    QColor result = QColorDialog::getColor(context.value().value<QColor>());

    if (!result.isValid())
        return PropertyEditor::modalClicked(context);

    return result;
}

PropertyEditor::EditStyle ColorPropertyEditor::getEditStyle(const PropertyContext &context) const
{
    return Modal;
}

QPixmap ColorPropertyEditor::getPreviewIcon(const PropertyContext &context) const
{
    QColor color = context.value().value<QColor>();

    QPixmap result(PROPERTY_EDITOR_DECORATION_WIDTH, PROPERTY_EDITOR_DECORATION_HEIGHT);
    result.fill(color);

    return result;
}

bool ImagesPropertyEditor::canHandle(const PropertyContext &context) const
{
    switch (context.property().type())
    {
    case qMetaTypeId<QImage>():
        return true;

    case qMetaTypeId<QPixmap>():
        return true;

    case qMetaTypeId<QBitmap>():
        return true;

    case qMetaTypeId<QIcon>():
        return true;

    default:
        break;
    }

    return false;
}

QString ImagesPropertyEditor::toString(const PropertyContext &context) const
{
    return PM::internal::getMetaTypeName(context.property().type());
}

QPixmap ImagesPropertyEditor::getPreviewIcon(const PropertyContext &context) const
{
    const QVariant value = context.value();

    switch (context.property().type())
    {
    case qMetaTypeId<QImage>():
    {
        const QImage image = value.value<QImage>();

        return QPixmap::fromImage(image);
    }

    case qMetaTypeId<QPixmap>():
    {
        return value.value<QPixmap>();
    }

    case qMetaTypeId<QBitmap>():
    {
        return value.value<QBitmap>();
    }

    case qMetaTypeId<QIcon>():
    {
        const QIcon icon = value.value<QIcon>();

        return icon.pixmap(PROPERTY_EDITOR_DECORATION_WIDTH, PROPERTY_EDITOR_DECORATION_HEIGHT);
    }

    default:
        break;
    }

    return QPixmap();
}

QString CursorPropertyEditor::cursorShapeName(Qt::CursorShape shape)
{
    QMetaEnum metaEnum = QMetaEnum::fromType<Qt::CursorShape>();

    return QString::fromLatin1(metaEnum.valueToKey(shape));
}

bool CursorPropertyEditor::canHandle(const PropertyContext &context) const
{
    return context.property().type() == qMetaTypeId<QCursor>();
}

QString CursorPropertyEditor::toString(const PropertyContext &context) const
{
    QCursor v = context.value().value<QCursor>();

    return cursorShapeName(v.shape());
}

bool BoolPropertyEditor::canHandle(const PropertyContext &context) const
{
    return context.property().type() == qMetaTypeId<bool>();
}

QString BoolPropertyEditor::toString(const PropertyContext &context) const
{
    return context.value().toBool() == true ? PROPERTY_EDITOR_TRUE_STRING : PROPERTY_EDITOR_FALSE_STRING;
}

std::variant<QWidget *, QVariantList> BoolPropertyEditor::getDropDown(const PropertyContext &context) const
{
    static QVariantList result{true, false};

    return result;
}

PropertyEditor::EditStyle BoolPropertyEditor::getEditStyle(const PropertyContext &context) const
{
    return DropDown;
}
