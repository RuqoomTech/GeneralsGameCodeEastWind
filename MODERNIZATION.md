# Evolution modernization

This repository carries a staged modernization of the Generals / Zero Hour engine. The goal is to preserve proven simulation and content behavior while removing the major runtime limits that block large modern mods.

## Locked direction

- Evolution runtime: x64 only.
- Primary Windows toolchain: CMake + Ninja + MinGW-w64 GCC.
- Renderer: Direct3D 12 only for new Evolution rendering work.
- Legacy Direct3D 8 code: compatibility/reference only during migration.
- Determinism: simulation, CRC, replay, RNG, Xfer and snapshots remain stable.
- Wire/replay data: explicit fixed-width serialization; never native object layout or pointer state.
- Assets: W3D remains supported; W3X is additive EA SAGE XML support.
- Multiplayer: future compatibility is required between Evolution builds, not retail 32-bit clients.
- Code organization: consolidate shared/Core functionality before adding another helper/module.

## Current phase

Steps 00-04 are complete. Step 04 established the x64 deterministic/network/replay foundation and retired the active i686 modernization lane after real Windows MinGW-w64 validation.

**Step 05 is active: renderer-first modernization + W3X asset foundation.**

Step 05A normalized the repository and is Windows-signed-off. Step 05B proved the required Win32/DXGI/D3D12 fundamentals on real Windows hardware. Step 05C2 is Windows-signed-off with D3D12 integrated behind the existing WW3D backend seam. Step 05D now adds the first indexed primitive path on that production backend. W3X parser/import work resumes after the D3D12 indexed-mesh foundation so W3D and W3X can converge on one renderer rather than two runtime architectures.

## Primary documents

Day-to-day development should normally need only these files:

1. [`PROJECT_STATE.md`](PROJECT_STATE.md) — current state, baseline and next work.
2. [`Modernization/ROADMAP.md`](Modernization/ROADMAP.md) — milestone order.
3. [`Modernization/DECISIONS.md`](Modernization/DECISIONS.md) — locked decisions.
4. [`Modernization/ARCHITECTURE_GUARDRAILS.md`](Modernization/ARCHITECTURE_GUARDRAILS.md) — invariants.
5. [`TESTING.md`](TESTING.md) — current validation commands and test inventory.
6. [`Modernization/WORKLOG.md`](Modernization/WORKLOG.md) — chronological history.

Subsystem references remain available when needed:

- `Modernization/W3X_IMPORT.md`
- `Modernization/W3X_SUPPORT.md`
- `Modernization/ASSET_PIPELINE.md`
- `Modernization/D3D12_RENDERER.md`
- `Modernization/PERFORMANCE_TARGETS.md`
- `Modernization/BUILD_SYSTEM.md`

Completed milestone specifications are retained under `Modernization/History/` as historical design/sign-off records. They should not be treated as the current command reference when `TESTING.md` or `PROJECT_STATE.md` says otherwise.

## Definition of done

A modernization milestone is complete only when:

- implementation is present in the authoritative tree;
- deterministic/network/replay invariants relevant to the change still pass;
- command-line tests are reproducible;
- current documentation is updated without duplicating the full history everywhere;
- no unrecorded architectural deviation is introduced.
