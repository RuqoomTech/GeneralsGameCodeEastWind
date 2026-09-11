cmake_minimum_required(VERSION 3.25)

# Use packagename_ROOT for FindPackage.
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

# Disable default MSVC setting CRT type so we can set it ourselves.
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

# Disable default MSVC warning level so we can set it ourselves.
if(POLICY CMP0092)
    cmake_policy(SET CMP0092 NEW)
endif()

# Allow specifying MSVC debug configurations.
#if(POLICY CMP0141)
#    cmake_policy(SET CMP0141 NEW)
#endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

include(FeatureSummary)
include(CMakeDependentOption)

# We don't support in tree builds, so help people make the right choice.
if (CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "Building in-source is not supported! Create a build dir and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif()

# Define VCPKG feature before the project block
# since they need to be initialized before the toolchain is
include(cmake/config-vcpkg.cmake)

# Top level project, doesn't really affect anything.
project(genzh LANGUAGES C CXX)

# This file handles extra settings wanted/needed for different compilers.
include(cmake/compilers.cmake)

include(FetchContent)

# Define project options/configuration before dependency discovery so focused
# presets can avoid configuring unrelated runtime/tool dependencies.
include(cmake/config.cmake)

# MinGW-w64 compatibility settings are target-scoped through core_config.
# For a real runtime build, preflight WIDL before populating any external
# dependencies so a missing MSYS2 tool fails immediately and diagnostically.
if(MINGW)
    include(cmake/mingw.cmake)
    if(NOT RTS_BUILD_TESTS_ONLY)
        include(cmake/widl.cmake)
        rts_require_widl_for_runtime()
    endif()
    include(cmake/reactos-atl.cmake)
endif()

# Utility headers are used by both the full runtime and the focused regression
# graph, so keep this dependency available in either mode.
add_subdirectory(Dependencies/Utility)

if(RTS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Core/Tests)
endif()

if(NOT RTS_BUILD_TESTS_ONLY)
    # Debug symbol stripping is only relevant to installable MinGW binaries.
    include(cmake/debug_strip.cmake)

    # Find/Add build dependencies and stubs shared by the real game targets.
    if((WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows") AND ${CMAKE_SIZEOF_VOID_P} EQUAL 4)
        include(cmake/miles.cmake)
        include(cmake/bink.cmake)
        include(cmake/dx8.cmake)
    endif()

    # Define a dummy stlport target when not on VC6.
    if(IS_VS6_BUILD)
        include(cmake/stlport.cmake)
    else()
        add_library(stlport INTERFACE)
    endif()

    include(cmake/gamespy.cmake)
    include(cmake/lzhl.cmake)
    include(cmake/stb.cmake)

    if(IS_VS6_BUILD)
        # The original max sdk does not compile against a modern compiler.
        # If there is a desire to make this work, then a fixed max sdk needs to be created.
        add_subdirectory(Dependencies/MaxSDK)
    endif()

    # To be removed when abandoning VC6.
    if(IS_VS6_BUILD)
        include_directories(Dependencies/Utility)
        add_compile_options(/FIUtility/CppMacros.h)
    endif()

    add_subdirectory(resources)
    add_subdirectory(Core)

    if(RTS_BUILD_ZEROHOUR)
        add_subdirectory(GeneralsMD)
    endif()

    if(RTS_BUILD_GENERALS)
        add_subdirectory(Generals)
    endif()
endif()

feature_summary(WHAT ENABLED_FEATURES DESCRIPTION "Enabled features:")
feature_summary(WHAT DISABLED_FEATURES DESCRIPTION "Disabled features:")
