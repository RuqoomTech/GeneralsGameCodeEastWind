# Step 04 — x64 Engine Migration

## Status

**ACTIVE — Step 04A implemented.**

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

## Planned migration slices

### Step 04B — pointer/handle correctness

- inventory pointer-to-`Int`/`DWORD`/`LONG` conversions in runtime code;
- replace address storage/arithmetic with pointer-sized types or real pointers;
- distinguish numeric IDs from addresses/handles;
- keep x86 behavior byte-compatible.

### Step 04C — allocator/pool/container bring-up

- audit memory pools, free lists, alignment and pointer arithmetic;
- remove 32-bit address assumptions;
- add x64 focused coverage for pool metadata before enabling them in the runtime.

### Step 04D — serialization/network/native-layout separation

- remove any dependence on native `sizeof(pointer)` or x64 STL object layout from replay/network/on-disk formats;
- preserve Step 01 byte fixtures;
- introduce explicit wire structs/conversion seams where native runtime objects cannot remain layout-compatible.

### Step 04E — x64 common/game-logic compile lane

- progressively add shared libraries and deterministic game-logic units to the x64 build graph;
- keep renderer/client platform dependencies excluded until their boundaries are ready;
- run deterministic CRC/RNG/replay compatibility comparisons against x86.

### Step 04F — x64 client/platform runtime

- port Win32 handles, window/input/audio/file/platform seams;
- resolve third-party architecture availability explicitly;
- do not resurrect D3D11 or make D3D8 emulation the Evolution architecture.

### Step 04G — x64 executable handoff

- enable the full x64 runtime once deterministic/core/client dependencies are clean;
- preserve the x86 reference executable in parallel;
- hand rendering to the renderer-neutral boundary and then D3D12 milestones.

## Current validation

Host-native 64-bit GCC 14.2 and Clang 17 focused suites pass 7/7 tests, including the architecture guard. The same focused suite passes at `-O0`, `-O2`, and `-O3`; GCC ASan and UBSan are also green. The validation container does not provide a MinGW-w64 x86_64 compiler, so no Windows x64 result is claimed from this step. The canonical Windows x64 command is:

```powershell
cmake --preset mingw64-tests
cmake --build --preset mingw64-tests
ctest --preset mingw64-tests --output-on-failure
```

A successful `mingw64-tests` run is the Step 04A Windows gate. It is **not** the full x64 runtime gate.
