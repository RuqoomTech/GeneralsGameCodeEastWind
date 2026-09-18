#include "Common/EvolutionCommandCodec.h"
#include "Common/EvolutionReplayFormat.h"
#include "Common/crc.h"
#include "GameNetwork/EvolutionProtocol.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
using Bytes = std::vector<std::uint8_t>;
constexpr std::int32_t LOGIC_CRC_MESSAGE_TYPE = 1095;
constexpr std::uint32_t GAMEPLAY_FRAMES_PER_BLOCK = 16;
constexpr std::uint32_t SESSION_BLOCKS = 4;

[[noreturn]] void Fail(const std::string &message)
{
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

void Expect(bool condition, const std::string &message)
{
    if (!condition) Fail(message);
}

void appendU32(Bytes &bytes, std::uint32_t value)
{
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
}

std::uint32_t readU32(const Bytes &bytes, std::size_t offset)
{
    if (bytes.size() - offset < 4U) Fail("truncated local transcript length");
    return static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
        (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
        (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

std::string toHex(const Bytes &bytes)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::uint8_t byte : bytes)
        out << std::setw(2) << static_cast<unsigned int>(byte);
    return out.str();
}

std::uint8_t hexNibble(char value)
{
    if (value >= '0' && value <= '9') return static_cast<std::uint8_t>(value - '0');
    if (value >= 'a' && value <= 'f') return static_cast<std::uint8_t>(value - 'a' + 10);
    if (value >= 'A' && value <= 'F') return static_cast<std::uint8_t>(value - 'A' + 10);
    Fail("invalid full-session fixture hex digit");
}

Bytes parseHex(const std::string &hex)
{
    if ((hex.size() % 2U) != 0U) Fail("odd-length full-session fixture hex");
    Bytes bytes;
    bytes.reserve(hex.size() / 2U);
    for (std::size_t i = 0; i < hex.size(); i += 2U)
        bytes.push_back(static_cast<std::uint8_t>((hexNibble(hex[i]) << 4U) | hexNibble(hex[i + 1U])));
    return bytes;
}

std::map<std::string, Bytes> loadFixtures(const char *path)
{
    std::ifstream input(path);
    if (!input) Fail(std::string("unable to open full-session fixture: ") + path);
    std::map<std::string, Bytes> fixtures;
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const std::size_t split = line.find('=');
        if (split == std::string::npos) Fail("malformed full-session fixture line");
        fixtures.emplace(line.substr(0, split), parseHex(line.substr(split + 1U)));
    }
    return fixtures;
}

const Bytes &fixture(const std::map<std::string, Bytes> &fixtures, const char *name)
{
    const auto it = fixtures.find(name);
    if (it == fixtures.end()) Fail(std::string("missing full-session fixture: ") + name);
    return it->second;
}

struct SessionEntry
{
    std::uint32_t frame = 0;
    std::uint8_t playerId = 0;
    std::uint8_t relayMask = 0;
    std::uint16_t commandId = 0;
    evolution::Command command;
};

std::uint32_t commandKey(const SessionEntry &entry)
{
    return (static_cast<std::uint32_t>(entry.playerId) << 16U) | entry.commandId;
}

Bytes encodeCommand(const evolution::Command &command)
{
    Bytes bytes;
    Expect(evolution::encodeCommandV1(command, bytes), "session command failed to encode");
    return bytes;
}

void updateLogicCrc(CRC &crc, const evolution::Command &command)
{
    if (command.messageType == LOGIC_CRC_MESSAGE_TYPE) return;
    const Bytes bytes = encodeCommand(command);
    crc.computeCRC(bytes.data(), static_cast<Int>(bytes.size()));
}

evolution::Command makeGameplayCommand(std::uint8_t playerId, std::uint32_t ordinal)
{
    evolution::Command command;
    command.messageType = static_cast<std::int32_t>(700 + playerId * 100U + (ordinal % 13U));

    evolution::CommandArgument arg;
    arg.kind = evolution::CommandArgumentKind::Integer;
    arg.signed32 = static_cast<std::int32_t>(ordinal * 37U + playerId * 11U) - 900;
    command.arguments.push_back(arg);

    arg = {};
    arg.kind = evolution::CommandArgumentKind::Real;
    arg.real32[0] = static_cast<float>(ordinal) * 0.125f + static_cast<float>(playerId);
    command.arguments.push_back(arg);

    arg = {};
    arg.kind = evolution::CommandArgumentKind::Boolean;
    arg.unsigned32 = (ordinal + playerId) & 1U;
    command.arguments.push_back(arg);

    if ((ordinal % 5U) == 0U)
    {
        arg = {};
        arg.kind = evolution::CommandArgumentKind::Location;
        arg.real32[0] = static_cast<float>(ordinal);
        arg.real32[1] = -static_cast<float>(ordinal) * 0.5f;
        arg.real32[2] = static_cast<float>(playerId) + 0.25f;
        command.arguments.push_back(arg);
    }

    return command;
}

evolution::Command makeCrcCommand(std::uint32_t value)
{
    evolution::Command command;
    command.messageType = LOGIC_CRC_MESSAGE_TYPE;
    evolution::CommandArgument arg;
    arg.kind = evolution::CommandArgumentKind::Integer;
    arg.signed32 = static_cast<std::int32_t>(value);
    command.arguments.push_back(arg);
    return command;
}

std::vector<SessionEntry> buildCanonicalSession(std::uint32_t &finalLogicCrc)
{
    std::vector<SessionEntry> entries;
    std::uint16_t nextIds[2] = {1000U, 2000U};
    std::uint32_t ordinal = 1U;
    std::uint32_t frame = 1U;
    CRC crc;

    for (std::uint32_t block = 0; block < SESSION_BLOCKS; ++block)
    {
        for (std::uint32_t frameInBlock = 0; frameInBlock < GAMEPLAY_FRAMES_PER_BLOCK; ++frameInBlock, ++frame)
        {
            for (std::uint8_t player = 0; player < 2U; ++player)
            {
                SessionEntry entry;
                entry.frame = frame;
                entry.playerId = player;
                entry.relayMask = player == 0U ? 0x02U : 0x01U;
                entry.commandId = nextIds[player]++;
                entry.command = makeGameplayCommand(player, ordinal++);
                entries.push_back(entry);
                updateLogicCrc(crc, entry.command);
            }
        }

        const std::uint32_t checkpoint = crc.get();
        for (std::uint8_t player = 0; player < 2U; ++player)
        {
            SessionEntry entry;
            entry.frame = frame;
            entry.playerId = player;
            entry.relayMask = player == 0U ? 0x02U : 0x01U;
            entry.commandId = nextIds[player]++;
            entry.command = makeCrcCommand(checkpoint);
            entries.push_back(entry);
        }
        ++frame;
    }

    finalLogicCrc = crc.get();
    return entries;
}

Bytes buildNetworkTranscript(const std::vector<SessionEntry> &canonical)
{
    std::map<std::uint32_t, std::vector<const SessionEntry *>> byFrame;
    for (const SessionEntry &entry : canonical) byFrame[entry.frame].push_back(&entry);

    Bytes transcript;
    for (const auto &framePair : byFrame)
    {
        std::vector<const SessionEntry *> sendOrder = framePair.second;
        if ((framePair.first & 1U) != 0U)
            std::reverse(sendOrder.begin(), sendOrder.end());

        for (const SessionEntry *entry : sendOrder)
        {
            evolution::NetworkCommandRecord record;
            record.playerId = entry->playerId;
            record.relayMask = entry->relayMask;
            record.commandId = entry->commandId;
            record.command = entry->command;
            Bytes packet;
            Expect(evolution::encodeRoutedCommandPacketV1(commandKey(*entry), entry->frame, {record}, packet),
                "full-session EVN1 packet encode failed");
            appendU32(transcript, static_cast<std::uint32_t>(packet.size()));
            transcript.insert(transcript.end(), packet.begin(), packet.end());
        }
    }
    return transcript;
}

Bytes buildReplayTranscript(const std::vector<SessionEntry> &canonical)
{
    Bytes transcript;
    evolution::ReplayHeader header;
    evolution::encodeReplayHeaderV1(header, transcript);
    for (const SessionEntry &entry : canonical)
    {
        evolution::ReplayCommandRecord record;
        record.frame = entry.frame;
        record.playerIndex = static_cast<std::int32_t>(entry.playerId);
        record.command = entry.command;
        Bytes encoded;
        Expect(evolution::encodeReplayCommandRecordV1(record, encoded), "full-session EVR1 record encode failed");
        transcript.insert(transcript.end(), encoded.begin(), encoded.end());
    }
    return transcript;
}

std::vector<SessionEntry> decodeNetworkTranscript(const Bytes &transcript)
{
    std::vector<SessionEntry> entries;
    std::size_t pos = 0;
    while (pos < transcript.size())
    {
        const std::uint32_t packetSize = readU32(transcript, pos);
        pos += 4U;
        if (packetSize > transcript.size() - pos) Fail("truncated full-session network transcript packet");

        evolution::NetworkPacketHeader header;
        std::vector<evolution::NetworkCommandRecord> records;
        const auto result = evolution::decodeRoutedCommandPacketV1(transcript.data() + pos, packetSize, header, records);
        Expect(result.ok(), "golden full-session EVN1 packet failed to decode");
        Expect(records.size() == 1U, "full-session expected one production routed record per datagram");

        SessionEntry entry;
        entry.frame = header.frame;
        entry.playerId = records[0].playerId;
        entry.relayMask = records[0].relayMask;
        entry.commandId = records[0].commandId;
        entry.command = records[0].command;
        Expect(header.sequence == commandKey(entry), "full-session EVN1 sequence lost player/command identity");
        entries.push_back(entry);
        pos += packetSize;
    }
    return entries;
}

bool decodeReplayTranscript(const Bytes &transcript, std::vector<SessionEntry> &entries)
{
    entries.clear();
    if (transcript.size() < evolution::REPLAY_HEADER_BYTES_V1) return false;
    evolution::ReplayHeader header;
    if (!evolution::decodeReplayHeaderV1(transcript.data(), transcript.size(), header).ok()) return false;

    std::size_t pos = evolution::REPLAY_HEADER_BYTES_V1;
    evolution::ReplaySequenceState sequence;
    while (pos < transcript.size())
    {
        evolution::ReplayCommandRecord record;
        const auto result = evolution::decodeReplayCommandRecordV1(transcript.data() + pos, transcript.size() - pos, record);
        if (!result.ok() || result.bytesConsumed == 0U ||
            !evolution::advanceReplaySequenceV1(sequence, record.frame)) return false;
        if (record.playerIndex < 0 || record.playerIndex > 1) return false;

        SessionEntry entry;
        entry.frame = record.frame;
        entry.playerId = static_cast<std::uint8_t>(record.playerIndex);
        entry.command = record.command;
        entries.push_back(entry);
        pos += result.bytesConsumed;
    }
    return true;
}

bool sameCommand(const evolution::Command &left, const evolution::Command &right)
{
    return encodeCommand(left) == encodeCommand(right);
}

void compareCanonicalReplay(
    const std::vector<SessionEntry> &canonical,
    const std::vector<SessionEntry> &replay)
{
    Expect(canonical.size() == replay.size(), "EVR1 full-session command count changed");
    for (std::size_t i = 0; i < canonical.size(); ++i)
    {
        Expect(canonical[i].frame == replay[i].frame, "EVR1 full-session frame changed");
        Expect(canonical[i].playerId == replay[i].playerId, "EVR1 full-session player identity changed");
        Expect(sameCommand(canonical[i].command, replay[i].command), "EVR1 full-session command bytes changed");
    }
}

std::vector<SessionEntry> deterministicNetworkDelivery(const Bytes &transcript, std::size_t &retries, std::size_t &duplicates)
{
    const std::vector<SessionEntry> emitted = decodeNetworkTranscript(transcript);
    std::vector<SessionEntry> arrival;
    std::vector<SessionEntry> delayed;
    std::set<std::uint32_t> seen;
    retries = 0;
    duplicates = 0;

    auto deliver = [&](const SessionEntry &entry) {
        const std::uint32_t key = commandKey(entry);
        if (!seen.insert(key).second)
        {
            ++duplicates;
            return;
        }
        arrival.push_back(entry);
    };

    for (std::size_t i = 0; i < emitted.size(); ++i)
    {
        const SessionEntry &entry = emitted[i];
        if ((i % 11U) == 0U)
        {
            ++retries; // First attempt is deterministically lost; retry carries identical bytes/identity.
            deliver(entry);
        }
        else if ((i % 7U) == 0U)
        {
            delayed.push_back(entry);
        }
        else
        {
            deliver(entry);
        }

        if ((i % 13U) == 0U)
            deliver(entry); // Duplicate datagram: ACKable but not executable twice.

        if (!delayed.empty() && (i % 3U) == 2U)
        {
            deliver(delayed.front());
            delayed.erase(delayed.begin());
        }
    }
    for (const SessionEntry &entry : delayed) deliver(entry);

    std::sort(arrival.begin(), arrival.end(), [](const SessionEntry &left, const SessionEntry &right) {
        if (left.frame != right.frame) return left.frame < right.frame;
        if (left.playerId != right.playerId) return left.playerId < right.playerId;
        return left.commandId < right.commandId;
    });
    return arrival;
}

std::uint32_t validateLogicCrcAndCheckpoints(const std::vector<SessionEntry> &entries)
{
    CRC crc;
    for (const SessionEntry &entry : entries)
    {
        if (entry.command.messageType == LOGIC_CRC_MESSAGE_TYPE)
        {
            Expect(entry.command.arguments.size() == 1U, "logic CRC checkpoint argument count changed");
            Expect(entry.command.arguments[0].kind == evolution::CommandArgumentKind::Integer,
                "logic CRC checkpoint argument kind changed");
            Expect(static_cast<std::uint32_t>(entry.command.arguments[0].signed32) == crc.get(),
                "full-session logic CRC checkpoint disagreed with deterministic state");
        }
        else
        {
            updateLogicCrc(crc, entry.command);
        }
    }
    return crc.get();
}

void checkGoldenFullSession(const std::map<std::string, Bytes> &fixtures, bool emitFixture)
{
    std::uint32_t expectedFinalCrc = 0;
    const std::vector<SessionEntry> canonical = buildCanonicalSession(expectedFinalCrc);
    const Bytes network = buildNetworkTranscript(canonical);
    const Bytes replay = buildReplayTranscript(canonical);

    if (emitFixture)
    {
        Bytes crcBytes;
        appendU32(crcBytes, expectedFinalCrc);
        Bytes countBytes;
        appendU32(countBytes, static_cast<std::uint32_t>(canonical.size()));
        std::cout << "network_stream=" << toHex(network) << '\n';
        std::cout << "replay_stream=" << toHex(replay) << '\n';
        std::cout << "final_logic_crc=" << toHex(crcBytes) << '\n';
        std::cout << "command_count=" << toHex(countBytes) << '\n';
        return;
    }

    Expect(network == fixture(fixtures, "network_stream"), "full-session EVN1 transcript changed from full-session golden bytes");
    Expect(replay == fixture(fixtures, "replay_stream"), "full-session EVR1 transcript changed from full-session golden bytes");
    Expect(fixture(fixtures, "final_logic_crc").size() == 4U, "full-session final CRC fixture size changed");
    Expect(fixture(fixtures, "command_count").size() == 4U, "full-session command-count fixture size changed");
    Expect(readU32(fixture(fixtures, "final_logic_crc"), 0) == expectedFinalCrc, "full-session final logic CRC changed");
    Expect(readU32(fixture(fixtures, "command_count"), 0) == canonical.size(), "full-session command count changed");

    std::vector<SessionEntry> replayDecoded;
    Expect(decodeReplayTranscript(replay, replayDecoded), "golden EVR1 full session failed to decode");
    compareCanonicalReplay(canonical, replayDecoded);
    Expect(validateLogicCrcAndCheckpoints(replayDecoded) == expectedFinalCrc, "EVR1 replay final logic CRC changed");

    std::size_t retries = 0;
    std::size_t duplicates = 0;
    const std::vector<SessionEntry> delivered = deterministicNetworkDelivery(network, retries, duplicates);
    Expect(delivered.size() == canonical.size(), "loss/retry/duplicate session changed unique command count");
    Expect(retries > 0U && duplicates > 0U, "full-session network delivery did not exercise retries and duplicates");
    for (std::size_t i = 0; i < canonical.size(); ++i)
    {
        Expect(delivered[i].frame == canonical[i].frame && delivered[i].playerId == canonical[i].playerId &&
            delivered[i].commandId == canonical[i].commandId && sameCommand(delivered[i].command, canonical[i].command),
            "network full-session deterministic ordering diverged from canonical command stream");
    }
    Expect(validateLogicCrcAndCheckpoints(delivered) == expectedFinalCrc,
        "network-delivered full session diverged from EVR1 replay logic CRC");
}

void checkVersionAndCorruptionRejection(const std::map<std::string, Bytes> &fixtures)
{
    const Bytes &networkTranscript = fixture(fixtures, "network_stream");
    const std::uint32_t packetSize = readU32(networkTranscript, 0);
    Expect(packetSize + 4U <= networkTranscript.size(), "golden network transcript first packet is truncated");
    Bytes packet(networkTranscript.begin() + 4, networkTranscript.begin() + 4 + packetSize);

    evolution::NetworkPacketHeader header;
    std::vector<evolution::NetworkCommandRecord> records;
    Bytes broken = packet;
    broken[4] = 2U;
    broken[5] = 0U;
    Expect(evolution::decodeRoutedCommandPacketV1(broken.data(), broken.size(), header, records).error ==
            evolution::NetworkDecodeError::UnsupportedVersion,
        "full-session gate accepted unsupported EVN1 version");

    broken = packet;
    const std::size_t commandOffset = evolution::NETWORK_HEADER_BYTES_V1 +
        evolution::NETWORK_COMMAND_BATCH_HEADER_BYTES_V1 + evolution::NETWORK_COMMAND_RECORD_HEADER_BYTES_V1;
    broken[commandOffset] = 2U;
    broken[commandOffset + 1U] = 0U;
    Expect(evolution::decodeRoutedCommandPacketV1(broken.data(), broken.size(), header, records).error ==
            evolution::NetworkDecodeError::InvalidCommand,
        "full-session gate accepted unsupported command codec inside EVN1");

    const Bytes &replay = fixture(fixtures, "replay_stream");
    evolution::ReplayHeader replayHeader;
    broken = replay;
    broken[4] = 2U;
    broken[5] = 0U;
    Expect(evolution::decodeReplayHeaderV1(broken.data(), broken.size(), replayHeader).error ==
            evolution::ReplayDecodeError::UnsupportedFormatVersion,
        "full-session gate accepted unsupported EVR1 format version");

    broken = replay;
    broken[6] = 2U;
    broken[7] = 0U;
    Expect(evolution::decodeReplayHeaderV1(broken.data(), broken.size(), replayHeader).error ==
            evolution::ReplayDecodeError::UnsupportedCommandCodecVersion,
        "full-session gate accepted unsupported EVR1 command codec version");

    broken = replay;
    const std::size_t firstReplayCommand = evolution::REPLAY_HEADER_BYTES_V1 + evolution::REPLAY_COMMAND_RECORD_HEADER_BYTES_V1;
    broken[firstReplayCommand] = 2U;
    broken[firstReplayCommand + 1U] = 0U;
    std::vector<SessionEntry> replayEntries;
    Expect(!decodeReplayTranscript(broken, replayEntries), "full-session gate accepted corrupted EVR1 command payload");

    broken.assign(replay.begin(), replay.end() - 1);
    Expect(!decodeReplayTranscript(broken, replayEntries), "full-session gate accepted truncated EVR1 tail");

    broken = replay;
    // Make the second record's frame older than the first while preserving otherwise-valid bytes.
    evolution::ReplayCommandRecord firstRecord;
    const auto firstResult = evolution::decodeReplayCommandRecordV1(
        replay.data() + evolution::REPLAY_HEADER_BYTES_V1,
        replay.size() - evolution::REPLAY_HEADER_BYTES_V1,
        firstRecord);
    Expect(firstResult.ok(), "unable to locate second replay record for ordering-corruption test");
    const std::size_t secondRecord = evolution::REPLAY_HEADER_BYTES_V1 + firstResult.bytesConsumed;
    broken[secondRecord + 0U] = 0U;
    broken[secondRecord + 1U] = 0U;
    broken[secondRecord + 2U] = 0U;
    broken[secondRecord + 3U] = 0U;
    Expect(!decodeReplayTranscript(broken, replayEntries), "full-session gate accepted backward EVR1 frame ordering");
}
} // namespace

int main(int argc, char **argv)
{
    static_assert(sizeof(void *) == 8, "Full-session validation is Evolution x64-only");
    static_assert(sizeof(float) == 4, "Full-session validation requires IEEE float32 width");

    if (argc == 2 && std::string(argv[1]) == "--emit-fixture")
    {
        const std::map<std::string, Bytes> empty;
        checkGoldenFullSession(empty, true);
        return 0;
    }
    if (argc != 2) Fail("Usage: evolution_full_session_test <fixture-file>");

    const auto fixtures = loadFixtures(argv[1]);
    checkGoldenFullSession(fixtures, false);
    checkVersionAndCorruptionRejection(fixtures);

    std::cout << "Evolution full-session golden passed: EVN1 delivery and EVR1 replay preserve one deterministic command/CRC timeline; version, corruption, truncation, and compatibility boundaries are stable.\n";
    return 0;
}
