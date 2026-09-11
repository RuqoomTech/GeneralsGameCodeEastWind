# MinGW-w64 specific compiler and linker configuration.
# Keep compatibility behavior attached to project targets through core_config;
# do not leak legacy flags/libraries into fetched third-party projects.

if(MINGW)
    message(STATUS "Configuring MinGW-w64 build settings")

    if(NOT TARGET core_config)
        message(FATAL_ERROR "cmake/mingw.cmake must be included after core_config is created")
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(IS_MINGW32 TRUE)
        message(STATUS "MinGW-w64 32-bit (i686) compatibility/reference build detected")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(IS_MINGW64 TRUE)
        message(STATUS "MinGW-w64 64-bit (x86_64) modernization lane detected")
        if(NOT RTS_BUILD_X64_READINESS)
            message(FATAL_ERROR
                "A 64-bit MinGW compiler was selected without the staged x64 readiness option. "
                "Use preset 'mingw64-tests' while the deterministic/headless runtime is being ported deliberately.")
        endif()
        if(NOT RTS_BUILD_X64_HEADLESS_CORE)
            message(FATAL_ERROR
                "A 64-bit MinGW compiler was selected without the Step 04D headless-core option. "
                "Use preset 'mingw64-tests'.")
        endif()
        if(NOT RTS_BUILD_TESTS_ONLY)
            message(FATAL_ERROR
                "The Step 04 x64 lane currently supports the focused deterministic/headless test graph only. "
                "Use preset 'mingw64-tests'. The full x64 runtime will be enabled subsystem-by-subsystem "
                "without opening the legacy D3D8 platform boundary.")
        endif()
    else()
        message(FATAL_ERROR "Unsupported MinGW pointer width: ${CMAKE_SIZEOF_VOID_P}")
    endif()

    # Preserve the legacy code assumptions without applying them to downloaded
    # dependencies. Strict aliasing remains disabled for the compatibility tree.
    target_compile_options(core_config INTERFACE -fno-strict-aliasing)

    target_compile_definitions(core_config INTERFACE
        __forceinline=inline\ __attribute__\(\(always_inline\)\)
        __int64=long\ long
        _int64=long\ long
        _USE_MATH_DEFINES
    )

    include(CheckCXXSymbolExists)
    check_cxx_symbol_exists(STDMETHODCALLTYPE "windows.h" HAVE_STDMETHODCALLTYPE)
    if(NOT HAVE_STDMETHODCALLTYPE)
        target_compile_definitions(core_config INTERFACE
            STDMETHODCALLTYPE=__stdcall
            STDMETHODIMP=HRESULT\ __stdcall
        )
    endif()

    # These are platform libraries used throughout the legacy runtime. Attach
    # them to the project configuration interface instead of globally injecting
    # them into every target (including FetchContent dependencies).
    target_link_libraries(core_config INTERFACE
        uuid
        ole32
        oleaut32
        gdi32
        user32
        comctl32
        winmm
        vfw32
        imm32
    )

    # Renderer/input/audio compatibility libraries are not needed by the focused
    # modernization graph. Keeping them out is what allows the first x64 lane to
    # compile architecture/serialization guards before the D3D8 runtime is ported.
    if(NOT RTS_BUILD_TESTS_ONLY)
        target_link_libraries(core_config INTERFACE d3d8 dinput8 dsound)
    endif()

    # Keep GCC runtime deployment self-contained for command-line builds.
    target_link_options(core_config INTERFACE -static-libgcc -static-libstdc++)

    # min-dx8-sdk exposes d3dx8d for MinGW. Retain the historical d3dx8 name
    # used by the game executable without creating a global linker rewrite.
    if(NOT RTS_BUILD_TESTS_ONLY AND NOT TARGET d3dx8)
        add_library(d3dx8 INTERFACE IMPORTED GLOBAL)
        set_target_properties(d3dx8 PROPERTIES
            INTERFACE_LINK_LIBRARIES "d3dx8d"
        )
        message(STATUS "Created d3dx8 -> d3dx8d alias for MinGW-w64")
    endif()

    message(STATUS "MinGW-w64 configuration complete")
endif()
