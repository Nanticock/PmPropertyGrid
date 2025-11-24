set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Widgets REQUIRED)

add_executable(property_grid_showcase
    resources.qrc
    main.cpp
)

target_link_libraries(property_grid_showcase
    PRIVATE
        PM::PropertyGrid
)
