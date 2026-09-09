# Baseline Manifest

## Authoritative source archive

- File: `GeneralsGameCode-main(2).zip`
- SHA-256: `3fa2e6807e842ba51cfa67b5251e4c7f0bdaaedc3dbea5c1f69ddfdde94f08fd`
- Adopted: 2026-09-09

## Anchor-file hashes from the unmodified uploaded baseline

These hashes help quickly determine whether a future upload has changed major modernization anchor points.

| File | SHA-256 |
|---|---|
| `CMakePresets.json` | `094b15a2890f078a2e0371cb65d96f7fbd3ffe7f0016393ccf0471cc234ecf59` |
| `Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h` | `dcac241f386554d37c2a46ff2cb653616b2c5c19d752b6f3bd37d20d743d2e56` |
| `Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Backend.cpp` | `cbdb17f347b438b280e4b904645b042121a1f1306448d6cb4a7f240a2abcbf71` |
| `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/assetmgr.cpp` | `efefb13738ab5d30c9fa479cfc58b5618f0b4cd1056b6fc0fc16a2a74ea6f39a` |

## Baseline documentation policy

The hashes above intentionally describe the uploaded source **before** modernization implementation patches. Documentation additions do not change the meaning of these anchor hashes.

When a future source update is adopted:

1. compute the new archive hash;
2. compare anchor files and relevant subsystem trees;
3. record upstream changes in `WORKLOG.md`;
4. update this manifest only after the new baseline is intentionally accepted;
5. forward-merge modernization work rather than replacing newer upstream source with an older full ZIP.
