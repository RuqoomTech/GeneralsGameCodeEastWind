#include "Common/EvolutionCommandCodec.h"
#include "Common/EvolutionReplayFormat.h"
#include "GameNetwork/EvolutionProtocol.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{

using Bytes = std::vector<std::uint8_t>;

[[noreturn]] void Fail(const std::string &message)
{
    std::cerr << message << '\n';
    std::exit(1);
}

void Expect(bool condition, const char *message)
{
    if (!condition) Fail(message);
}

std::uint8_t HexNibble(char value)
{
    if (value >= '0' && value <= '9') return static_cast<std::uint8_t>(value - '0');
    if (value >= 'a' && value <= 'f') return static_cast<std::uint8_t>(value - 'a' + 10);
    if (value >= 'A' && value <= 'F') return static_cast<std::uint8_t>(value - 'A' + 10);
    Fail("Invalid hex fixture digit");
}

Bytes ParseHex(const std::string &hex)
{
    if ((hex.size() % 2U) != 0U) Fail("Odd-length hex fixture");
    Bytes bytes;
    bytes.reserve(hex.size() / 2U);
    for (std::size_t i = 0; i < hex.size(); i += 2U)
        bytes.push_back(static_cast<std::uint8_t>((HexNibble(hex[i]) << 4U) | HexNibble(hex[i + 1U])));
    return bytes;
}

std::map<std::string, Bytes> LoadFixtures(const char *path)
{
    std::ifstream input(path);
    if (!input) Fail(std::string("Unable to open fixture: ") + path);

    std::map<std::string, Bytes> fixtures;
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const std::size_t split = line.find('=');
        if (split == std::string::npos) Fail("Malformed Step04E fixture line");
        fixtures.emplace(line.substr(0, split), ParseHex(line.substr(split + 1U)));
    }
    return fixtures;
}

const Bytes &Fixture(const std::map<std::string, Bytes> &fixtures, const char *name)
{
    const auto it = fixtures.find(name);
    if (it == fixtures.end()) Fail(std::string("Missing fixture: ") + name);
    return it->second;
}

evolution::Command BuildGoldenCommand()
{
    evolution::Command command;
    command.messageType = 1095;

    evolution::CommandArgument arg;
    arg.kind = evolution::CommandArgumentKind::Integer;
    arg.signed32 = -123456789;
    command.arguments.push_back(arg);

    arg = {};
    arg.kind = evolution::CommandArgumentKind::Real;
    arg.real32[0] = 1.5f;
    command.arguments.push_back(arg);

    arg = {};
    arg.kind = evolution::CommandArgumentKind::Boolean;
    arg.unsigned32 = 1;
    command.arguments.push_back(arg);

    arg = {};
    arg.kind = evolution::CommandArgumentKind::ObjectId;
    arg.signed32 = 0x10203040;
    command.arguments.push_back(arg);

    arg = {};
    arg.kind = evolution::CommandArgumentKind::DrawableId;
    arg.signed32 = -7;
    command.arguments.push_back(arg);

    arg = {};
    arg.kind = evolution::CommandArgumentKind::TeamId;
    arg.unsigned32 = 0xAABBCCDDU;
    command.arguments.push_back(arg);

    arg = {};
    arg.kind = evolution::CommandArgumentKind::Location;
    arg.real32[0] = 1.0f;
    arg.real32[1] = -2.0f;
    arg.real32[2] = 3.5f;
    command.arguments.push_back(arg);

    arg = {};
    arg.kind = evolution::CommandArgumentKind::Pixel;
    arg.integer32[0] = -10;
    arg.integer32[1] = 20;
    command.arguments.push_back(arg);

    arg = {};
    arg.kind = evolution::CommandArgumentKind::PixelRegion;
    arg.integer32[0] = 1;
    arg.integer32[1] = 2;
    arg.integer32[2] = 30;
    arg.integer32[3] = 40;
    command.arguments.push_back(arg);

    arg = {};
    arg.kind = evolution::CommandArgumentKind::Timestamp;
    arg.unsigned32 = 0x11223344U;
    command.arguments.push_back(arg);

    arg = {};
    arg.kind = evolution::CommandArgumentKind::WideChar;
    arg.wideChar16 = 0x03A9U;
    command.arguments.push_back(arg);

    return command;
}

