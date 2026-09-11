cmake_minimum_required(VERSION 3.25)

project(z_generals LANGUAGES C CXX)

if (RTS_BUILD_ZEROHOUR_DOCS)
    find_package(Doxygen REQUIRED)
    doxygen_add_docs(z_docs Code)
endif()

# Set where the build results will end up
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Add main build targets
add_subdirectory(Code)

# If we are building on windows for windows, try and get the game install path from the registry.
if("${CMAKE_HOST_SYSTEM}" MATCHES "Windows" AND "${CMAKE_SYSTEM}" MATCHES "Windows")
    include(${CMAKE_SOURCE_DIR}/cmake/registry.cmake)

    # Check the EA App/CD registry path
    fetch_registry_value(
        "HKEY_LOCAL_MACHINE/SOFTWARE/Electronic Arts/EA Games/Command and Conquer Generals Zero Hour"
        "InstallPath"
        RTS_INSTALL_PREFIX_ZEROHOUR
        "Zero Hour install path from registry")

    # Check the "First Decade" registry path
    fetch_registry_value(
        "HKEY_LOCAL_MACHINE/SOFTWARE/Electronic Arts/EA Games/Command and Conquer The First Decade"
        "zh_folder"
        RTS_INSTALL_PREFIX_ZEROHOUR
        "Zero Hour install path from registry")

    # Check the Steam registry path
    fetch_registry_value(
        "HKEY_LOCAL_MACHINE/SOFTWARE/Electronic Arts/EA Games/ZeroHour"
        "installPath"
        RTS_INSTALL_PREFIX_ZEROHOUR
        "Zero Hour install path from registry")
endif()

if(RTS_INSTALL_PREFIX_ZEROHOUR)
    rts_install_runtime_target(z_generals "${RTS_INSTALL_PREFIX_ZEROHOUR}")

    foreach(_rts_install_target IN ITEMS
        z_worldbuilder
        core_particleeditor
        core_debugwindow
        z_guiedit
        z_imagepacker
        z_mapcachebuilder
        z_w3dview
        z_wdump
    )
        if(TARGET ${_rts_install_target})
            rts_install_runtime_target(${_rts_install_target} "${RTS_INSTALL_PREFIX_ZEROHOUR}")
        endif()
    endforeach()
else()
    message(STATUS "Zero Hour install path not found. Registry keys for 'Command and Conquer Generals Zero Hour' not found. "
                   "Zero Hour targets will be built, but not installed. "
                   "You can specify the path manually by setting RTS_INSTALL_PREFIX_ZEROHOUR.")
endif()
