cmake_minimum_required(VERSION 3.10)

project(PmPropertyGrid)

## Add paths to check for cmake modules:
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# FIXME: use C++11, for now we are using C++17 until we implement a C++17 compat layer
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PM_BUILD_PROPERTY_GRID_EXAMPLES "Determines whether or not the example projects should be built with the library" ON)

add_subdirectory(src)

if(PM_BUILD_PROPERTY_GRID_EXAMPLES)
    add_subdirectory(examples)
endif()