void CheckCommandRoundTrip(const std::map<std::string, Bytes> &fixtures)
{
    const evolution::Command command = BuildGoldenCommand();
    Bytes encoded;
    Expect(evolution::encodeCommandV1(command, encoded), "Command encode failed");
    Expect(encoded == Fixture(fixtures, "command"), "Command bytes changed from the Step04E fixture");
    Expect(evolution::encodedCommandSizeV1(command) == encoded.size(), "Command size calculation disagrees with encode");

    evolution::Command decoded;
    const evolution::DecodeResult result = evolution::decodeCommandV1(encoded.data(), encoded.size(), decoded);
    Expect(result.ok(), "Command decode failed");
    Expect(result.bytesConsumed == encoded.size(), "Command decoder consumed the wrong byte count");
    Expect(decoded.messageType == 1095, "Command message type changed");
    Expect(decoded.arguments.size() == 11U, "Command argument count changed");
    Expect(decoded.arguments[0].signed32 == -123456789, "Integer argument changed");
    Expect(decoded.arguments[1].real32[0] == 1.5f, "Real argument changed");
    Expect(decoded.arguments[2].unsigned32 == 1U, "Boolean argument changed");
    Expect(decoded.arguments[3].signed32 == 0x10203040, "Object ID changed");
    Expect(decoded.arguments[4].signed32 == -7, "Drawable ID changed");
    Expect(decoded.arguments[5].unsigned32 == 0xAABBCCDDU, "Team ID changed");
    Expect(decoded.arguments[6].real32[0] == 1.0f && decoded.arguments[6].real32[1] == -2.0f && decoded.arguments[6].real32[2] == 3.5f,
        "Location changed");
    Expect(decoded.arguments[7].integer32[0] == -10 && decoded.arguments[7].integer32[1] == 20, "Pixel changed");
    Expect(decoded.arguments[8].integer32[0] == 1 && decoded.arguments[8].integer32[1] == 2 &&
        decoded.arguments[8].integer32[2] == 30 && decoded.arguments[8].integer32[3] == 40, "Pixel region changed");
    Expect(decoded.arguments[9].unsigned32 == 0x11223344U, "Timestamp changed");
    Expect(decoded.arguments[10].wideChar16 == 0x03A9U, "Wide char changed");
}

void CheckCommandRejection(const Bytes &golden)
{
    evolution::Command command;

    Bytes broken = golden;
    broken[0] = 2;
    Expect(evolution::decodeCommandV1(broken.data(), broken.size(), command).error == evolution::DecodeError::UnsupportedVersion,
        "Unsupported command version was accepted");

    broken = golden;
    broken[8] = 0xffU;
    Expect(evolution::decodeCommandV1(broken.data(), broken.size(), command).error == evolution::DecodeError::InvalidArgumentKind,
        "Unknown command argument kind was accepted");

    broken = golden;
    broken[9] = 1U;
    Expect(evolution::decodeCommandV1(broken.data(), broken.size(), command).error == evolution::DecodeError::ReservedFieldNonZero,
        "Non-zero reserved argument byte was accepted");

    broken = golden;
    broken[10] = 1U;
    broken[11] = 0U;
    Expect(evolution::decodeCommandV1(broken.data(), broken.size(), command).error == evolution::DecodeError::InvalidArgumentSize,
        "Wrong command argument size was accepted");

    broken = golden;
    // Boolean payload begins after integer (8 bytes) + real (8 bytes), then 4-byte boolean header.
    const std::size_t booleanValueOffset = 8U + 8U + 8U + 4U;
    broken[booleanValueOffset] = 2U;
    Expect(evolution::decodeCommandV1(broken.data(), broken.size(), command).error == evolution::DecodeError::InvalidBoolean,
        "Non-canonical boolean was accepted");

    broken.assign(golden.begin(), golden.end() - 1);
    Expect(evolution::decodeCommandV1(broken.data(), broken.size(), command).error == evolution::DecodeError::Truncated,
        "Truncated command was accepted");

    broken = golden;
    broken[2] = 1U;
    broken[3] = 1U; // 257 arguments
    Expect(evolution::decodeCommandV1(broken.data(), broken.size(), command).error == evolution::DecodeError::TooManyArguments,
        "Oversized argument count was accepted");
}

