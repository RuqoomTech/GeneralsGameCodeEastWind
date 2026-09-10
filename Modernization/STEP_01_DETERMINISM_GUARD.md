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

**DONE. Focused Windows sign-off passed on MinGW-w64 i686 / GCC 16.2 + Ninja on 2026-09-10.**

All Step 01 coverage lives in the existing `Core/Tests/DeterminismPrimitivesTest.cpp`; no family of per-substep test files was created. The Windows branch remains the existing opt-in `z_determinismtest` target under `RTS_BUILD_ZEROHOUR_EXTRAS`, but it is now a focused standalone target. It compiles the production RandomValue, Snapshot, Xfer, XferCRC, and Damage implementation units directly and does not link `z_gameengine`.

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

### 01C — Xfer primitive serialization — DONE

The focused Windows branch derives a capture sink from production `Xfer` and locks:

- `XferVersion`, byte/bool, 16/32/64-bit integer and `Real` widths;
- little-endian Win32 primitive bytes;
- primitive call ordering;
- `xferUser` raw-byte passthrough.

No Xfer production serialization behavior was changed.

### 01D — representative snapshot ordering — DONE

A test-only subclass exposes the protected real production `DamageInfoOutput::xfer()` method and dispatches it through the production `Xfer` primitive wrappers into the in-memory capture sink. This avoids the disk-oriented `XferSave` allocator/file graph while preserving the production snapshot field-order implementation. The exact 10-byte reference stream locks:

1. snapshot version;
2. actual damage dealt;
3. actual damage clipped;
4. no-effect flag.

Normal `Damage`, `Snapshot`, Xfer, save-game, and replay behavior is unchanged; only standalone-test compile seams are active under `RTS_STANDALONE_DETERMINISM_TEST`.

### 01E — determinism guard completion — IMPLEMENTED

The completion pass closes the remaining characterization gaps without adding another test module.

#### Compiler-sensitive float helpers

The legacy `fast_float_trunc`, `fast_float_floor`, and `fast_float_ceil` algorithms are now characterized by exact bit patterns, including their historical edge behavior.

Modern compilers previously diagnosed strict-aliasing violations from pointer type-punning in these helpers. For non-VC6 builds only, the bit conversion now uses `memcpy`; the arithmetic/bit-mask algorithm is unchanged. The VC6 assembly/reference branch remains untouched.

Before/after validation compared 199,122 deterministic finite float inputs across trunc/floor/ceil and produced an identical output stream/hash. GCC then passed the lightweight determinism gate at `-O0`, `-O2`, and `-O3` with `-Werror` **without** `-Wno-strict-aliasing`.

#### Production XferCRC checkpoint

The focused Windows harness exercises production `XferCRC` directly and locks:

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
- `GameMessage::Type` (`MSG_LOGIC_CRC` = 1095);
- player index;
- argument-group count;
- integer argument type/count;
- one 32-bit CRC argument.

The harness locks both the production byte-CRC result and an `XferCRC` checkpoint for that fixture, while separately locking the enum/tag values and native widths from which the replay format is formed.

This is a format/schema checkpoint, not a claim that a full `.rep` playback was executed in the Linux environment. Full replay playback remains part of the repository's existing Windows replay-compatibility procedure in `TESTING.md`.

### 01F — Windows sign-off harness decoupling — IMPLEMENTED

The first real MinGW-w64 GCC 16.2/Ninja user run compiled the full Zero Hour dependency graph through all 653 compilation steps, then failed only while linking `z_determinismtest` because that test linked the monolithic `z_gameengine` archive. The archive expects executable/device globals such as `TheKey_*`, `MapObject` storage, UI hooks, renderer hooks, and main-program language-file globals that are unrelated to Step 01.

The sign-off harness was therefore corrected rather than adding fake globals:

- `z_determinismtest` no longer links `z_gameengine`;
- it directly compiles the six files required by the gate: the existing test plus production RandomValue, Snapshot, Xfer, XferCRC, and Damage implementation units;
- standalone-only Xfer bodies outside the characterized primitive surface are inert, while the characterized primitive methods retain their production implementation;
- the real `DamageInfoOutput::xfer()` implementation remains under test;
- `XferSave` is no longer required by the capture harness;
- the MinGW i686 preset now uses Ninja instead of Unix Makefiles;
- the toolchain resolves both native MSYS2 unprefixed tools and conventional cross-prefixed tools;
- modern MinGW's built-in `_com_util` conversion helpers are no longer redefined by `comsupp_compat.h`;
- `z_determinismcheck` builds and runs the test in one target invocation.

These changes are test/build seams only; normal engine Xfer/Damage behavior remains on the unchanged production branches.

### 01G — Windows sign-off and replay-vector correction — DONE

The final user validation ran `z_determinismcheck` successfully on Windows using MinGW-w64 i686 / GCC 16.2 + Ninja and produced:

```text
Step 01 determinism guard passed: float helpers, CRC/RNG, Xfer/XferCRC, snapshot, ABI, and replay checkpoints.
```

During that final stabilization, two details from the provisional harness were corrected before sign-off:

- the current Zero Hour `GameMessage::MSG_LOGIC_CRC` enum value is `1095`, not the provisional `1093` checkpoint;
- the 19-byte replay fixture and its production CRC/XferCRC expected values were updated to match the corrected message value.

The focused MinGW target also uses supported interprocedural optimization when available so unused legacy inline/vtable material does not force unrelated MemoryPool/audio/module implementations into the standalone link. The successful Windows run is the authoritative sign-off evidence for this configuration.

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

## Windows sign-off

The focused compatibility gate has passed on the primary MinGW-w64 i686 path. Keep the commands below as the repeatable regression gate for Step 02 and later milestones.

### MinGW-w64 i686

The dedicated preset uses Ninja, enables only the required Zero Hour extras path, disables unrelated Generals/core tool targets, and resolves native MSYS2 MINGW32 tool names/paths.

```powershell
cmake --preset mingw-w64-i686-determinism
cmake --build --preset mingw-w64-i686-determinism --target z_determinismcheck
```

The second command builds and runs the gate in one step. After the first configure, normal reruns require only the second command.

### MSVC Win32 reference comparison (optional secondary comparison)

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

Expected final line:

```text
Step 01 determinism guard passed: float helpers, CRC/RNG, Xfer/XferCRC, snapshot, ABI, and replay checkpoints.
```

If a future Windows compiler disagrees on any ABI/layout check, do not weaken the characterization test to make it pass. Record the mismatch and explicitly stabilize that wire/storage format before continuing compiler migration.

## Exit gate

**PASSED.** The source tree contains the required characterization coverage and the focused Windows target passed on the primary MinGW-w64 i686 compatibility/reference configuration. MSVC Win32 remains a useful secondary comparison, but it is not required to begin Step 02.
