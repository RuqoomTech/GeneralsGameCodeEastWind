# Step 01 — Determinism / CRC / Xfer Characterization

## Purpose

Create a safety net before accepting compiler, ABI, architecture, and runtime changes.

## Scope

Characterize existing behavior; do not redesign simulation.

### Required targets

- deterministic RNG state transitions/sequences;
- CRC primitives and representative state CRCs;
- Xfer primitive serialization;
- snapshot byte ordering/width assumptions where feasible;
- critical packed/layout-sensitive structures;
- replay command/CRC checkpoints using known fixtures.

## Rules

- production behavior remains authoritative;
- tests must call the same primitive implementation used by production, not a duplicate reimplementation;
- compiler warnings about undefined behavior in deterministic paths require investigation;
- no "fix" is accepted solely because GCC behaves differently from MSVC/VC6;
- changes that intentionally alter retail behavior must be separately approved and documented.

## Progress

### 01A — CRC primitive characterization — DONE

Added one reusable deterministic-primitives test source at `Core/Tests/DeterminismPrimitivesTest.cpp`. This first slice calls the production `Common/crc.h` implementation directly and locks:

- initial zero state;
- null/zero/negative-length no-op behavior;
- `clear()` reset behavior;
- representative byte/string vectors;
- incremental multi-call equivalence;
- a carry-heavy vector that crosses the high bit and exercises the rotate/add/carry behavior.

No CRC production code was changed. The debug/legacy VC6 assembly implementation, Xfer/snapshot bytes, layout-sensitive structures, and replay fixtures are not covered by this slice yet.

Clang 17 produces the locked vectors at `-O0`, `-O2`, and `-O3`. GCC 14.2 passes at `-O0`; its optimized `-O2`/`-O3` and sanitizer builds also produce the locked CRC vectors when `-Wno-strict-aliasing` suppresses pre-existing float type-punning warnings in `Lib/BaseType.h`. ASan and UBSan pass the CRC harness with that warning suppression. The warning is unrelated to the CRC code path and is recorded for later Step 01 investigation rather than silently changing deterministic float behavior in this slice.

### Standalone command

```bash
g++ -std=c++20 -Wall -Wextra -Werror \
  -Wno-unknown-pragmas -Wno-unused-parameter -pedantic \
  -DRTS_STANDALONE_DETERMINISM_TEST \
  -IDependencies/Utility \
  -ICore/Libraries/Include \
  -ICore/Libraries/Source/WWVegas \
  -ICore/GameEngine/Include \
  Core/Tests/DeterminismPrimitivesTest.cpp \
  Core/GameEngine/Source/Common/RandomValue.cpp \
  -o DeterminismPrimitivesTest

./DeterminismPrimitivesTest
```

### 01B — deterministic game-logic RNG characterization — DONE

Extended the existing `Core/Tests/DeterminismPrimitivesTest.cpp`; no new RNG test source or duplicate RNG algorithm was added. The harness links the production `Core/GameEngine/Source/Common/RandomValue.cpp` implementation and locks:

- explicit game-logic seed initialization through `InitRandom(seed)`;
- `GetGameLogicRandomSeed()` replay/base-seed stability while values are drawn;
- `GetGameLogicRandomSeedCRC()` for initial and per-draw six-word RNG state transitions;
- repeatable integer sequences for seed `0` and `0x12345678`;
- signed-range output behavior;
- same-seed reset/replay reproducibility;
- retail-compatible `lo == hi` behavior, which still consumes one RNG state transition;
- retail-compatible `GameLogicRandomValueUnchanged`, which historically delegates to the normal logic RNG and advances state while `RETAIL_COMPATIBLE_CRC` is enabled.

`RandomValue.cpp` gained a narrow `RTS_STANDALONE_DETERMINISM_TEST` compile-time include seam. It bypasses only the legacy `PreRTS.h`/`GameLogic.h` dependency surface when building this standalone harness; normal engine builds still take the unchanged precompiled-header path. This allows the test to execute the production RNG implementation instead of copying its six-word add-with-carry algorithm into test code.

GCC 14.2 and Clang 17 pass the combined CRC/RNG vectors at `-O0`, `-O2`, and `-O3`. GCC optimized/sanitized runs retain the Step 01A `-Wno-strict-aliasing` suppression for the pre-existing unrelated `Lib/BaseType.h` float type-punning warnings. ASan and UBSan pass.

Real-valued RNG output is deliberately not locked in this slice; deterministic floating-point/compiler assumptions should be characterized separately rather than mixed into the integer/state transition gate.

### 01C — Xfer primitive serialization / byte-order characterization — IMPLEMENTED, WINDOWS RUN PENDING

