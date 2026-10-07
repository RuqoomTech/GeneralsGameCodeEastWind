# Current Source State

## Real monochrome filter and scene capture - 2026-10-07

The verified source parent is user commit `5a4b16b002d6b9607820da62662129b5983b3d74`. On 2026-10-07 the user authorized ongoing local commits of verified changes; the coordinator commits this continuation batch after validation. No GitHub pushes. The reusable agent prompt includes the updated commit authorization and latest build queue.

The unreachable default screen-filter RTT blit/class/registration is retired; default setup acceptance and ordinary direct scene rendering remain. Actual ScreenBWFilter now captures through a TextureClass-owned neutral render texture and composites through WW3D/IRenderBackend/D3D12. Canonical HLSL preserves monochrome.nvp RGB = lerp(source, luminance*tint, fade), with luminance = dot(source.rgb, .3/.59/.11) and alpha = luminance independently of RGB fade. White/red/green tint, original frame-count fade cadence/final fade-out draw, opaque blend, always depth/no depth writing, linear clamp/no mip sampling, normal RGBA writes and tactical viewport UVs remain. D3D12 pixel edges replace obsolete DX8 half-pixel offsets. Capture ownership retries invalid dimensions/resources and validates backend generations; saved output state is restored before post-render failure exits. BW registers before legacy manager RTT initialization failures. The obsolete DOT3 hardware fallback and four paired monochrome/inverse assembly files are retired. Other active effects and their native initialization remain genuine unfinished owners.

The existing render-texture seam adds explicit full-output capture mode, allowing target changes inside an open frame with the shared output D24S8 depth/stencil surface. Target barriers, PSO depth formats/validation, depth clears, decals, selected-target lifetime and camera/viewport restoration follow actual attachment state. Ordinary color-only projected-shadow behavior remains unchanged, including its between-scene restriction. Texture generations and replaced descriptor heaps retain existing fence ownership. Material constants grow from 8 to 12 DWORDs; total root signature budget is exactly 64 DWORDs. All additions are runtime rendering state, outside simulation/serialization/hash/network representations.

Validation: focused build passes 20/20 actions. Non-browser regressions pass 31/31, production GPU 4.70 seconds, total 20.52 seconds (`build/monochrome-regressions.txt`). Updated retirement policy separately passes 1/1. Eleven paired terrain/display/shroud/bridge/shader owners pass MinGW syntax checks (`build/monochrome-syntax.txt`). GPU pixels cover white/red/green tint, fade 0/.5/1, luminance alpha, RGBA/RGB masks, shared-depth rejection before and after target restoration, in-frame capture creation, invalid dimensions/self-sampling, nonidentity camera restoration, exact tactical pixel bounds, and alternating single-pixel columns that detect half-texel blur. Independent reviews found no concrete introduced capture/lifetime/shader defect. Existing whole-game warnings remain; no suppression is added. Native browser is excluded; its earlier controller failure is not a current full-suite pass.

Exact `cmake --build --preset mingw64-game --target z_generals -- -j1` completes compilation and fails at final link (156 actions; `build/mingw64-game/monochrome-game-build.txt`). Unresolved symbols remain 54, none added; printed references fall from 200 to 194 (`build/monochrome-link-summary.json`). First actual function is ScreenMotionBlurFilter::set(FilterModes), W3DShaderManager.cpp:819, Apply_Render_State_Changes. Counts do not include references hidden by linker summary messages. Remaining native shader/resource owners are not stubbed or excluded.

Next is coherent motion-blur capture/composite migration: retain the captured frame on skip-render views, exact zoom/pan UV arithmetic, normal versus additive overlay blend/order/count, GameLogic-frame versus rendered-view cadence, and vertex-selected alpha independently of capture alpha. Its soft-water path clears only tactical alpha (quantized minimum opacity), preserves frozen RGB, then writes RGB; full-target clearing would lose behavior. Keep the CPU camera/cadence decisions. Crossfade and paired water shroud/mask dispatch remain genuine later responsibilities. Consolidate shared capture ownership in the existing manager as more filters migrate; do not create a parallel framework. Textured Render2D still gates actual UI visibility. SDL3 Step06 and major W3X runtime remain deferred.

Fresh baseline reconstruction matches 4539 candidate/reconstructed files byte-for-byte with zero missing, extra or different files (`build/monochrome-patchcheck.txt`). Scoped git diff --check passes. No linked/booted game, normal-game backend creation, visible game frame, complete DX8 retirement, measured speedup, sealed milestone or user Windows sign-off is established. Earlier sections retain historical receipts only.

## Water tracks, diagnostics, and agent prompt - 2026-10-07

Current verified user HEAD is `5a4b16b002d6b9607820da62662129b5983b3d74` plus local continuation changes. No agent commits or GitHub pushes. `Modernization/AGENT_CONTINUATION_PROMPT.md` is refreshed for Claude Code/Codex with actual receipts, locked architecture, remaining queue and safe parallel-agent ownership.

