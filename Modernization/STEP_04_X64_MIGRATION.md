# Step 04 — x64 Engine Migration

## Status

**ACTIVE — Steps 04A, 04B and 04C implemented; Windows x64 validation still requires a user console run.**

Generals Evolution is an x64-only future target. The frozen i686 build is retained temporarily only as a deterministic simulation/replay reference while the x64 core is brought up. New Evolution features do not have to run on x86.

The migration must not obtain a nominal “x64 build” by widening serialized/network types, truncating pointers, disabling compatibility checks, or wrapping the legacy D3D8 renderer in unsafe casts.

## Locked architecture and compatibility rules

Native process concepts may widen on x64:

- pointers;
- `uintptr_t` / `intptr_t`;
- `size_t` / `ptrdiff_t`;
- allocator addresses and private allocator metadata;
- OS handles where the platform ABI requires it.

Game/wire/on-disk/deterministic concepts remain explicitly fixed-width unless a versioned format change is deliberately introduced:

- `Int` / `UnsignedInt` — 32-bit;
- `Short` — 16-bit;
- `ObjectID` / `DrawableID` and other logical IDs — 32-bit unless separately versioned;
- `Real` — IEEE-754 32-bit float;
- replay command fields;
- Xfer primitive widths;
- network protocol fields;
- CRC/RNG state;
- established deterministic fixtures.

Native pointer width, `size_t`, allocator state, object padding and memory addresses must never be serialized, sent over the network, or folded into deterministic CRC state.

### Multiplayer compatibility target

Evolution multiplayer compatibility is required **between our own Evolution/game editions only**. Retail 32-bit Generals/Zero Hour multiplayer interoperability is not a target.

Consequences:

- the old i686 executable is a temporary behavior/determinism oracle, not a permanent network peer;
- Evolution may introduce a clean, explicit, versioned wire protocol;
- old retail packet layout quirks do not constrain the future architecture;
- fixed-width logical fields are still required so compiler/native ABI cannot leak into the protocol;
- old replay loading is retained where practical, but it must not block the x64 architecture.

## Quick Windows dependency setup — Step 04C

The supported Windows modernization toolchain can now be bootstrapped from PowerShell with one command:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\setup-windows-dev.ps1
```

The default path prepares the x64 Evolution development lane and verifies:

- MSYS2;
- MinGW-w64 x86_64 GCC/G++;
- WIDL/tools;
- CMake;
- Ninja;
- Python;
- Git.

The script adds the MINGW64 tool directory to the user/current PATH unless `-SkipPathUpdate` is supplied.

The frozen i686 oracle is optional and explicit:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\setup-windows-dev.ps1 -IncludeLegacyX86
```

Useful maintenance modes:

```powershell
# Check an existing setup without installing/updating packages.
powershell -ExecutionPolicy Bypass -File .\scripts\setup-windows-dev.ps1 -VerifyOnly

# Keep existing MSYS2 packages untouched while filling missing dependencies.
powershell -ExecutionPolicy Bypass -File .\scripts\setup-windows-dev.ps1 -SkipMsysUpdate
```

The Linux validation environment cannot execute PowerShell or a real MinGW-w64 x86_64 toolchain, so the bootstrap is protected by a repository policy regression but is **not claimed Windows-tested** until console output is supplied.

## Step 04A — x64 readiness lane — IMPLEMENTED

Step 04A introduced the first real x64 build lane while deliberately keeping the full legacy runtime closed:

1. MinGW discovery is consolidated in `cmake/toolchains/mingw-w64-common.cmake`.
2. `mingw-w64-i686.cmake` remains the temporary oracle wrapper.
3. `mingw-w64-x86_64.cmake` selects the future target.
4. `mingw64-tests` configures/builds/runs the focused modernization graph with `RTS_BUILD_X64_READINESS=ON`.
5. Full x64 runtime configuration remains deliberately blocked until enough runtime boundaries are safe.
6. The focused x64 graph avoids the legacy D3D8/DirectInput/DirectSound and unused ATL dependency graph.
7. `architecture_width_step04a` proves that native pointer width can vary while fixed engine/wire IDs stay 32-bit and pointer round-trips use `uintptr_t`.

