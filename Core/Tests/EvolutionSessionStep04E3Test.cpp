#include "Common/EvolutionCommandCodec.h"
#include "Common/crc.h"
#include "GameNetwork/EvolutionProtocol.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
constexpr std::int32_t LOGIC_CRC_MESSAGE_TYPE = 1095;

void Fail(const std::string &message)
{
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

void Expect(bool condition, const std::string &message)
{
    if (!condition)
        Fail(message);
}

std::uint32_t commandKey(const evolution::NetworkCommandRecord &record)
{
    return (static_cast<std::uint32_t>(record.playerId) << 16U) | record.commandId;
}

evolution::Command makeIntegerCommand(std::int32_t messageType, std::int32_t value)
{
    evolution::Command command;
    command.messageType = messageType;
    evolution::CommandArgument argument;
    argument.kind = evolution::CommandArgumentKind::Integer;
    argument.signed32 = value;
    command.arguments.push_back(argument);
    return command;
}

evolution::NetworkCommandRecord makeRecord(
    std::uint8_t playerId,
    std::uint8_t relayMask,
    std::uint16_t commandId,
    std::int32_t messageType,
    std::int32_t value)
{
    evolution::NetworkCommandRecord record;
    record.playerId = playerId;
    record.relayMask = relayMask;
    record.commandId = commandId;
    record.command = makeIntegerCommand(messageType, value);
    return record;
}

std::vector<std::uint8_t> makePacket(std::uint32_t frame, const evolution::NetworkCommandRecord &record)
{
    std::vector<evolution::NetworkCommandRecord> records(1, record);
    std::vector<std::uint8_t> packet;
    Expect(evolution::encodeRoutedCommandPacketV1(commandKey(record), frame, records, packet),
        "production routed packet encode failed");
    return packet;
}

struct Ack
{
    std::uint8_t originalPlayerId = 0;
    std::uint16_t commandId = 0;
};

class Endpoint
{
public:
    explicit Endpoint(std::uint8_t slot) : m_slot(slot) {}

    void addLocal(std::uint32_t frame, const evolution::NetworkCommandRecord &record)
    {
        accept(frame, record);
    }

    bool receive(const std::vector<std::uint8_t> &packet, Ack &ack)
    {
        evolution::NetworkPacketHeader header;
        std::vector<evolution::NetworkCommandRecord> records;
        const evolution::NetworkDecodeResult result = evolution::decodeRoutedCommandPacketV1(
            packet.data(), packet.size(), header, records);
        if (!result.ok())
        {
            ++m_rejectedDatagrams;
            return false;
        }

        Expect(records.size() == 1U, "runtime Step04E2 sender must emit one routed record per gameplay datagram");
        const evolution::NetworkCommandRecord &record = records.front();
        Expect(header.sequence == commandKey(record), "EVN1 sequence no longer preserves player/command identity");
        Expect((record.relayMask & static_cast<std::uint8_t>(1U << m_slot)) != 0,
            "routed command relay mask does not include receiving endpoint");

        ack.originalPlayerId = record.playerId;
        ack.commandId = record.commandId;

        const std::uint32_t key = commandKey(record);
        if (!m_seen.insert(key).second)
        {
            ++m_duplicateDatagrams;
            return true; // Legacy ACK semantics still acknowledge a retry/duplicate.
        }

        m_frames[header.frame].push_back(record);
        ++m_acceptedRemoteCommands;
        return true;
    }

    bool executeFrame(std::uint32_t frame, std::size_t expectedCommands)
    {
        std::vector<evolution::NetworkCommandRecord> &commands = m_frames[frame];
        if (commands.size() != expectedCommands)
        {
            ++m_frameStalls;
            return false;
        }

        std::sort(commands.begin(), commands.end(), [](const evolution::NetworkCommandRecord &left,
                                                       const evolution::NetworkCommandRecord &right) {
            if (left.playerId != right.playerId)
                return left.playerId < right.playerId;
            return left.commandId < right.commandId;
        });

        for (const evolution::NetworkCommandRecord &record : commands)
        {
            m_executionOrder.push_back(commandKey(record));
            if (record.command.messageType == LOGIC_CRC_MESSAGE_TYPE)
                continue;

            std::vector<std::uint8_t> encoded;
            Expect(evolution::encodeCommandV1(record.command, encoded), "command re-encode failed during logic CRC update");
            m_logicCrc.computeCRC(encoded.data(), static_cast<Int>(encoded.size()));
        }
        m_frames.erase(frame);
        return true;
    }

    std::uint32_t logicCrc() const { return m_logicCrc.get(); }
    std::size_t acceptedRemoteCommands() const { return m_acceptedRemoteCommands; }
    std::size_t duplicateDatagrams() const { return m_duplicateDatagrams; }
    std::size_t rejectedDatagrams() const { return m_rejectedDatagrams; }
    std::size_t frameStalls() const { return m_frameStalls; }
    const std::vector<std::uint32_t> &executionOrder() const { return m_executionOrder; }

    std::uint32_t crcCheckpointValue(std::uint32_t frame) const
    {
        const auto it = m_frames.find(frame);
        if (it == m_frames.end())
            Fail("CRC checkpoint frame missing");
        for (const evolution::NetworkCommandRecord &record : it->second)
        {
            if (record.command.messageType == LOGIC_CRC_MESSAGE_TYPE && record.command.arguments.size() == 1U)
                return static_cast<std::uint32_t>(record.command.arguments[0].signed32);
        }
        Fail("CRC checkpoint command missing");
        return 0;
    }

private:
    void accept(std::uint32_t frame, const evolution::NetworkCommandRecord &record)
    {
        const std::uint32_t key = commandKey(record);
        if (m_seen.insert(key).second)
            m_frames[frame].push_back(record);
    }

    std::uint8_t m_slot = 0;
    CRC m_logicCrc;
    std::map<std::uint32_t, std::vector<evolution::NetworkCommandRecord>> m_frames;
    std::set<std::uint32_t> m_seen;
    std::vector<std::uint32_t> m_executionOrder;
    std::size_t m_acceptedRemoteCommands = 0;
    std::size_t m_duplicateDatagrams = 0;
    std::size_t m_rejectedDatagrams = 0;
    std::size_t m_frameStalls = 0;
};

void testPacketMetadata()
{
    const evolution::NetworkCommandRecord record = makeRecord(1, 0x01U, 0x1234U, 77, 9);
    const std::vector<std::uint8_t> packet = makePacket(0x11223344U, record);

    evolution::NetworkPacketHeader header;
    std::vector<evolution::NetworkCommandRecord> decoded;
    const evolution::NetworkDecodeResult result = evolution::decodeRoutedCommandPacketV1(
        packet.data(), packet.size(), header, decoded);
    Expect(result.ok(), "routed packet decode failed");
    Expect(header.packetType == evolution::NetworkPacketType::RoutedCommandBatch, "routed packet type changed");
    Expect(header.sequence == 0x00011234U, "command sequence/ID composition changed");
    Expect(header.frame == 0x11223344U, "frame was not preserved");
    Expect(decoded.size() == 1U, "decoded routed command count changed");
    Expect(decoded[0].playerId == 1U, "player identity was not preserved");
    Expect(decoded[0].relayMask == 0x01U, "relay mask was not preserved");
    Expect(decoded[0].commandId == 0x1234U, "command ID was not preserved");
}

void testRetryAckAndDuplicate()
{
    Endpoint receiver(1);
    const evolution::NetworkCommandRecord record = makeRecord(0, 0x02U, 100U, 300, 17);
    const std::vector<std::uint8_t> packet = makePacket(50, record);

    unsigned attempts = 1; // attempt 1 is intentionally dropped by the deterministic link.
    bool pending = true;

    ++attempts; // Retry after the production Connection retry interval expires.
    Ack ack;
    Expect(receiver.receive(packet, ack), "retry packet was rejected");
    Expect(ack.originalPlayerId == 0U && ack.commandId == 100U, "legacy ACK identity did not match retried command");
    pending = false; // Models existing Connection::processAck removal from the retry list.

    Ack duplicateAck;
    Expect(receiver.receive(packet, duplicateAck), "duplicate retry was not ACKable");
    Expect(duplicateAck.commandId == 100U, "duplicate ACK command ID changed");

    Expect(attempts == 2U, "loss scenario did not require exactly one retry");
    Expect(!pending, "ACK did not clear pending retry state");
    Expect(receiver.acceptedRemoteCommands() == 1U, "retry/duplicate executed the gameplay command more than once");
    Expect(receiver.duplicateDatagrams() == 1U, "duplicate datagram was not detected");
}

void testDelayedOrderingFrameSyncAndCrc()
{
    Endpoint peer0(0);
    Endpoint peer1(1);

    const std::uint32_t frame = 100;
    const evolution::NetworkCommandRecord p0a = makeRecord(0, 0x02U, 200U, 401, 5);
    const evolution::NetworkCommandRecord p0b = makeRecord(0, 0x02U, 201U, 402, 3);
    const evolution::NetworkCommandRecord p1a = makeRecord(1, 0x01U, 300U, 403, 7);
    const evolution::NetworkCommandRecord p1b = makeRecord(1, 0x01U, 301U, 404, 11);

    // Local commands enter the same frame data immediately, matching ConnectionManager behavior.
    peer0.addLocal(frame, p0b);
    peer0.addLocal(frame, p0a);
    peer1.addLocal(frame, p1b);
    peer1.addLocal(frame, p1a);

    Ack ack;
    // Deliver higher IDs first to force out-of-order arrival on both endpoints.
    Expect(peer1.receive(makePacket(frame, p0b), ack), "peer1 rejected delayed-order p0b");
    Expect(peer0.receive(makePacket(frame, p1b), ack), "peer0 rejected delayed-order p1b");

    Expect(!peer0.executeFrame(frame, 4U), "peer0 advanced a frame before all synchronized commands arrived");
    Expect(!peer1.executeFrame(frame, 4U), "peer1 advanced a frame before all synchronized commands arrived");

    Expect(peer1.receive(makePacket(frame, p0a), ack), "peer1 rejected p0a");
    Expect(peer0.receive(makePacket(frame, p1a), ack), "peer0 rejected p1a");

    Expect(peer0.executeFrame(frame, 4U), "peer0 failed to advance synchronized frame");
    Expect(peer1.executeFrame(frame, 4U), "peer1 failed to advance synchronized frame");
    Expect(peer0.executionOrder() == peer1.executionOrder(), "peers executed commands in different deterministic order");
    Expect(peer0.logicCrc() == peer1.logicCrc(), "peer logic CRCs diverged after identical synchronized command stream");
    Expect(peer0.frameStalls() == 1U && peer1.frameStalls() == 1U, "delayed delivery did not exercise the frame-stall gate");

    const std::uint32_t agreedCrc = peer0.logicCrc();
    const evolution::NetworkCommandRecord crc0 = makeRecord(
        0, 0x02U, 202U, LOGIC_CRC_MESSAGE_TYPE, static_cast<std::int32_t>(agreedCrc));
    const evolution::NetworkCommandRecord crc1 = makeRecord(
        1, 0x01U, 302U, LOGIC_CRC_MESSAGE_TYPE, static_cast<std::int32_t>(agreedCrc));

    peer0.addLocal(frame + 1U, crc0);
    peer1.addLocal(frame + 1U, crc1);
    Expect(peer1.receive(makePacket(frame + 1U, crc0), ack), "peer1 rejected logic CRC checkpoint");
    Expect(peer0.receive(makePacket(frame + 1U, crc1), ack), "peer0 rejected logic CRC checkpoint");
    Expect(peer0.crcCheckpointValue(frame + 1U) == agreedCrc, "peer0 received a mismatched logic CRC checkpoint");
    Expect(peer1.crcCheckpointValue(frame + 1U) == agreedCrc, "peer1 received a mismatched logic CRC checkpoint");
    Expect(peer0.executeFrame(frame + 1U, 2U) && peer1.executeFrame(frame + 1U, 2U),
        "CRC checkpoint frame did not synchronize");
    Expect(peer0.logicCrc() == agreedCrc && peer1.logicCrc() == agreedCrc,
        "logic CRC checkpoint command mutated deterministic logic state");
}

void testMalformedRuntimeBoundary()
{
    Endpoint receiver(1);
    const evolution::NetworkCommandRecord record = makeRecord(0, 0x02U, 400U, 500, 23);
    const std::vector<std::uint8_t> valid = makePacket(200, record);
    Ack ack;

    std::vector<std::uint8_t> badMagic = valid;
    badMagic[0] ^= 0xffU;
    Expect(!receiver.receive(badMagic, ack), "bad EVN1 magic crossed the runtime decode boundary");

    std::vector<std::uint8_t> truncated(valid.begin(), valid.end() - 1);
    Expect(!receiver.receive(truncated, ack), "truncated EVN1 datagram crossed the runtime decode boundary");

    std::vector<std::uint8_t> badCommandVersion = valid;
    const std::size_t commandOffset = evolution::NETWORK_HEADER_BYTES_V1 +
        evolution::NETWORK_COMMAND_BATCH_HEADER_BYTES_V1 + evolution::NETWORK_COMMAND_RECORD_HEADER_BYTES_V1;
    Expect(commandOffset + 1U < badCommandVersion.size(), "test packet is unexpectedly too small");
    badCommandVersion[commandOffset] = 2U;
    badCommandVersion[commandOffset + 1U] = 0U;
    Expect(!receiver.receive(badCommandVersion, ack), "unsupported command codec version crossed runtime boundary");

    Expect(receiver.rejectedDatagrams() == 3U, "malformed datagram rejection count changed");
    Expect(receiver.acceptedRemoteCommands() == 0U, "malformed datagram created a gameplay command");
}

void testIntentionalEvN1EmissionFailure()
{
    evolution::NetworkCommandRecord record = makeRecord(0, 0x02U, 500U, 600, 1);
    record.command.arguments.clear();
    record.command.arguments.resize(evolution::MAX_COMMAND_ARGUMENTS_V1 + 1U);
    for (evolution::CommandArgument &argument : record.command.arguments)
        argument.kind = evolution::CommandArgumentKind::Integer;

    std::vector<evolution::NetworkCommandRecord> records(1, record);
    std::vector<std::uint8_t> packet;
    Expect(!evolution::encodeRoutedCommandPacketV1(commandKey(record), 300, records, packet),
        "unrepresentable command unexpectedly emitted EVN1; legacy fallback would become unreachable");
}

void testNonGameplayPacketRejectedByRoutedBoundary()
{
    evolution::NetworkPacketHeader header;
    header.packetType = evolution::NetworkPacketType::Disconnect;
    header.sequence = 1;
    header.frame = 0;
    std::vector<std::uint8_t> packet;
    Expect(evolution::encodeNetworkPacketV1(header, nullptr, 0, packet), "control packet construction failed");

    evolution::NetworkPacketHeader decodedHeader;
    std::vector<evolution::NetworkCommandRecord> records;
    const evolution::NetworkDecodeResult result = evolution::decodeRoutedCommandPacketV1(
        packet.data(), packet.size(), decodedHeader, records);
    Expect(result.error == evolution::NetworkDecodeError::InvalidPacketType,
        "routed gameplay boundary accepted a non-gameplay EVN1 packet type");
}
} // namespace

int main()
{
    static_assert(sizeof(void *) == 8, "Step 04E3 session validation is Evolution x64-only");
    static_assert(sizeof(std::uint32_t) == 4, "Step 04E3 requires fixed-width uint32_t");

    testPacketMetadata();
    testRetryAckAndDuplicate();
    testDelayedOrderingFrameSyncAndCrc();
    testMalformedRuntimeBoundary();
    testIntentionalEvN1EmissionFailure();
    testNonGameplayPacketRejectedByRoutedBoundary();

    std::cout << "Step 04E3 Evolution session validation passed: two-peer routed EVN1 flow, retry/ACK, duplicates, delayed ordering, frame sync, CRC checkpoints, malformed rejection, and fallback boundary are stable.\n";
    return 0;
}
