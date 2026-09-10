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

## Current modernization working baseline

- Declared Step 01G handoff archive: `GeneralsGameCode-Step01G-Windows-Signoff-Baseline-Seal-full.zip`.
- Declared SHA-256: `bb179526f5a093397375220025e265ffc66ad223e8569a30b4d933562ac8718a`.
- SHA-256 of the archive bytes actually received for Step 02A: `796c7e5642d655bebdf0ca079d7a099a5af46d82cb11a9b27282021f800fce07`.
- The checksum mismatch is recorded rather than silently substituting an older repository. Step 02A is based strictly on the received archive bytes.
- Step 01G itself remains accepted based on the user's successful Windows `z_determinismcheck` execution on MinGW-w64 i686 / GCC 16.2 + Ninja on 2026-09-10.
- The original 2026-09-09 anchor hashes above remain historical provenance for the unmodified upstream source.
