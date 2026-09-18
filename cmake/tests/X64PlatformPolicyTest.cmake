include("${CMAKE_CURRENT_LIST_DIR}/PolicyTestHelpers.cmake")

# The supported MinGW modernization surface is x64-only.
rts_policy_read("CMakePresets.json" _presets)
rts_policy_require_text("${_presets}" "\"name\": \"mingw64-tests\"" "the canonical x64 test preset disappeared")
rts_policy_require_text("${_presets}" "mingw-w64-x86_64.cmake" "the x64 preset must use the canonical x86_64 toolchain")
foreach(_retired IN ITEMS "mingw32-" "mingw-w64-i686" "mingw32-tests")
    rts_policy_forbid_text("${_presets}" "${_retired}" "retired i686 preset token '${_retired}' is still exposed")
endforeach()

if(EXISTS "${RTS_SOURCE_DIR}/cmake/toolchains/mingw-w64-i686.cmake")
    message(FATAL_ERROR "Retired i686 toolchain wrapper was restored")
endif()
if(EXISTS "${RTS_SOURCE_DIR}/scripts/compare-determinism-timelines.py")
    message(FATAL_ERROR "Retired cross-architecture oracle comparator was restored")
endif()

rts_policy_read("cmake/toolchains/mingw-w64-common.cmake" _toolchain)
rts_policy_require_text("${_toolchain}" "x86_64-w64-mingw32" "canonical MinGW triplet must remain x86_64")
rts_policy_require_text("${_toolchain}" "set(_rts_mingw_pointer_size 8)" "canonical MinGW pointer size must remain 64-bit")
rts_policy_forbid_text("${_toolchain}" "i686-w64-mingw32" "common toolchain still contains the retired i686 branch")
rts_policy_forbid_text("${_toolchain}" "_rts_mingw_pointer_size 4" "common toolchain still models 32-bit pointers")

rts_policy_read("cmake/mingw.cmake" _mingw)
rts_policy_require_text("${_mingw}" "if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)" "MinGW configuration must reject non-x64 native pointers")
rts_policy_require_text("${_mingw}" "The 32-bit MinGW modernization lane is retired" "32-bit MinGW rejection must be diagnostic")
rts_policy_forbid_text("${_mingw}" "IS_MINGW32" "legacy MinGW32 feature switch was restored")

rts_policy_read("cmake/widl.cmake" _widl)
rts_policy_require_text("${_widl}" "--win64" "WIDL generation must target Win64")
rts_policy_require_text("${_widl}" "mingw-w64-x86_64-tools" "WIDL diagnostics must point to the x64 MSYS2 package")
rts_policy_forbid_text("${_widl}" "--win32" "WIDL generation still targets Win32")
rts_policy_forbid_text("${_widl}" "mingw-w64-i686" "WIDL diagnostics still advertise i686 packages")

rts_policy_read("scripts/setup-windows-dev.ps1" _bootstrap)
rts_policy_require_text("${_bootstrap}" "mingw-w64-x86_64-toolchain" "Windows bootstrap must install the x64 toolchain")
foreach(_retired IN ITEMS "IncludeLegacyX86" "mingw-w64-i686" "mingw32\\bin" "compare-determinism-timelines.py")
    rts_policy_forbid_text("${_bootstrap}" "${_retired}" "Windows bootstrap still exposes retired x86 token '${_retired}'")
endforeach()

rts_policy_read("Core/Tests/CMakeLists.txt" _tests)
rts_policy_require_text("${_tests}" "x64_platform_policy" "the x64 platform CTest policy gate is not registered")
rts_policy_require_text("${_tests}" "check_x64_platform" "the explicit x64 platform validation target is missing")
rts_policy_forbid_text("${_tests}" "CMAKE_SIZEOF_VOID_P EQUAL 4" "focused modernization tests still contain an active 32-bit oracle branch")
rts_policy_forbid_text("${_tests}" "reactos_atl" "focused modernization tests still link the retired i686 ATL seam")
rts_policy_forbid_text("${_tests}" "compare-determinism-timelines.py" "retired i686/x64 comparator test is still active")

# Retirement must not widen deterministic/wire ABI contracts.
rts_policy_read("Core/Tests/ArchitectureAbiTest.cpp" _arch)
foreach(_fixed IN ITEMS
    "Expect_Size(\"Evolution native pointer width\", 8U, sizeof(void *))"
    "Expect_Size(\"Int wire width\", 4U, sizeof(Int))"
    "Expect_Size(\"UnsignedInt wire width\", 4U, sizeof(UnsignedInt))"
    "Expect_Size(\"Short wire width\", 2U, sizeof(Short))"
    "Expect_Size(\"Real wire width\", 4U, sizeof(Real))"
    "Expect_Size(\"ObjectID wire width\", 4U, sizeof(ObjectID))"
    "Expect_Size(\"DrawableID wire width\", 4U, sizeof(DrawableID))")
    rts_policy_require_text("${_arch}" "${_fixed}" "architecture guard lost fixed/native width assertion '${_fixed}'")
endforeach()

rts_policy_read("Core/Tests/DeterminismPrimitivesTest.cpp" _determinism)
rts_policy_require_text("${_determinism}" "Expect_Int(\"logic CRC message enum\", 1095" "MSG_LOGIC_CRC must remain fixed at 1095")

# Crash diagnostics are native-only state. The Evolution x64 path must never
# truncate RIP/RSP/stack addresses into Win32 unsigned-long values.
rts_policy_read("Core/Libraries/Source/WWVegas/WWLib/Except.h" _except_header)
rts_policy_require_text("${_except_header}" "int Stack_Walk(std::uintptr_t *return_addresses" "exception stack addresses must use native uintptr_t")
rts_policy_require_text("${_except_header}" "extern std::uintptr_t ExceptionReturnAddress" "exception return addresses must remain native-width")

rts_policy_read("Core/Libraries/Source/WWVegas/WWLib/Except.cpp" _except_source)
rts_policy_require_text("${_except_source}" "#if defined(_WIN64)" "exception diagnostics must retain an explicit Win64 path")
rts_policy_require_text("${_except_source}" "context->Rip" "Win64 exception diagnostics must use RIP rather than EIP")
rts_policy_require_text("${_except_source}" "IMAGE_FILE_MACHINE_AMD64" "Win64 stack walking must select the AMD64 machine type")
rts_policy_require_text("${_except_source}" "STACKFRAME64 stack_frame" "Win64 stack walking must use STACKFRAME64")
rts_policy_require_text("${_except_source}" "DbgHelpLoader::stackWalk64" "Win64 stack walking must use the consolidated DbgHelpLoader")
rts_policy_require_text("${_except_source}" "DbgHelpLoader::symFromAddr" "Win64 symbol lookup must use a DWORD64-capable DbgHelp API")

rts_policy_read("Core/Libraries/Source/WWVegas/WWLib/DbgHelpLoader.cpp" _dbghelp_loader)
foreach(_entry IN ITEMS "SymFromAddr" "SymFunctionTableAccess64" "SymGetModuleBase64" "StackWalk64")
    rts_policy_require_text("${_dbghelp_loader}" "GetProcAddress(Inst->m_dllModule, \"${_entry}\")" "DbgHelpLoader must resolve native x64 entry point ${_entry}")
endforeach()

message(STATUS "x64 platform policy passed: retired i686 modernization surfaces remain absent, fixed-width ABI guards remain intact, and crash diagnostics stay native-width")
