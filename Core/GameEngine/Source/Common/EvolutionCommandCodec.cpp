/*
** Command & Conquer Generals Evolution
** Fixed-width command serialization shared by Evolution networking and replays.
*/

#include "Common/EvolutionCommandCodec.h"

#include <cstring>
#include <limits>

namespace evolution
{
namespace
{

static constexpr std::size_t COMMAND_HEADER_BYTES = 8;
static constexpr std::size_t ARGUMENT_HEADER_BYTES = 4;

std::size_t argumentPayloadSize(CommandArgumentKind kind)
{
    switch (kind)
    {
    case CommandArgumentKind::Integer:
    case CommandArgumentKind::Real:
    case CommandArgumentKind::ObjectId:
    case CommandArgumentKind::DrawableId:
    case CommandArgumentKind::TeamId:
    case CommandArgumentKind::Timestamp:
        return 4;
    case CommandArgumentKind::Boolean:
        return 1;
    case CommandArgumentKind::Location:
        return 12;
    case CommandArgumentKind::Pixel:
        return 8;
    case CommandArgumentKind::PixelRegion:
        return 16;
    case CommandArgumentKind::WideChar:
        return 2;
    }
    return 0;
}

void appendU8(std::vector<std::uint8_t> &out, std::uint8_t value)
{
    out.push_back(value);
}

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

void appendF32(std::vector<std::uint8_t> &out, float value)
{
    static_assert(sizeof(float) == sizeof(std::uint32_t), "Evolution protocol requires IEEE-754 float32 storage");
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    appendU32(out, bits);
}

class Reader
{
public:
    Reader(const std::uint8_t *data, std::size_t size) : m_data(data), m_size(size) {}

    bool readU8(std::uint8_t &value)
    {
        if (remaining() < 1)
            return false;
        value = m_data[m_pos++];
        return true;
    }

    bool readU16(std::uint16_t &value)
    {
        if (remaining() < 2)
            return false;
        value = static_cast<std::uint16_t>(m_data[m_pos]) |
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(m_data[m_pos + 1]) << 8U);
        m_pos += 2;
        return true;
    }

    bool readU32(std::uint32_t &value)
    {
        if (remaining() < 4)
            return false;
        value = static_cast<std::uint32_t>(m_data[m_pos]) |
            (static_cast<std::uint32_t>(m_data[m_pos + 1]) << 8U) |
            (static_cast<std::uint32_t>(m_data[m_pos + 2]) << 16U) |
            (static_cast<std::uint32_t>(m_data[m_pos + 3]) << 24U);
        m_pos += 4;
        return true;
    }

    bool readI32(std::int32_t &value)
    {
        std::uint32_t bits = 0;
        if (!readU32(bits))
            return false;
        value = static_cast<std::int32_t>(bits);
        return true;
    }

    bool readF32(float &value)
    {
        std::uint32_t bits = 0;
        if (!readU32(bits))
            return false;
        std::memcpy(&value, &bits, sizeof(bits));
        return true;
    }

