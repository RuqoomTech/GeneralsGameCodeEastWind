# Performance Targets and Measurement

## Principle

Do not optimize by anecdote. Establish repeatable scenes and capture CPU, GPU, memory, geometry, visibility, and asset metrics.

## Core frame metrics

Track at least:

- total CPU frame time;
- total GPU frame time once GPU timing is available;
- render submission time;
- scene traversal/culling time;
- terrain time;
- object/mesh time;
- shadow time;
- particle/effect time;
- draw calls by pass;
- vertices/triangles by pass;
- instanced draw count and instances per draw;
- visible and culled object counts;
- resource upload volume;
- texture memory and peak residency;
- vertex/index buffer memory;
- render-target/depth memory.

## Heavy-mod asset metrics

Development diagnostics should identify:

- largest resident textures;
- largest meshes by vertices/indices;
- most frequently submitted meshes;
- repeated meshes that are not instanced;
- most expensive shadow casters;
- assets with missing/ineffective LODs;
- W3X assets with parser/validation fallbacks;
- unusually large animation/skeleton data.

## Benchmark scenes

### B0 — Empty/reference

Minimal map and UI. Establish engine/render overhead floor.

### B1 — Legacy battle

Representative unmodified/legacy-quality Zero Hour battle for compatibility comparison.

### B2 — Heavy units

Large formation using the mod's high-poly vehicles/infantry and HD materials.

### B3 — Effects stress

Many particle systems, explosions, smoke, trails, shadows, and transparent effects.

### B4 — HD asset stress

Large variety of 2K/4K textures, buildings, props, terrain, and high-poly units.

### B5 — W3X validation scene

Representative W3X rigid model, skinned model, LOD group, multiple materials/UV sets, collision metadata, and animations as support matures.

## Performance goals

Do not hard-code a render FPS target into deterministic simulation. Render-rate targets apply only after legacy frame/tick coupling is understood and, where appropriate, separated safely.

Use frame-time budgets for the renderer:

- 16.67 ms GPU/CPU render frame as the baseline modern 60 Hz quality target on recommended hardware;
- 8.33 ms as a stretch/performance target where scene complexity and hardware allow;
- no deterministic simulation outcome may change because one machine renders faster or slower.

## Regression policy

Every completed optimization milestone should store:

- benchmark scene/version;
- hardware/compiler/build configuration;
- before metrics;
- after metrics;
- visual/correctness notes;
- replay/CRC status when relevant.
