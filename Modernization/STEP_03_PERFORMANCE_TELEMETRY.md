# Step 03 — HD Mod Performance Foundation

## Status

**ACTIVE. Step 03A implemented locally; Windows profile-build/runtime capture validation is required.**

Step 03 is observational only. Telemetry must never feed wall-clock, renderer, GPU, or capture results back into simulation, CRC, replay, network, Xfer, AI, or frame-pacing decisions.

## Step 03A — Render-frame telemetry foundation

Step 03A deliberately reuses the existing profiling/statistics seams instead of creating a second renderer statistics system:

- `rts/profile.h` remains the public profiling front door and now owns the small `PerformanceTelemetry` API;
- `Core/Libraries/Source/profile/performance_telemetry.cpp` owns capture/timing/serialization;
- existing `Debug_Statistics` remains the source of legacy draw, geometry, texture, and resource counters;
- `WW3D::Begin_Render` / `WW3D::End_Render` provide the primary onscreen render-frame bracket;
- existing Tracy support receives plots from the same telemetry sample when Tracy is enabled.

The normal Release path is unchanged. The render hook is compiled only when `RTS_BUILD_OPTION_PERF_TELEMETRY=ON`, which defines `RTS_PERF_TELEMETRY`. The canonical `mingw32-profile` preset enables this option; `mingw32-release` and `mingw32-debug` do not.

## Capture control

CSV capture is runtime opt-in. In a telemetry-enabled build, set:

```text
RTS_PERF_CAPTURE=<path-to-csv>
```

`RTS_PERF_CAPTURE=1` uses `RTSPerfCapture.csv` in the process working directory. An unset, empty, or `0` value leaves file capture disabled. Parent directories are not created automatically.

Example PowerShell workflow:

```powershell
cmake --preset mingw32-profile
cmake --build --preset mingw32-profile --target z_generals
$env:RTS_PERF_CAPTURE = "Step03A-baseline.csv"
# Launch the profile build through the normal Zero Hour runtime/install workflow.
Remove-Item Env:RTS_PERF_CAPTURE
```

Capture uses buffered I/O and flushes every 120 samples plus at shutdown. The capture path is intended for controlled benchmark/profile runs, not for normal gameplay builds.

## Stable CSV schema v1

Each successfully completed primary WW3D render frame emits one row:

```text
schema_version,capture_index,render_frame,sync_time_ms,render_cpu_us,draw_calls,triangles,vertices,dx8_triangles,dx8_vertices,skin_draws,skin_triangles,skin_vertices,sorted_triangles,sorted_vertices,texture_bytes,texture_count,texture_changes,lightmap_texture_bytes,lightmap_texture_count,procedural_texture_bytes,procedural_texture_count,memory_allocations,memory_frees
```

Semantics:

- `render_frame`: monotonically increasing WW3D render-frame index for the process;
- `sync_time_ms`: WW3D synchronized logic/render time at frame start; this is observational metadata and is not used to drive simulation;
- `render_cpu_us`: wall-clock duration from the accepted `WW3D::Begin_Render` path through `WW3D::End_Render`, including texture-loader update, main rendering, backend end-scene/present work, and statistics finalization;
- draw/geometry fields: existing legacy WW3D counters, including the current Zero Hour aggregate treatment of render-target geometry;
- texture fields: unique textures/resource bytes observed through existing `Debug_Statistics` texture accounting;
- `memory_allocations` / `memory_frees`: WW3D memory-log operation counts for the previous accounting interval, not process resident bytes.

When CSV capture is active and no debug texture-recording mode is already selected, Step 03A temporarily selects the existing `RECORD_TEXTURE_SIMPLE` mode for the primary render frame so texture count/byte fields are populated, then restores `RECORD_TEXTURE_NONE`. This adds deliberate profiling overhead; benchmark comparisons must use the same capture mode on both sides.

## Tracy integration

If `RTS_BUILD_OPTION_PROFILE_TRACY=ON` is also enabled, the same Step 03A sample publishes:

- `Render.CPU.us`;
- `Render.DrawCalls`;
- `Render.Triangles`;
- `Render.Vertices`;
- `Render.TextureBytes`;
- `Render.TextureChanges`;
- `Render.MemoryAllocations`;
- `Render.MemoryFrees`.

No separate counter collection path is introduced for Tracy.

## Test coverage

`performance_telemetry_step03a` is part of the focused modernization CTest graph. It verifies:

- explicit capture start/stop behavior;
- stable CSV schema v1;
- stable field count;
- frame identity serialization;
- derived total triangle/vertex counts;
- render/resource counter serialization.

The test compiles the production capture implementation directly and has no renderer/DirectX dependency.
Final local sealing also runs the capture path under GCC and Clang at `-O0`, `-O2`, and `-O3`, plus GCC AddressSanitizer and UBSan. A separate telemetry-enabled focused configure verifies that `RTS_PERF_TELEMETRY` propagates when requested, while a normal Release focused configure verifies that it is absent.

## Step 03A boundaries / remaining Step 03 work

Step 03A does **not** claim complete Step 03 coverage. Remaining work should be driven by captured evidence and includes:

- GPU timestamp timing where the current backend can support it safely, and later native D3D12 timestamps;
- full client/update/scene-traversal CPU phases beyond the WW3D primary render bracket;
- visibility/culling counts exposed in a stable capture schema;
- process/allocator resident and peak memory telemetry (coordinated with Step 04 rather than duplicated);
- mesh/texture asset hotspot reports and largest-resource lists;
- benchmark/replay capture automation and summary tooling;
- pass-level draw-call categorization as the renderer boundary matures.

Do not add optimization patches merely because a counter exists. Establish B0/B1 and heavy-mod benchmark captures first, then use the measurements to choose the next bottleneck.
