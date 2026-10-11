# GeneralsGameCodeEastWind continuation prompt

Copy the prompt below into Claude Code, Codex, or another coding agent running in this repository. The status snapshot is dated 2026-10-11; verify the live checkout and build before relying on it.

---

Continue my GeneralsGameCodeEastWind modernization project. Implement and validate changes, then continue from the next actual failure. Keep all work local: **do not push to GitHub, create a pull request, or mutate remote branches**. Commit verified changes locally at coherent milestones; I have authorized ongoing local commits. Preserve my existing work and any commits I make while you run.

## Objective

Make the real normal **x64 Zero Hour executable `z_generals`** compile, link, boot with valid Zero Hour data, create the D3D12 backend, and show actual game/UI geometry:

```text
normal game executable -> WW3D -> IRenderBackend -> D3D12Backend
```

Continue removing actual D3D8/D3DX8 responsibilities and obsolete names/files as their real callers migrate. A passing renderer smoke application is insufficient to complete this objective. The old standalone Step05B proof application was removed permanently; do not recreate it.

## Start from live evidence

1. Read applicable `AGENTS.md` instructions. Inspect `git status`, current HEAD, local changes, and `CMakePresets.json`.
2. Read the newest sections of `Modernization/CURRENT_STATE.md` and `PROJECT_STATE.md`. Older history describes earlier states; verify claims against current source.
3. Inspect existing build transcripts if present, then run the actual target:

   ```powershell
   # This machine has MinGW tools here; adapt only if the live environment differs.
   $env:PATH = 'C:/msys64/mingw64/bin;' + $env:PATH
   cmake --build --preset mingw64-game --target z_generals -- -j1
   ```

   If the preset has not been configured, first run `cmake --preset mingw64-game`.
4. Use the next actual compiler/link/runtime failure as the migration queue. Fix a repeated warning/blocker class with a controlled sweep of the same active responsibility. Do not preemptively rewrite unrelated renderer subsystems.
5. When `z_generals` links, immediately investigate valid data/runtime setup and launch it. Record backend creation, first frame, and visible game/UI geometry separately.

## Architecture and toolchain — locked

- Evolution is **x64 only**. The i686 modernization/oracle lane is retired. Evolution multiplayer compatibility is required only between this project's builds, with no retail 32-bit interoperability requirement.
- **Direct3D 12 is the only renderer target.** No D3D9 or D3D11 intermediate. Do not restore D3D8/D3DX8 SDK dependencies or create fake native resources to compile.
- All new rendering capability follows `WW3D -> IRenderBackend -> D3D12Backend`. Grow the seam only for actual migrated callers. Do not build a giant DX8 emulation wrapper.
- CMake + Ninja + MinGW-w64 GCC is the primary Windows toolchain; tested GCC is 16.2.0. Clang is secondary. Visual Studio IDE is unnecessary.
- The archival DX8 backend adapter and SDK build lane were removed after the original H1N baseline. Remaining DX8 code represents unfinished caller migration; it is not a supported renderer.
- Rename legacy renderer owners and remove obsolete files when responsibilities have actually migrated. Update includes, CMake, both game variants, diagnostics, and policy checks together. Keep accurate D3D12 names. Do not disguise an unmigrated DX8 implementation by renaming it.

## Determinism and ABI — locked

Preserve simulation/replay/network determinism, CRC, Xfer, RNG, snapshots, and deterministic frame behavior. No fast-math or reassociation on deterministic paths.

- `Int`, `UnsignedInt`, `ObjectID`, `DrawableID`: 32-bit.
- `Short`: 16-bit. `Real`: float32.
- Network/replay/Xfer/CRC/RNG representations stay fixed-width.
- `GameMessage::MSG_LOGIC_CRC = 1095` stays fixed.
- Native pointers, handles, `uintptr_t`, `intptr_t`, `size_t`, and `ptrdiff_t` may widen only for runtime use.
- Never serialize/hash pointers, addresses, allocator state, padding, native object layout, or `size_t`.
- Renderer timing, fences, resource state, telemetry, and GPU results remain downstream of simulation. Do not make deterministic commands depend on GPU execution or readback.

