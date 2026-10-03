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

## Camera and naming continuation

H1V extends the production D3D12 smoke test with non-symmetric view/projection transforms on dynamic, persistent color and persistent textured geometry, changes between draws within one command list, and 2D isolation from camera state. The 27-test graph passes locally. Telemetry schema v3 uses `submitted_triangles` and `submitted_vertices`; skin/sorted values are subsets of the submitted totals. `scripts/perf-summary.py` accepts v2 and v3 captures.

The real serial game command remains the migration queue. Its final result and patch reconstruction are recorded in `PROJECT_STATE.md`; focused GPU tests are not normal-game boot/frame evidence.

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

### Step 05H1L Windows continuation

H1L addresses the active WW3D asset-name pointer-to-`int` arithmetic blocker found at 125/940 and removes the exposed `prim_anim.h` non-void/no-return warning. Continue with:

```powershell
cmake --build --preset mingw64-game --target z_generals -- -j1
```

Report the first hard failure plus any actual compiler warnings. The `#pragma message` output remains informational.
### Step 05H1M Windows continuation

H1M fixes the renderer-neutral backend vertex value contract exposed when H1L reached the real Evolution `Render2D` caller at 154/942. Continue with:

```powershell
cmake --build --preset mingw64-game --target z_generals -- -j1
```

Report the first hard failure plus any actual compiler warnings. The `#pragma message` output remains informational. Do not replace the backend vertex path with DX8/D3DX compatibility code to bypass the next blocker.


### Step 05H1N Windows continuation

H1N fixes the `pointgr.cpp` WWMath compile blocker reached at 188/942 and removes the logical-audio pointer-width plus secondary-base intrusive-delete warnings from the same H1M Windows run. Continue with:

```powershell
cmake --build --preset mingw64-game --target z_generals -- -j1
```

Report the first hard failure plus all actual compiler warnings. `#pragma message` output remains informational. Keep runtime-only native pointers separate from fixed-width gameplay/network/replay/Xfer data.


### Step 05H1P Windows continuation result

The H1P procedural mip-isolation change cleared the `TerrainTex.cpp` `d3dx8tex.h` blocker on real Windows MinGW-w64 GCC 16.2. The incremental `z_generals` build advanced to `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp` at 110/126 and then stopped on that file's remaining `d3dx8tex.h` include. The emitted `#pragma message` remains informational; the other warnings in that run are tracked separately and are not part of the H1Q shader-math responsibility.

### Step 05H1Q Windows continuation result

H1Q cleared the `W3DShaderManager.cpp` D3DX math blocker on real Windows MinGW-w64 GCC 16.2. The incremental `z_generals` build advanced to `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTreeBuffer.cpp` at 8/18 and stopped in `drawTrees()` where the legacy tree vertex-shader constant setup still used `D3DXMATRIX`, matrix multiply, and transpose helpers. The enum/floating-point diagnostics in the same file are warnings and are not part of the H1R renderer-math responsibility.

### Step 05H1R Windows continuation

H1R keeps the existing DX8 tree draw path intact but rebuilds the old transposed world-view-projection shader constant using `D3DMATRIX`, `To_Matrix4x4`, and existing `Matrix4x4` multiplication, with no D3DX dependency. The disabled pixel-shader branch is also made D3DX-free so this active source remains clean if that branch is revisited. Continue incrementally with:

```powershell
cmake --build --preset mingw64-game --target z_generals -- -j1
```

Report the first hard failure plus all actual compiler warnings. Do not translate the tree shader or tree rendering subsystem preemptively. If `z_generals` links, stop compile-only migration and move directly to the real runtime/data environment, D3D12 backend creation, first Present, and first visible Generals/UI geometry.


### Step 05H1R Windows continuation result

H1R cleared the `W3DTreeBuffer.cpp` D3DX matrix/vector blocker on real Windows MinGW-w64 GCC 16.2. The incremental `z_generals` build advanced to `Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp` at 8/14 and stopped on that file's remaining `d3dx8math.h` include. The enum/floating-point and non-standard-layout `offsetof` diagnostics emitted before the stop remain warnings and are not part of the H1S water-math responsibility.

### Step 05H1S Windows continuation

H1S removes D3DX8 math from the active Evolution water source while preserving the old inverse-view texture-coordinate transform, sea world transform, and transposed world-view-projection shader constants with `D3DMATRIX` plus existing WWMath conversions. `Vector4` replaces D3DX vector constants. The archival inline `ps.1.1` D3DX assembler remains only under `!RTS_EVOLUTION_X64`; Evolution does not restore a D3DX8 dependency merely to compile, and those shaders remain scheduled for coherent D3D12 translation when the real water caller crosses the backend seam. Continue incrementally with:

```powershell
cmake --build --preset mingw64-game --target z_generals -- -j1
```

