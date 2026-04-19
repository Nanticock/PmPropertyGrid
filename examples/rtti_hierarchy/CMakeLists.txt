cmake_minimum_required(VERSION 3.16)

project(rtti_hierarchy_tests
    VERSION 1.0
    LANGUAGES CXX)

# ------------------------------------------------------------
# Configuration
# ------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
include(CTest)
enable_testing()

# ------------------------------------------------------------
# Fetch Catch2 (v2.x compatible with current test file)
# ------------------------------------------------------------

include(FetchContent)

FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v2.13.10
)

FetchContent_MakeAvailable(catch2)

# ------------------------------------------------------------
# Library sources (currently built directly into test target)
# ------------------------------------------------------------

set(RTTI_HIERARCHY_SOURCES
    rtti_hierarchy.cpp
    rtti_hierarchy.h
)

# ------------------------------------------------------------
# Test executable
# ------------------------------------------------------------

add_executable(rtti_hierarchy_tests
    rtti_hierarchy_tests.cpp
    ${RTTI_HIERARCHY_SOURCES}
)

# Include current source directory

target_include_directories(rtti_hierarchy_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link Catch2

target_link_libraries(rtti_hierarchy_tests
    PRIVATE
        Catch2::Catch2
)

# ------------------------------------------------------------
# Compiler warnings (sensible defaults for MSVC/GCC/Clang)
# ------------------------------------------------------------

if (MSVC)
    target_compile_options(rtti_hierarchy_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(rtti_hierarchy_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

# ------------------------------------------------------------
# Register test with CTest
# ------------------------------------------------------------

add_test(
    NAME rtti_hierarchy_tests
    COMMAND rtti_hierarchy_tests
)
