# Architectural Decisions

This file records decisions that should not be casually re-litigated in later sessions. A decision may be changed only by adding a new dated entry explaining why.

## 2026-09-09 — This source tree is authoritative

`GeneralsGameCode-main(2).zip` is the baseline from which modernization proceeds. Future roadmap and implementation state must be recorded in this repository.

## 2026-09-09 — Direct3D 12 is the only new renderer target

The Evolution renderer will target Direct3D 12 directly.

- no D3D9 modernization milestone;
- no D3D11 intermediate backend;
- existing DX8-era rendering is retained only as a temporary compatibility/reference path.

## 2026-09-09 — Evolution means x64 + D3D12

The high-end runtime target is x64. D3D12 work should be designed for the x64 Evolution runtime rather than preserving x86 as the long-term graphics platform.

## 2026-09-09 — Visual Studio IDE is not a build dependency

Primary Windows development target:

```text
CMake + Ninja + MinGW-w64 GCC
```

Clang is secondary. Standalone MSVC Build Tools may remain optional for comparison/compatibility, but the Visual Studio IDE must not be required.

## 2026-09-09 — W3D remains supported

Legacy W3D content is a first-class compatibility requirement. W3X support is additive, not a replacement that invalidates existing mods/assets.

## 2026-09-09 — Add EA SAGE W3X support

The project will support the **EA SAGE W3X 3D-asset format**, the XML-based evolution of W3D used by later SAGE titles.

This does **not** refer to the unrelated Warcraft III `.w3x` map format.

W3X support will be implemented through a format-specific loader feeding renderer-neutral asset structures. Do not translate W3X into fake W3D chunks as the permanent architecture.

## 2026-09-09 — Renderer must never control simulation

Simulation/game state produces renderable state. Renderer frame rate, GPU query results, resource residency, frame pacing, or D3D12 execution timing must never affect deterministic logic, CRC, network commands, or replay outcomes.

## 2026-09-09 — Performance architecture before eye candy

Measurement, memory, geometry, batching, LOD, x64, and renderer architecture come before optional effects such as bloom/AO/advanced lighting. Visual features must be built on a scalable renderer rather than hiding structural bottlenecks.

## 2026-09-11 — Begin x64 migration immediately after Step 03

The prior roadmap deferred the main x64 port until Step 10 and placed a standalone x86-memory-survival milestone at Step 04. That sequencing is superseded.

- Step 03 performance telemetry is completed first so migration work remains measurable.
- Step 04 now owns the staged x64 engine migration.
- x86 remains the deterministic compatibility/reference runtime throughout the migration.
- short-term x86 memory diagnostics may still be added when useful, but they are not a gate that delays x64 bring-up.
- old Step 10 becomes an x64 stabilization/soak gate rather than the start of the port.
- fixed replay/network/Xfer/on-disk widths must be separated from native pointer width; widening the process must not silently widen wire formats.


## 2026-09-12 — Evolution multiplayer does not target retail x86 interoperability

Future multiplayer compatibility is required between our own Evolution/game editions only. The retail 32-bit Generals/Zero Hour executable is not a required network peer.

- Evolution may define a clean, explicit, versioned fixed-width wire protocol.
- Native x64 pointers/handles/`size_t` must never leak into that protocol or CRC/replay state.
- The frozen i686 build remains only as a temporary deterministic/replay behavior oracle until golden x64 gates replace it.
- Retail packet-layout quirks must not block x64 runtime cleanup or the future D3D12 architecture.

## 2026-09-12 — EastWind owns architecture; upstream is a recurring correctness source

Future upstream refreshes are selective integrations, not branch replacements.

- hash and record the EastWind and upstream snapshots before each sync;
- normalize line endings when measuring divergence;
- import coherent gameplay/correctness groups into both Generals and Zero Hour where applicable;
- adapt upstream runtime/build changes to EastWind's x64, determinism and D3D12 decisions instead of copying x86 assumptions;
- never overwrite fixed-width wire/replay/Xfer guarantees, native-width allocator work, strict-aliasing fixes or the focused test graph merely to reduce diff size;
- document intentional divergence and add regression coverage for imported compatibility-sensitive behavior.

## 2026-09-12 — Evolution protocol v1 is explicit little-endian fixed-width data

Evolution network/replay compatibility is defined by versioned byte encodings, never by C++ object layout.

- command/network/replay fields use explicit fixed widths and little-endian encoding;
- `GameMessage`, STL container layout, native enums, pointers, `size_t`, padding and allocator state are never serialized as protocol state;
- network and replay share one command codec but retain separate outer containers;
- legacy `.rep` loading may remain as a compatibility path, but new Evolution format design is independent of legacy Recorder object layout;
- unsupported protocol/replay/command versions are rejected explicitly rather than guessed;
- retail x86 multiplayer packet compatibility is intentionally outside the Evolution contract.
