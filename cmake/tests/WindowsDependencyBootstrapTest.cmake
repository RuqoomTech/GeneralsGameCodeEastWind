if(NOT DEFINED RTS_SOURCE_DIR)
    message(FATAL_ERROR "RTS_SOURCE_DIR is required")
endif()

set(_script "${RTS_SOURCE_DIR}/scripts/setup-windows-dev.ps1")
if(NOT EXISTS "${_script}")
    message(FATAL_ERROR "Step 04C Windows dependency bootstrap is missing: ${_script}")
endif()

file(READ "${_script}" _contents)

foreach(_required
    "mingw-w64-x86_64-toolchain"
    "mingw-w64-x86_64-tools"
    "mingw-w64-x86_64-cmake"
    "mingw-w64-x86_64-ninja"
    "mingw-w64-x86_64-python"
    "mingw-w64-i686-toolchain"
    "IncludeLegacyX86"
    "mingw64-tests"
    "z_determinismcheck"
)
    string(FIND "${_contents}" "${_required}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "Dependency bootstrap is missing required token: ${_required}")
    endif()
endforeach()

# x64 must remain the default path. The i686 install may only happen behind the
# explicit compatibility-oracle switch.
string(FIND "${_contents}" "if ($IncludeLegacyX86)" _legacy_guard)
string(FIND "${_contents}" "pacman -S --needed --noconfirm mingw-w64-i686-toolchain" _legacy_install)
if(_legacy_guard EQUAL -1 OR _legacy_install EQUAL -1 OR _legacy_guard GREATER _legacy_install)
    message(FATAL_ERROR "The i686 dependency install is not guarded by IncludeLegacyX86")
endif()

message(STATUS "Step 04C Windows dependency bootstrap policy passed")