Water tracks now use WW3D/backend terrain material drawing with real wave/shroud textures and normalized samplers. Original CPU wave timing/motion, four-vertex topology (strip diagonal 0-3), UVs, white RGB/fade alpha, ordering, underwater height, reflection exclusion and full RGBA shroud projection remain. GPU constants perform texture projection. Failures retry while wave animation still advances. Saved projection is restored. Native buffers, locks, draws and obsolete resource lifecycle hooks are removed. Backend depth bias is explicit and cached independently in material PSOs; -8 D24 units preserves the near-pull intent without claiming exact old-driver equivalence. Game appearance remains unverified.

Unused native state-label helper declarations/definitions are retired; snapshots retain numeric values in existing WWDebug. Unused nonvirtual CloudMapTerrainTextureClass::restore is removed after no-caller searches and object relocation tracing, preserving texture generation and Apply behavior. TerrainTex now emits no native linker references. Remaining shader/water/resource responsibilities are genuine and must migrate coherently.

Validation: focused test compilation passes; latest non-browser regressions pass 31/31 (production GPU 4.54 seconds, total 16.32 seconds; `build/agent-prompt-regressions.txt`). GPU cases cover wave RGBA/shroud/alpha blending, projected coordinates, winding, depth-bias PSO isolation and disabled depth writes. Water-track focused syntax passes. Earlier eleven-owner paired syntax checks remain historical receipts. The unchanged browser failed controller creation in an earlier accidental full-suite run; no current full-suite browser pass is claimed.

Exact game build completes compilation and fails at final link: `build/mingw64-game/cloud-restore-game-build.txt`, 54 unique unresolved symbols / 200 printed references, none added versus diagnostic cleanup. Its first failure is W3DShaderManager.cpp:294/298, DX8Wrapper::Apply_Render_State_Changes. TerrainTex references disappeared, but the symbols remain in other owners. Prior diagnostics reduced 56 to 54 symbols and 378 to 200 printed references; prior water-track migration reduced 58 to 56 and 441 to 378. Hidden linker-summary repetitions are not counted. Next owner is actual shader dispatch and its remaining effects, followed by real native resources and textured Render2D visibility. Do not delete live flush/state behavior before dependent callers migrate.

Read-only late-water review permits narrow verified tessellation/UV reuse only. Its wholesale implementation drops authored bump/reflection/layers, changes a strip diagonal, replaces destination-alpha feathering, and ignores failures. Keep current complete shader/material/resource contracts. SDL3 Step06 and major W3X runtime remain deferred. No game link, boot/backend creation, visible game frame, complete DX8 removal, measured speedup, sealed milestone or user Windows sign-off is established.

Fresh reconstruction from baseline `2c81f9922beb5c5379c6d695c0e14ea3f15f55b3` matches all 4543 candidate/reconstructed files byte-for-byte, with zero missing, extra or different files (`build/agent-prompt-patchcheck.txt`). Scoped git diff --check passes. Earlier receipts below cover their earlier sources only.

## Foreground terrain and reusable agent handoff - 2026-10-07

Current verified user HEAD is `5a4b16b002d6b9607820da62662129b5983b3d74`. User commits may advance concurrently. Agents have made no commits or GitHub pushes. The reusable Claude Code/Codex prompt is `Modernization/AGENT_CONTINUATION_PROMPT.md`; verify live source and receipts before using its snapshot.

Foreground terrain now retains dirty persistent GPU tiles. CPU tessellation and static/dynamic lighting keep their original cadence and baseline; camera/world and cloud/noise projections use shader constants. Both texture stages sample the actual terrain atlas with independent authored UVs and samplers. Canonical HLSL performs vertex-alpha atlas interpolation followed by full RGBA diffuse and optional cloud/noise. Explicit derived shroud/mask passes and wireframe clear-line state preserve depth, blend, color masks and camera restoration. Failed uploads/draws report failure and invalid resources retry. The unused optimized lighting duplicate, native vertex/index ownership, blank native atlas alias, terrain/cloud shader registrations and six obsolete assembly assets are retired. Paired WorldBuilder references follow the CPU geometry owner, but no full WorldBuilder build/visual result is claimed.

Paired display code now uses actual monitor mode enumeration, backend gamma/readiness and supported CPU video texture formats. Public shroud headers explicitly include their filter declaration. The production root signature uses 60 DWORDs; explicit projection/color constants match the HLSL layout. GPU checks cover same-atlas stage binding, distinct UV/mip filtering, nonidentity transforms, interpolation alpha, resource lifetime, persistent reuse, and solid/wireframe/constant-color switching. This reduces repeated geometry transformation/uploads structurally; runtime speedup is not measured.

Validation: focused compilation passes; 31/31 non-browser regressions pass, including GPU smoke in 4.28 seconds (14.18 seconds total). Eleven terrain, paired display/shroud/bridge owners pass focused MinGW syntax checks. Receipts: `build/foreground-regressions.txt`, `build/foreground-syntax-final.txt`. A later non-browser run also passes 31/31 (GPU 4.24 seconds, total 10.59 seconds; `build/radar-regressions-nonbrowser.txt`). An accidental full-suite run used a case-sensitive exclusion that did not match the native browser test: that test failed controller completion with HRESULT `800700aa`; the other 31 passed (`build/radar-regressions.txt`). The browser owner is unchanged; its older separate native pass is historical, not a current full-suite pass.