Extended the existing `Core/Tests/DeterminismPrimitivesTest.cpp`; no additional Xfer test source and no copied Xfer implementation were added. When `RTS_ENGINE_DETERMINISM_TEST` is enabled, the harness derives a minimal capture sink from the production `Xfer` interface and calls the real base-class primitive wrappers. This locks:

- `XferVersion`, `Byte`, `UnsignedByte`, and `Bool` widths;
- `Int`, `Int64`, `UnsignedInt`, `Short`, and `UnsignedShort` widths;
- `Real` width and the serialized `1.0f` bit pattern;
- exact primitive call ordering in the captured stream;
- the existing 32-bit Windows little-endian byte representation;
- `xferUser` raw-byte passthrough.

This test deliberately characterizes the Xfer primitive contract rather than redesigning serialization or adding endian conversion. The production methods currently pass native memory bytes to `xferImplementation`, so the reference Windows byte stream is compatibility-sensitive.

The Xfer mode is intentionally not forced into the lightweight Linux harness: the real Xfer/GameEngine header graph is tied to legacy Windows/ATL/platform infrastructure. Instead, the existing `RTS_BUILD_ZEROHOUR_EXTRAS` switch now exposes `z_determinismtest`, which links the production Zero Hour GameEngine. Normal builds are unchanged while extras are disabled.

#### Windows MinGW-w64 reference run

From a Windows shell with the repository toolchain available:

```powershell
cmake --preset mingw-w64-i686 `
  -DRTS_BUILD_ZEROHOUR_EXTRAS=ON `
  -DRTS_BUILD_ZEROHOUR_TOOLS=OFF `
  -DRTS_BUILD_CORE_TOOLS=OFF `
  -DRTS_BUILD_GENERALS=OFF

cmake --build build/mingw-w64-i686 --target z_determinismtest -j

$test = Get-ChildItem build/mingw-w64-i686 -Filter z_determinismtest.exe -Recurse |
  Select-Object -First 1
& $test.FullName
```

Expected final line:

```text
Determinism CRC, game-logic RNG, Xfer primitive, and snapshot characterization tests passed.
```

#### Windows MSVC/reference run

From a 32-bit-capable Visual Studio Build Tools developer shell:

```powershell
cmake --preset win32 `
  -DRTS_BUILD_ZEROHOUR_EXTRAS=ON `
  -DRTS_BUILD_ZEROHOUR_TOOLS=OFF `
  -DRTS_BUILD_CORE_TOOLS=OFF `
  -DRTS_BUILD_GENERALS=OFF

cmake --build build/win32 --config Release --target z_determinismtest

$test = Get-ChildItem build/win32 -Filter z_determinismtest.exe -Recurse |
  Where-Object FullName -Match 'Release' | Select-Object -First 1
& $test.FullName
```

Windows execution is intentionally recorded as pending until the target is run on the reference platform. The Linux lightweight CRC/RNG test remains green.

### 01D — representative snapshot field-order characterization — IMPLEMENTED, WINDOWS RUN PENDING

Extended the existing engine-linked branch of `Core/Tests/DeterminismPrimitivesTest.cpp`; no new test source, snapshot type, or serialization helper was added. The test derives an in-memory capture sink from the production `XferSave` class and sends a real `DamageInfoOutput` snapshot through the production `XferSave::xferSnapshot()` dispatch.

This first snapshot slice locks:

- snapshot version byte first (`DamageInfoOutput` version 1);
- `m_actualDamageDealt` immediately after the version;
- `m_actualDamageClipped` immediately after dealt damage;
- `m_noEffect` last;
- exact 32-bit Windows little-endian bytes for the representative values `1.0f`, `-2.0f`, and `true`.

The expected stream is deliberately only 10 bytes, keeping this gate easy to audit. It exercises the actual `DamageInfoOutput::xfer()` and `XferSave::xferSnapshot()` production methods while overriding only the final byte sink in the test. No changes were made to `Damage.cpp`, `XferSave.cpp`, `Xfer.cpp`, snapshot classes, save-game format, replay format, gameplay, or renderer behavior.

The same Windows `z_determinismtest` commands above run both 01C and 01D. A successful run now ends with:

```text
Determinism CRC, game-logic RNG, Xfer primitive, and snapshot characterization tests passed.
```

Next small slice after Windows confirmation: characterize one layout-sensitive deterministic structure or establish the first replay/command CRC checkpoint.

## Acceptance criteria

- tests build from a lightweight configuration where practical;
- known vectors are stored in source control;
- GCC and the existing reference build produce the same results for covered behavior;
- replay comparison procedure is documented;
- `PROJECT_STATE.md`, `BACKLOG.md`, and `WORKLOG.md` are updated.
