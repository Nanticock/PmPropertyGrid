# PmPropertyGrid

A modern, feature-rich property grid widget for Qt applications written in C++. This library provides a flexible and customizable property editor that displays object properties in a tree-like structure, similar to property editors found in IDEs and game engines.

## Features

- **Comprehensive Type Support**: Built-in support for all major Qt types including:
  - Basic types: `int`, `bool`, `QString`, `double`
  - Geometric types: `QPoint`, `QPointF`, `QSize`, `QSizeF`, `QRect`, `QRectF`, `QLine`, `QLineF`
  - Graphics types: `QColor`, `QFont`, `QIcon`, `QPixmap`, `QBitmap`, `QImage`
  - Advanced types: `QVector2D`, `QVector3D`, `QVector4D`, `QMatrix4x4`, `QPolygon`, `QPolygonF`
  - Date/Time types: `QDate`, `QTime`, `QDateTime`
  - UI types: `QKeySequence`, `QCursor`, `QChar`
  - Object types: `QObject*` and custom enums

- **Property Attributes**: Rich attribute system for enhanced property configuration:
  - `DescriptionAttribute`: Add helpful descriptions to properties
  - `DefaultValueAttribute`: Define default values for properties
  - `CategoryAttribute`: Organize properties into collapsible categories
  - `ReadOnlyAttribute`: Mark properties as read-only

- **Flexible API**: Multiple ways to add properties:
  - Simple property addition with automatic type detection
  - Detailed property configuration with attributes
  - Batch property addition with categories

- **Interactive Features**:
  - Collapsible categories for better organization
  - Real-time property value change notifications
  - Support for custom property editors
  - Responsive tree-based layout

- **Cross-Platform**: Compatible with Qt 5.15+ and Qt 6.x on Windows, macOS, and Linux

## Requirements

- **CMake**: 3.10 or higher
- **Qt**: 5.15.2 or Qt 6.5.3+ (Widgets module required)
- **C++ Standard**: C++17
- **Compiler**: MSVC 2019, GCC, or Clang with C++17 support

## Building

### Using CMake

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Integration with Qt Creator

The project includes Qt Creator configuration files (`.user` files) for easy development setup.

## Quick Start

```cpp
#include <PropertyGrid.h>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    PM::PropertyGrid propertyGrid;
    
    // Add simple properties
    propertyGrid.addProperty("Name", QString("My Object"));
    propertyGrid.addProperty("Enabled", true);
    propertyGrid.addProperty("Color", QColor(Qt::red));
    
    // Add properties with attributes
    propertyGrid.addProperty("Width", 100, 
        PM::DescriptionAttribute("Object width in pixels"),
        PM::DefaultValueAttribute(50),
        PM::CategoryAttribute("Geometry"));
    
    // Add read-only property
    propertyGrid.addProperty("Status", QString("Ready"), 
        PM::ReadOnlyAttribute());
    
    // Connect to value changes
    QObject::connect(&propertyGrid, &PM::PropertyGrid::propertyValueChanged,
        [](const PM::PropertyContext &context) {
            qDebug() << "Property" << context.property().name 
                     << "changed to" << context.value();
        });
    
    propertyGrid.show();
    return app.exec();
}
```

## Examples

The `examples/property_grid_showcase` directory contains a comprehensive demonstration of the library's capabilities, showcasing:

- All supported property types
- Property attributes and categorization
- Interactive property editing
- Real-time value change handling

To build and run the showcase:

```bash
cd build/examples/property_grid_showcase
./property_grid_showcase  # or property_grid_showcase.exe on Windows
```

## API Overview

### Core Classes

- **`PM::PropertyGrid`**: Main widget for displaying and editing properties
- **`PM::Property`**: Represents a single property with type information and attributes
- **`PM::PropertyContext`**: Provides context for property operations including current value
- **`PM::PropertyEditor`**: Base class for custom property editors

### Attribute System

The library uses a flexible attribute system to enhance property behavior:

```cpp
// Property with description and default value
propertyGrid.addProperty("Opacity", 1.0f,
    PM::DescriptionAttribute("Object transparency (0.0 - 1.0)"),
    PM::DefaultValueAttribute(0.5f));

// Categorized read-only property
propertyGrid.addProperty("Object ID", 12345,
    PM::CategoryAttribute("System"),
    PM::ReadOnlyAttribute());
```

## Project Structure

```
PmPropertyGrid/
├── src/                    # Core library source code
│   ├── PropertyGrid.h/cpp  # Main property grid widget
│   ├── Property.h/cpp      # Property definition and attributes
│   ├── PropertyEditor.h/cpp # Property editor framework
│   └── PropertyGrid*.h/cpp  # Internal implementation files
├── examples/               # Example applications
│   └── property_grid_showcase/ # Comprehensive demo
├── build/                  # Build output directory
├── cmake/                  # CMake modules
└── tests/                  # Unit tests (coming soon)
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit pull requests, report bugs, or suggest new features.

## Development Status

This library is actively developed and ready for production use. Current development focuses on:

- [ ] Dark mode and theme support
- [ ] Additional property types
- [ ] Custom property editor framework
- [ ] Comprehensive unit tests
- [ ] Documentation improvements

## Acknowledgments

Built with Qt framework and follows modern C++17 practices for type safety and performance.
