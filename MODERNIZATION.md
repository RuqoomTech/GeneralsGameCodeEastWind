# Generals / Zero Hour Modernization Program

This repository is the authoritative baseline for the modernization work described below. The roadmap, implementation state, architectural decisions, and milestone history must be maintained in this tree so that future work does not depend on chat history or an external planning document.

## Product goal

Turn the existing Generals / Zero Hour runtime into a modern, high-performance RTS platform capable of supporting a very large mod with:

- high-poly models and substantially larger geometry budgets;
- HD/2K/4K textures and modern compressed texture formats;
- dense battles with large unit counts, effects, and repeated assets;
- substantially lower CPU rendering overhead;
- much larger memory headroom through an x64 Evolution runtime;
- a native Direct3D 12 renderer;
- modern shadows, materials, lighting, HDR, and post-processing;
- native support for legacy W3D assets;
- first-class support for EA SAGE W3X assets;
- continued deterministic simulation, CRC, replay, and multiplayer behavior unless an intentional compatibility break is explicitly approved and tested.

This is **not a rewrite of the game simulation**. The strategy is to preserve proven game logic and content behavior while progressively replacing client/runtime limitations.

## Read first

1. [`PROJECT_STATE.md`](PROJECT_STATE.md) — authoritative implementation state and baseline identity.
2. [`Modernization/ROADMAP.md`](Modernization/ROADMAP.md) — ordered implementation roadmap.
3. [`Modernization/DECISIONS.md`](Modernization/DECISIONS.md) — locked architectural decisions.
4. [`Modernization/ARCHITECTURE_GUARDRAILS.md`](Modernization/ARCHITECTURE_GUARDRAILS.md) — rules every implementation step must obey.
5. [`Modernization/CURRENT_STATE.md`](Modernization/CURRENT_STATE.md) — verified facts about the source tree.
6. [`Modernization/BUILD_SYSTEM.md`](Modernization/BUILD_SYSTEM.md) — command-line build target.
7. [`Modernization/ASSET_PIPELINE.md`](Modernization/ASSET_PIPELINE.md) — asset modernization architecture.
8. [`Modernization/W3X_SUPPORT.md`](Modernization/W3X_SUPPORT.md) — EA SAGE W3X support plan.
9. [`Modernization/D3D12_RENDERER.md`](Modernization/D3D12_RENDERER.md) — target renderer architecture.
10. [`Modernization/PERFORMANCE_TARGETS.md`](Modernization/PERFORMANCE_TARGETS.md) — measurement and performance gates.
11. [`Modernization/BACKLOG.md`](Modernization/BACKLOG.md) — milestone inventory and status.
12. [`Modernization/WORKLOG.md`](Modernization/WORKLOG.md) — chronological record of completed modernization work.
13. [`Modernization/REFERENCES.md`](Modernization/REFERENCES.md) — external format/API references used for validation.
14. [`Modernization/STEP_01_DETERMINISM_GUARD.md`](Modernization/STEP_01_DETERMINISM_GUARD.md) — deterministic compatibility contract and Windows sign-off gate.
15. [`Modernization/STEP_05_W3X_IMPORT_FOUNDATION.md`](Modernization/STEP_05_W3X_IMPORT_FOUNDATION.md) — first W3X implementation specification.
16. [`Modernization/BASELINE_MANIFEST.md`](Modernization/BASELINE_MANIFEST.md) — accepted source hash and anchor-file hashes.

## Priority order

When goals conflict, use this order:

1. deterministic simulation / replay / CRC safety;
2. reproducible command-line builds;
3. measurement and profiling;
4. memory headroom and resource correctness;
5. asset compatibility and W3D/W3X loading correctness;
6. high-poly geometry scalability;
7. HD texture efficiency;
8. draw-call, batching, LOD, and visibility efficiency;
9. x64 Evolution runtime;
10. renderer separation;
11. D3D12 correctness and performance;
12. advanced visual features.

## Runtime tracks

### Compatibility/reference track

- x86;
- existing Direct3D 8-era renderer and wrapper path;
- legacy W3D behavior;
- deterministic/replay reference;
- retained while the Evolution runtime is incomplete.

### Evolution track

- x64;
- Direct3D 12 only;
- W3D plus W3X asset support;
- explicit 16-bit and 32-bit geometry paths;
- modern GPU resource management;
- modern shader/material pipeline;
- high-end asset budgets;
- controlled breaking changes only where explicitly documented.

There is **no D3D9 or D3D11 intermediate renderer milestone** in this program.

## Definition of done for a milestone

A roadmap step is not complete merely because code compiles. A completed step must have:

- implementation merged into this source tree;
- tests or reproducible validation appropriate to the subsystem;
- relevant documentation updated;
- `PROJECT_STATE.md`, `BACKLOG.md`, and `WORKLOG.md` updated;
- no unexplained deterministic/replay regression;
- no unrecorded architectural deviation.
