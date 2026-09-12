# Step 04E — Evolution Network / Replay Protocol

## Status

**ACTIVE. 04E1 protocol foundation implemented on 2026-09-12.**

Step 04D is fully Windows cross-architecture verified: the x64 and frozen i686 focused graphs both passed 17/17, the Step 01 i686 determinism gate passed, and the 12,000-frame i686/x64 timelines matched at all eight checkpoints. Step 04E therefore stops treating the retail x86 ABI as a future multiplayer contract.

## Compatibility policy

- Future multiplayer is Evolution-to-Evolution only.
- The runtime is x64-only once Step 04F retires the frozen oracle.
- Retail Generals/Zero Hour x86 multiplayer interoperability is not required.
- Old replay loading remains a best-effort compatibility path and must not define new protocol layout.
- Protocol/replay fields are fixed-width and little-endian regardless of compiler, optimization level, pointer width, or allocator layout.

## 04E1 implementation

### Shared command codec

`Common/EvolutionCommandCodec.h/.cpp` defines command codec v1 independently of `GameMessage` object layout.

Command header v1:

| Field | Width |
|---|---:|
| codec version | u16 |
| argument count | u16 |
| message type | i32 |

Each argument is encoded as:

| Field | Width |
|---|---:|
| argument kind | u8 |
| reserved | u8 (must be zero) |
| payload bytes | u16 |
| typed payload | explicit |

Payload widths are protocol definitions, not `sizeof(runtime_type)`: integer/object/drawable IDs are i32, team/timestamp are u32, real is IEEE-754 float32, boolean is canonical u8 0/1, location is three float32 values, pixel is two i32 values, pixel region is four i32 values, and wide-char is one u16 code unit.

`Common/EvolutionGameMessageAdapter.h/.cpp` is the only shared runtime adapter between `GameMessage` and the protocol DTO. This keeps network and future replay integration from growing duplicate conversion switches.

### Network framing

`GameNetwork/EvolutionProtocol.h/.cpp` defines Evolution Network Protocol v1:

- magic `EVN1`;
- protocol version u16;
- packet type u16;
- payload length u32;
- sequence u32;
- frame u32.

Command-batch payloads contain a u16 command count plus explicitly framed records. Each record carries player ID u8, command ID u16, payload length u32, and one command-codec-v1 payload. Reserved bytes are required to be zero. Unknown versions/types, invalid lengths, truncated records and malformed command payloads are rejected.

The existing `NetPacketGameCommandData` command payload path is migrated to the shared Evolution command codec. This intentionally changes Evolution command payload bytes; retail x86 multiplayer compatibility is not a target. The older transport/small-packet outer framing still exists while the full runtime is brought into the x64 lane and will be replaced/routed through the Evolution v1 outer packet in the next 04E integration slice.

### Replay framing

`Common/EvolutionReplayFormat.h/.cpp` defines an additive Evolution replay v1 container:

- magic `EVR1`;
- replay format version u16;
- command-codec version u16;
- header byte count u32;
- flags u32.

Each command record carries frame u32, player index i32, payload size u32, then the exact command-codec-v1 bytes used by networking.

The legacy `Recorder` reader/writer remains untouched in 04E1 so existing `.rep` compatibility is not destroyed. Wiring new recordings/playback to the Evolution container belongs to the next 04E runtime integration slice.

## Golden fixtures and rejection gates

`Core/Tests/Fixtures/Step04EEvolutionProtocolV1.txt` freezes exact bytes for:

- a command containing every supported argument kind;
- a command-batch payload;
- a full Evolution network v1 packet;
- an Evolution replay v1 header;
- an Evolution replay command record.

`evolution_protocol_v1_step04e` verifies exact bytes, encode/decode round trips, and rejection of unsupported versions, invalid argument kinds/sizes, non-canonical booleans, non-zero reserved fields, truncation, invalid packet types and payload-size mismatches.

`evolution_protocol_source_policy_step04e` prevents regression to native-layout serialization and requires the live game-command network payload path to remain routed through the shared codec.

The Windows focused graph also compiles `EvolutionGameMessageAdapter.cpp` so the production runtime adapter is checked by both MinGW i686 and x86_64 lanes even though host Unix validation cannot include the historical `<new.h>` GameMemory header.

## 04E1 local validation

The sealed 04E1 candidate passes the focused **19/19** test graph in all of these local configurations:

- GCC 14.2: O0, O2, O3;
- Clang 17: O0, O2, O3;
- GCC AddressSanitizer;
- GCC UndefinedBehaviorSanitizer.

Every configuration also passes `z_headlessdeterminismcheck` against the unchanged Step 04D fixture and `z_evolutionprotocolcheck` against the frozen Step 04E v1 byte fixture. This establishes compiler/optimization/sanitizer stability for the protocol foundation. It is not a substitute for the real Windows `mingw64-tests` 19-test gate or an x64-to-x64 multiplayer session.

## What 04E1 deliberately does not claim

- No real x64-to-x64 multiplayer session has been run yet.
- The Evolution outer network packet is not yet the transport's sole packet container.
- The new replay container is not yet the Recorder default.
- The frozen i686 oracle is not removed yet.

Those are 04E2/04E3 validation and integration work. Step 04F may remove i686 only after representative Evolution network/replay sessions and golden command streams are authoritative.
