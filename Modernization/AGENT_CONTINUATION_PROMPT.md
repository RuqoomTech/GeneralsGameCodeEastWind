# GeneralsGameCodeEastWind continuation prompt

Copy the prompt below into Claude Code, Codex, or another coding agent running in this repository. The status snapshot is dated 2026-10-07; verify the live checkout and build before relying on it.

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

The current tested source is user commit `5a4b16b002d6b9607820da62662129b5983b3d74` plus local continuation changes. Latest focused compilation passes 20/20 actions; non-browser regressions pass **31/31**, production GPU 4.70 seconds (20.52 seconds total), receipt `build/monochrome-regressions.txt`. Updated retirement policy separately passes. Eleven paired terrain/display/shroud/bridge/shader owners pass focused MinGW syntax (`build/monochrome-syntax.txt`). Browser controller creation failed in an earlier accidental full-suite run; do not report a current browser/full-suite pass.

Water tracks now use actual terrain material drawing, original CPU wave timing/motion/topology/UVs/fade alpha, full RGBA projected shroud, and explicit backend depth bias. The -8 D24-unit bias preserves near-pull intent; old-driver numeric equivalence and game appearance remain unverified. Native track buffers/locks/draw/state/lifecycle hooks, unused diagnostic label helpers, and unused CloudMap restore are removed. Texture generation and actual pending renderer responsibilities remain intact.

ScreenBWFilter now owns a TextureClass capture and composites through the material/backend route. Canonical HLSL preserves luminance .3/.59/.11, white/red/green tint, RGB fade, and alpha equal to luminance independently of fade. Original CPU rendered-view fade cadence/final frame, opaque blend, always depth/no depth writing, RGBA mask, linear clamp/no mip and tactical UVs remain. D3D12 quad pixel edges have no obsolete -0.5 bias. Capture validates resize/backend generations and restores output state before failure exits. Default screen filtering explicitly renders directly; its unreachable RTT implementation/registration and the obsolete BW DOT3 fallback/four paired monochrome assembly assets are retired. Keep remaining active motion-blur/crossfade/water effects until their full caller behavior migrates.

The existing render-texture APIs take optional `use_output_depth=false`. True is explicit full-output capture: it permits allocation/selection in an open scene, binds shared output D24S8, transitions targets and restores output camera/viewport. Default color-only shadow behavior retains between-scene restrictions. Material constants grow to 12 DWORD, total root signature exactly 64 DWORD; do not add root constants beyond this budget. GPU fixtures cover tint/fade/alpha, shared depth both sides of restoration, invalid targets/self-sampling, saved camera, tactical pixel bounds and alternating single-pixel columns to catch half-texel blur. Simulation and wire contracts do not change.

The exact serial game build completes compilation and exits 1 at executable link (156 actions; `build/mingw64-game/monochrome-game-build.txt`). **54 unique unresolved symbols / 194 printed references**, none added versus the previous 54/200 candidate. First function is **ScreenMotionBlurFilter::set(FilterModes)**, `W3DShaderManager.cpp:819`, `DX8Wrapper::Apply_Render_State_Changes()`. Obtain fresh counts from the actual log; linker-summary hidden repetitions are not counted. No normal game link, boot/backend creation or visible frame is established. Fresh baseline patch reconstruction matches 4539 files byte-for-byte; receipt `build/monochrome-patchcheck.txt`.

Next actual migration: motion-blur retained capture and composite. Preserve skip-render frozen frames, exact zoom/pan UV arithmetic, overlay order/count and normal versus additive blending, GameLogic-frame versus rendered-view CPU cadence, vertex-selected alpha independently of capture alpha, and soft-water tactical alpha-only clear with quantized minimum opacity followed by RGB-only writes. Full clearing would erase its retained image. The existing BW material needs a narrow vertex-alpha operation; using ordinary Modulate would multiply by capture alpha incorrectly. Consolidate shared capture ownership into the existing shader manager as filters migrate; do not build a parallel framework. Generic native manager initialization, crossfade, water shroud/mask and texture resources remain genuine pending owners.

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
