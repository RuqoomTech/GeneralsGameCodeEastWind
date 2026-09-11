# Step 03 — HD Mod Performance Foundation

## Status

**DONE — implementation complete.**

Step 03 is observational only. Telemetry must never feed wall-clock, renderer, GPU, capture, visibility, or resource measurements back into simulation, CRC, replay, network, Xfer, AI, or frame-pacing decisions.

## Completed scope

Step 03 deliberately reuses existing profiling/statistics seams instead of creating a second profiler hierarchy:

- `rts/profile.h` is the public telemetry/profiling front door;
- `Core/Libraries/Source/profile/performance_telemetry.cpp` owns capture timing and serialization;
- existing WW3D `Debug_Statistics` remains the source of draw/geometry/texture/resource counters;
- `GameEngine::update()` provides the engine/client/message/network/logic CPU phase brackets;
- `GameClient::update()` publishes a profile-build drawable visibility snapshot;
- `WW3D::Begin_Render` / `WW3D::End_Render` provide the primary render CPU bracket;
- existing Tracy support receives plots from the same sample;
- `scripts/perf-summary.py` summarizes repeatable captures without third-party Python packages.

The canonical `mingw32-profile` preset enables `RTS_BUILD_OPTION_PERF_TELEMETRY`. Normal `mingw32-release` and `mingw32-debug` builds do not compile the runtime call sites guarded by `RTS_PERF_TELEMETRY`.

## Capture control

Set:

```text
RTS_PERF_CAPTURE=<path-to-csv>
```

`RTS_PERF_CAPTURE=1` writes `RTSPerfCapture.csv` in the process working directory. Unset, empty, or `0` disables file capture. Parent directories are not created automatically.

Example:

```powershell
cmake --preset mingw32-profile
cmake --build --preset mingw32-profile --target z_generals
$env:RTS_PERF_CAPTURE = "Step03-baseline.csv"
# Launch Zero Hour and run a repeatable replay/scene.
Remove-Item Env:RTS_PERF_CAPTURE
python scripts/perf-summary.py Step03-baseline.csv
```

## Stable CSV schema v2

Step 03B upgrades the original render-only schema to one row per engine update. A row can therefore represent an update that did not render or did not advance game logic.

```text
schema_version,capture_index,render_frame,logic_frame,sync_time_ms,rendered,logic_updated,update_cpu_us,client_cpu_us,logic_cpu_us,network_cpu_us,message_cpu_us,render_cpu_us,drawable_total,drawable_visible,drawable_shrouded,draw_calls,triangles,vertices,dx8_triangles,dx8_vertices,skin_draws,skin_triangles,skin_vertices,sorted_triangles,sorted_vertices,texture_bytes,texture_count,texture_changes,lightmap_texture_bytes,lightmap_texture_count,procedural_texture_bytes,procedural_texture_count,memory_allocations,memory_frees
```

Key semantics:

- `logic_frame`: frame at `GameEngine::update()` entry;
- `rendered`: `1` when the primary WW3D render bracket ran during that engine update;
- `logic_updated`: `1` when `TheGameLogic->UPDATE()` ran during that engine update;
- `update_cpu_us`: complete `GameEngine::update()` wall-clock bracket;
- `client_cpu_us`: `TheGameClient->UPDATE()` bracket, including rendering performed from that path;
- `logic_cpu_us`, `network_cpu_us`, `message_cpu_us`: corresponding explicit engine phases;
- `render_cpu_us`: primary WW3D render bracket only;
- `drawable_total`: current client drawable list size;
- `drawable_visible`: drawables not shrouded for which at least one `DrawModule` reports visible;
- `drawable_shrouded`: drawables fully obscured by the existing shroud state;
- drawable fields are a client visibility proxy, **not** a GPU/frustum/occlusion result;
- draw/geometry/texture/resource fields retain the Step 03A WW3D `Debug_Statistics` semantics.

Capture still uses buffered I/O and periodically flushes. Texture capture can deliberately add overhead because it temporarily enables the existing simple texture-accounting mode when no debug texture mode is already selected. Compare only equivalent capture configurations.

## Tracy plots

With Tracy enabled, the same sample publishes engine/update/render/resource plots including:

- `Update.CPU.us`;
- `Update.Client.us`;
- `Update.Logic.us`;
- `Update.Network.us`;
- `Render.CPU.us`;
- render draw/triangle/vertex/texture/allocation counters;
- total/visible/shrouded drawable counts.

No parallel counter-collection implementation exists for Tracy.

## Automated summary

`scripts/perf-summary.py` validates schema v2 and reports:

- sample/render/logic-update counts;
- mean/p50/p95/p99/max CPU phase timings;
- mean/max visibility, draw, geometry, texture, and allocation counters;
- optional machine-readable JSON via `--json`.

This is intentionally a summary tool, not a benchmark controller. Replays/scenes and machine conditions must still be kept comparable by the operator.

## Test coverage

`performance_telemetry_step03a` remains the historical CTest name but now protects the completed Step 03 schema v2. It verifies:

- explicit capture start/stop;
- stable schema and field count;
- update/render/logic state markers;
- drawable visibility serialization;
- derived triangle/vertex totals;
- render/resource counters.

The production capture implementation remains renderer-independent in the focused test graph.

## Deferred measurements

Step 03 is complete without adding throwaway Direct3D 8 GPU-query infrastructure. Native GPU timestamp/pass telemetry belongs to the D3D12 renderer where command queues and timestamp heaps can expose it correctly. Process/allocator memory diagnostics are owned by the x64/memory migration rather than duplicated here. Asset-hotspot reporting belongs with the W3D/W3X/texture/geometry pipeline steps where resource identity is available.

This boundary is intentional: Step 03 now provides enough CPU/render/resource evidence to guide later work without delaying the x64 migration for telemetry that would be discarded with the legacy renderer.
