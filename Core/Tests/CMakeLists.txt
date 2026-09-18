# Focused modernization characterization/regression tests.
# This directory intentionally stays independent of the monolithic game targets so
# `RTS_BUILD_TESTS_ONLY=ON` does not fetch/configure renderer, tool, or runtime deps.

# MinGW focused tests must not resolve libstdc++/libgcc from the ambient PATH.
# A mixed MSYS2 PATH (for example mingw64 compiler + ucrt64 runtime DLL) is ABI-incompatible
# and can crash otherwise portable tests inside the C++ standard library before the test body runs.
# Mirror the self-contained GCC runtime policy already carried by core_config, but keep these
# standalone tests free of legacy engine libraries/definitions.
if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++)
endif()

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
function(rts_add_source_policy_test test_name script_name)
    add_test(
        NAME ${test_name}
        COMMAND ${CMAKE_COMMAND}
            -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/tests/${script_name}
    )
    set_tests_properties(${test_name} PROPERTIES LABELS "policy")
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

# Deterministic primitives guard for the supported x64 modernization graph.
set(_rts_determinism_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/DeterminismPrimitivesTest.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/RandomValue.cpp
)

add_executable(determinism_test ${_rts_determinism_sources})
target_compile_definitions(determinism_test PRIVATE
    RTS_STANDALONE_DETERMINISM_TEST=1
)
target_include_directories(determinism_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas
)
target_link_libraries(determinism_test PRIVATE
    core_config
    core_utility_no_pch
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(determinism_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -Wno-unused-parameter -pedantic
    )
endif()

add_test(NAME determinism_primitives COMMAND determinism_test)
add_custom_target(check_determinism
    COMMAND $<TARGET_FILE:determinism_test>
    DEPENDS determinism_test
    USES_TERMINAL
)


# Performance telemetry regression. The capture implementation is intentionally
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
add_test(NAME performance_telemetry COMMAND performance_telemetry_test)

# Build-system regression: exercise the shared runtime install policy
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

# Architecture ABI guard: native pointers are 64-bit while fixed wire and
# serialization types and IDs remain explicitly 16/32-bit.
add_executable(architecture_abi_test
    ${CMAKE_CURRENT_SOURCE_DIR}/ArchitectureAbiTest.cpp
)
target_include_directories(architecture_abi_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
)
target_link_libraries(architecture_abi_test PRIVATE
    core_config
    core_utility_no_pch
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(architecture_abi_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
    )
endif()
add_test(NAME architecture_abi COMMAND architecture_abi_test)


# Wire/replay ABI guard. Runtime GameMessage layout may widen on x64,
# but protocol/replay widths and the frozen legacy command-packet byte capacity
# must remain architecture-independent.
add_executable(wire_abi_test
    ${CMAKE_CURRENT_SOURCE_DIR}/WireAbiTest.cpp
)
target_include_directories(wire_abi_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
)
target_link_libraries(wire_abi_test PRIVATE
    core_config
    core_utility_no_pch
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(wire_abi_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
    )
endif()
add_test(NAME wire_abi COMMAND wire_abi_test)


rts_add_source_policy_test(wire_pointer_policy WirePointerPolicyTest.cmake)

# Native-width runtime substrate guard. This instantiates the actual
# WWLib object pool and forces multiple backing blocks so the block-list header,
# free-list pointers, and object alignment are exercised with native pointer width.
add_executable(runtime_native_width_test
    ${CMAKE_CURRENT_SOURCE_DIR}/RuntimeNativeWidthTest.cpp
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWLib/FastAllocator.cpp
)
target_include_directories(runtime_native_width_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas
)
target_link_libraries(runtime_native_width_test PRIVATE
    core_config
    core_utility_no_pch
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(runtime_native_width_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
    )
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND UNIX)
    # The legacy WWLib umbrella headers collide with libc's strlcpy/strlcat
    # declarations on Clang/Linux. This is a host-only focused-test shim; the
    # Windows x64 target does not depend on these libc declarations.
    target_compile_definitions(runtime_native_width_test PRIVATE
        HAVE_STRLCPY=1 HAVE_STRLCAT=1 HAVE_WCSLCPY=1 HAVE_WCSLCAT=1
    )
    target_compile_options(runtime_native_width_test PRIVATE
        -Wno-implicit-exception-spec-mismatch
        -Wno-unused-parameter
        -Wno-unused-but-set-variable
    )
endif()
add_test(NAME runtime_native_width COMMAND runtime_native_width_test)

rts_add_source_policy_test(windows_bootstrap_policy WindowsBootstrapPolicyTest.cmake)

rts_add_source_policy_test(runtime_pointer_policy RuntimePointerPolicyTest.cmake)

# Runtime compatibility guards for selected upstream correctness fixes while
# protecting locally owned x64/determinism adaptations.
add_executable(coordinate_ops_test
    ${CMAKE_CURRENT_SOURCE_DIR}/CoordinateOpsTest.cpp
)
target_include_directories(coordinate_ops_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
)
target_link_libraries(coordinate_ops_test PRIVATE
    core_config
    core_utility_no_pch
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(coordinate_ops_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
    )
endif()
add_test(NAME coordinate_ops COMMAND coordinate_ops_test)

rts_add_source_policy_test(runtime_compatibility_policy RuntimeCompatibilityPolicyTest.cmake)


# Renderer-free deterministic/headless core gate:
# it compiles the production GameLogic RNG and the shared production FPU policy,
# executes a 12k-frame fixed-width micro-simulation, and verifies checkpoint CRCs.
add_executable(headless_determinism_test
    ${CMAKE_CURRENT_SOURCE_DIR}/HeadlessDeterminismTest.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/RandomValue.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/GameLogic/System/FPUControl.cpp
)
target_compile_definitions(headless_determinism_test PRIVATE
    RTS_STANDALONE_DETERMINISM_TEST=1
)
target_include_directories(headless_determinism_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas
    ${CMAKE_SOURCE_DIR}/GeneralsMD/Code/GameEngine/Include
)
target_link_libraries(headless_determinism_test PRIVATE
    core_config
    core_utility_no_pch
)
set_target_properties(headless_determinism_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(headless_determinism_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -Wno-unused-parameter -pedantic
        -fno-fast-math
        -ffp-contract=off
    )
elseif(MSVC)
    target_compile_options(headless_determinism_test PRIVATE /fp:strict)
endif()
add_test(
    NAME headless_determinism
    COMMAND headless_determinism_test
        --verify ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/HeadlessDeterminismTimeline.txt
)
add_custom_target(check_headless_determinism
    COMMAND $<TARGET_FILE:headless_determinism_test>
        --verify ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/HeadlessDeterminismTimeline.txt
    DEPENDS headless_determinism_test
    USES_TERMINAL
)

rts_add_source_policy_test(determinism_policy DeterminismPolicyTest.cmake)

# Evolution protocol/replay v1 guard. The codec and framers are standalone
# fixed-width code with frozen golden bytes.
add_executable(evolution_protocol_test
    ${CMAKE_CURRENT_SOURCE_DIR}/EvolutionProtocolTest.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionCommandCodec.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionReplayFormat.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/GameNetwork/EvolutionProtocol.cpp
)
target_include_directories(evolution_protocol_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
)
set_target_properties(evolution_protocol_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(evolution_protocol_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
    )
endif()
add_test(
    NAME evolution_protocol_v1
    COMMAND evolution_protocol_test
        ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/EvolutionProtocolV1.txt
)
add_custom_target(check_evolution_protocol
    COMMAND $<TARGET_FILE:evolution_protocol_test>
        ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/EvolutionProtocolV1.txt
    DEPENDS evolution_protocol_test
    USES_TERMINAL
)

rts_add_source_policy_test(evolution_protocol_policy EvolutionProtocolPolicyTest.cmake)

# Compile the production GameMessage adapter in the Windows focused graph even
# though the standalone codec test intentionally avoids the legacy memory-pool runtime.
# The historical GameMemory header includes <new.h>, so host Unix validation leaves
# this adapter to the source-policy test while the Windows Evolution lane compiles it.
if(WIN32)
    add_library(evolution_game_message_adapter_compile OBJECT
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionGameMessageAdapter.cpp
    )
    target_include_directories(evolution_game_message_adapter_compile PRIVATE
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas
    )
    target_link_libraries(evolution_game_message_adapter_compile PRIVATE
        core_config
        core_utility_no_pch
    )
    set_target_properties(evolution_game_message_adapter_compile PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(evolution_game_message_adapter_compile PRIVATE
            -Wall -Wextra -Wno-unknown-pragmas -pedantic
        )
    endif()
endif()

# Evolution runtime bridge. This portable executable proves that routed
# EVN1 metadata survives decode and that the same command payload can be written
# to/read from EVR1 without byte drift. Production legacy runtime TUs remain a
# Windows/full-runtime gate; the source-policy test below pins their bridge seam.
add_executable(evolution_runtime_bridge_test
    ${CMAKE_CURRENT_SOURCE_DIR}/EvolutionRuntimeBridgeTest.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionCommandCodec.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionReplayFormat.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/GameNetwork/EvolutionProtocol.cpp
)
target_include_directories(evolution_runtime_bridge_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
)
set_target_properties(evolution_runtime_bridge_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(evolution_runtime_bridge_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
    )
endif()
add_test(
    NAME evolution_runtime_bridge
    COMMAND evolution_runtime_bridge_test
)
add_custom_target(check_evolution_runtime
    COMMAND $<TARGET_FILE:evolution_runtime_bridge_test>
    DEPENDS evolution_runtime_bridge_test
    USES_TERMINAL
)

rts_add_source_policy_test(evolution_runtime_policy EvolutionRuntimePolicyTest.cmake)


# Deterministic two-endpoint Evolution session gate. This is x64-only.
# The harness uses the production command codec and routed EVN1 packet composition;
# it only controls deterministic delivery faults and legacy ACK arrival timing.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_executable(evolution_session_test
        ${CMAKE_CURRENT_SOURCE_DIR}/EvolutionSessionTest.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionCommandCodec.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/GameNetwork/EvolutionProtocol.cpp
    )
    target_include_directories(evolution_session_test PRIVATE
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
    )
    target_link_libraries(evolution_session_test PRIVATE
        core_config
        core_utility_no_pch
    )
    set_target_properties(evolution_session_test PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(evolution_session_test PRIVATE
            -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
            -fno-fast-math
            -ffp-contract=off
        )
    elseif(MSVC)
        target_compile_options(evolution_session_test PRIVATE /fp:strict)
    endif()
    add_test(NAME evolution_session COMMAND evolution_session_test)
    add_custom_target(check_evolution_session
        COMMAND $<TARGET_FILE:evolution_session_test>
        DEPENDS evolution_session_test
        USES_TERMINAL
    )

    rts_add_source_policy_test(evolution_session_policy EvolutionSessionPolicyTest.cmake)

    # Freeze a representative end-to-end command session as exact
    # EVN1/EVR1 transcript bytes. It replays the same deterministic command/CRC
    # timeline through both media and locks version/corruption/fallback policy.
    add_executable(evolution_full_session_test
        ${CMAKE_CURRENT_SOURCE_DIR}/EvolutionFullSessionTest.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionCommandCodec.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionReplayFormat.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/GameNetwork/EvolutionProtocol.cpp
    )
    target_include_directories(evolution_full_session_test PRIVATE
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
    )
    target_link_libraries(evolution_full_session_test PRIVATE
        core_config
        core_utility_no_pch
    )
    set_target_properties(evolution_full_session_test PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(evolution_full_session_test PRIVATE
            -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
            -fno-fast-math
            -ffp-contract=off
        )
    elseif(MSVC)
        target_compile_options(evolution_full_session_test PRIVATE /fp:strict)
    endif()
    add_test(
        NAME evolution_full_session
        COMMAND evolution_full_session_test
            ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/EvolutionFullSessionV1.txt
    )
    add_custom_target(check_evolution_full_session
        COMMAND $<TARGET_FILE:evolution_full_session_test>
            ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/EvolutionFullSessionV1.txt
        DEPENDS evolution_full_session_test
        USES_TERMINAL
    )

    rts_add_source_policy_test(evolution_full_session_policy EvolutionFullSessionPolicyTest.cmake)
endif()

# x64 platform policy: keep the retired i686 modernization surface out of the
# supported toolchain while preserving fixed-width wire/replay contracts.
rts_add_source_policy_test(x64_platform_policy X64PlatformPolicyTest.cmake)
add_custom_target(check_x64_platform
    COMMAND ${CMAKE_COMMAND}
        -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/tests/X64PlatformPolicyTest.cmake
    USES_TERMINAL
)

# Direct3D 12 is integrated behind the existing WW3D backend seam. The policy
# test is host-portable; Windows x64 additionally compiles and runs the real
# production backend against a hidden HWND.
rts_add_source_policy_test(d3d12_backend_policy D3D12BackendPolicyTest.cmake)
add_custom_target(check_d3d12_backend_policy
    COMMAND ${CMAKE_COMMAND}
        -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/tests/D3D12BackendPolicyTest.cmake
    USES_TERMINAL
)

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_executable(d3d12_backend_smoke_test
        ${CMAKE_CURRENT_SOURCE_DIR}/D3D12BackendSmokeTest.cpp
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp
    )
    target_include_directories(d3d12_backend_smoke_test PRIVATE
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
    )
    target_include_directories(d3d12_backend_smoke_test SYSTEM PRIVATE
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas
    )
    target_link_libraries(d3d12_backend_smoke_test PRIVATE
        core_config
        core_utility_no_pch
        d3d12
        dxgi
        dxguid
        user32
    )
    set_target_properties(d3d12_backend_smoke_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(d3d12_backend_smoke_test PRIVATE
            -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
        )
    endif()
    add_test(NAME d3d12_backend_smoke COMMAND d3d12_backend_smoke_test)
    set_tests_properties(d3d12_backend_smoke PROPERTIES LABELS "gpu")
    add_custom_target(check_d3d12_backend
        COMMAND $<TARGET_FILE:d3d12_backend_smoke_test>
        DEPENDS d3d12_backend_smoke_test
        USES_TERMINAL
    )
endif()
