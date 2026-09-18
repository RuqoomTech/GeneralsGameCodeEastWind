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

The focused graph currently contains 26 tests. Names describe permanent responsibilities rather than the milestone in which each test was introduced.

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
- `d3d12_shell_policy`

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


## Evolution D3D12 runtime shell

The first x64 Evolution executable is a standalone D3D12 clear/present shell. It deliberately does not configure the legacy D3D8 game runtime.

Configure and build it on Windows with the canonical MinGW-w64 toolchain:

```powershell
cmake --preset mingw64-d3d12-shell
cmake --build --preset mingw64-d3d12-shell
```

Interactive launch:

```powershell
.\build\mingw64-d3d12-shell\Evolution\GeneralsEvolution.exe
```

Automated smoke run (creates the real D3D12 device/swap chain, renders 120 frames, waits for GPU completion and exits):

```powershell
.\build\mingw64-d3d12-shell\Evolution\GeneralsEvolution.exe --frames 120
```

Useful diagnostic options are `--debug`, `--warp`, and `--no-vsync`. `--debug` requests the Windows D3D12 debug layer when the optional Graphics Tools component is installed.

To stage a self-contained MinGW executable (apart from Windows system graphics DLLs):

```powershell
cmake --install build\mingw64-d3d12-shell
.\build\mingw64-d3d12-shell\stage\GeneralsEvolution.exe --frames 120
```

Expected runtime output includes the selected adapter, negotiated D3D feature level, initialization success, and a successful frame-count exit. A real Windows build/run transcript is required before the D3D12 shell is marked Windows-signed-off.

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

The final Step 04F baseline is Windows-signed-off with MinGW-w64 GCC/G++ 16.2. Step 05A is also Windows-signed-off: 25/25 cleaned-name tests plus every explicit deterministic/Evolution/x64-platform gate passed. Step 05B adds one host-portable D3D12-shell policy test, taking the focused graph to 26 tests; the actual D3D12 executable still requires its own Windows build/smoke transcript.

## Known warnings

Legacy `GameMemory` warnings around custom allocation operators are known. They are not current blockers unless a warning becomes an error or directly affects the active subsystem.

## Retired workflow

The active modernization graph no longer supports an i686 MinGW oracle, `mingw32-*` modernization presets, the old cross-architecture timeline comparator, or `-IncludeLegacyX86`. Historical records are retained in `Modernization/WORKLOG.md` and `Modernization/History/`.