The readiness lane is not a claim that the whole game already runs as a Win64 executable.

## Step 04B — wire/replay ABI freeze + first pointer/handle audit — IMPLEMENTED

Step 04B established the first architecture-independent command/wire boundary and removed concrete pointer truncation without opening the D3D8 renderer graph.

Implemented:

1. `GameMessage::Type` has explicit `Int` underlying width.
2. `NetworkDefs.h` no longer derives command capacity from `sizeof(GameMessage)`.
3. The historical 1008-byte command payload / 28-command compatibility capacity is an explicit constant rather than a runtime-class ABI side effect.
4. `NetworkDefs.h` no longer includes `MessageStream.h` solely for `GameMessage` layout.
5. `WindowMsgData` is `uintptr_t`, because it is native in-process callback/userdata rather than wire state.
6. `waveOutOpen` callback/instance userdata use `DWORD_PTR`.
7. IME candidate-list address arithmetic uses typed byte pointers.
8. `wire_replay_abi_step04b` and `pointer_wire_source_audit_step04b` guard the boundary.

Step 04B freezes the old packet assumptions only long enough to prevent ABI leakage. It does **not** require Evolution multiplayer to remain retail-wire-compatible.

## Step 04C — native-width runtime substrate + dependency bootstrap — IMPLEMENTED

Step 04C moves concrete shared-runtime infrastructure to native-width-safe behavior while preserving the frozen x86 oracle where possible.

### Allocator/pool conversion

1. `WWLib/ObjectPoolClass` no longer stores its backing-block chain through `uint32*`.
   - The old Win32 layout happened to work because a pointer was four bytes.
   - On x64 it stored an eight-byte link but advanced the object start by only four bytes, allowing the block-chain pointer and first pooled object/free-list link to overlap.
   - A native `BlockHeader*` chain now owns that metadata. Its footprint remains one pointer: four bytes on x86, eight bytes on x64.
2. `GameMemory.cpp` native byte arithmetic now uses `size_t` where raw allocation sizes/strides are calculated.
3. memory-pool block alignment follows `sizeof(void*)`, retaining four-byte behavior on the frozen x86 oracle and becoming eight-byte aligned on x64.
4. blob-size multiplication is checked before allocation.
5. pointer alignment verification uses `uintptr_t` rather than truncating an address through `unsigned`.
6. `FastFixedAllocator` rounds element stride to native pointer alignment and aligns its chunk storage accordingly.
7. `FastAllocatorGeneral` replaces the fixed four-byte user prefix with a pointer-aligned header whose size remains four bytes on x86 and becomes eight bytes on x64.
8. `FastAllocatorGeneral::Realloc` copies only the valid payload prefix, fixing the old total-size over-read / shrink-overflow behavior exposed by sanitizer coverage.

`GameMemoryInit.cpp` intentionally retains its separate `MEM_BOUND_ALIGNMENT = 4`: that helper rounds **pool allocation counts**, not byte addresses. Widening it would change pool sizing behavior without solving a pointer-width problem.

### Native callback/GUI/platform cleanup

1. `WindowVideoManager` hashes native pointer keys through `uintptr_t` instead of `UnsignedInt`.
2. keyboard-layout handle extraction passes through `uintptr_t` before explicitly taking the fixed low 16 bits.
3. list-box multi-selection retrieval now returns the native `Int*` selection pointer through an `Int**` contract instead of storing that address in `Int`.
4. the modern Core GameSpy chat caller uses that explicit `Int**` overload instead of disguising an address-of-pointer as `Int*`.

These are native client/runtime values only. None is added to CRC/replay/network serialization.

### Focused regression coverage

Step 04C adds:

- `runtime_native_width_step04c` — instantiates production `ObjectPoolClass`, `FastFixedAllocator` and `FastAllocatorGeneral`, forces multiple backing blocks, checks native alignment/uniqueness/corruption and exercises realloc grow/shrink;
- `runtime_pointer_source_audit_step04c` — prevents the repaired pointer-width patterns from regressing in source paths not yet compiled by the focused graph;
- `windows_dependency_bootstrap_step04c` — locks the x64-default dependency bootstrap and requires the i686 install to remain behind `-IncludeLegacyX86`.

