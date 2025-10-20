set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Widgets REQUIRED)

set(PUBLIC_HEADERS
    Property.h
    QtCompat_p.h
    PropertyGrid.h
    PropertyEditor.h
    PropertyContext.h
)

add_library(PmPropertyGrid STATIC
    Property.cpp
    PropertyEditor.cpp
    PropertyGrid.ui
    PropertyGrid_p.h
    PropertyGrid.cpp
    PropertyGridTreeItem_p.h
    PropertyGridTreeItem.cpp
    PropertyGridTreeModel_p.h
    PropertyGridTreeModel.cpp
    PropertyContext_p.h
    PropertyContext.cpp

    ${PUBLIC_HEADERS}
)

add_library(PM::PropertyGrid ALIAS PmPropertyGrid)

target_link_libraries(PmPropertyGrid
    # using versionless aliasses like Qt::Core doesn't work prior to Qt5.15
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)
