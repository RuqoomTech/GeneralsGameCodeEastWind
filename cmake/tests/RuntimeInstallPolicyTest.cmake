# CMake-level regression for the shared runtime install/debug-artifact policy.
# This intentionally builds a tiny nested target so unsupported generator
# expressions are caught during CMake generation, before the real game graph.

foreach(_required_var IN ITEMS RTS_SOURCE_DIR RTS_TEST_WORK_DIR RTS_TEST_GENERATOR)
    if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
        message(FATAL_ERROR "RuntimeInstallPolicyTest requires ${_required_var}")
    endif()
endforeach()

file(REMOVE_RECURSE "${RTS_TEST_WORK_DIR}")
file(MAKE_DIRECTORY "${RTS_TEST_WORK_DIR}/src")

file(WRITE "${RTS_TEST_WORK_DIR}/src/main.cpp" "int main() { return 0; }\n")

set(_probe_cmakelists [=[
cmake_minimum_required(VERSION 3.25)
project(runtime_install_policy_probe LANGUAGES CXX)
include("@RTS_SOURCE_DIR@/cmake/debug_strip.cmake")
add_executable(runtime_install_policy_probe main.cpp)
if(MINGW)
    add_debug_strip_target(runtime_install_policy_probe)
endif()
rts_install_runtime_target(runtime_install_policy_probe bin)
]=])
string(CONFIGURE "${_probe_cmakelists}" _probe_cmakelists @ONLY)
file(WRITE "${RTS_TEST_WORK_DIR}/src/CMakeLists.txt" "${_probe_cmakelists}")

set(_configure_command
    "${CMAKE_COMMAND}"
    -S "${RTS_TEST_WORK_DIR}/src"
    -B "${RTS_TEST_WORK_DIR}/build"
    -G "${RTS_TEST_GENERATOR}"
    -DCMAKE_BUILD_TYPE=Release
)

if(DEFINED RTS_TEST_TOOLCHAIN_FILE AND NOT "${RTS_TEST_TOOLCHAIN_FILE}" STREQUAL "")
    list(APPEND _configure_command "-DCMAKE_TOOLCHAIN_FILE=${RTS_TEST_TOOLCHAIN_FILE}")
elseif(DEFINED RTS_TEST_CXX_COMPILER AND NOT "${RTS_TEST_CXX_COMPILER}" STREQUAL "")
    list(APPEND _configure_command "-DCMAKE_CXX_COMPILER=${RTS_TEST_CXX_COMPILER}")
endif()

execute_process(
    COMMAND ${_configure_command}
    RESULT_VARIABLE _configure_result
    OUTPUT_VARIABLE _configure_stdout
    ERROR_VARIABLE _configure_stderr
)
if(NOT _configure_result EQUAL 0)
    message(FATAL_ERROR
        "Runtime install policy configure failed (${_configure_result}).\n"
        "stdout:\n${_configure_stdout}\n"
        "stderr:\n${_configure_stderr}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${RTS_TEST_WORK_DIR}/build"
    RESULT_VARIABLE _build_result
    OUTPUT_VARIABLE _build_stdout
    ERROR_VARIABLE _build_stderr
)
if(NOT _build_result EQUAL 0)
    message(FATAL_ERROR
        "Runtime install policy build failed (${_build_result}).\n"
        "stdout:\n${_build_stdout}\n"
        "stderr:\n${_build_stderr}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${RTS_TEST_WORK_DIR}/build" --prefix "${RTS_TEST_WORK_DIR}/stage"
    RESULT_VARIABLE _install_result
    OUTPUT_VARIABLE _install_stdout
    ERROR_VARIABLE _install_stderr
)
if(NOT _install_result EQUAL 0)
    message(FATAL_ERROR
        "Runtime install policy install failed (${_install_result}).\n"
        "stdout:\n${_install_stdout}\n"
        "stderr:\n${_install_stderr}")
endif()

file(GLOB _installed_runtime
    "${RTS_TEST_WORK_DIR}/stage/bin/runtime_install_policy_probe"
    "${RTS_TEST_WORK_DIR}/stage/bin/runtime_install_policy_probe.exe"
)
if(NOT _installed_runtime)
    message(FATAL_ERROR "Runtime install policy probe did not install its executable")
endif()

if(RTS_TEST_EXPECT_MINGW_DEBUG_SIDECAR)
    file(GLOB _installed_debug_sidecar
        "${RTS_TEST_WORK_DIR}/stage/bin/runtime_install_policy_probe.debug"
        "${RTS_TEST_WORK_DIR}/stage/bin/runtime_install_policy_probe.exe.debug"
    )
    if(NOT _installed_debug_sidecar)
        message(FATAL_ERROR "MinGW runtime install policy probe did not install its .debug sidecar")
    endif()
endif()

message(STATUS "Runtime install policy probe passed")
