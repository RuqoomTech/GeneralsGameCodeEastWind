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

## Acceptance criteria

- tests build from a lightweight configuration where practical;
- known vectors are stored in source control;
- GCC and the existing reference build produce the same results for covered behavior;
- replay comparison procedure is documented;
- `PROJECT_STATE.md`, `BACKLOG.md`, and `WORKLOG.md` are updated.