## Reduce unnecessary renderer CPU work

Extend D3D12 for evidence-backed rendering work. Retain unchanged geometry in default-heap GPU resources and update it only when content/lighting changes or resources become invalid. Prefer shader constants for camera/world transforms, texture projection, animation, and material work where the actual caller permits it.

Preserve geometry topology, UVs, lighting cadence, winding, alpha, draw ordering, textures, authored mips, samplers, blend/depth/cull state, and resource lifetime. Carry complete behavior across the seam. Do not move gameplay, RNG, Xfer, physics decisions, or deterministic command translation onto the GPU. Do not claim a performance speedup without measurement; fewer CPU transforms/uploads are a structural change, not a benchmark.

## Current snapshot — verify before using

Source has advanced well past H1N. Recent work includes real mesh/material/texture/shadow routes, trees and shroud, and persistent flat terrain, bib, road, scorch, and bridge geometry. Canonical HLSL performs world and projected terrain texture coordinates. Bridges preserve separate base/cloud and shroud passes. Shared result diagnostics belong to existing WWDebug instead of D3DX error-string helpers.

The normal game previously completed compilation and failed at the executable linker with remaining legacy renderer references. It has **not** been established to link, boot, or show a real game frame. Focused GPU tests passing does not establish those milestones. Current symbol counts and line numbers are volatile: obtain them from a fresh build.

Foreground `HeightMap.cpp` terrain now uses persistent dirty GPU tiles, GPU world/projection constants, and the actual `lerp(base, blend, vertexAlpha)` atlas behavior. Its blend differs from flat terrain/road multiplication. Explicit shroud, mask, and wireframe clear-line passes preserve their state. Retired terrain/cloud shader registrations and six assembly assets are removed. The blank native atlas alias is removed; both atlas stages sample the actual terrain texture. Static/dynamic lighting and simulation remain CPU-owned. Do not redo this migration or restore retired owners.

### Latest GPU river and retained water updates - 2026-10-11

The live source parent is user commit `8f7b1dec9` (water), which includes the river/backend translation following local `024a670cf`. Preserve concurrent user commits. The current verified batch fixes retained geometry invalidation, extends the GPU height fixture and records the complete river validation. Commits stay local; no GitHub pushes.

Actual drawRiverWater now uses retained terrain geometry and the canonical river HLSL route through WW3D/IRenderBackend/D3D12. The shader evaluates longitudinal scrolling/wobble, world/camera transforms, separate base/edge UVs, sparkle UVs and world-XY noise projection. It preserves the original four-texture behavior: RGB = vertexRGB * baseRGB + (sparkleRGB * noiseRGB + edgeRGB) * vertexAlpha; alpha = baseAlpha * edgeAlpha. Normal blending is SourceAlpha/InverseSourceAlpha; additive is SourceAlpha/One. LessEqual, disabled depth writes, no cull and the original wireframe debug flag remain draw state. Linear wrapped samplers retain the actual layer ownership.

Existing per-vertex source-cell shroud lookup and packed-byte RGB/alpha quantization remain unchanged. Substituting smooth projected pixel fog would change river edge brightness/opacity. No new CPU animation or transform work is added. Trigger geometry caches retain the original point ordering, riverStart, non-closing length calculation, paired topology and authored UV progress. Scripted Z-only changes update paired cached heights without recomputing lengths or indices. XY/start changes of the same paired vertex count also retain GPU resources; only size changes recreate river geometry.

Update_Static_Indexed_Terrain_Geometry updates same-size vertices in the existing default-heap buffer on the active command list, with ordered vertex-read/copy/read barriers and staging owned by the existing frame-fence lifetime. It preserves handles and index buffers without per-change CPU fence waits or resource creation. Moving grid water now uses this same retained path for height changes; cell-size changes retain topology and resources, while row topology changes recreate indices. Failed updates retain dirty state for retry. Simulation heights, physics, timing, RNG, Xfer and fixed-width representations remain unchanged; runtime cache/dirty state is never serialized or hashed.

