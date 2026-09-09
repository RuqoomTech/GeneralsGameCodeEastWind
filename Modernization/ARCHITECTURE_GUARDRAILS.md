# Architecture Guardrails

## Determinism

Protected areas include, at minimum:

- RNG and simulation decisions;
- CRC generation;
- Xfer/snapshot serialization;
- replay command streams;
- multiplayer command/network serialization;
- object IDs and deterministic ordering;
- simulation timing/ticks;
- any binary layout whose bytes are hashed or serialized.

Changing compiler or architecture is not accepted until these behaviors are characterized sufficiently to detect regressions.

## Renderer isolation

Rendering is downstream of simulation:

```text
Simulation / deterministic state
            |
            v
     client/view extraction
            |
            v
       render world
            |
            v
         renderer
```

Do not place D3D12 types in gameplay, game logic, network, replay, or deterministic serialization code.

## Legacy renderer

The existing DX8-era path is a reference/fallback during migration. New D3D12 architecture should not become a giant emulation of every `DX8Wrapper` state mutation.

The existing `IRenderBackend` seam is useful migration groundwork, but it is not automatically the final D3D12 API. Grow it only for real migrated callers and introduce higher-level renderer-neutral concepts where required.

## Asset compatibility

- W3D remains supported.
- W3X is additive.
- New loaders must validate inputs and fail cleanly.
- Asset parsing must not depend on GPU objects.
- Parsing/import should produce CPU-side neutral data before backend upload.
- W3X parser behavior must be testable without launching the full game.

## W3X naming collision

`.w3x` is also used by Warcraft III map files. In this project, W3X means **EA SAGE W3X model/asset XML**. Loader validation should inspect content/schema/root structure rather than trusting only the extension.

## Code organization and consolidation

- Before adding a new shared/Core file, search for an existing module with the same responsibility.
- Prefer extending/refactoring one coherent module over creating parallel helper headers or duplicate scanners.
- Public headers should expose API and data contracts, not large implementation bodies, unless header-only behavior is materially justified.
- New files should represent a distinct architectural responsibility, not simply the next incremental step number.
- Exploratory pre-step code should be consolidated before runtime integration so temporary seams do not harden into permanent architecture.

## Build portability

- CMake is the build definition.
- Ninja is the primary generator target.
- helper scripts may wrap presets but must not become a second build system;
- runtime/test targets must not require the Visual Studio IDE;
- dependencies should be target-scoped where practical.

## Memory and x64

Do not convert pointer-sized or serialized data casually during x64 migration. Explicitly distinguish:

- runtime pointer size;
- on-disk format widths;
- network/replay widths;
- GPU index/offset widths.

## Performance changes

Every significant optimization should answer:

1. what metric is expected to improve;
2. what benchmark reproduces the workload;
3. what correctness risk exists;
4. what before/after result was measured.

Avoid speculative complexity without profiling evidence.