The exact game build exposed radar native capability selection, a projected-shadow header dependency, and view native clearing in sequence. Radar now preserves first-choice CPU RGB24 terrain and ARGB32 overlay/shroud formats, converted by existing TextureClass/SurfaceClass ownership. Both projected-shadow headers include `lightenvironment.h`; view depth/stencil clearing uses the existing backend. The latest exact serial build, `build/mingw64-game/foreground-integration-build.txt`, compiles these fixes and exits 1 at `W3DWaterTracks.h:51`, undeclared `DX8VertexBufferClass`, followed by its genuine native buffer/draw operations. Water-track migration is the next actual queue; do not restore native buffer includes. Earlier post-terrain link evidence was 58 unique unresolved symbols and 441 printed references; current compilation failure cannot establish a new link count. Actual radar/video/UI visibility remains pending textured Render2D caller migration; CPU texture uploads alone do not establish it.

Fresh reconstruction from baseline `2c81f9922beb5c5379c6d695c0e14ea3f15f55b3` matches all 4543 candidate/reconstructed files byte-for-byte, with zero missing, extra or different files (`build/foreground-handoff-patchcheck.txt`). `git diff --check` passes. No linked game, boot/backend creation, visible game frame, complete DX8 removal, sealed milestone or user Windows sign-off is established. SDL3 Step06 and major W3X runtime work remain deferred; historical receipts below describe their own earlier candidates.

## Persistent roads and terrain extra blends - 2026-10-06

Both real road owners now cache persistent backend geometry per road type. Native road buffers, locks, device formats, draw calls and dead dynamic-light drawing fragments are removed from their four files. Existing tessellation, terrain conforming, road UVs, packed diffuse, stacking/type order, variant-specific visibility and aggregate limits remain. A 16-bit index limit prevents truncation. Counts are initialized, and alpha packing uses unsigned shifts. Content/lighting dirtiness is now independent of visibility dirtiness, fixing stale lighting when the view does not change. Pending rebuilds survive material/texture preparation failure; failed uploads keep per-type GPU dirtiness for retry. Unchanged frames reuse geometry; world/cloud/noise coordinates use GPU constants.

The actual third-texture terrain blend caller shares this route. Its existing visible-tile selection, per-corner alpha/static diffuse, cliff flip, winding, capacity/growth and white debug mode are retained. It uses transient neutral geometry because its visibility output is still rebuilt per call; capacity is reserved and only selected vertices/indices are initialized. Camera state is restored on every exit. This does not claim that all terrain geometry is now persistent.

Canonical HLSL now preserves the caller-driven dual-layer road order `base * cloud * noise * diffuse`; base and single-layer modes retain diffuse before optional layers. Full RGBA modulation, alpha blending, LEQUAL/no depth writing, engine cull inversion, RGB-only writes, actual authored mips and distinct point-min/linear-mag noise filtering remain explicit. Base mip override applies only in dual-layer mode; dual noise retains its texture filter, while single noise honors the original global mip override. Native road shader classes, registrations and enums are retired only after both roads and extra-blend callers stop using them. Six obsolete flat/road assembly shader sources and the remaining Generals CMake entry are removed. Foreground terrain's different vertex-alpha atlas blending remains a genuine pending shader responsibility.

Validation: strict focused build passes; 31/31 non-browser tests pass (production GPU smoke 4.11 seconds, 14.35 seconds total), covering both terrain/road shader ordering, layer alpha blending, persistent reuse and resource lifetime. Both road variants and shared terrain/shader dispatch pass focused MinGW syntax checks. Read-only reviews validated variant-specific cache behavior and found two extra-blend sampler corrections; those corrections are applied and rechecked. The exact serial `z_generals` build compiles all changed sources and fails at executable linking. Its corrected rerun recompiles `HeightMap.cpp` and reaches the same link. Unique unresolved symbols remain 59, with none added; printed reference occurrences decrease from 567 to 516. First failure: `FlatHeightMap.cpp:588`, post-terrain `DX8Wrapper::Apply_Render_State_Changes()`. Transcripts: `build/mingw64-game/roads-extra-blends-game-build.txt`, `build/mingw64-game/roads-extra-blends-game-recheck.txt`, `build/roads-extra-blends-link-summary.json`, `build/roads-extra-blends-regressions.txt`, `build/roads-extra-blends-syntax.txt`.

Fresh baseline reconstruction from `2c81f9922beb5c5379c6d695c0e14ea3f15f55b3` is checked against all 4548 remaining candidate files; the receipt is `build/roads-extra-blends-patchcheck.txt`. All agent changes remain local without commits or GitHub pushes. No linked normal game, boot/frame, measured runtime speedup, complete DX8 removal, sealed milestone or user Windows sign-off is claimed.

Next evidence-backed owners: remaining post-terrain scorch/bridge/waypoint state; foreground terrain's `lerp(base, blend, vertexAlpha)` shader and dirty tile uploads; native resource callers; and the old D3DX error-string logger, which can consolidate into existing WWDebug result diagnostics without stubbing resources. SDL3 and W3X runtime remain deferred to normal game boot/frame.

## Persistent terrain and bib continuation - 2026-10-06

