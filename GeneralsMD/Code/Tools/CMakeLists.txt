# Build tools

# Build useful tool binaries.
if(RTS_BUILD_ZEROHOUR_TOOLS)
    add_subdirectory(GUIEdit)
    add_subdirectory(ImagePacker)
    add_subdirectory(MapCacheBuilder)
    add_subdirectory(W3DView)
    add_subdirectory(wdump)
    add_subdirectory(WorldBuilder)
endif()

# Build less useful tool/test binaries.
if(RTS_BUILD_ZEROHOUR_EXTRAS)
    add_subdirectory(Autorun)
    add_subdirectory(Launcher)
    add_subdirectory(PATCHGET)

    # Focused Step 01 compatibility gate. Compile the production determinism/Xfer
    # implementation units directly instead of linking the monolithic z_gameengine
    # archive, which would pull renderer/UI/network executable globals into this test.
    add_executable(z_determinismtest
        ${CMAKE_SOURCE_DIR}/Core/Tests/DeterminismPrimitivesTest.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/RandomValue.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/System/Snapshot.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/System/Xfer.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/System/XferCRC.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/GameLogic/System/Damage.cpp
    )
    target_compile_definitions(z_determinismtest PRIVATE
        RTS_ZEROHOUR=1
        RTS_ENGINE_DETERMINISM_TEST=1
        RTS_STANDALONE_DETERMINISM_TEST=1
    )
    target_include_directories(z_determinismtest PRIVATE
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include/Precompiled
    )
    # Legacy GameEngine headers depend on the C++ compatibility macros being
    # visible before PreRTS.h and its transitive includes. The full engine
    # target carries the same precompiled-header contract.
    target_precompile_headers(z_determinismtest PRIVATE
        [["Utility/CppMacros.h"]]
    )
    target_link_libraries(z_determinismtest PRIVATE
        core_config
        core_utility_no_pch
        zi_gameengine_include
        corei_libraries_include
        corei_libraries_source_wwvegas
    )
    if(MINGW)
        target_link_libraries(z_determinismtest PRIVATE reactos_atl)
        # Remove unused legacy inline methods/vtables before the MinGW linker
        # sees their references to full-engine implementations and globals.
        include(CheckIPOSupported)
        check_ipo_supported(LANGUAGES CXX)
        set_property(TARGET z_determinismtest PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        target_link_options(z_determinismtest PRIVATE -mconsole)
    endif()

    add_custom_target(z_determinismcheck
        COMMAND $<TARGET_FILE:z_determinismtest>
        DEPENDS z_determinismtest
        USES_TERMINAL
    )
endif()
