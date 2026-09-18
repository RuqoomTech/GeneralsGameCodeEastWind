# Project state

Updated: 2026-09-18

## Current baseline

Step 04 is complete and Windows-signed-off. The supported Evolution modernization path is x64-only.

Real Windows MinGW-w64 GCC/G++ 16.2 validation for the final Step 04F baseline passed the complete 25-test focused graph plus the explicit deterministic, protocol, runtime, session, full-session and x64-platform checks.

Step 05 is active. Step 05A is implemented locally and awaiting its Windows seal; 05B is the next functional slice.

## Locked architecture

- Evolution runtime is x64-only.
- New renderer work targets Direct3D 12 only; no D3D9/D3D11 intermediate.
- The D3D8-era renderer remains a temporary compatibility/reference implementation.
- CMake + Ninja + MinGW-w64 GCC is the primary Windows modernization toolchain; Clang is secondary.
- Simulation, replay, networking, CRC, RNG, Xfer and snapshot behavior remain deterministic.
- `Int`, `UnsignedInt`, `ObjectID`, `DrawableID` remain 32-bit; `Short` remains 16-bit; `Real` remains float32.
- Pointers, `uintptr_t`, `intptr_t`, `size_t` and `ptrdiff_t` use native width and are never serialized as protocol state.
- Evolution network/replay formats use explicit fixed-width fields and versioning.
- W3D remains intact. W3X is additive EA SAGE XML support and is not connected to the runtime asset manager until the staged importer is ready.
- Future multiplayer compatibility is Evolution-to-Evolution only; retail x86 interoperability is not required.
- Prefer consolidation over duplicate helpers/files.

## Completed foundation

- Determinism characterization and fixed replay/CRC anchors.
- Command-line CMake/Ninja build foundation.
- Performance telemetry schema and summary tooling.
- x64 native-width runtime substrate.
- Renderer-free deterministic 12,000-frame timeline.
- Fixed-width Evolution command codec, EVN1 network framing and EVR1 replay framing.
- Staged Win64 EVN1 gameplay routing while legacy ACK/control/session traffic remains in place.
- EVR1 replay sidecar recording/playback with legacy replay compatibility behavior.
- Deterministic two-endpoint session validation including loss/retry, duplicates, delay/order and CRC checkpoints.
- Full-session network/replay golden transcript and corruption/version validation.
- Retirement of the active i686 MinGW modernization/oracle lane.

Historical milestone detail is kept in `Modernization/WORKLOG.md` and `Modernization/History/`.

## Step 05 — W3X parser/import foundation

Existing W3X pre-work already provides:

- W3D/W3X format recognition;
- XML document-envelope probing;
- direct `AssetDeclaration` child discovery/classification;
- one shared public API in `rts/w3x_document.h` with implementation in `Core/Libraries/Source/rts/w3x_document.cpp`.

Step 05 proceeds in coherent slices:

1. **05A — developer baseline cleanup (VERIFY):** permanent test/build naming, policy-helper consolidation, stale wrapper removal, historical-doc organization and documentation cleanup.
2. **05B — XML parser component:** select/integrate a real XML parser behind a narrow project-owned interface with useful source diagnostics.
3. **05C — neutral W3X import model + rigid mesh fixture:** decode the first supported W3X content without renderer/GPU types.
4. **05D — format routing/validation:** route W3D to the existing chunk path and W3X to the importer while keeping W3D behavior unchanged.

Runtime asset-manager integration remains deliberately deferred until these parser/import gates are stable.

## Current developer commands

Windows focused graph:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\setup-windows-dev.ps1
cmake --preset mingw64-tests
cmake --build --preset mingw64-tests
ctest --preset mingw64-tests --output-on-failure
```

The current durable explicit checks are documented in `TESTING.md`.

## Known non-blockers

Legacy `GameMemory` warnings such as custom `operator new` returning null remain known technical debt unless they become errors or affect the active work.

Representative full-client multiplayer/replay execution remains an x64 stabilization task. It is not a reason to restore the retired i686 modernization lane.
