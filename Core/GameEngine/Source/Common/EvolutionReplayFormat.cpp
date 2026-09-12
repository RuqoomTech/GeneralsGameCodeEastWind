/*
** Command & Conquer Generals Evolution
** Versioned replay framing for Evolution command records.
*/

#include "Common/EvolutionReplayFormat.h"

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

void appendI32(std::vector<std::uint8_t> &out, std::int32_t value)
{
    appendU32(out, static_cast<std::uint32_t>(value));
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

bool readI32(const std::uint8_t *data, std::size_t size, std::size_t &pos, std::int32_t &value)
{
    std::uint32_t raw = 0;
    if (!readU32(data, size, pos, raw)) return false;
    value = static_cast<std::int32_t>(raw);
    return true;
}
} // namespace

void encodeReplayHeaderV1(const ReplayHeader &header, std::vector<std::uint8_t> &output)
{
    output.clear();
    output.reserve(REPLAY_HEADER_BYTES_V1);
    appendU32(output, REPLAY_MAGIC_V1);
    appendU16(output, REPLAY_FORMAT_VERSION_V1);
    appendU16(output, COMMAND_CODEC_VERSION_V1);
    appendU32(output, static_cast<std::uint32_t>(REPLAY_HEADER_BYTES_V1));
    appendU32(output, header.flags);
}

ReplayDecodeResult decodeReplayHeaderV1(const std::uint8_t *data, std::size_t size, ReplayHeader &header)
{
    if (data == nullptr || size < REPLAY_HEADER_BYTES_V1)
        return {ReplayDecodeError::Truncated, 0};

    std::size_t pos = 0;
    std::uint32_t magic = 0;
    std::uint16_t formatVersion = 0;
    std::uint16_t codecVersion = 0;
    std::uint32_t headerBytes = 0;
    if (!readU32(data, size, pos, magic) || !readU16(data, size, pos, formatVersion) ||
        !readU16(data, size, pos, codecVersion) || !readU32(data, size, pos, headerBytes) ||
        !readU32(data, size, pos, header.flags))
        return {ReplayDecodeError::Truncated, pos};
    if (magic != REPLAY_MAGIC_V1) return {ReplayDecodeError::InvalidMagic, pos};
    if (formatVersion != REPLAY_FORMAT_VERSION_V1) return {ReplayDecodeError::UnsupportedFormatVersion, pos};
    if (codecVersion != COMMAND_CODEC_VERSION_V1) return {ReplayDecodeError::UnsupportedCommandCodecVersion, pos};
    if (headerBytes != REPLAY_HEADER_BYTES_V1) return {ReplayDecodeError::RecordSizeMismatch, pos};
    return {ReplayDecodeError::None, pos};
}

bool encodeReplayCommandRecordV1(const ReplayCommandRecord &record, std::vector<std::uint8_t> &output)
{
    std::vector<std::uint8_t> commandBytes;
    if (!encodeCommandV1(record.command, commandBytes) ||
        commandBytes.size() > std::numeric_limits<std::uint32_t>::max())
        return false;

    output.clear();
    output.reserve(REPLAY_COMMAND_RECORD_HEADER_BYTES_V1 + commandBytes.size());
    appendU32(output, record.frame);
    appendI32(output, record.playerIndex);
    appendU32(output, static_cast<std::uint32_t>(commandBytes.size()));
    output.insert(output.end(), commandBytes.begin(), commandBytes.end());
    return true;
}

ReplayDecodeResult decodeReplayCommandRecordV1(
    const std::uint8_t *data,
    std::size_t size,
    ReplayCommandRecord &record)
{
    if (data == nullptr || size < REPLAY_COMMAND_RECORD_HEADER_BYTES_V1)
        return {ReplayDecodeError::Truncated, 0};

    std::size_t pos = 0;
    std::uint32_t payloadBytes = 0;
    ReplayCommandRecord decoded;
    if (!readU32(data, size, pos, decoded.frame) || !readI32(data, size, pos, decoded.playerIndex) ||
        !readU32(data, size, pos, payloadBytes))
        return {ReplayDecodeError::Truncated, pos};
    if (payloadBytes > size - pos)
        return {ReplayDecodeError::Truncated, pos};

    const DecodeResult commandResult = decodeCommandV1(data + pos, payloadBytes, decoded.command);
    if (!commandResult.ok())
        return {ReplayDecodeError::InvalidCommand, pos + commandResult.bytesConsumed};
    if (commandResult.bytesConsumed != payloadBytes)
        return {ReplayDecodeError::RecordSizeMismatch, pos + commandResult.bytesConsumed};

    pos += payloadBytes;
    record = decoded;
    return {ReplayDecodeError::None, pos};
}

} // namespace evolution