Native river setup, river inline assembly/handle, unused cleanup declaration, CPU inverse-view noise transform and unused wobble helper are retired. The unused m_waterPixelShader prototype had only creation/deletion/zeroing and is also removed after whole active-source reference checks; actual sea uses m_dwWaveVertexShader/m_dwWavePixelShader and remains pending. Water.cpp no longer includes or assembles D3DX8. The policy gate now requires the retained route and retirement of the CPU noise helper/assembler. The 64-DWORD root signature is unchanged. GPU sin evaluation can differ slightly from the old lookup; this affects rendering only. No measured frame-time speedup is claimed.

Validation receipts:

- `build/river-water-final-tests-build.txt`: focused graph compiles and links.
- `build/river-water-final-regressions.txt`: **32/32 non-browser tests pass**, GPU 6.20 seconds, total 58.68 seconds. Browser was excluded.
- Production GPU fixtures verify nonidentity transforms, authored-progress animation across time-only frames on retained handles, separate base/edge UVs, exact layer RGB/alpha, both blend modes, culling, depth and disabled depth writes. Ordered old/new colors and heights on the same handle verify submission order, persistence into the next frame, zero time-only uploads and exactly one changed-vertex upload. Invalid update handles/types/counts/scopes and missing/conflicting shader resources are rejected.
- `build/river-water-final-syntax.txt`: actual shared water owner passes MinGW syntax checking. Read-only contract review found no introduced issue after the scripted-height invalidation fix.
- `build/mingw64-game/river-water-final-game-build.txt`: exact serial z_generals compilation passes, executable link fails. `build/river-water-final-link-summary.json`: **53 unique unresolved symbols / 152 printed references**, versus 53/168 before river migration; none added. First owner remains ShroudTextureShader::set(int), W3DShaderManager.cpp:738, Apply_Render_State_Changes.
- `build/river-water-patchcheck.txt`: fresh original-baseline reconstruction and byte comparison after all source/document edits; **4535 candidate / 4535 reconstructed files, zero missing, extra or different**; reconstruction passed.

Next migrate real sea/reflection/sky callers as one coherent responsibility. Existing render textures support color-only arbitrary sizes or output-sized shared depth; sea needs an independently depth-tested 256x256 reflection target in that same resource owner. Preserve mirror camera/culling/restoration, signed RG bump sampling before filtering, vertex-predivided reflection UV interpolation, reflection RGBA times vertex RGBA and the separate Equal-depth shroud pass. Descriptor rebuilds must preserve typed SRV formats. Existing MD bump asset loading is explicitly disabled; verify actual assets and ownership instead of silently substituting flat-water rendering. The historical bump gradient conversion has horizontal boundary reads that need defined neighbors when migrated. Retire remaining shared shroud install hooks only after their actual sea callers cross the seam.

Normal game link/boot, in-game D3D12 creation, first visible game/UI frame, complete DX8 removal, measured speedup, sealing and user Windows sign-off remain unproven. SDL3 Step06 and major W3X runtime remain deferred. Keep the full persistent goal active.

### Latest GPU flat and grid water - 2026-10-11

The verified source parent is local commit `864299da19a949c051d51cce68f048905142647a`. Verified coherent milestones are committed locally; GitHub pushes remain forbidden.

The actual shared WaterRenderObjClass grid and trapezoid callers now draw through WW3D/IRenderBackend/D3D12. Canonical HLSL performs world/camera transforms, UV scrolling, flat-water wave/feather deformation, noise projection, sparkle/noise composition and RGB-only projected shroud. Trapezoid topology uploads once to retained default-heap geometry; trigger/sector/layer identity bounds the runtime cache when heights move. Grid topology is cached, resting grid geometry is retained, and height mutations invalidate uploads. Moving grid heights still use the existing dynamic upload path: physics, height decisions, animation timing and Xfer remain CPU-owned and unchanged. Lighting color and render transforms are uniforms. Runtime cache pointers, native resource state and dirty hints are never serialized or hashed.

