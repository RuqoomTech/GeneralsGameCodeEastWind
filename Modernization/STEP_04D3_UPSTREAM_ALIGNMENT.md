# Step 04D3 — Upstream Alignment / Divergence Reduction

Status: **IMPLEMENTED LOCALLY — WINDOWS REVALIDATION PENDING**

Date: 2026-09-12

## Inputs

EastWind authoritative input:

- archive: `GeneralsGameCodeEastWind.zip`
- SHA-256: `e290c9bb51c4fc271ed89428b531d33298e498336f61fa4a8db6abcab5ad8502`

Upstream comparison snapshot:

- archive: `GeneralsGameCode-main (1).zip`
- SHA-256: `c5c561ca47ffe874c31732c3cb86bcc0016f246f5ee36cc3435bae50e26427d1`

The EastWind archive remains authoritative. Upstream is a correctness/gameplay source, not a replacement architecture baseline.

## Alignment result

Before integration:

- 4,482 shared files;
- 4,374 byte-identical shared files;
- 15 line-ending-only shared differences;
- 93 material shared-file differences;
- 75 EastWind-only files;
- 0 upstream-only files.

After this selective integration:

- the same 4,482 shared files remain comparable;
- 4,374 remain byte-identical;
- 45 now differ only by line endings;
- material shared-file differences fall from **93 to 63**;
- upstream still has no file that EastWind lacks.

The remaining material divergence is intentional or requires a dedicated subsystem migration rather than a blind merge.

## Imported upstream groups

### Dozer / Worker disabled-task resumption

Imported coherently for Generals and Zero Hour:

- `ObjectModule::onDisabledEdge` dispatch;
- Dozer/Worker module ownership of temporary-disable behavior;
- selective task remembering for EMP/hacked/subdued/underpowered states;
- safer resume validation for build/repair/fortify targets;
- previous-task cleanup helpers;
- removal of the Zero Hour `Object.cpp` special-case dozer hook.

The important Xfer correction is preserved exactly: version-gated fields now test the serialized/read `version`, not `currentVersion`. This prevents current code from reading v2-only fields from an older v1 stream.

### Production cancellation

Imported coherently for both editions:

- `cancelUpgrade` returns success/failure;
- internal cancellation operates directly on `ProductionEntry`;
- unit batches cannot be refunded after production has begun when retail CRC compatibility is disabled;
- cancel-all behavior handles non-refundable entries without looping forever.

### Neutron missile outer-radius correction

Imported for both editions with explicit compatibility switches:

- `PRESERVE_RETAIL_NUKE_MISSILE_OUTER_RADIUS_SEARCH`;
- `PRESERVE_RETAIL_NUKE_MISSILE_OUTER_RADIUS_DAMAGE`;
- bounding-sphere-aware range search;
- improved force/damage sampling for large structures;
- unary coordinate operators required by the implementation.

EastWind's strict-aliasing-safe floating-point helpers in `BaseType.h` remain authoritative and were not replaced by upstream's pointer-punning implementation.

### GameMemory robustness — adapted, not copied

Imported ideas while retaining Step 04C native-width allocation logic:

- compiler-portable `NOINLINE` macro;
- noinline pre-main memory-manager initialization implementation;
- release builds no longer maintain debug-only link-tester counters;
- null-safe delete/free paths;
- standard sized global delete/delete[] overloads.

The modern GCC 16 `noexcept` contract from Step 04D2 is retained on both unsized and sized standard global deletes. Native pointer alignment, `size_t` raw sizes, overflow checks, and x64 allocator layout remain EastWind-owned.

### Bink / Miles runtime loading

The source-built `Dependencies/Bink` and `Dependencies/Miles` runtime loaders are now the link seam for the real runtime instead of `binkstub` / `milesstub` import targets.

- Bink loads/unloads at video-subsystem runtime;
- WWAudio loads/unloads Miles at audio-subsystem runtime;
- on x64, the legacy 32-bit DLLs are intentionally unsupported and the loaders expose neutral fallback functions, allowing engine bring-up to continue without a hard load-time dependency;
- the old DirectX 8 dependency remains x86-only and was not widened or wrapped.

The large newer upstream MilesAudioManager lifecycle refactor was deliberately not imported in this step. It should be reviewed together with the remaining legacy WWAudio userdata/pointer-width seams so 32-bit assumptions are not copied into Evolution.

### Legacy renderer safety

Imported:

- font point-size clamp to 512;
- dynamic glyph buffers that can grow beyond the historical fixed page length when a single glyph requires it.

This is a compatibility-renderer safety improvement only; it does not change the D3D12 direction.

## Explicit non-imports

The following upstream state is intentionally not authoritative in EastWind:

- upstream's older 32-bit GameMemory size/alignment implementation;
- upstream's strict-aliasing-unsafe float bit punning;
- upstream's 32-bit `waveOutOpen` callback/userdata casts;
- wholesale upstream CMake/build replacement;
- hard Bink/Miles import-stub dependency;
- any change that widens fixed replay/network/Xfer/CRC fields to native width;
- any renderer change that creates a D3D11 intermediate path.

## Regression coverage

Step 04D3 adds:

- `upstream_coordinate_ops_step04d3`;
- `upstream_alignment_source_policy_step04d3`.

The source-policy test locks the selected upstream imports and also checks the intentional EastWind adaptations, including native GameMemory alignment, strict-aliasing-safe float helpers, runtime loader wiring, Xfer version checks, and removal of Bink/Miles stub targets.

The Step 04D fixture reader also now accepts either LF or CRLF fixture headers. Checkpoint values and deterministic behavior are unchanged.

## Validation

Local host validation after the final integration:

- GCC 14.2 O0: 17/17;
- GCC 14.2 O2: 17/17;
- GCC 14.2 O3: 17/17;
- Clang 17 O0: 17/17;
- Clang 17 O2: 17/17;
- Clang 17 O3: 17/17;
- GCC ASan: 17/17;
- GCC UBSan: 17/17.

Every configuration verifies the unchanged Step 04D 12,000-frame timeline fixture.

Immediately before Step 04D3, the user supplied a real Windows x64 run of the EastWind baseline in which MinGW-w64 GCC 16.2.0 built `mingw64-tests`, CTest passed 15/15, and `z_headlessdeterminismcheck` matched the checked-in fixture. That result signs off the pre-04D3 x64 focused lane; Step 04D3 itself still requires a Windows rerun because it adds two tests and changes gameplay/runtime source.

## Ongoing upstream-sync policy

For future upstream snapshots:

1. hash and record both snapshots;
2. normalize line endings for comparison;
3. treat EastWind modernization architecture as authoritative;
4. group upstream changes by coherent gameplay/runtime responsibility;
5. import fixes into both Generals and Zero Hour where applicable;
6. adapt native-width/build/runtime changes instead of copying x86 assumptions;
7. add a regression for every imported compatibility-sensitive behavior;
8. record intentional remaining divergence.