Report the first hard failure plus all actual compiler warnings. Do not preemptively rewrite the whole water renderer. If `z_generals` links, stop compile-only migration and move directly to runtime/data setup, D3D12 backend creation, first Present, and first visible game/UI geometry.

### Step 05H1T local linker continuation

The requested `z_generals -j1` build reached the final link after H1S. The opaque `collect2: ld returned 8` failure was a linker startup failure caused by DLL lookup through an incompatible inherited `PATH`. The canonical Windows MinGW toolchain now launches C/C++ compilation and linking with its selected bin directory prepended, including fresh compiler ABI checks. Existing compiler/linker launchers are retained and repeated toolchain loading does not duplicate the environment prefix.

DirectInput imports now belong to the device source interface rather than an early executable-only link entry. WW3D declares the existing telemetry library that its statistics implementation calls. These changes address GNU library ordering without restoring a DX8 renderer dependency.

Local validation passes fresh C/C++ configure, build and execution with pre-existing launchers and the original shell environment, plus all 27 `mingw64-tests` regressions. The normal game still exposes unresolved direct DX8 renderer callers; it has not linked or produced a visible frame. Continue using:

```powershell
cmake --build --preset mingw64-game --target z_generals -- -j1
```

The full serial compile completed. The final link reports 107 distinct unresolved legacy renderer symbols; the DirectInput and telemetry symbols are resolved. The x64 warning sweep adds native-width integer setters beside the existing gadget integer getters and uses them for local menu/lobby/chat/map tags. Sabotage-frame callback transport uses `std::uintptr_t` while the frame counter remains `UnsignedInt`.

Migrate the remaining real callers through `WW3D -> IRenderBackend -> D3D12Backend`. The backend smoke test is not game boot evidence, and local validation is not user Windows sign-off.

### Step 05H1U D3D12-only continuation — 2026-10-01

The DX8 backend adapter/SDK CMake lane is retired. Full-game configuration now requires Windows x64 Evolution. D3D12 output ownership covers presentation intervals, stencil capability, screen coordinate normalization, synchronous RGBA8 capture for the existing screenshot/movie callers, and real submission counters for telemetry. Capture retains the output on the GPU before flip-discard Present; its cost is one additional output-sized GPU image and copy per submitted scene.

Run the existing gates:

```powershell
cmake --preset mingw64-game
cmake --build --preset mingw64-game --target z_generals -- -j1
cmake --preset mingw64-tests
cmake --build --preset mingw64-tests -- -j1
ctest --preset mingw64-tests
```

The local game compile clears the migrated output/statistics callers and reaches a failed final link with 98 distinct remaining legacy renderer symbols. The next actual error is `W3DProjectedShadow.cpp` calling `DX8Wrapper::Create_Render_Target`. It requires coherent off-screen resource, mesh/material, and shadow draw migration. Do not restore the old backend, drop required sources, or substitute empty drawing implementations to resolve it.

The 27 focused regressions pass. The GPU smoke asserts real dynamic/static/textured submission counts, accepts presentation intervals 0-4 and rejects 5, reads known pixel colors after deferred and immediate presentation, and resizes to 643x479 to verify RGBA order, orientation, and GPU row padding. Empty/open-scene/just-resized captures are rejected. In-game screenshot/movie files, real game boot/frame, and optional Generals/viewer targets remain separate unfulfilled runtime/build gates. Continue Step05H; Step06 SDL3 and major W3X runtime work remain deferred.


### Step 05H1AA Windows continuation — 2026-10-03

The authoritative `(4).zip` tree configured successfully on real Windows MinGW-w64 GCC 16.2 and stopped at 2/107 in `Core/Libraries/Source/WWVegas/WW3D2/shattersystem.cpp`. Seven CPU-side color conversion calls still referenced `DX8Wrapper::Convert_Color` after the wrapper implementation ownership had been retired from this path. H1AA keeps shatter behavior intact and replaces only those conversions with local renderer-neutral ARGB helpers: unpack is the established `0xAARRGGBB` channel decode and pack delegates to `Vector3::Convert_To_ARGB(alpha)`, matching the prior GCC implementation. The D3D12 policy now forbids `DX8Wrapper` from returning to `shattersystem.cpp`.

Continue on Windows with the existing configured tree:

```powershell
cmake --build --preset mingw64-game --target z_generals -- -j1
```

Use the next first hard compiler/link/runtime failure as the queue. Do not restore `DX8Wrapper` merely for color conversion, and do not mix unrelated shatter interpolation cleanup into this compile unblock.

### Step 05H1AB Windows continuation

H1AB addresses the `W3DDisplay.cpp:101: browserhost.h: No such file or directory` blocker exposed at 55/107 after H1AA. Apply the H1AB hotfix and continue without cleaning:

```powershell
cmake --build --preset mingw64-game --target z_generals -- -j1
```

Windows sign-off requires that real build to advance past both `W3DDisplay.cpp` and `W3DWebBrowser.cpp`.