The tested candidate is `main` at user commit `127198b1781280b96b2be893aa150d2dbd1bb918` plus the local bib, dispatch, test and policy changes. User commits may advance concurrently. No agent commits or GitHub pushes were made.

The actual flat terrain tiles now use persistent default-heap geometry. CPU tessellation, static lighting, packed diffuse, culling and authored tile UVs remain in their existing owner. Geometry is rebuilt/uploaded only after content changes or resource invalidation. Canonical HLSL applies the world transform and world-XY shroud/cloud/noise projection from per-draw constants; the previous per-frame CPU vertex transformation/projection loop and duplicate vertex array are removed. The preferred original `fterrain*.nvp` single-pass RGBA multiplication, optional layers, samplers, LEQUAL/depth writing, culling and RGB-only mask remain explicit. Both superseded `FlatTerrainShader` implementations and registrations are retired. Unmigrated roads, bridges and waypoints still require their genuine post-terrain state responsibilities.

The backend supports persistent terrain and material draws through the same resource pool and shared draw-state helpers. Uploads during an open scene use a separate command list on the same queue. Geometry handles become invalid immediately on release; buffers and slots remain retained until the referencing frame fence completes. Global geometry generations prevent stale handles from becoming valid after backend recreation. Runtime upload counters remain observational and downstream of simulation.

Paired Generals/Zero Hour bib owners now use separate persistent normal/highlight batches, actual authored textures and normalized samplers. Original aggregate capacity checks, stacking order, UVs, winding, packed ambient-plus-diffuse lighting, alpha blending, always depth comparison, disabled depth writes and RGB-only mask are preserved. Bib/highlight/lighting changes and stale handles invalidate the cache. Dirty state is cleared only after both batches upload successfully. Saved camera state is restored on every draw exit. Native buffer headers, locks and DX8 calls are removed from all four bib files.

Validation: strict focused build passes; 31/31 non-browser tests pass, including the production GPU smoke in 4.19 seconds (21.16 seconds total). New GPU cases verify world/projection constants, moving cloud and camera reuse with zero additional geometry uploads, independence from mutated source vertices, rejected invalid indices/geometry kinds, safe release and replacement inside a pending frame, and persistent bib material blending with preserved destination alpha. Both bib variants pass focused MinGW syntax checks. Read-only agents found no introduced terrain/backend/bib correctness defect. Existing full-game and no-PCH legacy warnings remain; no warning suppression was added. The unchanged browser owner retains its separate earlier 1/1 native smoke receipt.

The exact `cmake --build --preset mingw64-game --target z_generals -- -j1` compiles all updated game sources and fails at executable linking. Unique unresolved symbols remain 59, with no additions; reference occurrences decrease from 578 to 567. The first failure is `FlatHeightMap.cpp:594`, `DX8Wrapper::Apply_Render_State_Changes()`, in the remaining post-terrain dispatch. Logs: `build/mingw64-game/gpu-terrain-bibs-game-build.txt`, `build/gpu-terrain-bibs-link-summary.json`, `build/gpu-terrain-bibs-regressions.txt`, `build/gpu-terrain-bibs-paired-syntax.txt`. No current game link, boot, visible game frame, complete DX8 removal, performance speedup measurement, sealed milestone or user Windows sign-off is claimed.

## Tree and shroud continuation - 2026-10-05

The current checkout remains user commit `87ac65056fd6fa20e3247ab71bb96682b7b96344` plus local changes. The user prohibits GitHub pushes.

The real tree caller now submits CPU batches through `IRenderBackend` material draws. Native vertex/index buffers, assembly shader handles, declarations and tree device calls are removed from the owner and its header. Atlas pixels use retained CPU surfaces, the existing mip generator, explicit BGRA packing and backend mip selection. The original lighting, UV layout, winding, visibility, shadows, push/topple calculations and fixed-width Xfer fields remain. The `Trees.nvv` position and diffuse operations run on the CPU before the ordinary backend vertex shader: sway is applied every render, RGB darkening preserves alpha, the world transform is explicit, and shroud UVs use the unswayed position. Trees that do not fit the geometry capacity are no longer published to the update buffer. Enum-to-float conversions are explicit without changing arithmetic order. The optional atlas debug draw uses the same backend seam; the disabled archival DX8 statistics gate is retired from the tree dispatch.

The existing material path now binds a second texture and independent sampler for RGB-only shroud modulation in canonical HLSL. Primary texture alpha, alpha testing, blend/depth state and subsequent unlayered draws remain explicit. Secondary resources receive the same generation and self-sampling checks as primary resources. Sampler descriptors retain stable slots in a bounded cache; minimum mip selection is owned by the existing `TextureFilterClass` and uses sampler LOD rather than native texture state. This also handles the plain `TextureClass` atlas-overflow fallback without a derived-object cast. GPU pixel checks cover distinct UV sets, wrap/clamp and point/linear filtering, zero shroud alpha, alpha-test thresholds, mip selection, interleaved unlayered drawing and invalid/stale/self-sampled resources. Both production shader-state tests also exercise filter default/copy/secondary-stage mip state.

