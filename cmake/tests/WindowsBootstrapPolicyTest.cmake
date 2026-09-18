if(NOT DEFINED RTS_SOURCE_DIR)
    message(FATAL_ERROR "RTS_SOURCE_DIR is required")
endif()

set(_script "${RTS_SOURCE_DIR}/scripts/setup-windows-dev.ps1")
if(NOT EXISTS "${_script}")
    message(FATAL_ERROR "Windows dependency bootstrap is missing: ${_script}")
endif()

file(READ "${_script}" _contents)

foreach(_required
    "mingw-w64-x86_64-toolchain"
    "mingw-w64-x86_64-tools"
    "mingw-w64-x86_64-cmake"
    "mingw-w64-x86_64-ninja"
    "mingw-w64-x86_64-python"
    "mingw64-tests"
    "check_headless_determinism"
    "check_evolution_full_session"
    "check_x64_platform"
)
    string(FIND "${_contents}" "${_required}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "Dependency bootstrap is missing required x64 token: ${_required}")
    endif()
endforeach()

foreach(_retired
    "IncludeLegacyX86"
    "mingw-w64-i686"
    "mingw32"
    "compare-determinism-timelines.py"
)
    string(FIND "${_contents}" "${_retired}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR "Windows bootstrap still exposes retired x86 token '${_retired}'")
    endif()
endforeach()

# MSYS2/Wine widl uses -V for its version query; --version prints usage and
# exits non-zero. Keep the bootstrap aligned with cmake/widl.cmake.
string(FIND "${_contents}" "Assert-Tool (Join-Path $mingw64Bin 'widl.exe') 'WIDL' @('-V')" _widl_pos)
if(_widl_pos EQUAL -1)
    message(FATAL_ERROR "Dependency bootstrap must verify x64 WIDL with -V")
endif()

string(FIND "${_contents}" "widl.exe') 'WIDL' @('--version')" _bad_widl_long_version)
if(NOT _bad_widl_long_version EQUAL -1)
    message(FATAL_ERROR "Dependency bootstrap must not query WIDL with unsupported --version")
endif()

# Native tool verification must capture the full process output and snapshot
# LASTEXITCODE before piping the text through Select-Object.
string(FIND "${_contents}" "$output = @(& $Path @Arguments 2>&1)" _capture_output)
string(FIND "${_contents}" "$exitCode = $LASTEXITCODE" _capture_exit)
string(FIND "${_contents}" "$output | Select-Object -First 2" _display_output)
if(_capture_output EQUAL -1 OR _capture_exit EQUAL -1 OR _display_output EQUAL -1)
    message(FATAL_ERROR "Dependency bootstrap must capture native probe output and exit code before displaying it")
endif()
if(NOT _capture_output LESS _capture_exit OR NOT _capture_exit LESS _display_output)
    message(FATAL_ERROR "Dependency bootstrap must snapshot LASTEXITCODE before Select-Object processes native output")
endif()

string(FIND "${_contents}" "& $Path @Arguments | Select-Object -First 2" _bad_direct_probe_pipe)
if(NOT _bad_direct_probe_pipe EQUAL -1)
    message(FATAL_ERROR "Dependency bootstrap must not pipe native version probes directly into Select-Object -First")
endif()

message(STATUS "Windows bootstrap policy passed")
