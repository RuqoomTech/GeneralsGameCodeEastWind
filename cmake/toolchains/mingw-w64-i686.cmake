# TheSuperHackers @build JohnsterID 05/01/2026 Add MinGW-w64 i686 cross-compilation toolchain
# MinGW-w64 32-bit (i686) Toolchain File
# Use with: cmake --preset mingw-w64-i686

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

# Resolve a real 32-bit MinGW-w64 toolchain. Native MSYS2 MINGW32 installs
# unprefixed tools in C:/msys64/mingw32/bin, while Linux cross toolchains
# conventionally expose i686-w64-mingw32-* names.
if(CMAKE_HOST_WIN32)
    set(_RTS_MINGW_HINTS "C:/msys64/mingw32/bin")
    find_program(_RTS_MINGW_CC NAMES i686-w64-mingw32-gcc gcc cc HINTS ${_RTS_MINGW_HINTS} REQUIRED)
    find_program(_RTS_MINGW_CXX NAMES i686-w64-mingw32-g++ g++ c++ HINTS ${_RTS_MINGW_HINTS} REQUIRED)
    find_program(_RTS_MINGW_RC NAMES i686-w64-mingw32-windres windres HINTS ${_RTS_MINGW_HINTS} REQUIRED)
    find_program(_RTS_MINGW_AR NAMES i686-w64-mingw32-ar ar HINTS ${_RTS_MINGW_HINTS} REQUIRED)
    find_program(_RTS_MINGW_RANLIB NAMES i686-w64-mingw32-ranlib ranlib HINTS ${_RTS_MINGW_HINTS} REQUIRED)
    find_program(_RTS_MINGW_DLLTOOL NAMES i686-w64-mingw32-dlltool dlltool HINTS ${_RTS_MINGW_HINTS} REQUIRED)
else()
    find_program(_RTS_MINGW_CC NAMES i686-w64-mingw32-gcc REQUIRED)
    find_program(_RTS_MINGW_CXX NAMES i686-w64-mingw32-g++ REQUIRED)
    find_program(_RTS_MINGW_RC NAMES i686-w64-mingw32-windres REQUIRED)
    find_program(_RTS_MINGW_AR NAMES i686-w64-mingw32-ar REQUIRED)
    find_program(_RTS_MINGW_RANLIB NAMES i686-w64-mingw32-ranlib REQUIRED)
    find_program(_RTS_MINGW_DLLTOOL NAMES i686-w64-mingw32-dlltool REQUIRED)
endif()

set(CMAKE_C_COMPILER "${_RTS_MINGW_CC}")
set(CMAKE_CXX_COMPILER "${_RTS_MINGW_CXX}")
set(CMAKE_RC_COMPILER "${_RTS_MINGW_RC}")
set(CMAKE_AR "${_RTS_MINGW_AR}")
set(CMAKE_RANLIB "${_RTS_MINGW_RANLIB}")
set(CMAKE_DLLTOOL "${_RTS_MINGW_DLLTOOL}")

# Target environment.
if(CMAKE_HOST_WIN32)
    get_filename_component(_RTS_MINGW_BIN_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
    get_filename_component(_RTS_MINGW_ROOT "${_RTS_MINGW_BIN_DIR}" DIRECTORY)
    set(CMAKE_FIND_ROOT_PATH "${_RTS_MINGW_ROOT}")
else()
    set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)
endif()

# Search programs in the host environment, target headers/libraries in MinGW.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Force 32-bit pointer size for pre-project compatibility checks.
set(CMAKE_SIZEOF_VOID_P 4)

# Disable MFC-dependent tools (not compatible with MinGW-w64).
set(RTS_BUILD_CORE_TOOLS OFF CACHE BOOL "Disable MFC-dependent core tools for MinGW" FORCE)
set(RTS_BUILD_GENERALS_TOOLS OFF CACHE BOOL "Disable MFC-dependent Generals tools for MinGW" FORCE)
set(RTS_BUILD_ZEROHOUR_TOOLS OFF CACHE BOOL "Disable MFC-dependent Zero Hour tools for MinGW" FORCE)
