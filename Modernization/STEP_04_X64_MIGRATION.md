# Step 04 — x64 Engine Migration

## Status

**ACTIVE — Step 04B implemented; Windows x64 validation pending user console run.**

The modernization program now moves the engine toward x64 immediately after the performance baseline. The signed-off i686 runtime remains the deterministic compatibility/reference build throughout the migration.

The migration is staged. The project must not obtain a nominal “x64 build” by widening serialized/network types, disabling compatibility checks, or wrapping the whole legacy D3D8 renderer in unsafe casts.

## Locked compatibility rule

Native process concepts may widen on x64:

- pointers;
- `size_t` / `ptrdiff_t`;
- allocator addresses;
- OS handles where the platform ABI requires it.

Wire/on-disk/deterministic concepts remain explicitly fixed-width unless a versioned format change is approved:

- `Int` / `UnsignedInt`;
- `ObjectID` / `DrawableID` / formation IDs;
- replay command fields;
- Xfer primitive widths;
- network packet fields;
- CRC/RNG state;
- established save/replay fixtures.

Pointer values must never be made compatible by truncating them to 32-bit integers.

## Step 04A — x64 readiness lane — IMPLEMENTED

Step 04A introduces the first real x64 build lane while deliberately keeping the full runtime closed:

1. MinGW toolchain discovery is consolidated in `cmake/toolchains/mingw-w64-common.cmake`.
2. `mingw-w64-i686.cmake` remains the tiny compatibility/reference wrapper.
3. `mingw-w64-x86_64.cmake` selects the new x64 lane.
4. `mingw64-tests` configures/builds/runs the focused modernization graph with `RTS_BUILD_X64_READINESS=ON`.
5. Full x64 runtime configuration is intentionally rejected with a diagnostic until the required runtime boundaries are ported.
6. Focused tests no longer drag D3D8/DirectInput/DirectSound link requirements into the architecture-readiness graph.
7. ReactOS ATL is retained for the signed-off i686 determinism target but is not populated for the x64 focused graph when there is no consumer.
8. `architecture_width_step04a` proves that native pointer width can vary while fixed engine/wire IDs remain 32-bit and pointer round-trips use `uintptr_t`.

The x64 readiness lane is not a claim that Zero Hour itself is already an x64 executable.

## Step 04B — wire/replay ABI freeze + pointer/handle audit — IMPLEMENTED

Step 04B establishes the first architecture-independent command/wire boundary and fixes concrete native-pointer truncation in the x64 core/client lane without opening the D3D8 renderer graph.

Implemented changes:

1. `GameMessage::Type` now has explicit `Int` underlying width, preserving the 32-bit replay/network command type independently of compiler enum ABI choices.
2. `NetworkDefs.h` no longer sizes the legacy command byte buffer from the runtime `GameMessage` class. The historical 1008-byte command payload and 28-command compatibility count are frozen as protocol constants.
3. `NetworkDefs.h` no longer includes `MessageStream.h`; the packet contract is no longer coupled to the runtime message-object definition.
4. `WindowMsgData` is now `uintptr_t`. It is an in-process GUI callback payload that routinely carries pointers and therefore follows native pointer width; it is explicitly not wire/replay/CRC state.
5. `waveOutOpen` callback and instance userdata now use `DWORD_PTR`, eliminating Win64 truncation of both the callback address and `this`.
6. IME candidate-list address arithmetic now uses typed byte-pointer arithmetic instead of converting the candidate-list pointer through `UnsignedInt`.
7. `wire_replay_abi_step04b` guards fixed packet/scalar widths and the frozen legacy command-packet byte layout.
8. `pointer_wire_source_audit_step04b` prevents reintroduction of the three concrete pointer-width truncations and the `GameMessage`-layout packet sizing dependency.

Audit classification for this slice:

- **native pointer/address — fixed now:** GUI `WindowMsgData`, wave-output callback userdata, IME candidate-list address arithmetic;
- **logical IDs — remain 32-bit:** `ObjectID`, `DrawableID`, player/team/squad IDs and command IDs;
- **wire/replay — remain fixed-width:** command type, packet frame/count/header fields, argument tags, replay/network primitive fields;
- **legacy ABI assumption — isolated:** the old 1008-byte command-buffer capacity is retained as an explicit frozen compatibility constant rather than inferred from a C++ object;
- **renderer-only / full-client legacy casts — deferred:** D3D8/WW3D and other paths not required by the focused x64 core lane remain for later subsystem bring-up rather than being papered over with casts.

The live `NetGameCommandMsg` packet path already performs field-oriented serialization. Step 04B does not version or redesign that protocol; it removes the architecture-dependent capacity calculation and freezes the scalar contract before deeper x64 bring-up.

## Remaining migration slices

### Step 04C — deeper pointer-width/runtime conversion

- memory pools, allocator metadata and free lists;
- containers and pointer-based indices/order;
- file/resource and platform abstractions;
- remaining callback/userdata/handle assumptions required by the x64 core lane.

### Step 04D — x64 deterministic simulation

- progressively add common/game-logic units to the x64 build graph;
- define/enforce deterministic floating-point policy (no fast-math/reassociation);
- establish golden replay CRC timelines and compare x86 oracle versus x64.

### Step 04E — x64 network/replay validation

- x64-to-x64 multiplayer command-stream validation;
- identical logical CRC timelines;
- replay compatibility where practical;
- explicit protocol compatibility gates.

### Step 04F — retire x86

- remove i686 build/runtime support only after the golden replay/network gates pass;
- retain fixed-width protocol/replay invariants, not obsolete Win32 object-layout assumptions;
- x64 becomes the sole authoritative Evolution engine architecture.

## Current validation

Step 04B adds two focused regressions, bringing the focused graph to 9 tests. Host-native validation results for this repository are recorded in the worklog. The validation container does not provide a MinGW-w64 x86_64 compiler, so no Windows x64 result is claimed. The canonical Windows x64 command is:

```powershell
cmake --preset mingw64-tests
cmake --build --preset mingw64-tests
ctest --preset mingw64-tests --output-on-failure
```

A successful `mingw64-tests` run is the Step 04B Windows x64-readiness gate. It is **not** the full x64 runtime gate.
