# WIDL integration for MinGW-w64 Windows builds.
# Native MSYS2 MINGW32 provides widl.exe and the Windows IDL headers directly
# under the selected MinGW root. Linux cross-build hosts may instead use Wine's
# WIDL/include layout. Do not mutate PATH to make either layout work.

if(NOT MINGW)
    return()
endif()

set(RTS_WIDL_ROOT "" CACHE PATH "Optional root containing bin/widl and/or Windows IDL headers")
set(RTS_WIDL_INCLUDE_DIR "" CACHE PATH "Optional directory containing Windows IDL files such as oaidl.idl")

set(_rts_widl_hints)
if(RTS_WIDL_ROOT)
    list(APPEND _rts_widl_hints "${RTS_WIDL_ROOT}/bin")
endif()
if(DEFINED ENV{WIDL_ROOT} AND NOT "$ENV{WIDL_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{WIDL_ROOT}" _rts_widl_env_root)
    list(APPEND _rts_widl_hints "${_rts_widl_env_root}/bin")
endif()
if(RTS_MINGW_BIN_DIR)
    list(APPEND _rts_widl_hints "${RTS_MINGW_BIN_DIR}")
endif()

find_program(WIDL_EXECUTABLE
    NAMES widl.exe widl widl-stable
    HINTS ${_rts_widl_hints}
    NO_CMAKE_FIND_ROOT_PATH
    DOC "WIDL compiler for MinGW-w64"
)

if(WIDL_EXECUTABLE)
    execute_process(
        COMMAND "${WIDL_EXECUTABLE}" -V
        OUTPUT_VARIABLE _rts_widl_stdout
        ERROR_VARIABLE _rts_widl_stderr
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )
    set(_rts_widl_version_output "${_rts_widl_stdout} ${_rts_widl_stderr}")
    if(_rts_widl_version_output MATCHES "version ([0-9.]+)")
        message(STATUS "Found widl: ${WIDL_EXECUTABLE} (version ${CMAKE_MATCH_1})")
    else()
        message(STATUS "Found widl: ${WIDL_EXECUTABLE}")
    endif()
    set(IDL_COMPILER "${WIDL_EXECUTABLE}")
    set(IDL_COMPILER_FOUND TRUE)
else()
    set(IDL_COMPILER_FOUND FALSE)
    message(WARNING
        "widl was not found. The full MinGW runtime needs it for EABrowser IDL generation. "
        "On MSYS2 MINGW32 install mingw-w64-i686-tools (also part of the i686 toolchain group), "
        "or set RTS_WIDL_ROOT/WIDL_ROOT. The mingw32-tests preset does not require WIDL.")
endif()

if(RTS_WIDL_INCLUDE_DIR)
    set(_rts_widl_system_include_dir "${RTS_WIDL_INCLUDE_DIR}")
else()
    set(_rts_widl_include_hints)
    if(RTS_WIDL_ROOT)
        list(APPEND _rts_widl_include_hints
            "${RTS_WIDL_ROOT}/include"
            "${RTS_WIDL_ROOT}/include/wine/windows"
            "${RTS_WIDL_ROOT}/include/wine/wine/windows")
    endif()
    if(DEFINED _rts_widl_env_root)
        list(APPEND _rts_widl_include_hints
            "${_rts_widl_env_root}/include"
            "${_rts_widl_env_root}/include/wine/windows"
            "${_rts_widl_env_root}/include/wine/wine/windows")
    endif()
    if(RTS_MINGW_ROOT)
        # Canonical MSYS2 MINGW32 layout: /mingw32/include/oaidl.idl.
        list(APPEND _rts_widl_include_hints "${RTS_MINGW_ROOT}/include")
    endif()
    if(NOT CMAKE_HOST_WIN32)
        list(APPEND _rts_widl_include_hints
            /usr/include/wine/wine/windows
            /usr/include/wine/windows
            /usr/include/wine-development/windows
            /opt/wine-stable/include/wine/windows
            /usr/local/include/wine/windows)
    endif()

    find_path(_rts_widl_system_include_dir
        NAMES oaidl.idl
        HINTS ${_rts_widl_include_hints}
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
        DOC "Windows IDL include directory used by WIDL"
    )
endif()

set(WIDL_INCLUDE_PATHS)
if(_rts_widl_system_include_dir)
    list(APPEND WIDL_INCLUDE_PATHS "-I${_rts_widl_system_include_dir}")
    message(STATUS "WIDL system IDL include directory: ${_rts_widl_system_include_dir}")
elseif(IDL_COMPILER_FOUND)
    message(WARNING
        "widl was found but the Windows IDL headers were not located. Set RTS_WIDL_INCLUDE_DIR "
        "to the directory containing oaidl.idl if IDL generation fails.")
endif()

function(add_idl_file target_name idl_file)
    if(NOT IDL_COMPILER_FOUND)
        message(FATAL_ERROR "add_idl_file(${target_name}) requires WIDL, but no WIDL compiler was discovered")
    endif()

    get_filename_component(idl_basename "${idl_file}" NAME_WE)
    get_filename_component(idl_dir "${idl_file}" DIRECTORY)
    set(header_file "${CMAKE_CURRENT_BINARY_DIR}/${idl_basename}.h")
    set(iid_file "${CMAKE_CURRENT_BINARY_DIR}/${idl_basename}_i.c")

    set(_widl_flags
        --win32
        "-I${idl_dir}"
        ${WIDL_INCLUDE_PATHS}
        -D__WIDL__
        -DDECLSPEC_ALIGN\(x\)=
    )

    add_custom_command(
        OUTPUT "${header_file}"
        COMMAND "${IDL_COMPILER}" ${_widl_flags} -h -o "${header_file}" "${idl_file}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        DEPENDS "${idl_file}"
        COMMENT "Compiling IDL to header with widl: ${idl_file}"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${iid_file}"
        COMMAND "${IDL_COMPILER}" ${_widl_flags} -u -o "${iid_file}" "${idl_file}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        DEPENDS "${idl_file}"
        COMMENT "Compiling IDL to IID with widl: ${idl_file}"
        VERBATIM
    )

    set(${target_name}_HEADER "${header_file}" PARENT_SCOPE)
    set(${target_name}_IID "${iid_file}" PARENT_SCOPE)
endfunction()
