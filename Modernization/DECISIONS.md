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
