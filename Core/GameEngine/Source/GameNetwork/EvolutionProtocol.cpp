/*
** Command & Conquer Generals Evolution
** Versioned outer framing for Evolution multiplayer packets.
*/

#include "GameNetwork/EvolutionProtocol.h"

#include <limits>

namespace evolution
{
namespace
{
void appendU16(std::vector<std::uint8_t> &out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void appendU32(std::vector<std::uint8_t> &out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
}

bool readU16(const std::uint8_t *data, std::size_t size, std::size_t &pos, std::uint16_t &value)
{
    if (size - pos < 2) return false;
    value = static_cast<std::uint16_t>(data[pos]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[pos + 1]) << 8U);
    pos += 2;
    return true;
}

bool readU32(const std::uint8_t *data, std::size_t size, std::size_t &pos, std::uint32_t &value)
{
    if (size - pos < 4) return false;
    value = static_cast<std::uint32_t>(data[pos]) |
        (static_cast<std::uint32_t>(data[pos + 1]) << 8U) |
        (static_cast<std::uint32_t>(data[pos + 2]) << 16U) |
        (static_cast<std::uint32_t>(data[pos + 3]) << 24U);
    pos += 4;
    return true;
}

bool isKnownPacketType(std::uint16_t value)
{
    return value >= static_cast<std::uint16_t>(NetworkPacketType::CommandBatch) &&
        value <= static_cast<std::uint16_t>(NetworkPacketType::RoutedCommandBatch);
}
} // namespace


namespace
{
bool encodeCommandBatchImplV1(
    const std::vector<NetworkCommandRecord> &commands,
    bool routed,
    std::vector<std::uint8_t> &output)
{
    if (commands.size() > MAX_NETWORK_COMMANDS_V1) return false;

    output.clear();
    appendU16(output, static_cast<std::uint16_t>(commands.size()));
    appendU16(output, 0); // reserved
    for (const NetworkCommandRecord &record : commands)
    {
        if (!routed && record.relayMask != 0)
            return false;

        std::vector<std::uint8_t> commandBytes;
        if (!encodeCommandV1(record.command, commandBytes) ||
            commandBytes.size() > std::numeric_limits<std::uint32_t>::max())
            return false;

        output.push_back(record.playerId);
        output.push_back(routed ? record.relayMask : 0U);
        appendU16(output, record.commandId);
        appendU32(output, static_cast<std::uint32_t>(commandBytes.size()));
        output.insert(output.end(), commandBytes.begin(), commandBytes.end());
    }
    return true;
}

NetworkDecodeResult decodeCommandBatchImplV1(
    const std::uint8_t *data,
    std::size_t size,
    bool routed,
    std::vector<NetworkCommandRecord> &commands)
{
    if (data == nullptr || size < NETWORK_COMMAND_BATCH_HEADER_BYTES_V1)
        return {NetworkDecodeError::Truncated};

    std::size_t pos = 0;
    std::uint16_t commandCount = 0;
    std::uint16_t reserved = 0;
    if (!readU16(data, size, pos, commandCount) || !readU16(data, size, pos, reserved))
        return {NetworkDecodeError::Truncated};
    if (reserved != 0) return {NetworkDecodeError::ReservedFieldNonZero};
    if (commandCount > MAX_NETWORK_COMMANDS_V1) return {NetworkDecodeError::TooManyCommands};

    std::vector<NetworkCommandRecord> decoded;
    decoded.reserve(commandCount);
    for (std::uint16_t i = 0; i < commandCount; ++i)
    {
        if (size - pos < NETWORK_COMMAND_RECORD_HEADER_BYTES_V1)
            return {NetworkDecodeError::Truncated};

        NetworkCommandRecord record;
        record.playerId = data[pos++];
        const std::uint8_t relayOrReserved = data[pos++];
        if (!routed && relayOrReserved != 0) return {NetworkDecodeError::ReservedFieldNonZero};
        record.relayMask = routed ? relayOrReserved : 0U;
        if (!readU16(data, size, pos, record.commandId)) return {NetworkDecodeError::Truncated};
        std::uint32_t commandBytes = 0;
        if (!readU32(data, size, pos, commandBytes)) return {NetworkDecodeError::Truncated};
        if (commandBytes > size - pos) return {NetworkDecodeError::Truncated};

        const DecodeResult commandResult = decodeCommandV1(data + pos, commandBytes, record.command);
        if (!commandResult.ok()) return {NetworkDecodeError::InvalidCommand};
        if (commandResult.bytesConsumed != commandBytes) return {NetworkDecodeError::PayloadSizeMismatch};
        pos += commandBytes;
        decoded.push_back(record);
    }
    if (pos != size) return {NetworkDecodeError::PayloadSizeMismatch};

    commands = decoded;
    return {NetworkDecodeError::None};
}
} // namespace

bool encodeCommandBatchV1(const std::vector<NetworkCommandRecord> &commands, std::vector<std::uint8_t> &output)
{
    return encodeCommandBatchImplV1(commands, false, output);
}

NetworkDecodeResult decodeCommandBatchV1(
    const std::uint8_t *data,
    std::size_t size,
    std::vector<NetworkCommandRecord> &commands)
{
    return decodeCommandBatchImplV1(data, size, false, commands);
}

bool encodeRoutedCommandBatchV1(const std::vector<NetworkCommandRecord> &commands, std::vector<std::uint8_t> &output)
{
    return encodeCommandBatchImplV1(commands, true, output);
}

NetworkDecodeResult decodeRoutedCommandBatchV1(
    const std::uint8_t *data,
    std::size_t size,
    std::vector<NetworkCommandRecord> &commands)
{
    return decodeCommandBatchImplV1(data, size, true, commands);
}

bool encodeRoutedCommandPacketV1(
    std::uint32_t sequence,
    std::uint32_t frame,
    const std::vector<NetworkCommandRecord> &commands,
    std::vector<std::uint8_t> &output)
{
    std::vector<std::uint8_t> payload;
    if (!encodeRoutedCommandBatchV1(commands, payload))
        return false;

    NetworkPacketHeader header;
    header.packetType = NetworkPacketType::RoutedCommandBatch;
    header.sequence = sequence;
    header.frame = frame;
    return encodeNetworkPacketV1(header, payload.data(), payload.size(), output);
}

NetworkDecodeResult decodeRoutedCommandPacketV1(
    const std::uint8_t *data,
    std::size_t size,
    NetworkPacketHeader &header,
    std::vector<NetworkCommandRecord> &commands)
{
    const std::uint8_t *payload = nullptr;
    std::size_t payloadSize = 0;
    const NetworkDecodeResult packetResult = decodeNetworkPacketV1(data, size, header, payload, payloadSize);
    if (!packetResult.ok())
        return packetResult;
    if (header.packetType != NetworkPacketType::RoutedCommandBatch)
        return {NetworkDecodeError::InvalidPacketType};
    return decodeRoutedCommandBatchV1(payload, payloadSize, commands);
}

bool encodeNetworkPacketV1(
    const NetworkPacketHeader &header,
    const std::uint8_t *payload,
    std::size_t payloadSize,
    std::vector<std::uint8_t> &output)
{
    if (payloadSize > std::numeric_limits<std::uint32_t>::max()) return false;
    if (payloadSize != 0 && payload == nullptr) return false;
    if (!isKnownPacketType(static_cast<std::uint16_t>(header.packetType))) return false;

    output.clear();
    output.reserve(NETWORK_HEADER_BYTES_V1 + payloadSize);
    appendU32(output, NETWORK_MAGIC_V1);
    appendU16(output, NETWORK_PROTOCOL_VERSION_V1);
    appendU16(output, static_cast<std::uint16_t>(header.packetType));
    appendU32(output, static_cast<std::uint32_t>(payloadSize));
    appendU32(output, header.sequence);
    appendU32(output, header.frame);
    if (payloadSize != 0)
        output.insert(output.end(), payload, payload + payloadSize);
    return true;
}

NetworkDecodeResult decodeNetworkPacketV1(
    const std::uint8_t *data,
    std::size_t size,
    NetworkPacketHeader &header,
    const std::uint8_t *&payload,
    std::size_t &payloadSize)
{
    payload = nullptr;
    payloadSize = 0;
    if (data == nullptr || size < NETWORK_HEADER_BYTES_V1)
        return {NetworkDecodeError::Truncated};

    std::size_t pos = 0;
    std::uint32_t magic = 0;
    std::uint16_t version = 0;
    std::uint16_t packetType = 0;
    std::uint32_t encodedPayloadSize = 0;
    if (!readU32(data, size, pos, magic) || !readU16(data, size, pos, version) ||
        !readU16(data, size, pos, packetType) || !readU32(data, size, pos, encodedPayloadSize) ||
        !readU32(data, size, pos, header.sequence) || !readU32(data, size, pos, header.frame))
        return {NetworkDecodeError::Truncated};
    if (magic != NETWORK_MAGIC_V1) return {NetworkDecodeError::InvalidMagic};
    if (version != NETWORK_PROTOCOL_VERSION_V1) return {NetworkDecodeError::UnsupportedVersion};
    if (!isKnownPacketType(packetType)) return {NetworkDecodeError::InvalidPacketType};
    if (encodedPayloadSize != size - NETWORK_HEADER_BYTES_V1)
        return {NetworkDecodeError::PayloadSizeMismatch};

    header.packetType = static_cast<NetworkPacketType>(packetType);
    payload = data + NETWORK_HEADER_BYTES_V1;
    payloadSize = encodedPayloadSize;
    return {NetworkDecodeError::None};
}

} // namespace evolution