The focused graph therefore contains 12 tests at Step 04C.

### Step 04C audit classification

- **native pointer/address — converted:** allocator block links, allocator prefix/stride/alignment, GUI selection pointer, pointer-key hashing;
- **native handle with fixed extracted scalar — converted:** `HKL` is first represented as `uintptr_t`, then its intentional low 16-bit value is extracted;
- **logical ID/scalar — remains fixed-width:** `ObjectID`, `DrawableID`, command IDs and game/network logical integers;
- **wire/replay — remains fixed-width:** no pointer/native-size field was introduced;
- **legacy scalar-in-pointer GUI item-data tags — still to normalize as their subsystems enter the x64 compile lane; they must bridge through `uintptr_t`/`intptr_t` rather than direct pointer-to-`Int` casts;
- **renderer-only D3D8/WW3D issues — deferred:** do not widen the scope by constructing a D3D8 compatibility wrapper;
- **legacy x86 crash/debug helpers — defer or replace when the x64 platform layer is brought up rather than preserving 32-bit address APIs indefinitely.

## Remaining migration slices

### Step 04D — x64 deterministic/headless core bring-up — NEXT

- progressively compile real Common/GameLogic/runtime units in the x64 lane without the legacy renderer;
- resolve the next pointer/native-size blockers encountered by that compile lane rather than performing blind global cast replacement;
- normalize remaining scalar-in-pointer userdata contracts needed by the core/client boundary;
- enforce deterministic floating-point policy: no `-ffast-math`, unsafe reassociation or architecture-dependent state in CRC/replay;
- establish representative golden CRC timelines at frames such as 0, 1, 10, 100, 1000, 5000, 10000 and end;
- require x64 logical CRCs to match the frozen x86 oracle for the selected deterministic fixtures.

### Step 04E — Evolution network/replay protocol + x64 validation

- define an explicit versioned Evolution wire protocol rather than inheriting C++ object layout;
- serialize commands field-by-field with defined widths/endianness;
- validate Evolution x64-to-x64 multiplayer command streams and CRCs;
- add compiler/build-configuration cross-checks where practical;
- retain legacy replay reading where practical, without making retail x86 multiplayer a compatibility requirement.

### Step 04F — retire x86

After representative x64 replay/simulation/network gates are authoritative:

- remove i686 presets/toolchain/runtime support;
- remove obsolete Win32 object-layout guards that are not protocol guarantees;
- keep fixed-width game/wire/replay assertions;
- make x64 the sole authoritative Evolution engine architecture.

After the x64 core is stable enough, continue to the renderer-neutral scene/asset boundary and the D3D12 Evolution renderer. There is no D3D11 intermediate step.

## Current validation

Local host-native validation for Step 04C:

- GCC 14.2: focused CMake/Ninja/CTest at `-O0`, `-O2`, `-O3` — **12/12 passed** in each configuration;
- Clang 17: focused CMake/Ninja/CTest at `-O0`, `-O2`, `-O3` — **12/12 passed** in each configuration;
- GCC AddressSanitizer — **12/12 passed**;
- GCC UndefinedBehaviorSanitizer — **12/12 passed**;
- production allocator runtime guard reports 64-bit native pointers on the validation host.

No MinGW-w64 x86_64 compiler or PowerShell runtime is available in this environment, so no Win64 build/test/bootstrap pass is claimed.

Canonical Windows Step 04C x64 gate:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\setup-windows-dev.ps1
cmake --preset mingw64-tests
cmake --build --preset mingw64-tests
ctest --preset mingw64-tests --output-on-failure
```

Optional frozen-oracle gate:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\setup-windows-dev.ps1 -IncludeLegacyX86
cmake --preset mingw-w64-i686-determinism
cmake --build --preset mingw-w64-i686-determinism --target z_determinismcheck
```

A `mingw64-tests` pass validates the focused Step 04C x64 substrate. It is still not the full x64 game-runtime gate; Step 04D opens that lane progressively.