void CheckNetworkFraming(const std::map<std::string, Bytes> &fixtures)
{
    evolution::NetworkCommandRecord record;
    record.playerId = 2U;
    record.commandId = 0x3456U;
    record.command = BuildGoldenCommand();
    std::vector<evolution::NetworkCommandRecord> records = {record};

    Bytes batch;
    Expect(evolution::encodeCommandBatchV1(records, batch), "Network command-batch encode failed");
    Expect(batch == Fixture(fixtures, "network_batch"), "Network command-batch bytes changed from the Step04E fixture");

    std::vector<evolution::NetworkCommandRecord> decodedRecords;
    Expect(evolution::decodeCommandBatchV1(batch.data(), batch.size(), decodedRecords).ok(), "Network command-batch decode failed");
    Expect(decodedRecords.size() == 1U && decodedRecords[0].playerId == 2U && decodedRecords[0].commandId == 0x3456U &&
        decodedRecords[0].command.messageType == 1095, "Network command-batch metadata changed");

    evolution::NetworkPacketHeader header;
    header.packetType = evolution::NetworkPacketType::CommandBatch;
    header.sequence = 0x12345678U;
    header.frame = 100U;

    Bytes encoded;
    Expect(evolution::encodeNetworkPacketV1(header, batch.data(), batch.size(), encoded), "Network packet encode failed");
    Expect(encoded == Fixture(fixtures, "network"), "Network bytes changed from the Step04E fixture");

    evolution::NetworkPacketHeader decodedHeader;
    const std::uint8_t *payload = nullptr;
    std::size_t payloadSize = 0;
    Expect(evolution::decodeNetworkPacketV1(encoded.data(), encoded.size(), decodedHeader, payload, payloadSize).ok(), "Network packet decode failed");
    Expect(decodedHeader.packetType == evolution::NetworkPacketType::CommandBatch && decodedHeader.sequence == 0x12345678U && decodedHeader.frame == 100U,
        "Network header changed");
    Expect(payloadSize == batch.size() && Bytes(payload, payload + payloadSize) == batch, "Network payload changed");

    Bytes brokenBatch = batch;
    brokenBatch[2] = 1U;
    Expect(evolution::decodeCommandBatchV1(brokenBatch.data(), brokenBatch.size(), decodedRecords).error == evolution::NetworkDecodeError::ReservedFieldNonZero,
        "Non-zero command-batch reserved field was accepted");
    brokenBatch = batch;
    brokenBatch[5] = 1U;
    Expect(evolution::decodeCommandBatchV1(brokenBatch.data(), brokenBatch.size(), decodedRecords).error == evolution::NetworkDecodeError::ReservedFieldNonZero,
        "Non-zero command-record reserved field was accepted");
    brokenBatch = batch;
    brokenBatch[8] += 1U;
    Expect(evolution::decodeCommandBatchV1(brokenBatch.data(), brokenBatch.size(), decodedRecords).error == evolution::NetworkDecodeError::Truncated,
        "Oversized command-record payload was accepted");

    Bytes broken = encoded;
    broken[0] ^= 0xffU;
    Expect(evolution::decodeNetworkPacketV1(broken.data(), broken.size(), decodedHeader, payload, payloadSize).error == evolution::NetworkDecodeError::InvalidMagic,
        "Invalid network magic was accepted");
    broken = encoded;
    broken[4] = 2U;
    Expect(evolution::decodeNetworkPacketV1(broken.data(), broken.size(), decodedHeader, payload, payloadSize).error == evolution::NetworkDecodeError::UnsupportedVersion,
        "Unsupported network version was accepted");
    broken = encoded;
    broken[6] = 0xffU;
    broken[7] = 0xffU;
    Expect(evolution::decodeNetworkPacketV1(broken.data(), broken.size(), decodedHeader, payload, payloadSize).error == evolution::NetworkDecodeError::InvalidPacketType,
        "Unknown network packet type was accepted");
    broken = encoded;
    broken[8] += 1U;
    Expect(evolution::decodeNetworkPacketV1(broken.data(), broken.size(), decodedHeader, payload, payloadSize).error == evolution::NetworkDecodeError::PayloadSizeMismatch,
        "Wrong network payload length was accepted");
    Expect(evolution::decodeNetworkPacketV1(encoded.data(), evolution::NETWORK_HEADER_BYTES_V1 - 1U, decodedHeader, payload, payloadSize).error == evolution::NetworkDecodeError::Truncated,
        "Truncated network header was accepted");
}

