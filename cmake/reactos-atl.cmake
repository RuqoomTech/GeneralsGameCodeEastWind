# ReactOS ATL headers for MinGW-w64 builds.
# This is a source-only dependency: do not configure the ReactOS build itself.

if(MINGW)
    message(STATUS "Setting up ReactOS ATL for MinGW-w64")

    FetchContent_Declare(
        reactos_atl_source
        GIT_REPOSITORY https://github.com/reactos/reactos.git
        GIT_TAG        0.4.15-release
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
        SOURCE_SUBDIR  __rts_source_only__
    )
    FetchContent_MakeAvailable(reactos_atl_source)

    if(NOT TARGET reactos_atl)
        add_library(reactos_atl INTERFACE)
    endif()

    target_include_directories(reactos_atl SYSTEM INTERFACE
        "${reactos_atl_source_SOURCE_DIR}/sdk/lib/pseh/include"
        "${reactos_atl_source_SOURCE_DIR}/sdk/lib/atl"
    )

    target_compile_definitions(reactos_atl INTERFACE
        _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
        _ATL_NO_DEBUG_CRT
        ATL_NO_ASSERT_ON_DESTROY_NONEXISTENT_WINDOW
        ATL_NO_DEFAULT_LIBS
        _USE_DUMMY_PSEH
    )

    message(STATUS "ReactOS ATL headers: ${reactos_atl_source_SOURCE_DIR}/sdk/lib/atl")
    message(STATUS "ReactOS PSEH headers: ${reactos_atl_source_SOURCE_DIR}/sdk/lib/pseh/include")
    message(STATUS "Using ReactOS PSEH dummy mode with the MinGW-w64 CRT")
else()
    add_library(reactos_atl INTERFACE)
endif()