Paired Generals/Zero Hour shroud owners now retain `SurfaceClass` CPU storage and copy visible rectangles/borders to their actual destination texture surfaces. No pointer survives a native unlock. Existing fog values, interpolation, timing, projection bounds and input-to-simulation behavior are unchanged. Native shroud surface allocation/copy calls and obsolete native member types are removed. This prepares actual tree shroud sampling; other terrain/material callers still need their full rendering migrations.

Validation: the focused graph builds; 31/31 non-browser tests pass (GPU smoke 3.56 seconds), and the native browser test passes separately in 1.89 seconds with approved profile/controller access. Focused source checks pass for the tree (including the optional atlas debug path), terrain dispatch and both shroud variants. A read-only agent audit found no introduced ownership, shader or determinism defect. The latest exact `cmake --build --preset mingw64-game --target z_generals -- -j1` compiles the updated game sources and reaches the executable linker, failing with 59 unique unresolved legacy symbols, down from 61, with none added. Removed symbols are the old vertex/pixel shader handle globals. Transcript: `build/mingw64-game/tree-shroud-game-build.txt`; comparison: `build/tree-shroud-link-summary.json`. Fresh reconstruction from `2c81f9922beb5c5379c6d695c0e14ea3f15f55b3` matches all 4554 candidate files byte-for-byte, with zero missing, extra or different files (`build/tree-shroud-patchcheck.txt`). The first current blockers are `FlatHeightMap` lighting/render state and actual terrain draws. Their migration must preserve terrain texture layers, cloud/noise/shroud projection and material masks with real draw consumers. No current linked game, boot, visible frame, complete DX8 removal, sealed milestone or user Windows sign-off is claimed.

## CPU texture ownership and shoreline continuation - 2026-10-05

Current checkout is user commit `87ac65056fd6fa20e3247ab71bb96682b7b96344` plus local changes. All work remains local; the user prohibits GitHub pushes.

`SurfaceClass` now owns CPU pixels. File surfaces reuse the existing RGBA decoder, preserving Surface/BitmapHandler BGRA storage. Copies, scaling and locks update a local revision; overlapping copies read a snapshot and invalid rectangles are rejected. Backend upload expands packed 16-bit components to their normalized range, forces XRGB alpha opaque, and preserves alpha-only RGB as zero. The one-byte R3G3B2 writer no longer overwrites its neighbor. A native import bridge remains for staged callers, without creating native surfaces.

Regular 2D `TextureClass` retains the exact CPU surfaces returned to procedural writers and all decoded authored mip levels. Changed revisions trigger a new generation-safe backend upload; GPU render targets remain GPU-owned, with real dimensions and mip memory accounting. Cube, volume, depth, native loader/fallback and legacy Apply paths still require their complete caller migrations. CPU ownership alone does not complete shroud, mouse or profiler paths that still request native surfaces.

Both original shoreline loops now submit CPU geometry through the material backend, preserving sorted continuation, visibility, winding, depth comparison, LUT UVs and alpha-only coverage writes. D3D12 material PSOs support per-channel write masks. The font atlas copy now uses CPU SurfaceClass rather than native DX8 rectangles. Water's consumption of shoreline alpha remains staged with its full material migration.

Terrain no longer inherits or registers the retired native device-reset cleanup hook. Its explicit resource methods and destructor ownership remain. D3D12 output resize retains asset resources; GPU checks confirm an uploaded texture survives resize and is sampled correctly, with a render texture remaining valid too. Terrain readiness now queries the actual backend.

Local validation: 31/31 non-browser regressions pass, including production GPU pixel checks for all sixteen channel masks and rejected invalid masks; the real native browser regression passes separately with approved controller/profile access. CPU tests cover BGRA/RGBA conversion, glyph alpha, overlapping copies, scaling, revision updates and the packed-pixel sentinel. The latest exact serial `z_generals` command compiles the changed sources and fails at the executable link with 61 unique unresolved legacy symbols, down from 64, with no newly introduced symbols. Transcript: `build/mingw64-game/terrain-lifecycle-game-build.txt`. The remaining queue begins with terrain/tree DX8 draw-state ownership, then actual terrain drawing. Fresh reconstruction from `2c81f9922beb5c5379c6d695c0e14ea3f15f55b3` matches all 4554 candidate files byte-for-byte, with zero missing, extra or different files. No current game link, boot, visible frame, sealed milestone or user Windows sign-off is claimed.

## Step05H continuation and late branch reuse - 2026-10-05

Current reviewed baseline is `2c81f9922beb5c5379c6d695c0e14ea3f15f55b3`. The normal `cmake --build --preset mingw64-game --target z_generals -- -j1` invocation compiled the current game graph and reached the executable link; it failed with 65 unique unresolved legacy renderer symbols. This is compile evidence only. No current game boot, visible frame, or user Windows sign-off is recorded here.

`late` is fourteen commits ahead of that baseline. Its historical `build/zlink16.log` records a normal game link, but does not pin an invocation, source SHA, or boot/frame result. Review found behavior losses, so the branch is being reused by responsibility rather than merged wholesale.

Selected reuse in the working candidate:

- Paired box render objects submit the original eight vertices and twelve triangles through the current material backend. Axis-aligned boxes retain translation-only positioning; oriented boxes retain full transforms. The original white-emissive material is preserved as prelit vertex RGBA. Generals retains its CPU triangle sorting queue; Zero Hour retains immediate submission.
- Paired DDS constructors decode fixed DXT1-5 FourCC identifiers on the CPU; file layouts and native overloads still needed by staged callers remain intact.
- Packed ARGB conversion is consolidated in the existing `ww3dformat` owner for mesh material preprocessing and shatter interpolation. It retains channel shifts and the established WWMath quantization.
- Texture format selection describes actual CPU conversion support rather than querying the retired native device. Unsupported signed bump formats are rejected for their future complete material migration.
- Terrain tracks retain generated edges, time/distance fade, module order, authored textures/sampler, per-module transforms and shared strip topology. Native buffers and their obsolete member names are retired; rejected draws are reported.
- The native browser now observes actual navigation completion. Request supersession, redirect IDs, weak callback ownership and handler removal preserve lifetime safety. Real missing-file failures retain Failed/HRESULT instead of reporting success when the request is merely accepted.

Local verification: the focused graph builds successfully; 31/31 non-browser tests pass including the actual D3D12 GPU smoke, and the native browser smoke passes separately with approved access to its profile/controller. Both game variants' changed WW3D source files pass focused MinGW syntax checks. The normal `z_generals` rerun compiles the candidate and fails at the executable linker with 64 unique unresolved legacy renderer symbols, no new symbols relative to the 65-symbol baseline. Game boot/frame and Windows user sign-off remain unproven.

Outstanding repairs before larger `late` imports:

| Owner | Evidence requiring repair |
| --- | --- |
| Texture/surface | Detached procedural surfaces never upload font/video/radar writes; RGBA/BGRA is inconsistent; mip queries and generation report incomplete results. Cube/volume/depth placeholders do not preserve resource behavior. |
| Shader manager/scene | Stored shader state has no draw consumers; cloud/shroud/noise/monochrome logic is omitted; scene stencil marking is removed and masks exceed the backend's eight-bit contract. |
| Smudge/tree/water | Smudge indices are rebased twice and background capture is unpopulated; ordinary tree breeze can freeze; water effects and matrix conventions need complete migration. |
| Terrain/roads/bridges | CPU topology can be reused, but secondary UVs, blend stages, cloud/shroud and lighting must reach actual submissions. |

Scorch and bibs are promising subsequent CPU geometry ports after their texture, transform and failure-propagation contracts are verified. SDL3 remains Step06 after normal D3D12 game boot/frame; W3X runtime work remains deferred to the same gate.

## Historical baseline through Step05H1N

The remaining baseline notes record verified source facts through the Step 05H1N point-orientation/runtime-pointer/ownership candidate on 2026-09-19. Steps 05D through 05G are Windows-signed-off. Step 05H is the active normal-game x64 migration: successive real `mingw64-game` builds have cleared WWSaveLoad pointer identity, crash/debug diagnostics, registry/native-handle width, WWMath D3D8 leakage, profiler/debug pointer identities, compression pointer arithmetic, and now the first active D3DX8 dependency cluster. H1L is locally sealed only; Windows sign-off requires the real `z_generals` build to advance past the updated WW3D asset/name paths.

## Build system

- Top-level project uses CMake 3.25+; modern project code uses C++20 through `core_config` while the standalone W3X characterization targets remain C++98-compatible.
- VC6 and existing MSVC preset families remain available as historical/comparison paths.
- The supported MinGW/Ninja modernization lane is x86_64-only. `mingw64-tests` remains the focused regression preset; Step 05H1 adds `mingw64-game` for the normal Zero Hour `z_generals` migration graph. Step 04F removed the former `mingw32-*` and `mingw-w64-i686*` presets rather than preserving dormant aliases.
- `Core/Tests/CMakeLists.txt` integrates W3X, determinism, ABI, runtime and Evolution protocol/session coverage using permanent subsystem-oriented test names.
- C++-only `-Wsuggest-override` and MinGW compatibility/link settings are target-scoped through `core_config` rather than globally leaking into C/vendored targets.
- Direct `FetchContent_Populate()` use in the ReactOS ATL, legacy zlib, and LZHL source-only paths has been removed.
- The MinGW toolchain validates the `x86_64-w64-mingw32` triplet, supports `RTS_MINGW_ROOT`, and shares its resolved bin path with WIDL/debug-strip discovery. WIDL supports explicit root/include overrides and is not required for the focused test graph.
- Full-game MinGW runtime configuration requires WIDL before populating runtime FetchContent dependencies; the focused `mingw64-tests` graph intentionally avoids those unrelated full-runtime dependencies. Step 05H1 deliberately exposes the full `z_generals` graph through `mingw64-game` so remaining renderer blockers are discovered from the real executable rather than a synthetic app.
- Generals and Zero Hour install rules use `rts_install_runtime_target()` instead of repeating MSVC-only PDB generator expressions. MSVC keeps optional PDB installation; MinGW Release installs the `.debug` sidecar emitted by the existing strip workflow.
- MinGW toolchain discovery is x86_64-only through `mingw-w64-common.cmake` plus the canonical x86_64 wrapper. `mingw64-tests` configures the focused regression graph, including the Windows x64 D3D12 backend smoke test; `mingw64-game` configures the normal Zero Hour executable with tools disabled and the same Evolution x64 policy.
- The former CMake hard gate that blocked the monolithic x64 game runtime has been removed in Step 05H1. This does not imply the game already links: remaining direct renderer/platform dependencies are now intentionally exposed by building the normal `z_generals` target. There is no parallel Evolution application tree.
- Step 04, Step 05A and Steps 05C2 through 05G are Windows-signed-off. The temporary Step 05B proof shell was Windows-verified and then removed. The host-portable graph remains 26 tests; Windows x64 adds the real `d3d12_backend_smoke` GPU test. Step 05H1 passes the host 26/26 graph and all source-policy probes locally; Windows `mingw64-game` configure/build is still required before any game-runtime sign-off.

