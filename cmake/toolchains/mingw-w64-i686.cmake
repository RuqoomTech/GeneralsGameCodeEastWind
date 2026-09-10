# Canonical MinGW-w64 i686 toolchain for the Windows compatibility/reference build.
# Use with the mingw32-* presets. Native Windows defaults to MSYS2 MINGW32 but
# accepts -DRTS_MINGW_ROOT=...; Linux hosts use the conventional cross prefix.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(CMAKE_HOST_WIN32)
    if(DEFINED ENV{MINGW_PREFIX} AND NOT "$ENV{MINGW_PREFIX}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{MINGW_PREFIX}" _rts_mingw_default_root)
    elseif(DEFINED ENV{MSYSTEM_PREFIX} AND NOT "$ENV{MSYSTEM_PREFIX}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{MSYSTEM_PREFIX}" _rts_mingw_default_root)
    else()
        set(_rts_mingw_default_root "C:/msys64/mingw32")
    endif()
    # MSYS2 shells commonly expose /mingw32 rather than a native Windows path.
    if(_rts_mingw_default_root MATCHES "^/mingw32/?$")
        set(_rts_mingw_default_root "C:/msys64/mingw32")
    endif()

    set(RTS_MINGW_ROOT "${_rts_mingw_default_root}" CACHE PATH
        "MinGW-w64 i686 installation root (for MSYS2 this is normally C:/msys64/mingw32)")
    set(RTS_MINGW_BIN_DIR "${RTS_MINGW_ROOT}/bin" CACHE INTERNAL "Resolved MinGW-w64 binary directory")

    function(_rts_find_mingw_tool out_var unprefixed prefixed)
        find_program(_tool_path
            NAMES "${unprefixed}"
            HINTS "${RTS_MINGW_BIN_DIR}"
            NO_DEFAULT_PATH
        )
        if(NOT _tool_path)
            find_program(_tool_path NAMES "${prefixed}" REQUIRED)
        endif()
        set(${out_var} "${_tool_path}" PARENT_SCOPE)
        unset(_tool_path CACHE)
    endfunction()

    _rts_find_mingw_tool(_RTS_MINGW_CC gcc i686-w64-mingw32-gcc)
    _rts_find_mingw_tool(_RTS_MINGW_CXX g++ i686-w64-mingw32-g++)
    _rts_find_mingw_tool(_RTS_MINGW_RC windres i686-w64-mingw32-windres)
    _rts_find_mingw_tool(_RTS_MINGW_AR ar i686-w64-mingw32-ar)
    _rts_find_mingw_tool(_RTS_MINGW_RANLIB ranlib i686-w64-mingw32-ranlib)
    _rts_find_mingw_tool(_RTS_MINGW_DLLTOOL dlltool i686-w64-mingw32-dlltool)
else()
    set(RTS_MINGW_ROOT "/usr/i686-w64-mingw32" CACHE PATH "MinGW-w64 i686 target root")
    find_program(_RTS_MINGW_CC NAMES i686-w64-mingw32-gcc REQUIRED)
    find_program(_RTS_MINGW_CXX NAMES i686-w64-mingw32-g++ REQUIRED)
    find_program(_RTS_MINGW_RC NAMES i686-w64-mingw32-windres REQUIRED)
    find_program(_RTS_MINGW_AR NAMES i686-w64-mingw32-ar REQUIRED)
    find_program(_RTS_MINGW_RANLIB NAMES i686-w64-mingw32-ranlib REQUIRED)
    find_program(_RTS_MINGW_DLLTOOL NAMES i686-w64-mingw32-dlltool REQUIRED)
    get_filename_component(RTS_MINGW_BIN_DIR "${_RTS_MINGW_CC}" DIRECTORY)
    set(RTS_MINGW_BIN_DIR "${RTS_MINGW_BIN_DIR}" CACHE INTERNAL "Resolved MinGW-w64 binary directory")
endif()

# Refuse an accidentally selected UCRT64/MINGW64 compiler. This catches the
# common PowerShell/PATH mistake before CMake configures hundreds of targets.
execute_process(
    COMMAND "${_RTS_MINGW_CC}" -dumpmachine
    OUTPUT_VARIABLE _rts_mingw_machine
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT _rts_mingw_machine MATCHES "^i[3-6]86-w64-mingw32")
    message(FATAL_ERROR
        "Expected an i686 MinGW-w64 compiler, but '${_RTS_MINGW_CC} -dumpmachine' returned "
        "'${_rts_mingw_machine}'. Set RTS_MINGW_ROOT to the 32-bit MinGW root.")
endif()

set(CMAKE_C_COMPILER "${_RTS_MINGW_CC}")
set(CMAKE_CXX_COMPILER "${_RTS_MINGW_CXX}")
set(CMAKE_RC_COMPILER "${_RTS_MINGW_RC}")
set(CMAKE_AR "${_RTS_MINGW_AR}")
set(CMAKE_RANLIB "${_RTS_MINGW_RANLIB}")
set(CMAKE_DLLTOOL "${_RTS_MINGW_DLLTOOL}")

set(CMAKE_FIND_ROOT_PATH "${RTS_MINGW_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Preserve the Step 01 pre-project compatibility expectation.
set(CMAKE_SIZEOF_VOID_P 4)

# MFC-dependent tools are not part of the MinGW compatibility build.
set(RTS_BUILD_CORE_TOOLS OFF CACHE BOOL "Disable MFC-dependent core tools for MinGW" FORCE)
set(RTS_BUILD_GENERALS_TOOLS OFF CACHE BOOL "Disable MFC-dependent Generals tools for MinGW" FORCE)
set(RTS_BUILD_ZEROHOUR_TOOLS OFF CACHE BOOL "Disable MFC-dependent Zero Hour tools for MinGW" FORCE)