The complete ps.1.1 flat-water behavior is translated: base RGBA times diffuse, sparkle times noise added to RGB, optional RGB-only shroud, authored alpha, normal/additive blend and destination-alpha shoreline coverage. The selected full grid shader also uses RGB-only shroud; the old native capability fallback's alpha-add/full-RGBA stage behavior is not the canonical shader contract. Both preserve LessEqual depth and disabled depth writes; grid preserves winding/cull and trapezoids disable cull. Explicit linear samplers disable anisotropy as the old caller required. DestinationAlpha and InverseDestinationAlpha are backend state only; a separate complete blend discriminator prevents PSO key collisions. GPU water vertex flags use vertex-visible projection constants, preserving the 64-DWORD root signature.

The obsolete trapezoid assembly shader creation/handle, flat-water native setup, grid native buffer allocation and unused grid material/light allocations are removed. Shared sea/river/sky native resources still have real callers and remain pending migration. GPU sin/cos and reciprocal tessellation arithmetic can differ slightly from historical CPU lookup/double arithmetic; these are rendering-only changes. No simulation fast-math or deterministic representation changes are introduced. Fewer CPU transforms and uploads are established structurally; no measured frame-time speedup is claimed.

Validation receipts:

- `build/flat-water-complete-tests-build.txt`: focused graph compiles and links.
- `build/flat-water-final-regressions.txt`: **32/32 non-browser tests pass**, GPU 7.32 seconds, total 30.82 seconds. Browser was excluded.
- GPU fixtures exercise distinct layer/projection textures, nonidentity world/camera transforms, RGB-only shroud with zero layer alpha, destination-alpha and additive blending, PSO factor-key separation, depth occlusion and disabled depth writes. Flat, feathered and grid animation modes run multiple uniform-only frames on the same retained handle with zero additional static geometry uploads.
- Initial GPU validation caught vertex reads of pixel-visible constants; final source uses vertex-visible flags. A new fixture's depth probe also needed the same world transform as water to cover its sampled pixel; its expected red/depth assertion was preserved.
- `build/flat-water-final-syntax.txt`: actual shared water owner passes MinGW syntax checking. Focused read-only contract review found no introduced material defect.
- `build/mingw64-game/flat-water-verified-game-build.txt`: exact serial `z_generals` compilation passes, executable link fails. `build/flat-water-link-summary.json`: **53 unique unresolved symbols / 168 printed references**, down from 54/185, none added. DX8Wrapper::getBackBufferFormat is removed from the linker queue; first remaining owner is still ShroudTextureShader::set(int), W3DShaderManager.cpp:738, Apply_Render_State_Changes, through genuine remaining sea callers.
- `build/flat-water-patchcheck.txt`: fresh original-baseline reconstruction and byte comparison after all source/document edits; **4535 candidate / 4535 reconstructed files, zero missing, extra or different**; reconstruction passed.

Next migrate actual sea/reflection and river owners coherently. Sea requires its 256x256 reflection target with depth, signed bump/reflection sampling, initialized strip count and projected shroud behavior. River already applies shroud to vertex color and must not receive it twice; move its real render transforms/UV work into GPU constants where the contract permits. Retire the shared ShroudTextureShader hooks only after their remaining real callers have migrated. The `late` water donor omits required behavior and cannot be copied wholesale.

Normal game link/boot, D3D12 creation in that executable, first visible game/UI frame, complete DX8 removal, measured speedup, sealing and user Windows sign-off remain unproven. SDL3 Step06 and major W3X runtime remain deferred. Keep the persistent goal active.

### Latest projected mesh shroud - 2026-10-09

The verified source parent is local commit `436c056c9a1aede46d94040c7b5dbe5a53823591`. Verified coherent milestones are committed locally; GitHub pushes remain forbidden.

