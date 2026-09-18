# MinGW-w64 specific compiler and linker configuration.
# Keep compatibility behavior attached to project targets through core_config;
# do not leak legacy flags/libraries into fetched third-party projects.

if(MINGW)
    message(STATUS "Configuring MinGW-w64 build settings")

    if(NOT TARGET core_config)
        message(FATAL_ERROR "cmake/mingw.cmake must be included after core_config is created")
    endif()

    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR
            "The 32-bit MinGW modernization lane is retired. "
            "Generals Evolution requires an x86_64 MinGW compiler and 64-bit native pointers.")
    endif()

    set(IS_MINGW64 TRUE)
    message(STATUS "MinGW-w64 64-bit (x86_64) Evolution lane detected")
    if(NOT RTS_BUILD_EVOLUTION_X64)
        message(FATAL_ERROR
            "An x64 MinGW compiler was selected without the staged x64 readiness option. "
            "Use preset 'mingw64-tests' while the full runtime is enabled subsystem-by-subsystem.")
    endif()
    if(RTS_BUILD_TESTS_ONLY AND NOT RTS_BUILD_HEADLESS_CORE)
        message(FATAL_ERROR
            "The focused x64 test graph requires RTS_BUILD_HEADLESS_CORE. "
            "Use preset 'mingw64-tests'.")
    endif()
    if(NOT RTS_BUILD_TESTS_ONLY)
        message(FATAL_ERROR
            "The x64 full-game runtime remains gated while direct DX8Wrapper call sites are migrated "
            "onto the in-place Direct3D 12 backend. Use preset 'mingw64-tests' during this renderer migration.")
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

    # Keep GCC runtime deployment self-contained for command-line builds.
    target_link_options(core_config INTERFACE -static-libgcc -static-libstdc++)

    message(STATUS "MinGW-w64 configuration complete")
endif()