## Performance telemetry

- Step 03 is complete. `rts/profile.h` exposes one consolidated `PerformanceTelemetry` seam; no second profiler hierarchy was introduced.
- The historical `mingw32-profile` capture preset is retired by Step 04F. Telemetry remains compile-time gated and observational; a new real gameplay capture waits for the full Win64 profile/runtime target.
- CSV schema v2 emits one observational row per `GameEngine::update()` with update/client/message/network/logic CPU phases plus the primary WW3D render CPU bracket.
- The same row carries draw/geometry/texture/resource counters and a drawable total/visible/shrouded visibility proxy.
- `scripts/perf-summary.py` reports timing percentiles and mean/max resource counters using only the Python standard library.
- Tracy plots consume the same sample.
- GPU timestamp work is deferred to D3D12 rather than adding temporary D3D8 query infrastructure.
- Telemetry values are never consumed by simulation, frame pacing, CRC, replay, network, or Xfer behavior.

## x64 migration

- Step 04 is complete and Windows verified. The final 04F baseline passed 25/25 on real Windows MinGW-w64 GCC 16.2 plus every explicit deterministic/Evolution/x64-platform gate.
- `cmake/toolchains/mingw-w64-common.cmake` now describes only x86_64. The former i686 wrapper/preset/bootstrap path is retired by 04F.
- `mingw64-tests` enables `RTS_BUILD_EVOLUTION_X64` and `RTS_BUILD_HEADLESS_CORE`; it is the canonical x86_64 focused preset and additionally builds the real D3D12 backend smoke executable on Windows.
- Focused x64 readiness does not link legacy D3D8/DirectInput/DirectSound and does not populate ReactOS ATL when there is no consumer.
- `architecture_abi` enforces fixed-width engine/wire primitives and IDs while permitting native pointers/`uintptr_t` to widen.
- Step 04D centralized `setFPMode()` in Core and established the x64 round-to-nearest deterministic timeline; the historical i686 x87 result remains recorded as provenance, not as an active build lane.
- Step 04D3 selectively aligns with the supplied upstream snapshot: Dozer/Worker Xfer/task fixes, production cancellation, neutron radius behavior, adapted GameMemory robustness, runtime Bink/Miles loading, and glyph-buffer safety. Material shared-file divergence fell from 93 to 63 without replacing EastWind x64/determinism infrastructure.
- The full x64 Zero Hour executable is no longer configuration-gated. Step 05H1 adds the `mingw64-game` preset specifically to let the normal target reveal the remaining direct `DX8Wrapper` compile/link blockers. The existing WW3D backend seam remains the only D3D12 migration path.
- The first `mingw64-game` compile reached WWSaveLoad before renderer blockers and exposed raw 32-bit pointer identity persistence. Step 05H1A replaces serialized addresses/native pointer sizes with explicit 32-bit `PersistPointerToken` values and keeps native pointers only in the in-memory remap table; this does not alter simulation/network/replay formats.
- Steps 05H1B through 05H1J then cleared native-width crash/debug/registry/profiler/compression blockers and warning classes exposed by the same real game graph. Step 05H1K removes D3DX8 utility/math dependencies from the active x64 graph: unused D3DX headers are removed across WW3D and game-device/client callers, point/sorting and shared Bezier math use WWMath, missing-texture mip generation reuses `BitmapHandler`, and Evolution `SurfaceClass` copy/scale uses the existing CPU bitmap path. Genuine D3DX-bound shader/water/terrain/tree renderer responsibilities are intentionally left for coherent D3D12 migration; archival DX8-only source remains untouched.
- The i686 modernization/oracle lane is retired. Retail x86 multiplayer interoperability is not required; supported Evolution development proceeds on x64.

## Renderer

The x64 renderer is now migrating in-place behind WW3D `IRenderBackend`. The production D3D12 backend owns DXGI adapter/device creation, command submission, flip-model swap chain, render/depth targets, viewport/scissor, clears, present and fences. Step 05D adds an immutable root signature/PSO, shader compilation, transient upload-buffer lifetime tracking, and `DrawIndexedInstanced` for the first renderer-neutral indexed position/color primitive. Step 05E externalizes that shader into canonical HLSL. Step 05F adds persistent default-heap vertex/index resources with explicit create/draw/release lifetime and upload-to-default copy transitions. Step 05G adds the first sampled-texture path: default-heap RGBA8 upload, shader-visible SRVs, a D3D12 static sampler and persistent textured indexed drawing. Step 05H1 moves the first normal-game caller responsibility: untextured `Render2DClass` screen-space geometry used by `W3DDisplay` lines/rectangles now emits renderer-neutral color vertices/indices and uses explicit D3D12 opaque/alpha/additive 2D PSOs with depth disabled. The DX8 backend remains excluded from the x64 WW3D source selection; remaining direct `DX8Wrapper` callers are now the real `z_generals` migration queue. Step 05H1K removes active D3DX8 utility dependencies without creating a D3DX compatibility layer: WWMath handles point/sorting/Bezier transforms, `BitmapHandler` handles CPU image conversion/mips, and remaining D3DX8 use is confined to archival/non-Evolution code or genuine renderer responsibilities that must cross the D3D12 seam coherently.

