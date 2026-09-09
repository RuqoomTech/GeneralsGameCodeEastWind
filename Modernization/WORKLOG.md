# Modernization Worklog

This is a chronological implementation record. Add an entry whenever a modernization milestone or meaningful architectural slice is merged.

## 2026-09-09 — Authoritative baseline established

Baseline:

- `GeneralsGameCode-main(2).zip`
- SHA-256 `3fa2e6807e842ba51cfa67b5251e4c7f0bdaaedc3dbea5c1f69ddfdde94f08fd`

Actions:

- adopted this source tree as the sole modernization baseline;
- audited current build system, renderer seam, and W3D loader architecture;
- recorded x64 + D3D12 as the Evolution target;
- recorded CMake + Ninja + MinGW-w64 GCC as the primary build direction;
- recorded additive EA SAGE W3X support;
- created roadmap, decisions, guardrails, asset, renderer, performance, and build documentation;
- no gameplay/renderer/build implementation changes were made as part of this documentation step.

### Existing upstream work recognized

The baseline already contains a partial renderer abstraction (`IRenderBackend` + `DX8Backend`) routing a subset of WW3D operations. This work is recorded as `PARTIAL`; it is not claimed as newly implemented by the modernization docs pass.
