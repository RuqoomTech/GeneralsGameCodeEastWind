# Testing

This file is the current command reference for the Evolution modernization work. Historical milestone documents may contain older target names or retired x86 commands; use this file for the supported workflow.

## Windows focused graph

Prerequisites can be installed or verified with:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\setup-windows-dev.ps1
```

Configure, build and run the complete focused graph:

```powershell
cmake --preset mingw64-tests
cmake --build --preset mingw64-tests
ctest --preset mingw64-tests --output-on-failure
```

The preset is x64-only and does not require the Visual Studio IDE.

## Focused test inventory

The focused graph currently contains 25 tests. Names describe permanent responsibilities rather than the milestone in which each test was introduced.

### W3X

- `w3x_asset_format_test`
- `w3x_document_probe_test`
- `w3x_child_discovery_test`

### Determinism and telemetry

- `determinism_primitives`
- `performance_telemetry`
- `headless_determinism`
- `determinism_policy`

### Build / ABI / native-width policies

- `buildsystem_runtime_install_policy`
- `architecture_abi`
- `wire_abi`
- `wire_pointer_policy`
- `runtime_native_width`
- `windows_bootstrap_policy`
- `runtime_pointer_policy`
- `coordinate_ops`
- `runtime_compatibility_policy`
- `x64_platform_policy`

### Evolution network/replay

- `evolution_protocol_v1`
- `evolution_protocol_policy`
- `evolution_runtime_bridge`
- `evolution_runtime_policy`
- `evolution_session`
- `evolution_session_policy`
- `evolution_full_session`
- `evolution_full_session_policy`

## Explicit checks

These targets are useful when validating one subsystem or collecting a concise sign-off transcript:

```powershell
cmake --build --preset mingw64-tests --target `
    check_determinism `
    check_headless_determinism `
    check_evolution_protocol `
    check_evolution_runtime `
    check_evolution_session `
    check_evolution_full_session `
    check_x64_platform
```

Expected success messages use subsystem names rather than historical step numbers.

## Golden fixtures

The active golden fixtures are:

- `Core/Tests/Fixtures/HeadlessDeterminismTimeline.txt`
- `Core/Tests/Fixtures/EvolutionProtocolV1.txt`
- `Core/Tests/Fixtures/EvolutionFullSessionV1.txt`

Protocol/replay golden bytes remain frozen unless an explicit versioned format change is introduced. The headless timeline is deterministic and must not be regenerated merely to make a failing test pass.

## Local host validation

The focused graph is also useful on host GCC/Clang for fast regression checks because it avoids renderer/tool/runtime dependencies that are irrelevant to these gates. Example:

```bash
cmake -S . -B build/tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DRTS_BUILD_TESTS_ONLY=ON \
  -DRTS_BUILD_TESTS=ON \
  -DRTS_BUILD_ZEROHOUR=OFF \
  -DRTS_BUILD_GENERALS=OFF \
  -DRTS_BUILD_CORE_TOOLS=OFF \
  -DRTS_BUILD_EVOLUTION_X64=ON \
  -DRTS_BUILD_HEADLESS_CORE=ON
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

For significant deterministic/network/replay changes, validate Release and Debug plus Clang and available sanitizers before packaging.

## Windows sign-off rule

Do not claim a Windows gate passed from host/container testing. Windows sign-off requires actual Windows console output using the authoritative candidate tree.

The final Step 04F baseline is Windows-signed-off with MinGW-w64 GCC/G++ 16.2: 25/25 focused tests plus every explicit deterministic/Evolution/x64-platform gate passed.

## Known warnings

Legacy `GameMemory` warnings around custom allocation operators are known. They are not current blockers unless a warning becomes an error or directly affects the active subsystem.

## Retired workflow

The active modernization graph no longer supports an i686 MinGW oracle, `mingw32-*` modernization presets, the old cross-architecture timeline comparator, or `-IncludeLegacyX86`. Historical records are retained in `Modernization/WORKLOG.md` and `Modernization/History/`.
