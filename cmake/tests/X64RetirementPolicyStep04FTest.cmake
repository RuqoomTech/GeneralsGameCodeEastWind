if(NOT DEFINED RTS_SOURCE_DIR)
    message(FATAL_ERROR "RTS_SOURCE_DIR is required")
endif()

function(_read relative_path out_var)
    set(_path "${RTS_SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${_path}")
        message(FATAL_ERROR "Step 04F required file is missing: ${relative_path}")
    endif()
    file(READ "${_path}" _contents)
    set(${out_var} "${_contents}" PARENT_SCOPE)
endfunction()

function(_require_contains contents token description)
    string(FIND "${contents}" "${token}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "Step 04F regression: ${description}")
    endif()
endfunction()

function(_require_absent contents token description)
    string(FIND "${contents}" "${token}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR "Step 04F regression: ${description}")
    endif()
endfunction()

# The supported MinGW modernization surface is x64-only.
_read("CMakePresets.json" _presets)
_require_contains("${_presets}" "\"name\": \"mingw64-tests\"" "the canonical x64 test preset disappeared")
_require_contains("${_presets}" "mingw-w64-x86_64.cmake" "the x64 preset must use the canonical x86_64 toolchain")
foreach(_retired IN ITEMS "mingw32-" "mingw-w64-i686" "mingw32-tests")
    _require_absent("${_presets}" "${_retired}" "retired i686 preset token '${_retired}' is still exposed")
endforeach()

if(EXISTS "${RTS_SOURCE_DIR}/cmake/toolchains/mingw-w64-i686.cmake")
    message(FATAL_ERROR "Step 04F regression: retired i686 toolchain wrapper was restored")
endif()
if(EXISTS "${RTS_SOURCE_DIR}/scripts/compare-determinism-timelines.py")
    message(FATAL_ERROR "Step 04F regression: retired cross-architecture oracle comparator was restored")
endif()

_read("cmake/toolchains/mingw-w64-common.cmake" _toolchain)
_require_contains("${_toolchain}" "x86_64-w64-mingw32" "canonical MinGW triplet must remain x86_64")
_require_contains("${_toolchain}" "set(_rts_mingw_pointer_size 8)" "canonical MinGW pointer size must remain 64-bit")
_require_absent("${_toolchain}" "i686-w64-mingw32" "common toolchain still contains the retired i686 branch")
_require_absent("${_toolchain}" "_rts_mingw_pointer_size 4" "common toolchain still models 32-bit pointers")

_read("cmake/mingw.cmake" _mingw)
_require_contains("${_mingw}" "if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)" "MinGW configuration must reject non-x64 native pointers")
_require_contains("${_mingw}" "Step 04F retired the MinGW-w64 i686 modernization/oracle lane" "32-bit MinGW rejection must be diagnostic")
_require_absent("${_mingw}" "IS_MINGW32" "legacy MinGW32 feature switch was restored")

_read("cmake/widl.cmake" _widl)
_require_contains("${_widl}" "--win64" "WIDL generation must target Win64")
_require_contains("${_widl}" "mingw-w64-x86_64-tools" "WIDL diagnostics must point to the x64 MSYS2 package")
_require_absent("${_widl}" "--win32" "WIDL generation still targets Win32")
_require_absent("${_widl}" "mingw-w64-i686" "WIDL diagnostics still advertise i686 packages")

_read("scripts/setup-windows-dev.ps1" _bootstrap)
_require_contains("${_bootstrap}" "mingw-w64-x86_64-toolchain" "Windows bootstrap must install the x64 toolchain")
foreach(_retired IN ITEMS "IncludeLegacyX86" "mingw-w64-i686" "mingw32\\bin" "compare-determinism-timelines.py")
    _require_absent("${_bootstrap}" "${_retired}" "Windows bootstrap still exposes retired x86 token '${_retired}'")
endforeach()

_read("Core/Tests/CMakeLists.txt" _tests)
_require_contains("${_tests}" "x64_retirement_policy_step04f" "the Step 04F CTest policy gate is not registered")
_require_contains("${_tests}" "z_step04fcheck" "the explicit Step 04F validation target is missing")
_require_absent("${_tests}" "CMAKE_SIZEOF_VOID_P EQUAL 4" "focused modernization tests still contain an active 32-bit oracle branch")
_require_absent("${_tests}" "reactos_atl" "focused modernization tests still link the retired i686 ATL seam")
_require_absent("${_tests}" "determinism_timeline_compare_tool_step04d" "retired i686/x64 comparator test is still active")

# Retirement must not widen deterministic/wire ABI contracts.
_read("Core/Tests/ArchitectureMigrationTest.cpp" _arch)
foreach(_fixed IN ITEMS
    "Expect_Size(\"Evolution native pointer width\", 8U, sizeof(void *))"
    "Expect_Size(\"Int wire width\", 4U, sizeof(Int))"
    "Expect_Size(\"UnsignedInt wire width\", 4U, sizeof(UnsignedInt))"
    "Expect_Size(\"Short wire width\", 2U, sizeof(Short))"
    "Expect_Size(\"Real wire width\", 4U, sizeof(Real))"
    "Expect_Size(\"ObjectID wire width\", 4U, sizeof(ObjectID))"
    "Expect_Size(\"DrawableID wire width\", 4U, sizeof(DrawableID))")
    _require_contains("${_arch}" "${_fixed}" "architecture guard lost fixed/native width assertion '${_fixed}'")
endforeach()

_read("Core/Tests/DeterminismPrimitivesTest.cpp" _determinism)
_require_contains("${_determinism}" "Expect_Int(\"logic CRC message enum\", 1095" "MSG_LOGIC_CRC must remain fixed at 1095")

message(STATUS "Step 04F x64 retirement policy passed: i686 modernization/oracle surfaces are gone and fixed-width ABI guards remain intact")
