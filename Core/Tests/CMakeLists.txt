# Focused modernization characterization/regression tests.
# This directory intentionally stays independent of the monolithic game targets so
# `RTS_BUILD_TESTS_ONLY=ON` does not fetch/configure renderer, tool, or runtime deps.

function(rts_add_w3x_test target_name source_file)
    add_executable(${target_name} ${source_file} ${ARGN})
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
    )
    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 98
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -pedantic)
    endif()
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

rts_add_w3x_test(
    w3x_asset_format_test
    ${CMAKE_CURRENT_SOURCE_DIR}/W3XAssetFormatTest.cpp
)
rts_add_w3x_test(
    w3x_document_probe_test
    ${CMAKE_CURRENT_SOURCE_DIR}/W3XDocumentProbeTest.cpp
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/rts/w3x_document.cpp
)
rts_add_w3x_test(
    w3x_child_discovery_test
    ${CMAKE_CURRENT_SOURCE_DIR}/W3XChildDiscoveryTest.cpp
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/rts/w3x_document.cpp
)

# Step 01 determinism gate. On Windows/i686 this is the exact compatibility gate
# signed off in Step 01G: production RandomValue/Snapshot/Xfer/XferCRC/Damage units,
# the legacy CppMacros prelude, and MinGW IPO to discard unused legacy link leakage.
# Non-Windows builds exercise the lightweight CRC/RNG/float characterization from
# the same test source so CTest can provide a local regression gate.
set(_rts_determinism_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/DeterminismPrimitivesTest.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/RandomValue.cpp
)

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    list(APPEND _rts_determinism_sources
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/System/Snapshot.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/System/Xfer.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/System/XferCRC.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/GameLogic/System/Damage.cpp
    )
endif()

add_executable(z_determinismtest ${_rts_determinism_sources})
target_compile_definitions(z_determinismtest PRIVATE
    RTS_STANDALONE_DETERMINISM_TEST=1
)
target_include_directories(z_determinismtest PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas
)
target_link_libraries(z_determinismtest PRIVATE
    core_config
    core_utility_no_pch
)

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_compile_definitions(z_determinismtest PRIVATE
        RTS_ZEROHOUR=1
        RTS_ENGINE_DETERMINISM_TEST=1
    )
    # Preserve Zero Hour header precedence from the former zi_gameengine_include
    # linkage while keeping the focused test independent of GeneralsMD configure.
    target_include_directories(z_determinismtest BEFORE PRIVATE
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include/Precompiled
        ${CMAKE_SOURCE_DIR}/GeneralsMD/Code/GameEngine/Include
    )
    target_precompile_headers(z_determinismtest PRIVATE
        [["Utility/CppMacros.h"]]
    )

    if(MINGW)
        target_link_libraries(z_determinismtest PRIVATE reactos_atl)
        include(CheckIPOSupported)
        check_ipo_supported(RESULT _rts_determinism_ipo_supported OUTPUT _rts_determinism_ipo_error LANGUAGES CXX)
        if(NOT _rts_determinism_ipo_supported)
            message(FATAL_ERROR
                "The Step 01 MinGW determinism gate requires C++ IPO/LTO to preserve the "
                "focused link boundary. CMake reported: ${_rts_determinism_ipo_error}")
        endif()
        set_property(TARGET z_determinismtest PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        target_link_options(z_determinismtest PRIVATE -mconsole)
    endif()
else()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(z_determinismtest PRIVATE
            -Wall -Wextra -Werror -Wno-unknown-pragmas -Wno-unused-parameter -pedantic
        )
    endif()
endif()

add_test(NAME determinism_step01 COMMAND z_determinismtest)
add_custom_target(z_determinismcheck
    COMMAND $<TARGET_FILE:z_determinismtest>
    DEPENDS z_determinismtest
    USES_TERMINAL
)


# Step 03A telemetry regression. The capture implementation is intentionally
# dependency-free so its stable CSV schema can be exercised in the focused graph.
add_executable(performance_telemetry_test
    ${CMAKE_CURRENT_SOURCE_DIR}/PerformanceTelemetryTest.cpp
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/profile/performance_telemetry.cpp
)
target_include_directories(performance_telemetry_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
)
set_target_properties(performance_telemetry_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(performance_telemetry_test PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
add_test(NAME performance_telemetry_step03a COMMAND performance_telemetry_test)

# Step 02B build-system regression: exercise the shared runtime install policy
# in a tiny nested project. This catches unsupported linker-specific generator
# expressions (notably TARGET_PDB_FILE on GNU/MinGW) during CMake generation.
# Keep it on Ninja, the canonical modernization generator, so nested-generator
# assumptions remain explicit and predictable.
if(CMAKE_GENERATOR STREQUAL "Ninja")
    set(_rts_runtime_install_policy_args
        -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DRTS_TEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/runtime-install-policy
        -DRTS_TEST_GENERATOR=${CMAKE_GENERATOR}
        -DRTS_TEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DRTS_TEST_EXPECT_MINGW_DEBUG_SIDECAR=${MINGW}
    )
    if(CMAKE_TOOLCHAIN_FILE)
        list(APPEND _rts_runtime_install_policy_args
            -DRTS_TEST_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        )
    endif()

    add_test(
        NAME buildsystem_runtime_install_policy
        COMMAND ${CMAKE_COMMAND}
            ${_rts_runtime_install_policy_args}
            -P ${CMAKE_SOURCE_DIR}/cmake/tests/RuntimeInstallPolicyTest.cmake
    )
endif()