    std::size_t position() const { return m_pos; }
    std::size_t remaining() const { return m_size - m_pos; }

private:
    const std::uint8_t *m_data = nullptr;
    std::size_t m_size = 0;
    std::size_t m_pos = 0;
};

bool isKnownKind(std::uint8_t value)
{
    return value <= static_cast<std::uint8_t>(CommandArgumentKind::WideChar);
}

bool encodeArgument(const CommandArgument &arg, std::vector<std::uint8_t> &output)
{
    const std::size_t payloadBytes = argumentPayloadSize(arg.kind);
    if (payloadBytes == 0 || payloadBytes > std::numeric_limits<std::uint16_t>::max())
        return false;

    appendU8(output, static_cast<std::uint8_t>(arg.kind));
    appendU8(output, 0); // reserved
    appendU16(output, static_cast<std::uint16_t>(payloadBytes));

    switch (arg.kind)
    {
    case CommandArgumentKind::Integer:
    case CommandArgumentKind::ObjectId:
    case CommandArgumentKind::DrawableId:
        appendI32(output, arg.signed32);
        break;
    case CommandArgumentKind::Real:
        appendF32(output, arg.real32[0]);
        break;
    case CommandArgumentKind::Boolean:
        appendU8(output, arg.unsigned32 == 0 ? 0U : 1U);
        break;
    case CommandArgumentKind::TeamId:
    case CommandArgumentKind::Timestamp:
        appendU32(output, arg.unsigned32);
        break;
    case CommandArgumentKind::Location:
        appendF32(output, arg.real32[0]);
        appendF32(output, arg.real32[1]);
        appendF32(output, arg.real32[2]);
        break;
    case CommandArgumentKind::Pixel:
        appendI32(output, arg.integer32[0]);
        appendI32(output, arg.integer32[1]);
        break;
    case CommandArgumentKind::PixelRegion:
        appendI32(output, arg.integer32[0]);
        appendI32(output, arg.integer32[1]);
        appendI32(output, arg.integer32[2]);
        appendI32(output, arg.integer32[3]);
        break;
    case CommandArgumentKind::WideChar:
        appendU16(output, arg.wideChar16);
        break;
    }
    return true;
}

DecodeError decodeArgument(Reader &reader, CommandArgument &arg)
{
    std::uint8_t rawKind = 0;
    std::uint8_t reserved = 0;
    std::uint16_t payloadBytes = 0;
    if (!reader.readU8(rawKind) || !reader.readU8(reserved) || !reader.readU16(payloadBytes))
        return DecodeError::Truncated;
    if (!isKnownKind(rawKind))
        return DecodeError::InvalidArgumentKind;
    if (reserved != 0)
        return DecodeError::ReservedFieldNonZero;

    arg = CommandArgument{};
    arg.kind = static_cast<CommandArgumentKind>(rawKind);
    if (payloadBytes != argumentPayloadSize(arg.kind))
        return DecodeError::InvalidArgumentSize;
    if (reader.remaining() < payloadBytes)
        return DecodeError::Truncated;

    switch (arg.kind)
    {
    case CommandArgumentKind::Integer:
    case CommandArgumentKind::ObjectId:
    case CommandArgumentKind::DrawableId:
        if (!reader.readI32(arg.signed32)) return DecodeError::Truncated;
        break;
    case CommandArgumentKind::Real:
        if (!reader.readF32(arg.real32[0])) return DecodeError::Truncated;
        break;
    case CommandArgumentKind::Boolean:
    {
        std::uint8_t value = 0;
        if (!reader.readU8(value)) return DecodeError::Truncated;
        if (value > 1U) return DecodeError::InvalidBoolean;
        arg.unsigned32 = value;
        break;
    }
    case CommandArgumentKind::TeamId:
    case CommandArgumentKind::Timestamp:
        if (!reader.readU32(arg.unsigned32)) return DecodeError::Truncated;
        break;
    case CommandArgumentKind::Location:
        if (!reader.readF32(arg.real32[0]) || !reader.readF32(arg.real32[1]) || !reader.readF32(arg.real32[2]))
            return DecodeError::Truncated;
        break;
    case CommandArgumentKind::Pixel:
        if (!reader.readI32(arg.integer32[0]) || !reader.readI32(arg.integer32[1]))
            return DecodeError::Truncated;
        break;
    case CommandArgumentKind::PixelRegion:
        for (std::size_t i = 0; i < 4; ++i)
            if (!reader.readI32(arg.integer32[i])) return DecodeError::Truncated;
        break;
    case CommandArgumentKind::WideChar:
        if (!reader.readU16(arg.wideChar16)) return DecodeError::Truncated;
        break;
    }
    return DecodeError::None;
}

} // namespace

std::size_t encodedCommandSizeV1(const Command &command)
{
    if (command.arguments.size() > MAX_COMMAND_ARGUMENTS_V1)
        return 0;

    std::size_t size = COMMAND_HEADER_BYTES;
    for (const CommandArgument &arg : command.arguments)
    {
        const std::size_t payload = argumentPayloadSize(arg.kind);
        if (payload == 0)
            return 0;
        size += ARGUMENT_HEADER_BYTES + payload;
    }
    return size;
}

bool encodeCommandV1(const Command &command, std::vector<std::uint8_t> &output)
{
    const std::size_t encodedSize = encodedCommandSizeV1(command);
    if (encodedSize == 0)
        return false;

    output.clear();
    output.reserve(encodedSize);
    appendU16(output, COMMAND_CODEC_VERSION_V1);
    appendU16(output, static_cast<std::uint16_t>(command.arguments.size()));
    appendI32(output, command.messageType);
    for (const CommandArgument &arg : command.arguments)
        if (!encodeArgument(arg, output)) return false;
    return output.size() == encodedSize;
}

DecodeResult decodeCommandV1(const std::uint8_t *data, std::size_t size, Command &command)
{
    Reader reader(data, size);
    std::uint16_t version = 0;
    std::uint16_t argumentCount = 0;
    std::int32_t messageType = 0;

    if (!reader.readU16(version) || !reader.readU16(argumentCount) || !reader.readI32(messageType))
        return {DecodeError::Truncated, reader.position()};
    if (version != COMMAND_CODEC_VERSION_V1)
        return {DecodeError::UnsupportedVersion, reader.position()};
    if (argumentCount > MAX_COMMAND_ARGUMENTS_V1)
        return {DecodeError::TooManyArguments, reader.position()};

    Command decoded;
    decoded.messageType = messageType;
    decoded.arguments.reserve(argumentCount);
    for (std::uint16_t i = 0; i < argumentCount; ++i)
    {
        CommandArgument arg;
        const DecodeError error = decodeArgument(reader, arg);
        if (error != DecodeError::None)
            return {error, reader.position()};
        decoded.arguments.push_back(arg);
    }

    command = decoded;
    return {DecodeError::None, reader.position()};
}

} // namespace evolution
