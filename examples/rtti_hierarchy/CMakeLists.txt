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
# ABI-specific source selection
# ------------------------------------------------------------
#
# MSVC is true for both cl.exe and clang-cl
# (CMAKE_CXX_SIMULATE_ID == "MSVC" for clang-cl).
# Everything else (GCC, Clang on Linux/macOS/Android/…) uses the Itanium ABI.

if (MSVC)
    set(RTTI_ABI_IMPL rtti_hierarchy_msABI.cpp)
    set(RTTI_ABI_PRIVATE_HEADERS MicrosoftRTTI_p.h)
    message(STATUS "rtti_hierarchy: MS ABI selected (cl.exe / clang-cl)")
else()
    set(RTTI_ABI_IMPL rtti_hierarchy_itaniumABI.cpp)
    set(RTTI_ABI_PRIVATE_HEADERS "")
    message(STATUS "rtti_hierarchy: Itanium ABI selected (GCC / Clang)")
endif()

# ------------------------------------------------------------
# Test executable
# ------------------------------------------------------------

add_executable(rtti_hierarchy_tests
    rtti_hierarchy_tests.cpp
    rtti_hierarchy.cpp
    rtti_hierarchy.h
    rtti_hierarchy_impl_p.h
    ${RTTI_ABI_IMPL}
    ${RTTI_ABI_PRIVATE_HEADERS}
)

# Make sure big object files are enabled,
# Catch2's single-header distribution can easily exceed the object file size limit on some platforms.
target_compile_options(rtti_hierarchy_tests
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
        $<$<CXX_COMPILER_ID:GNU>:-Wa,-mbig-obj>
        $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:/bigobj>
        $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:GNU>>:-Wa,-mbig-obj>
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

# On Apple platforms libc++.dylib does not re-export libc++abi.dylib.
# The typeinfo objects for the ABI type-info hierarchy types
# (__class_type_info, __si_class_type_info, __vmi_class_type_info) are
# defined in libc++abi.dylib, so we must link against it explicitly.
if (APPLE)
    target_link_libraries(rtti_hierarchy_tests PRIVATE c++abi)
endif()

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
