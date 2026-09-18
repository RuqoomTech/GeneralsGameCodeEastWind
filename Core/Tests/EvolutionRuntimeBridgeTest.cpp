#include "Common/EvolutionCommandCodec.h"
#include "Common/EvolutionReplayFormat.h"
#include "GameNetwork/EvolutionProtocol.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
using Bytes = std::vector<std::uint8_t>;

[[noreturn]] void Fail(const char *message)
{
    std::cerr << message << '\n';
    std::exit(1);
}

void Expect(bool condition, const char *message)
{
    if (!condition) Fail(message);
}

evolution::Command BuildCommand()
{
    evolution::Command command;
    command.messageType = 1095;

    evolution::CommandArgument integer;
    integer.kind = evolution::CommandArgumentKind::Integer;
    integer.signed32 = -17;
    command.arguments.push_back(integer);

    evolution::CommandArgument location;
    location.kind = evolution::CommandArgumentKind::Location;
    location.real32[0] = 1.25f;
    location.real32[1] = -5.5f;
    location.real32[2] = 42.0f;
    command.arguments.push_back(location);
    return command;
}
} // namespace

int main()
{
    evolution::NetworkCommandRecord networkRecord;
    networkRecord.playerId = 3U;
    networkRecord.relayMask = 0x52U;
    networkRecord.commandId = 0x2345U;
    networkRecord.command = BuildCommand();

    Bytes originalCommandBytes;
    Expect(evolution::encodeCommandV1(networkRecord.command, originalCommandBytes), "Unable to encode source command");

    std::vector<evolution::NetworkCommandRecord> records = {networkRecord};
    Bytes routedBatch;
    Expect(evolution::encodeRoutedCommandBatchV1(records, routedBatch), "Unable to encode routed command batch");

    evolution::NetworkPacketHeader packetHeader;
    packetHeader.packetType = evolution::NetworkPacketType::RoutedCommandBatch;
    packetHeader.sequence = 0x03002345U;
    packetHeader.frame = 1234U;

    Bytes packetBytes;
    Expect(evolution::encodeNetworkPacketV1(packetHeader, routedBatch.data(), routedBatch.size(), packetBytes),
        "Unable to encode EVN1 routed packet");

    evolution::NetworkPacketHeader decodedHeader;
    const std::uint8_t *payload = nullptr;
    std::size_t payloadSize = 0;
    Expect(evolution::decodeNetworkPacketV1(packetBytes.data(), packetBytes.size(), decodedHeader, payload, payloadSize).ok(),
        "Unable to decode EVN1 routed packet");
    Expect(decodedHeader.packetType == evolution::NetworkPacketType::RoutedCommandBatch &&
        decodedHeader.frame == 1234U && decodedHeader.sequence == 0x03002345U,
        "EVN1 routed packet metadata changed");

    std::vector<evolution::NetworkCommandRecord> decodedRecords;
    Expect(evolution::decodeRoutedCommandBatchV1(payload, payloadSize, decodedRecords).ok() && decodedRecords.size() == 1U,
        "Unable to decode EVN1 routed batch");
    Expect(decodedRecords[0].playerId == networkRecord.playerId &&
        decodedRecords[0].relayMask == networkRecord.relayMask &&
        decodedRecords[0].commandId == networkRecord.commandId,
        "EVN1 routed record identity changed");

    evolution::ReplayCommandRecord replayRecord;
    replayRecord.frame = decodedHeader.frame;
    replayRecord.playerIndex = decodedRecords[0].playerId;
    replayRecord.command = decodedRecords[0].command;

    Bytes replayBytes;
    Expect(evolution::encodeReplayCommandRecordV1(replayRecord, replayBytes), "Unable to encode EVR1 replay record");

    evolution::ReplayCommandRecord decodedReplay;
    const evolution::ReplayDecodeResult replayResult =
        evolution::decodeReplayCommandRecordV1(replayBytes.data(), replayBytes.size(), decodedReplay);
    Expect(replayResult.ok() && replayResult.bytesConsumed == replayBytes.size(), "Unable to decode EVR1 replay record");
    Expect(decodedReplay.frame == decodedHeader.frame && decodedReplay.playerIndex == networkRecord.playerId,
        "EVR1 frame/player identity changed");

    Bytes replayCommandBytes;
    Expect(evolution::encodeCommandV1(decodedReplay.command, replayCommandBytes), "Unable to re-encode EVR1 command");
    Expect(replayCommandBytes == originalCommandBytes,
        "Command payload changed between routed EVN1 and EVR1");

    std::cout << "Evolution runtime bridge passed: routed EVN1 metadata and EVR1 command payloads remain stable.\n";
    return 0;
}
