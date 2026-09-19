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

The host-portable focused graph contains 26 tests. On Windows x64 it contains one additional real GPU/backend smoke test (`d3d12_backend_smoke`), for 27 tests total. Names describe permanent responsibilities rather than the milestone in which each test was introduced.

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
- `d3d12_backend_policy`

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
    check_x64_platform `
    check_d3d12_backend_policy
```

Expected success messages use subsystem names rather than historical step numbers.

## Golden fixtures

The active golden fixtures are:

- `Core/Tests/Fixtures/HeadlessDeterminismTimeline.txt`
- `Core/Tests/Fixtures/EvolutionProtocolV1.txt`
- `Core/Tests/Fixtures/EvolutionFullSessionV1.txt`

Protocol/replay golden bytes remain frozen unless an explicit versioned format change is introduced. The headless timeline is deterministic and must not be regenerated merely to make a failing test pass.


## Direct3D 12 backend validation

The D3D12 implementation now lives in the existing WW3D backend tree; there is no standalone Evolution shell preset or application folder.

On Windows x64, the normal `mingw64-tests` build compiles a smoke executable from the production `D3D12Backend.cpp`. The build stages the canonical `Shaders/PrimitiveColor.hlsl` asset beside that executable. The test creates a hidden HWND, constructs the backend through `Create_Render_Backend()`, compiles the staged HLSL, creates persistent default-heap indexed color and textured geometry, uploads a real 2×2 RGBA8 texture through `CopyTextureRegion`, binds it through a shader-visible SRV heap plus D3D12 static sampler, reuses those resources across frames, preserves the transient/deferred-present paths, releases resources after GPU synchronization, presents several frames, and exits.

Run only that GPU smoke test with:

```powershell
ctest --preset mingw64-tests -R d3d12_backend_smoke --output-on-failure
```

Or use the explicit target:

```powershell
cmake --build --preset mingw64-tests --target check_d3d12_backend
```

Expected success message:

```text
D3D12 backend smoke passed: WW3D uploaded RGBA8 texture data, bound shader-visible SRV/static-sampler state, reused textured/default-heap geometry across frames, and released resources safely.
```

The source-policy half remains host-portable:

```powershell
cmake --build --preset mingw64-tests --target check_d3d12_backend_policy
```

The full Zero Hour x64 executable migration graph is enabled separately through `mingw64-game`. Use it to expose the next real compile/link blocker without forcing the full legacy renderer tree into the focused test graph:

```powershell
cmake --preset mingw64-game
cmake --build --preset mingw64-game --target z_generals -- -j1
```

Keep `-j1` while 05H is using the first actual compiler/linker failure as the migration queue. Do not restore the removed standalone shell as a workaround.

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

The final Step 04F and Step 05A baselines are Windows-signed-off with MinGW-w64 GCC/G++ 16.2. Step 05B is also Windows-signed-off: the D3D12 proof executable built, presented successfully on Intel UHD 770 and WARP, installed/staged correctly, and the 26-test focused graph remained green. Step 05C2 is Windows-signed-off: the in-place production backend passed 27/27 plus `check_d3d12_backend` and all deterministic/network/replay/x64 gates. Steps 05D through 05G are Windows-signed-off through the production GPU smoke path, covering indexed drawing, staged canonical HLSL, persistent default-heap geometry, and sampled RGBA8 texture/SRV/static-sampler binding. Step 05H is active and is validated iteratively through the real `mingw64-game` `z_generals` build; each H1 hotfix remains Windows-pending until that build advances past its intended blocker.

## Known warnings

Step 05H1F removes the x64/UB warnings already exposed in the active `mingw64-game` transcript for `ThreadClass`, `Buffer`, `CriticalSectionClass`, and `Compare_EXE_Version`; these are fixed at the owning types rather than suppressed. `#pragma message` output is informational rather than a compiler warning. Legacy `GameMemory` warnings around custom allocation operators may still appear later in the full graph and remain technical debt unless they become errors or affect active work.

## Retired workflow

The active modernization graph no longer supports an i686 MinGW oracle, `mingw32-*` modernization presets, the old cross-architecture timeline comparator, or `-IncludeLegacyX86`. Historical records are retained in `Modernization/WORKLOG.md` and `Modernization/History/`.

### Step 05H1I Windows continuation

H1I addresses the `core_debug/debug_stack.cpp` blocker found at 112/1077 by making its C-library declarations explicit and avoiding the Win32 `StackWalk` macro name in the engine-owned API. Continue the real-game migration with:

```powershell
cmake --build --preset mingw64-game --target z_generals -- -j1
```

The informational `#pragma message` lines are not compiler warnings. Report the first hard failure and any newly exposed warnings.

### Step 05H1J Windows continuation

H1J addresses the `core_compression/EAC/huffencode.cpp` pointer-to-`long` blocker found at 4/826 and sweeps the same unambiguous native-address truncation pattern from active WW3D/debug code. Continue with:

```powershell
cmake --build --preset mingw64-game --target z_generals -- -j1
```

Report the first hard failure plus any actual compiler warnings. Fixed-width compressed/game/network/replay fields must not be widened as part of warning cleanup.


### Step 05H1K Windows continuation

H1K removes D3DX8 utility/math dependencies from the active Evolution WW3D source set after the H1J build reached `GeneralsMD/.../WW3D2/assetmgr.cpp` at 132/947. Continue with:

```powershell
cmake --build --preset mingw64-game --target z_generals -- -j1
```

Report the first hard failure plus any actual compiler warnings. `#pragma message` output remains informational. Do not restore D3DX8 headers or add a D3DX-on-D3D12 compatibility layer to bypass the next blocker.
