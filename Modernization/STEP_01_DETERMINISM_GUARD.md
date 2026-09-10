# Step 01 — Determinism / CRC / Xfer Characterization

## Purpose

Create a concrete safety net before compiler, ABI, architecture, build-system, or runtime modernization can alter deterministic behavior silently.

## Scope

Characterize and protect existing behavior; do not redesign simulation or serialization in this milestone.

### Required targets

- deterministic RNG state transitions and integer/real sequences;
- CRC primitives and representative state CRCs;
- Xfer primitive serialization and `XferCRC` folding;
- representative snapshot byte ordering;
- compiler-sensitive float helper behavior;
- critical Win32 packed/layout assumptions that affect replay/network formats;
- a known replay command-record/CRC checkpoint.

## Rules

- production behavior is authoritative;
- tests call production implementations rather than copied algorithms;
- deterministic compiler warnings must be investigated before migration;
- retail quirks remain locked unless a later change is explicitly approved;
- Win32 native-layout assumptions are compatibility constraints, not a design to copy into x64;
- Evolution/x64 must introduce explicit wire/storage layouts where native ABI would differ.

## Implementation status

**Implementation complete. Windows engine-linked sign-off pending.**

All Step 01 coverage lives in the existing `Core/Tests/DeterminismPrimitivesTest.cpp`; no family of per-substep test files was created. The engine-linked Windows branch remains the existing opt-in `z_determinismtest` target under `RTS_BUILD_ZEROHOUR_EXTRAS`.

### 01A — CRC primitive characterization — DONE

Locks the production `Common/crc.h` behavior for:

- initial/clear/no-op semantics;
- representative binary/text vectors;
- incremental updates;
- carry/high-bit behavior.

### 01B — game-logic RNG characterization — DONE

Links the production `Core/GameEngine/Source/Common/RandomValue.cpp` through the narrow `RTS_STANDALONE_DETERMINISM_TEST` include seam and locks:

- explicit seed initialization;
- base/replay-seed stability;
- six-word RNG state CRC transitions;
- deterministic integer sequences;
- signed ranges;
- equal-range retail behavior;
- reset reproducibility;
- retail-compatible `GameLogicRandomValueUnchanged` state advancement.

The completion pass additionally locks real-valued game-logic RNG by exact IEEE-754 output bits and state CRC after each draw. Equal-range real RNG is characterized separately because, unlike the retail-compatible integer equal-range path, it returns without advancing state.

### 01C — Xfer primitive serialization — DONE IN HARNESS / WINDOWS SIGN-OFF PENDING

The engine-linked branch derives a capture sink from production `Xfer` and locks:

- `XferVersion`, byte/bool, 16/32/64-bit integer and `Real` widths;
- little-endian Win32 primitive bytes;
- primitive call ordering;
- `xferUser` raw-byte passthrough.

No Xfer production serialization behavior was changed.

### 01D — representative snapshot ordering — DONE IN HARNESS / WINDOWS SIGN-OFF PENDING

A real production `DamageInfoOutput` is dispatched through `XferSave::xferSnapshot()` into the in-memory capture sink. The exact 10-byte reference stream locks:

1. snapshot version;
2. actual damage dealt;
3. actual damage clipped;
4. no-effect flag.

No `Damage`, `Snapshot`, `XferSave`, save-game, or replay implementation was changed.

### 01E — determinism guard completion — IMPLEMENTED

The completion pass closes the remaining characterization gaps without adding another test module.

#### Compiler-sensitive float helpers

The legacy `fast_float_trunc`, `fast_float_floor`, and `fast_float_ceil` algorithms are now characterized by exact bit patterns, including their historical edge behavior.

Modern compilers previously diagnosed strict-aliasing violations from pointer type-punning in these helpers. For non-VC6 builds only, the bit conversion now uses `memcpy`; the arithmetic/bit-mask algorithm is unchanged. The VC6 assembly/reference branch remains untouched.

Before/after validation compared 199,122 deterministic finite float inputs across trunc/floor/ceil and produced an identical output stream/hash. GCC then passed the lightweight determinism gate at `-O0`, `-O2`, and `-O3` with `-Werror` **without** `-Wno-strict-aliasing`.

#### Production XferCRC checkpoint

The Windows engine-linked harness now exercises production `XferCRC` directly and locks:

- initial state;
- 32-bit network-order folding;
- 16-bit and 8-bit tail handling across separate Xfer calls;
- `Real` folding.

#### Win32 ABI/layout gate

The reference compatibility target now explicitly checks the native sizes/offsets that currently leak into replay/network formats, including:

- 32-bit pointer width;
- 2-byte `WideChar`;
- `ObjectID`, `DrawableID`, `Coord3D`, `ICoord2D`, and `IRegion2D` replay widths;
- `GameMessage::Type` and replay argument-tag widths;
- packed `TransportMessageHeader` size/offsets;
- the current Win32 `GameMessage` ABI size;
- `numCommandsPerCommandPacket`;
- packed `CommandPacket` offsets and retail size.

These checks are intentionally strict. If a compiler/STL/architecture changes them, modernization must decouple the wire format rather than accepting silent packet/replay drift.

#### Replay command-record checkpoint

A known 19-byte `MSG_LOGIC_CRC` replay record fixture mirrors the raw field order used by `RecorderClass::writeToFile()`:

- frame;
- `GameMessage::Type` (`MSG_LOGIC_CRC` = 1093);
- player index;
- argument-group count;
- integer argument type/count;
- one 32-bit CRC argument.

The harness locks both the production byte-CRC result and an `XferCRC` checkpoint for that fixture, while separately locking the enum/tag values and native widths from which the replay format is formed.

This is a format/schema checkpoint, not a claim that a full `.rep` playback was executed in the Linux environment. Full replay playback remains part of the repository's existing Windows replay-compatibility procedure in `TESTING.md`.

## Validation performed locally

The lightweight production CRC/RNG/float-helper path passes:

- GCC 14.2: `-O0`, `-O2`, `-O3` with `-Wall -Wextra -Werror -pedantic`;
- Clang 17: `-O0`, `-O2`, `-O3` with the same warning policy;
- GCC AddressSanitizer;
- GCC UBSan.

The previous `-Wno-strict-aliasing` workaround is no longer needed.

W3X A0/A1/A2 standalone regressions also remain green.

## Lightweight command

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Werror \
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

Expected output:

```text
Determinism float-helper, CRC, and game-logic RNG characterization tests passed.
```

## Required Windows sign-off

Run the engine-linked compatibility gate before Step 02 accepts compiler/build changes.

### MinGW-w64 i686

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

### MSVC Win32 reference comparison

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

Expected final line for either engine-linked run:

```text
Step 01 determinism guard passed: float helpers, CRC/RNG, Xfer/XferCRC, snapshot, ABI, and replay checkpoints.
```

If the two Windows compilers disagree on any ABI/layout check, do not weaken the characterization test to make it pass. Record the mismatch and explicitly stabilize that wire/storage format before continuing compiler migration.

## Exit gate

Step 01 implementation is complete when the source tree contains the above characterization coverage. The milestone is signed off for Step 02 only after the Windows engine-linked target passes on the compatibility/reference configuration (and preferably both MinGW-w64 i686 and MSVC Win32).
