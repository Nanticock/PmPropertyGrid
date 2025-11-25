set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

add_executable(object_property_grid
    main.cpp
    ObjectPropertyGrid.h ObjectPropertyGrid.cpp

    MainWindow.h MainWindow.cpp MainWindow.ui
)

target_link_libraries(object_property_grid
    PRIVATE
        PM::PropertyGrid
)
target_link_libraries(object_property_grid PRIVATE Qt${QT_VERSION_MAJOR}::Core)