Both game variants now forward their actual mesh shroud material pass to the existing renderer-neutral terrain/projection preparation. Mesh geometry reaches WW3D/IRenderBackend/D3D12 with the original shroud texture, sampler, world-XY projection, texture Replace, Equal depth, disabled depth writes and RGB-only multiplicative blend (debug fog uses its existing alpha blend). Exact-depth mesh overlays reuse the base pass's CPU world positions, including ALIGNED/ORIENTED camera-facing meshes and world-space skins; projection stays in canonical HLSL. This intentionally preserves base arithmetic instead of separately rounding its transform on the GPU.

The unused transparent-object flag had no callers or reads and is removed; no invented alternate transparent shader is added. Alpha-tested holes rely on stored base depth, as in the native pass. Procedural shroud submission stays immediate, including SORT meshes: the exact native owner at `d15625e4f^` used Render/Draw_Sorting_IB_VB immediately for additional passes and deferred only base categories through Render_Sorted/Insert_Triangles. Sorted-base visibility limitations are preserved, not fixed or claimed improved. Unused FlatShroudTextureShader, registration/list/enum and setShroudTex utility are retired after full active-source no-caller searches. Actual ShroudTextureShader and its water hooks remain pending renderer owners.

Validation receipts:

- `build/shroud-tests-build.txt`: focused graph builds and links.
- `build/shroud-regressions.txt`: **32/32 non-browser tests pass**, GPU 5.06 seconds, total 17.96 seconds. Browser was excluded.
- Production GPU fixture uses nonidentity CPU rigid rotation/translation, camera translation and depth slope. Three covered pixels verify projected RGB multiplication at distinct D24 values. Zero shroud alpha preserves destination alpha; alpha-tested holes, background and a base without depth writes stay unshrouded. Conflicting authored UVs detect projection omission.
- `build/shroud-syntax.txt` and `build/shroud-mesh-syntax.txt`: fourteen paired/shared owners pass MinGW syntax checks.
- `build/mingw64-game/shroud-verified-game-build.txt`: exact serial game compilation passes, executable link fails. `build/shroud-link-summary.json`: **54 unique unresolved symbols / 185 printed references**, unchanged and none added. First owner remains ShroudTextureShader::set(int), W3DShaderManager.cpp:738, Apply_Render_State_Changes. Removing already-unreferenced utilities does not remove the genuine water link dependency.
- `build/shroud-patchcheck.txt`: fresh original-baseline reconstruction and byte comparison after source/document edits; **4535 candidate / 4535 reconstructed files, zero missing, extra or different files**.

Next migrate actual water shroud callers with their complete rendering behavior. Sea needs its 256x256 reflection target with depth, signed bump/reflection sampling and initialized strip index count. Grid needs full RGBA shroud modulation and its base alpha-add operation. Trapezoid shader needs sparkle/noise, RGB-only shroud and destination-alpha shoreline feathering; shared flat-water setup serves both grid and trapezoid. River already applies shroud in vertex color and must not receive a second projected shroud. The `late` water donor omits required behavior and cannot be copied wholesale.

Normal game link/boot, backend creation, first visible game/UI frame, complete DX8 removal, measured speedup, sealing and user Windows sign-off remain unproven. SDL3 Step06 and major W3X runtime remain deferred.

### Latest capture/filter migration - 2026-10-08

The verified source parent is local commit `872f6b535580b315f51118fead265c061f177fd9`. The user requires verified coherent local commits and forbids pushes. This batch advances the real game migration; normal game link/boot, a visible frame, complete DX8 removal and user Windows sign-off remain unproven.

ScreenBWFilter, ScreenMotionBlurFilter and ScreenCrossFadeFilter now share one TextureClass capture owned by W3DShaderManager. Size/generation checks, target transitions, shared output depth and output camera/viewport restoration remain on the backend seam. New/lost captures are initialized and rendered as actual scenes before frozen reuse; crossfade seeds missing RGB once instead of sampling an uninitialized image. Motion blur preserves the original CPU GameLogic-frame count/camera decisions, cumulative pan/zoom UV arithmetic, first opaque replacement and ordered normal/additive overlays. A narrow existing material-effects bit selects vertex alpha independently of captured alpha, without increasing the 64-DWORD root signature. Direct END_PAN activation initializes its prior delta and guards zero-length normalization.

