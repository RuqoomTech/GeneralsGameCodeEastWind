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

No CRC production code was changed. The debug/legacy VC6 assembly implementation, RNG, Xfer/snapshot bytes, layout-sensitive structures, and replay fixtures are not covered by this slice yet.

Clang 17 produces the locked vectors at `-O0`, `-O2`, and `-O3`. GCC 14.2 passes at `-O0`; its optimized `-O2`/`-O3` and sanitizer builds also produce the locked CRC vectors when `-Wno-strict-aliasing` suppresses pre-existing float type-punning warnings in `Lib/BaseType.h`. ASan and UBSan pass the CRC harness with that warning suppression. The warning is unrelated to the CRC code path and is recorded for later Step 01 investigation rather than silently changing deterministic float behavior in this slice.

### Standalone command

```bash
g++ -std=c++20 -Wall -Wextra -Werror \
  -Wno-unknown-pragmas -pedantic \
  -IDependencies/Utility \
  -ICore/Libraries/Include \
  -ICore/GameEngine/Include \
  Core/Tests/DeterminismPrimitivesTest.cpp \
  -o DeterminismPrimitivesTest

./DeterminismPrimitivesTest
```

Next small slice: deterministic game-logic RNG seed/state/sequence characterization, reusing this test file where practical rather than creating another parallel test module.

## Acceptance criteria

- tests build from a lightweight configuration where practical;
- known vectors are stored in source control;
- GCC and the existing reference build produce the same results for covered behavior;
- replay comparison procedure is documented;
- `PROJECT_STATE.md`, `BACKLOG.md`, and `WORKLOG.md` are updated.
