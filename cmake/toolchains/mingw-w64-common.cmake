# Shared MinGW-w64 command-line toolchain discovery for modernization lanes.
# Wrapper toolchains set RTS_MINGW_ARCH before including this file.

if(NOT DEFINED RTS_MINGW_ARCH)
    message(FATAL_ERROR "RTS_MINGW_ARCH must be set before including mingw-w64-common.cmake")
endif()

if(RTS_MINGW_ARCH STREQUAL "i686")
    set(_rts_mingw_triplet "i686-w64-mingw32")
    set(_rts_mingw_msys_dir "mingw32")
    set(_rts_mingw_processor "i686")
    set(_rts_mingw_pointer_size 4)
elseif(RTS_MINGW_ARCH STREQUAL "x86_64")
    set(_rts_mingw_triplet "x86_64-w64-mingw32")
    set(_rts_mingw_msys_dir "mingw64")
    set(_rts_mingw_processor "x86_64")
    set(_rts_mingw_pointer_size 8)
else()
    message(FATAL_ERROR "Unsupported RTS_MINGW_ARCH='${RTS_MINGW_ARCH}'")
endif()

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR "${_rts_mingw_processor}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(CMAKE_HOST_WIN32)
    set(_rts_mingw_default_root "C:/msys64/${_rts_mingw_msys_dir}")
    foreach(_rts_prefix_env MINGW_PREFIX MSYSTEM_PREFIX)
        if(DEFINED ENV{${_rts_prefix_env}} AND NOT "$ENV{${_rts_prefix_env}}" STREQUAL "")
            file(TO_CMAKE_PATH "$ENV{${_rts_prefix_env}}" _rts_candidate_root)
            if(_rts_candidate_root STREQUAL "/${_rts_mingw_msys_dir}")
                # Native Windows CMake does not reliably resolve MSYS virtual roots.
                # Preserve the Step 02 canonical MSYS2 installation mapping instead
                # of requiring PATH or generator overrides.
                set(_rts_candidate_root "C:/msys64/${_rts_mingw_msys_dir}")
            endif()
            if(_rts_candidate_root MATCHES "[/\\]${_rts_mingw_msys_dir}/?$")
                set(_rts_mingw_default_root "${_rts_candidate_root}")
                break()
            endif()
        endif()
    endforeach()

    set(RTS_MINGW_ROOT "${_rts_mingw_default_root}" CACHE PATH
        "MinGW-w64 ${RTS_MINGW_ARCH} installation root (MSYS2: C:/msys64/${_rts_mingw_msys_dir})")
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

    _rts_find_mingw_tool(_RTS_MINGW_CC gcc "${_rts_mingw_triplet}-gcc")
    _rts_find_mingw_tool(_RTS_MINGW_CXX g++ "${_rts_mingw_triplet}-g++")
    _rts_find_mingw_tool(_RTS_MINGW_RC windres "${_rts_mingw_triplet}-windres")
    _rts_find_mingw_tool(_RTS_MINGW_AR ar "${_rts_mingw_triplet}-ar")
    _rts_find_mingw_tool(_RTS_MINGW_RANLIB ranlib "${_rts_mingw_triplet}-ranlib")
    _rts_find_mingw_tool(_RTS_MINGW_DLLTOOL dlltool "${_rts_mingw_triplet}-dlltool")
else()
    set(RTS_MINGW_ROOT "/usr/${_rts_mingw_triplet}" CACHE PATH "MinGW-w64 ${RTS_MINGW_ARCH} target root")
    find_program(_RTS_MINGW_CC NAMES "${_rts_mingw_triplet}-gcc" REQUIRED)
    find_program(_RTS_MINGW_CXX NAMES "${_rts_mingw_triplet}-g++" REQUIRED)
    find_program(_RTS_MINGW_RC NAMES "${_rts_mingw_triplet}-windres" REQUIRED)
    find_program(_RTS_MINGW_AR NAMES "${_rts_mingw_triplet}-ar" REQUIRED)
    find_program(_RTS_MINGW_RANLIB NAMES "${_rts_mingw_triplet}-ranlib" REQUIRED)
    find_program(_RTS_MINGW_DLLTOOL NAMES "${_rts_mingw_triplet}-dlltool" REQUIRED)
    get_filename_component(RTS_MINGW_BIN_DIR "${_RTS_MINGW_CC}" DIRECTORY)
    set(RTS_MINGW_BIN_DIR "${RTS_MINGW_BIN_DIR}" CACHE INTERNAL "Resolved MinGW-w64 binary directory")
endif()

execute_process(
    COMMAND "${_RTS_MINGW_CC}" -dumpmachine
    OUTPUT_VARIABLE _rts_mingw_machine
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT _rts_mingw_machine MATCHES "^${_rts_mingw_triplet}")
    message(FATAL_ERROR
        "Expected a ${_rts_mingw_triplet} compiler, but '${_RTS_MINGW_CC} -dumpmachine' returned "
        "'${_rts_mingw_machine}'. Set RTS_MINGW_ROOT to the matching MSYS2 MinGW root.")
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

# Available before project() so the focused build graph can make architecture
# decisions consistently with the selected wrapper toolchain.
set(CMAKE_SIZEOF_VOID_P ${_rts_mingw_pointer_size})

# MFC-dependent tools are outside both MinGW modernization lanes.
set(RTS_BUILD_CORE_TOOLS OFF CACHE BOOL "Disable MFC-dependent core tools for MinGW" FORCE)
set(RTS_BUILD_GENERALS_TOOLS OFF CACHE BOOL "Disable MFC-dependent Generals tools for MinGW" FORCE)
set(RTS_BUILD_ZEROHOUR_TOOLS OFF CACHE BOOL "Disable MFC-dependent Zero Hour tools for MinGW" FORCE)