Soft-water retained captures reset only tactical alpha, with the original packed-byte opacity quantization. A small backend pass channel mask protects alpha from ordinary scene geometry; explicit shoreline coverage may override it to write alpha. Dynamic/static color, textured material, terrain and 2D draw paths preserve their existing state while applying the pass mask. Deferred sorting snapshots the effective channel mask at submission, so restoration before flush does not change queued draws. Clear remains independent of draw channel masks. This is renderer runtime state, never simulation/wire/Xfer/CRC data.

Crossfade retains the shared raw RGB while updating projected mask alpha/depth, then restores output, requests the original second normal-scene render and composites the retained scene. Circle mode uses the existing terrain-layer operation for independent secondary UVs and full RGBA mask multiplication, preserving source-alpha/inverse-source-alpha blending and output alpha. Terrain masks retain their existing projection; a narrow additional-material seam now carries real rigid/skinned mesh mask geometry through the same GPU world-XY projection. Shroud mesh transparency remains a separate pending owner. The alpha-mask scene scope also protects RGB from ordinary trees and objects. Native motion/crossfade set/reset/registration lists, native MaskTextureShader/registration and its obsolete pass hooks are removed. Four unused paired motion-blur assembly prototypes and their CMake references are retired. Remaining native shroud, wireframe/scene state, water, smudge and resource owners still require actual migration.


Latest validation: 32/32 non-browser regressions pass (`build/capture-final-regressions.txt`, GPU 6.90s, total20.34s). Fourteen paired/shared source owners pass syntax checks. Exact serial game compilation still reaches the executable linker:54 unique unresolved symbols/185 printed references versus194, none added. First owner is ShroudTextureShader::set(int), W3DShaderManager.cpp:738, DX8Wrapper::Apply_Render_State_Changes. Fresh original-baseline reconstruction receipt is build/capture-patchcheck.txt. Keep no-link/no-boot/no-visible-frame/no-Windows-signoff limits explicit. This older queue is superseded by the projected mesh shroud section above; actual water shroud owners remain. Do not resume motion-blur or crossfade native implementations, restore retired assembly prototypes, or split their shared raw RGB history.

Actual textured Render2D still needs paired migration for first visible UI: Zero Hour currently routes only untextured/non-grayscale draws. Keep existing screen-space camera/viewport isolation, already-normalized positions and authored UVs. Grayscale uses staged texture-factor/DOT3 math, not ordinary luminance weights. GPU fixtures and CPU texture uploads do not prove radar/video/UI visibility.

The `late` branch's main water implementations are unsafe to copy wholesale: some replace authored bump maps with flat dummy pixels, omit reflection/layers, change nonplanar strip diagonals, use alpha blending instead of destination-alpha feathering, and ignore failures. Reuse only verified topology/UV or neutral geometry fragments while preserving current resource ownership and full shader/material behavior.

Another later candidate is tree per-frame CPU vertex transformation/upload. Move authored tree geometry and the original shader's sway/world/color/shroud operations through a real D3D12 caller when the build queue supports it. Keep push/topple/FX, RNG and Xfer on their existing CPU paths.

The local branch named `late` contains potentially reusable work. Compare narrow owner-specific changes against current contracts. Adapt useful implementations selectively; do not merge the branch wholesale or replace newer ownership/determinism fixes.

## Legacy shader migration

Translate each assembly shader when its actual caller crosses the seam. Bring shader logic, textures, samplers, constants, vertex format, PSO, blend/depth state, and caller behavior together. Avoid a premature giant shader framework.

Canonical shared HLSL:
`Core/Libraries/Source/WWVegas/WW3D2/Shaders/PrimitiveColor.hlsl`.
Runtime D3D compiler use is acceptable for now.

## W3D and W3X

W3D behavior remains intact. W3X means EA SAGE XML W3X and is additive; never permanently fake it as W3D chunks. Both loaders should eventually produce renderer-neutral CPU mesh/material data consumed through WW3D/D3D12. No GPU/D3D12 types in persistent W3X asset structures.

