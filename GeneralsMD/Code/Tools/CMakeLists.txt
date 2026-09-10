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

    # Determinism characterization intentionally links the production Zero Hour
    # GameEngine so Xfer primitive wrappers are tested without copying them.
    add_executable(z_determinismtest
        ${CMAKE_SOURCE_DIR}/Core/Tests/DeterminismPrimitivesTest.cpp
    )
    target_compile_definitions(z_determinismtest PRIVATE
        RTS_ENGINE_DETERMINISM_TEST=1
    )
    target_link_libraries(z_determinismtest PRIVATE
        z_gameengine
        zi_always
    )
endif()