void CheckReplayFraming(const std::map<std::string, Bytes> &fixtures)
{
    evolution::ReplayHeader header;
    header.flags = 0x5A5AA5A5U;
    Bytes encodedHeader;
    evolution::encodeReplayHeaderV1(header, encodedHeader);
    Expect(encodedHeader == Fixture(fixtures, "replay_header"), "Replay header changed from the Step04E fixture");

    evolution::ReplayHeader decodedHeader;
    Expect(evolution::decodeReplayHeaderV1(encodedHeader.data(), encodedHeader.size(), decodedHeader).ok(), "Replay header decode failed");
    Expect(decodedHeader.flags == header.flags, "Replay flags changed");

    evolution::ReplayCommandRecord record;
    record.frame = 100U;
    record.playerIndex = 2;
    record.command = BuildGoldenCommand();
    Bytes encodedRecord;
    Expect(evolution::encodeReplayCommandRecordV1(record, encodedRecord), "Replay command encode failed");
    Expect(encodedRecord == Fixture(fixtures, "replay_record"), "Replay command record changed from the Step04E fixture");

    evolution::ReplayCommandRecord decodedRecord;
    const evolution::ReplayDecodeResult recordResult = evolution::decodeReplayCommandRecordV1(encodedRecord.data(), encodedRecord.size(), decodedRecord);
    Expect(recordResult.ok() && recordResult.bytesConsumed == encodedRecord.size(), "Replay command decode failed");
    Expect(decodedRecord.frame == 100U && decodedRecord.playerIndex == 2 && decodedRecord.command.messageType == 1095,
        "Replay command metadata changed");

    Bytes broken = encodedHeader;
    broken[0] ^= 0xffU;
    Expect(evolution::decodeReplayHeaderV1(broken.data(), broken.size(), decodedHeader).error == evolution::ReplayDecodeError::InvalidMagic,
        "Invalid replay magic was accepted");
    broken = encodedHeader;
    broken[4] = 2U;
    Expect(evolution::decodeReplayHeaderV1(broken.data(), broken.size(), decodedHeader).error == evolution::ReplayDecodeError::UnsupportedFormatVersion,
        "Unsupported replay version was accepted");
    broken = encodedHeader;
    broken[6] = 2U;
    Expect(evolution::decodeReplayHeaderV1(broken.data(), broken.size(), decodedHeader).error == evolution::ReplayDecodeError::UnsupportedCommandCodecVersion,
        "Unsupported replay command codec was accepted");

    broken = encodedRecord;
    broken[8] += 1U;
    Expect(evolution::decodeReplayCommandRecordV1(broken.data(), broken.size(), decodedRecord).error == evolution::ReplayDecodeError::Truncated,
        "Replay record with oversized command payload was accepted");
    broken = encodedRecord;
    broken[evolution::REPLAY_COMMAND_RECORD_HEADER_BYTES_V1] = 2U;
    Expect(evolution::decodeReplayCommandRecordV1(broken.data(), broken.size(), decodedRecord).error == evolution::ReplayDecodeError::InvalidCommand,
        "Replay record with invalid command payload was accepted");
    Expect(evolution::decodeReplayCommandRecordV1(encodedRecord.data(), 11U, decodedRecord).error == evolution::ReplayDecodeError::Truncated,
        "Truncated replay record was accepted");
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) Fail("Usage: evolution_protocol_step04e_test <fixture-file>");
    const auto fixtures = LoadFixtures(argv[1]);

    static_assert(sizeof(std::int32_t) == 4, "Evolution protocol requires int32_t");
    static_assert(sizeof(std::uint32_t) == 4, "Evolution protocol requires uint32_t");
    static_assert(sizeof(float) == 4, "Evolution protocol requires float32");
    static_assert(evolution::NETWORK_HEADER_BYTES_V1 == 20U, "Evolution network v1 header size changed");
    static_assert(evolution::REPLAY_HEADER_BYTES_V1 == 16U, "Evolution replay v1 header size changed");
    static_assert(evolution::REPLAY_COMMAND_RECORD_HEADER_BYTES_V1 == 12U, "Evolution replay command header size changed");

    CheckCommandRoundTrip(fixtures);
    CheckCommandRejection(Fixture(fixtures, "command"));
    CheckNetworkFraming(fixtures);
    CheckReplayFraming(fixtures);

    std::cout << "Step 04E Evolution protocol v1 guard passed: command codec, network framing, replay framing, golden bytes, and malformed-input rejection.\n";
    return 0;
}