Do not resume major W3X runtime work until the normal D3D12 game path visibly works. Existing seams include `Core/Libraries/Include/rts/w3x_document.h`, `Core/Libraries/Source/rts/w3x_document.cpp`, and the W3X tests in `Core/Tests`.

## SDL3 — dedicated Step06 after boot/frame

Do not insert SDL platform migration into remaining Step05H compilation without a compelling architectural reason. First control normal x64 game boot/frame, then plan Step06.

```text
Generals GameWindow/Gadget/InGameUI
    -> normalized local platform input -> SDL3

WW3D -> IRenderBackend -> D3D12Backend
```

SDL3 owns the application window/platform events. On Windows obtain its native HWND; D3D12/DXGI owns rendering and the swapchain. Eventually cover window/event loop, keyboard/mouse/text/IME/Unicode, controllers, cursor/clipboard, displays/DPI, window mode/focus/minimize. Keep the abstraction small (`PlatformWindow` / `PlatformEvents`). SDL does not replace Generals UI, WW3D, or the renderer.

Input flow: SDL event -> normalized local input -> existing game command translation -> deterministic simulation-frame command. No SDL timestamps or native event metadata in deterministic state.

## Ownership, consolidation, and warnings

- Inspect Core/shared code for an existing owner before adding a file/helper. Merge duplicated responsibilities; keep public headers small and implementation in `.cpp`. New files need a distinct permanent responsibility.
- Respect paired Generals/GeneralsMD owners. Test both when shared contracts change.
- Fix real x64/UB/ownership warnings at the owning logic: pointer truncation, native handles, wrong deletion types, callback pointer widths, secondary-base intrusive deletion, missing return contracts, invalid throwing allocation contracts.
- Do not blindly suppress warnings. Historical `#pragma message`, optional ZLIB, and missing install-path status notes do not require suppression.
- Do not stub native resource operations, dead-strip actual callers, or exclude genuine renderer responsibilities solely to make linking pass.

## Parallel agents and build coordination

Use as many agents as the environment supports for independent, bounded responsibilities. Assign explicit file ownership. Tell each agent others are editing, to preserve others' changes, and to leave integration commits to the coordinator. No agent may push. Reuse agents for related reviews.

One coordinator owns shared backend/HLSL changes and integration. Keep production sources and shared/PCH headers frozen during actual game builds. Parallel tests may use separate build directories. Do not let multiple agents mutate/build the same graph concurrently. Ask reviewers to verify complete caller state, resource generations/lifetime, camera restoration on every exit, and paired variant behavior.

## Validation and reporting

Run meaningful local regressions after changes:

```powershell
cmake --build --preset mingw64-tests -- -j2
ctest --preset mingw64-tests --output-on-failure
cmake --build --preset mingw64-game --target z_generals -- -j1
git diff --check
```

Handle tests needing native browser/profile access explicitly; report what ran and what was excluded. Record build exit codes, first remaining failure, and before/after unresolved symbols without treating a smaller count as game integration. Use production GPU pixel/resource tests for new blend/projection/lifetime behavior and real game launch for boot/visual claims.

Before calling anything sealed, reconstruct a binary patch against a fresh baseline archive and compare all candidate files **byte-for-byte**, including tracked additions/deletions and any intentional untracked artifacts. Preserve the user's real index. An ignored local helper `build/patchcheck-late-reuse.py`, if present, uses baseline `2c81f9922beb5c5379c6d695c0e14ea3f15f55b3`; inspect its assumptions before running, especially new untracked files. Do not stage or commit just to satisfy a helper.

Update the existing state documents with actual receipts. Distinguish focused tests, full-game compilation, game linking, runtime/backend creation, visible frame, measured performance, and user Windows sign-off. **Never claim my Windows sign-off without my actual result.**

Provide concise progress updates and keep advancing the active objective. Ask for missing information only when it blocks dependent work. At a context/tool limit, leave a precise continuation record with changes, commands/results, remaining failure, agent ownership, and next step. Do not mark the overall goal complete until the normal game objective is actually achieved.
