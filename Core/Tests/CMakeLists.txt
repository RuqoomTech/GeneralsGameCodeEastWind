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

# Step 01 determinism gate. Step 04F retired the former Windows/i686 oracle
# configuration; this focused guard now exercises the architecture-independent
# deterministic primitives on the supported x64 modernization graph.
set(_rts_determinism_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/DeterminismPrimitivesTest.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/RandomValue.cpp
)

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
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(z_determinismtest PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -Wno-unused-parameter -pedantic
    )
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

# Step 04A architecture-width migration guard. On the Step 04F x64-only lane,
# this continues to prove that native pointers are 64-bit while fixed wire/
# serialization types and IDs remain explicitly 16/32-bit.
add_executable(architecture_migration_test
    ${CMAKE_CURRENT_SOURCE_DIR}/ArchitectureMigrationTest.cpp
)
target_include_directories(architecture_migration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
)
target_link_libraries(architecture_migration_test PRIVATE
    core_config
    core_utility_no_pch
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(architecture_migration_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
    )
endif()
add_test(NAME architecture_width_step04a COMMAND architecture_migration_test)


# Step 04B wire/replay ABI guard. Runtime GameMessage layout may widen on x64,
# but protocol/replay widths and the frozen legacy command-packet byte capacity
# must remain architecture-independent.
add_executable(wire_abi_step04b_test
    ${CMAKE_CURRENT_SOURCE_DIR}/WireAbiStep04BTest.cpp
)
target_include_directories(wire_abi_step04b_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
)
target_link_libraries(wire_abi_step04b_test PRIVATE
    core_config
    core_utility_no_pch
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(wire_abi_step04b_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
    )
endif()
add_test(NAME wire_replay_abi_step04b COMMAND wire_abi_step04b_test)


add_test(
    NAME pointer_wire_source_audit_step04b
    COMMAND ${CMAKE_COMMAND}
        -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/tests/X64PointerWireAuditTest.cmake
)

# Step 04C native-width runtime substrate guard. This instantiates the actual
# WWLib object pool and forces multiple backing blocks so the block-list header,
# free-list pointers, and object alignment are exercised with native pointer width.
add_executable(runtime_native_width_step04c_test
    ${CMAKE_CURRENT_SOURCE_DIR}/RuntimeNativeWidthStep04CTest.cpp
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WWLib/FastAllocator.cpp
)
target_include_directories(runtime_native_width_step04c_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas
)
target_link_libraries(runtime_native_width_step04c_test PRIVATE
    core_config
    core_utility_no_pch
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(runtime_native_width_step04c_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
    )
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND UNIX)
    # The legacy WWLib umbrella headers collide with libc's strlcpy/strlcat
    # declarations on Clang/Linux. This is a host-only focused-test shim; the
    # Windows x64 target does not depend on these libc declarations.
    target_compile_definitions(runtime_native_width_step04c_test PRIVATE
        HAVE_STRLCPY=1 HAVE_STRLCAT=1 HAVE_WCSLCPY=1 HAVE_WCSLCAT=1
    )
    target_compile_options(runtime_native_width_step04c_test PRIVATE
        -Wno-implicit-exception-spec-mismatch
        -Wno-unused-parameter
        -Wno-unused-but-set-variable
    )
endif()
add_test(NAME runtime_native_width_step04c COMMAND runtime_native_width_step04c_test)

add_test(
    NAME windows_dependency_bootstrap_step04c
    COMMAND ${CMAKE_COMMAND}
        -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/tests/WindowsDependencyBootstrapTest.cmake
)

add_test(
    NAME runtime_pointer_source_audit_step04c
    COMMAND ${CMAKE_COMMAND}
        -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/tests/X64RuntimePointerAuditStep04CTest.cmake
)

# Step 04D3 upstream-alignment guards. The runtime check exercises the only
# shared math API added by the sync; the source-policy check locks the selected
# upstream fixes while explicitly protecting EastWind-owned x64 adaptations.
add_executable(upstream_alignment_step04d3_test
    ${CMAKE_CURRENT_SOURCE_DIR}/UpstreamAlignmentStep04D3Test.cpp
)
target_include_directories(upstream_alignment_step04d3_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
)
target_link_libraries(upstream_alignment_step04d3_test PRIVATE
    core_config
    core_utility_no_pch
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(upstream_alignment_step04d3_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
    )
endif()
add_test(NAME upstream_coordinate_ops_step04d3 COMMAND upstream_alignment_step04d3_test)

add_test(
    NAME upstream_alignment_source_policy_step04d3
    COMMAND ${CMAKE_COMMAND}
        -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/tests/UpstreamAlignmentStep04D3Test.cmake
)


# Step 04D deterministic/headless core gate. This is deliberately renderer-free:
# it compiles the production GameLogic RNG and the shared production FPU policy,
# executes a 12k-frame fixed-width micro-simulation, and verifies checkpoint CRCs.
add_executable(headless_determinism_step04d_test
    ${CMAKE_CURRENT_SOURCE_DIR}/HeadlessDeterminismStep04DTest.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/RandomValue.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/GameLogic/System/FPUControl.cpp
)
target_compile_definitions(headless_determinism_step04d_test PRIVATE
    RTS_STANDALONE_DETERMINISM_TEST=1
)
target_include_directories(headless_determinism_step04d_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
    ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas
    ${CMAKE_SOURCE_DIR}/GeneralsMD/Code/GameEngine/Include
)
target_link_libraries(headless_determinism_step04d_test PRIVATE
    core_config
    core_utility_no_pch
)
set_target_properties(headless_determinism_step04d_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(headless_determinism_step04d_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -Wno-unused-parameter -pedantic
        -fno-fast-math
        -ffp-contract=off
    )
elseif(MSVC)
    target_compile_options(headless_determinism_step04d_test PRIVATE /fp:strict)
endif()
add_test(
    NAME headless_determinism_step04d
    COMMAND headless_determinism_step04d_test
        --verify ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/Step04DHeadlessTimeline.txt
)
add_custom_target(z_headlessdeterminismcheck
    COMMAND $<TARGET_FILE:headless_determinism_step04d_test>
        --verify ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/Step04DHeadlessTimeline.txt
    DEPENDS headless_determinism_step04d_test
    USES_TERMINAL
)

add_test(
    NAME deterministic_headless_policy_step04d
    COMMAND ${CMAKE_COMMAND}
        -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/tests/DeterministicHeadlessPolicyStep04DTest.cmake
)

# Step 04E Evolution protocol/replay v1 guard. The codec and framers are standalone
# fixed-width code; Step 04F keeps the exact frozen bytes while retiring i686.
add_executable(evolution_protocol_step04e_test
    ${CMAKE_CURRENT_SOURCE_DIR}/EvolutionProtocolStep04ETest.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionCommandCodec.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionReplayFormat.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/GameNetwork/EvolutionProtocol.cpp
)
target_include_directories(evolution_protocol_step04e_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
)
set_target_properties(evolution_protocol_step04e_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(evolution_protocol_step04e_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
    )
endif()
add_test(
    NAME evolution_protocol_v1_step04e
    COMMAND evolution_protocol_step04e_test
        ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/Step04EEvolutionProtocolV1.txt
)
add_custom_target(z_evolutionprotocolcheck
    COMMAND $<TARGET_FILE:evolution_protocol_step04e_test>
        ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/Step04EEvolutionProtocolV1.txt
    DEPENDS evolution_protocol_step04e_test
    USES_TERMINAL
)

add_test(
    NAME evolution_protocol_source_policy_step04e
    COMMAND ${CMAKE_COMMAND}
        -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/tests/EvolutionProtocolPolicyStep04ETest.cmake
)

# Compile the production GameMessage adapter in the Windows focused graph even
# though the standalone codec test intentionally avoids the legacy memory-pool runtime.
# The historical GameMemory header includes <new.h>, so host Unix validation leaves
# this adapter to the source-policy test while both MinGW oracle/Evolution lanes compile it.
if(WIN32)
    add_library(evolution_game_message_adapter_step04e OBJECT
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionGameMessageAdapter.cpp
    )
    target_include_directories(evolution_game_message_adapter_step04e PRIVATE
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas
    )
    target_link_libraries(evolution_game_message_adapter_step04e PRIVATE
        core_config
        core_utility_no_pch
    )
    set_target_properties(evolution_game_message_adapter_step04e PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(evolution_game_message_adapter_step04e PRIVATE
            -Wall -Wextra -Wno-unknown-pragmas -pedantic
        )
    endif()
endif()

# Step 04E2 staged runtime bridge. This portable executable proves that routed
# EVN1 metadata survives decode and that the same command payload can be written
# to/read from EVR1 without byte drift. Production legacy runtime TUs remain a
# Windows/full-runtime gate; the source-policy test below pins their bridge seam.
add_executable(evolution_runtime_bridge_step04e2_test
    ${CMAKE_CURRENT_SOURCE_DIR}/EvolutionRuntimeBridgeStep04E2Test.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionCommandCodec.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionReplayFormat.cpp
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/GameNetwork/EvolutionProtocol.cpp
)
target_include_directories(evolution_runtime_bridge_step04e2_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
)
set_target_properties(evolution_runtime_bridge_step04e2_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(evolution_runtime_bridge_step04e2_test PRIVATE
        -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
    )
endif()
add_test(
    NAME evolution_runtime_bridge_step04e2
    COMMAND evolution_runtime_bridge_step04e2_test
)
add_custom_target(z_evolutionruntimecheck
    COMMAND $<TARGET_FILE:evolution_runtime_bridge_step04e2_test>
    DEPENDS evolution_runtime_bridge_step04e2_test
    USES_TERMINAL
)

add_test(
    NAME evolution_runtime_integration_policy_step04e2
    COMMAND ${CMAKE_COMMAND}
        -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/tests/EvolutionRuntimeIntegrationPolicyStep04E2Test.cmake
)


# Step 04E3 deterministic two-endpoint Evolution session gate. This is x64-only:
# Step 04F retires i686; this remains an Evolution x64-only runtime/session gate.
# The harness uses the production command codec and routed EVN1 packet composition;
# it only controls deterministic delivery faults and legacy ACK arrival timing.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_executable(evolution_session_step04e3_test
        ${CMAKE_CURRENT_SOURCE_DIR}/EvolutionSessionStep04E3Test.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionCommandCodec.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/GameNetwork/EvolutionProtocol.cpp
    )
    target_include_directories(evolution_session_step04e3_test PRIVATE
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
    )
    target_link_libraries(evolution_session_step04e3_test PRIVATE
        core_config
        core_utility_no_pch
    )
    set_target_properties(evolution_session_step04e3_test PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(evolution_session_step04e3_test PRIVATE
            -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
            -fno-fast-math
            -ffp-contract=off
        )
    elseif(MSVC)
        target_compile_options(evolution_session_step04e3_test PRIVATE /fp:strict)
    endif()
    add_test(NAME evolution_session_x64_step04e3 COMMAND evolution_session_step04e3_test)
    add_custom_target(z_evolutionsessioncheck
        COMMAND $<TARGET_FILE:evolution_session_step04e3_test>
        DEPENDS evolution_session_step04e3_test
        USES_TERMINAL
    )

    add_test(
        NAME evolution_session_integration_policy_step04e3
        COMMAND ${CMAKE_COMMAND}
            -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/tests/EvolutionSessionPolicyStep04E3Test.cmake
    )

    # Step 04E4 freezes a representative end-to-end command session as exact
    # EVN1/EVR1 transcript bytes. It replays the same deterministic command/CRC
    # timeline through both media and locks version/corruption/fallback policy.
    add_executable(evolution_full_session_step04e4_test
        ${CMAKE_CURRENT_SOURCE_DIR}/EvolutionFullSessionStep04E4Test.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionCommandCodec.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/Common/EvolutionReplayFormat.cpp
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Source/GameNetwork/EvolutionProtocol.cpp
    )
    target_include_directories(evolution_full_session_step04e4_test PRIVATE
        ${CMAKE_SOURCE_DIR}/Core/GameEngine/Include
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Include
    )
    target_link_libraries(evolution_full_session_step04e4_test PRIVATE
        core_config
        core_utility_no_pch
    )
    set_target_properties(evolution_full_session_step04e4_test PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(evolution_full_session_step04e4_test PRIVATE
            -Wall -Wextra -Werror -Wno-unknown-pragmas -pedantic
            -fno-fast-math
            -ffp-contract=off
        )
    elseif(MSVC)
        target_compile_options(evolution_full_session_step04e4_test PRIVATE /fp:strict)
    endif()
    add_test(
        NAME evolution_full_session_x64_step04e4
        COMMAND evolution_full_session_step04e4_test
            ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/Step04E4EvolutionFullSessionV1.txt
    )
    add_custom_target(z_evolutionfullsessioncheck
        COMMAND $<TARGET_FILE:evolution_full_session_step04e4_test>
            ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/Step04E4EvolutionFullSessionV1.txt
        DEPENDS evolution_full_session_step04e4_test
        USES_TERMINAL
    )

    add_test(
        NAME evolution_full_session_integration_policy_step04e4
        COMMAND ${CMAKE_COMMAND}
            -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/tests/EvolutionFullSessionPolicyStep04E4Test.cmake
    )
endif()

# Step 04F retires the frozen i686 modernization/oracle surface. The policy gate
# checks supported presets/toolchains/bootstrap/test wiring while preserving the
# fixed-width wire/replay contracts already guarded by Steps 04A-04E4.
add_test(
    NAME x64_retirement_policy_step04f
    COMMAND ${CMAKE_COMMAND}
        -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/tests/X64RetirementPolicyStep04FTest.cmake
)
add_custom_target(z_step04fcheck
    COMMAND ${CMAKE_COMMAND}
        -DRTS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/tests/X64RetirementPolicyStep04FTest.cmake
    USES_TERMINAL
)