The backend direction is now:

```text
WW3D callers
    |
    v
IRenderBackend
    |
    +-- D3D12Backend   (Windows x64 / Evolution)
    `-- DX8Backend     (archival 32-bit/reference path only)
```

Verified routed operations include scene begin/end, deferred present/flip, clear, viewport, gamma, ambient/light environment, cached-state invalidation, backend proof geometry/textures, and the first real untextured `Render2D` draw path. `W3DDisplay::setGamma()` also now reaches the existing backend seam instead of `DX8Wrapper`. The D3D12 backend does not emulate the full `DX8Wrapper` API. Textured `Render2D` is intentionally not faked in 05H1: it remains the next coherent resource-lifetime migration, where real `TextureClass` data must map to renderer-neutral texture handles. Meshes, materials, transforms, render targets and the remaining state/draw call sites must continue moving responsibility-by-responsibility.

## Asset system

Legacy W3D loading uses the WW3D asset manager and chunk/prototype loader system.

Representative Zero Hour path:

```text
WW3DAssetManager::Load_3D_Assets
    -> FileClass
    -> ChunkLoadClass
    -> hierarchy / animation managers
    -> PrototypeLoaderClass implementations
    -> RenderObj prototypes
```

A corresponding Generals copy exists. This duplication is one reason new W3X parsing should be implemented in shared/Core code where practical.

## W3X

No W3X runtime loader was found in this baseline.

The modernization program defines W3X as the EA SAGE XML-based evolution of W3D used by later SAGE games. W3X support therefore starts as a new additive asset path.

## Determinism guard

Step 01 now has a consolidated characterization harness in `Core/Tests/DeterminismPrimitivesTest.cpp`. The lightweight path directly exercises production CRC, game-logic RNG, and compiler-sensitive float helpers under GCC/Clang optimization variants. Historically, the retired Windows/i686 `determinism_test` extended that same harness through production Xfer primitives, XferCRC, the real `DamageInfoOutput::xfer()` snapshot method, Win32 ABI/network assumptions, and a replay command-record checkpoint. Step 04F no longer builds that x86-only branch; active x64 coverage is supplied by the portable Step 01 primitives plus the 04A-04E4 fixed-width, deterministic timeline, replay/network, session, and CRC gates.

One concrete compiler hazard was removed without changing the legacy numeric algorithm: modern/non-VC6 `fast_float_trunc`, `fast_float_floor`, and `fast_float_ceil` now move IEEE-754 bits with `memcpy` rather than aliasing a `float` through an `unsigned *`. A 199,122-input before/after probe produced identical output bits; the VC6/reference assembly branch remains untouched.

The Step 01G Windows gate remains signed-off historical provenance: on 2026-09-10, the retired MinGW-w64 i686 / GCC 16.2 + Ninja `check_determinism` passed the complete float-helper, CRC/RNG, Xfer/XferCRC, snapshot, ABI, and replay checkpoint set. Step 04F deliberately does not preserve an active i686 compiler lane merely to rerun that historical gate.

## Modernization risk areas

- deterministic behavior across compilers;
- x86/32-bit address assumptions while x64 is brought up;
- legacy binary/on-disk layout assumptions;
- 16-bit geometry assumptions in legacy W3D rendering paths;
- DX8 state-machine coupling;
- duplicate Generals / Zero Hour implementation areas;
- high-poly/high-resolution asset pressure;
- legacy tools that may require old Microsoft/MFC components.

- Step 05H1L follows the H1K Windows build to `assetmgr.cpp` at 125/940 and removes four active Zero Hour asset-name address-to-`int` subtraction sites. String positions are now computed as pointer differences before the small Win32/API length conversion, and `PrimitiveAnimationChannelClass::KeyClass::Set_Time` is corrected to its side-effect-only `void` contract to remove the exposed non-void/no-return warning.
- Step 05H1M follows the H1L Windows build to the real Evolution `Render2D` caller at 154/942 and gives both renderer-neutral backend vertex PODs explicit equality operators required by Westwood `DynamicVectorClass`; both remain trivially copyable with unchanged D3D12 upload layout.
- Step 05H1N follows the H1M Windows build to `pointgr.cpp` at 188/942: point orientation uses `Matrix3D::Rotate_Vector`, logical-audio runtime pointers cross `On_Event` as `std::uintptr_t`, and `PivotMapClass`/`SnapPointsClass` delete from complete-object overrides instead of their offset secondary `RefCountClass` base. Host validation is 26/26; Windows sign-off is pending.
