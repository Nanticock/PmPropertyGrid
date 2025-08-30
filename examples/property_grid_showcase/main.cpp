#include <PropertyGrid.h>

#include <QtCompat_p.h>

#include <QApplication>
#include <QBitmap>
#include <QCheckBox>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QHeaderView>
#include <QKeySequence>
#include <QLayout>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

PM::Property createProperty(const QString &name, int type, const QString &description = "", const QVariant &defaultValue = QVariant(),
                            const QString &category = "")
{
    PM::Property result(name, type);

    if (!description.isEmpty())
        result.addAttribute(PM::DescriptionAttribute(description));

    if (defaultValue.isValid() && result.type() == PM::internal::getVariantTypeId(defaultValue))
        result.addAttribute(PM::DefaultValueAttribute(defaultValue));

    if (!category.isEmpty())
        result.addAttribute(PM::CategoryAttribute(category));

    return result;
}

void addProperty(PM::PropertyGrid &grid, const QString &name, const QVariant &value, const QString &description = "",
                 const QVariant &defaultValue = QVariant(), const QString &category = "")
{
    PM::Property property = createProperty(name, PM::internal::getVariantTypeId(value), description, defaultValue, category);

    grid.addProperty(property, value);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    PM::PropertyGrid propertyGrid;
    propertyGrid.addPropertyEditor<PM::FontPropertyEditor>();
    propertyGrid.addPropertyEditor<PM::SizePropertyEditor>();
    propertyGrid.addPropertyEditor<PM::BoolPropertyEditor>();
    propertyGrid.addPropertyEditor<PM::RectPropertyEditor>();
    propertyGrid.addPropertyEditor<PM::ColorPropertyEditor>();
    propertyGrid.addPropertyEditor<PM::ImagesPropertyEditor>();
    propertyGrid.addPropertyEditor<PM::CursorPropertyEditor>();

    // add properties
    addProperty(propertyGrid, "Property1", "Value1", "Description of property1");
    addProperty(propertyGrid, "text", "The quick brown fox jumps over the lazy dog", "the text to be displayed in the UI");

    QString category = "General";
    {
        addProperty(propertyGrid, "Executable directories", "",
                    "Path to use when searching for executable files while building a VC++ project. Corresponds to environment variable PATH.", "",
                    category);

        addProperty(propertyGrid, "Include directories", "",
                    "Path to use when searching for include files while building a VC++ project. Corresponds to environment variable INCLUDE.", "",
                    category);

        addProperty(propertyGrid, "External include directories", "",
                    "Path to treat as external/system during compilation and skip in build up-to-date check.", "", category);
    }

    category = "Public project content";
    {
        addProperty(propertyGrid, "Public include directories", "", "One or more directories to add to the include path in the referencing projects.",
                    "", category);

        addProperty(
            propertyGrid, "All header files are public", false,
            "Specifies if directories or all project header files should be automatically added to the include path in the referencing projects.",
            true, category);

        addProperty(propertyGrid, "Public C++ modules directories", "",
                    "One or more this project directories containing one or more C++ modules/header unit sources to make automatically "
                    "available in the referencing projects.",
                    "", category);
    }

    category = "Category/Sub-category";
    {
        addProperty(propertyGrid, "length (int property)", 50, "Test int property", -1, category);
    }

    QFont font = QApplication::font();
    // font.setBold(true);
    // font.setUnderline(true);
    addProperty(propertyGrid, "font (QFont property)", font);
    addProperty(propertyGrid, "enabled (bool property)", true);
    addProperty(propertyGrid, "color (QColor property)", QColor(Qt::red), "Test color property", QColor(Qt::black));
    addProperty(propertyGrid, "app (QObject property)", QVariant::fromValue(&app));
    addProperty(propertyGrid, "size (QSize property)", QSize(5, 10), "", QSize(), category);
    addProperty(propertyGrid, "sizeF (QSizeF property)", QSizeF(5, 10), "", QSizeF(), category);
    addProperty(propertyGrid, "rect (QRect property)", QRect(5, 10, 50, 100));
    addProperty(propertyGrid, "rectF (QRectF property)", QRectF(5, 10, 50, 100));
    addProperty(propertyGrid, "point (QPoint property)", QPoint(50, 100));
    addProperty(propertyGrid, "pointF (QPointF property)", QPointF(50, 100));
    addProperty(propertyGrid, "layout (QEnum property)", QLayout::SizeConstraint::SetFixedSize);
    addProperty(propertyGrid, "date (QDate property)", QDate::currentDate());
    addProperty(propertyGrid, "time (QTime property)", QTime::currentTime());
    addProperty(propertyGrid, "date-time (QDateTime property)", QDateTime::currentDateTime());
    addProperty(propertyGrid, "keyboard shortcut (QKeySequence property)", QKeySequence("ctrl+c"));
    addProperty(propertyGrid, "separator (QChar property)", QChar(','));
    addProperty(propertyGrid, "cursor (QCursor property)", QCursor(Qt::CursorShape::ArrowCursor));
    // exotic types
    addProperty(propertyGrid, "vector2D (QVector2D property)", QVector2D(1, 2));
    addProperty(propertyGrid, "vector3D (QVector3D property)", QVector3D(1, 2, 3));
    addProperty(propertyGrid, "vector4D (QVector4D property)", QVector4D(1, 2, 3, 4));
    addProperty(propertyGrid, "matrix4x4 (QMatrix4x4 property)", QMatrix4x4());
    addProperty(propertyGrid, "line (QLine property)", QLine(50, 100, 100, 50));
    addProperty(propertyGrid, "lineF (QLineF property)", QLineF(50, 100, 100, 50));
    addProperty(propertyGrid, "polygon (QPolygon property)", QPolygon({50, 100, 100, 50}));
    addProperty(propertyGrid, "polygonF (QPolygonF property)", QPolygonF({50, 100, 100, 50}));
    addProperty(propertyGrid, "icon (QIcon property)", QIcon(":/icons/preferences-svgrepo-com.svg"));
    addProperty(propertyGrid, "pixmap (QPixmap property)", QPixmap(":/icons/preferences-svgrepo-com.svg"));
    addProperty(propertyGrid, "bitmap (QBitmap property)", QBitmap("C:/Users/hp/Downloads/ProbeMaestro_icon.png"));
    addProperty(propertyGrid, "image (QImage property)", QImage("C:/Users/hp/Downloads/ProbeMaestro_icon.png"));
    // Simple properties
    propertyGrid.addProperty("default color (no-attributes property)", QColor(Qt::green));
    propertyGrid.addProperty("current color (Read-only property)", QColor(Qt::blue), PM::ReadOnlyAttribute());
    propertyGrid.addProperty("sample text1 (Value from variant)", QVariant("string value 1"));
    propertyGrid.addProperty("sample text2 (Value from variant)", QVariant("string value 2"), PM::ReadOnlyAttribute());
    propertyGrid.addProperty("sample text3 (Value with no attributes)", "string value 3");
    propertyGrid.addProperty("sample text4 (Value with multi-attributes)", QString("string value 4"), PM::Attribute(),
                             PM::DescriptionAttribute("N/A"));
    propertyGrid.addProperty("bounds (QRect)", QRect(), PM::ReadOnlyAttribute());

    propertyGrid.show();

    QCheckBox checkbox(&propertyGrid);
    checkbox.setText("show categories");
    checkbox.setChecked(true);

    QObject::connect(&checkbox, &QCheckBox::stateChanged, &checkbox,
                     [&](int)
                     {
                         //
                         propertyGrid.setShowCategories(checkbox.isChecked());
                     });

    propertyGrid.layout()->addWidget(&checkbox);

    QObject::connect(&propertyGrid, &PM::PropertyGrid::propertyValueChanged, &propertyGrid,
                     [](const PM::PropertyContext &context)
                     {
    // for some reson this line causes builds to fail in Qt5 under macOS
#if !(defined(Q_OS_MACOS) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
                         qInfo() << "propertyValueChanged" << context.property().name() << context.value();
#endif
                     });

    return app.exec();
}
