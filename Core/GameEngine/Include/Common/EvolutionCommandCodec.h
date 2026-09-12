/*
** Command & Conquer Generals Evolution
** Fixed-width command serialization shared by Evolution networking and replays.
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace evolution
{

static constexpr std::uint16_t COMMAND_CODEC_VERSION_V1 = 1;
static constexpr std::size_t MAX_COMMAND_ARGUMENTS_V1 = 256;

// Wire tags are frozen protocol values. They intentionally mirror the historical
// GameMessage argument ordering, but do not depend on compiler enum width.
enum class CommandArgumentKind : std::uint8_t
{
    Integer = 0,
    Real = 1,
    Boolean = 2,
    ObjectId = 3,
    DrawableId = 4,
    TeamId = 5,
    Location = 6,
    Pixel = 7,
    PixelRegion = 8,
    Timestamp = 9,
    WideChar = 10,
};

struct CommandArgument
{
    CommandArgumentKind kind = CommandArgumentKind::Integer;

    std::int32_t signed32 = 0;
    std::uint32_t unsigned32 = 0;
    float real32[3] = {0.0f, 0.0f, 0.0f};
    std::int32_t integer32[4] = {0, 0, 0, 0};
    std::uint16_t wideChar16 = 0;
};

struct Command
{
    std::int32_t messageType = 0;
    std::vector<CommandArgument> arguments;
};

enum class DecodeError
{
    None,
    Truncated,
    UnsupportedVersion,
    TooManyArguments,
    InvalidArgumentKind,
    InvalidArgumentSize,
    InvalidBoolean,
    ReservedFieldNonZero,
};

struct DecodeResult
{
    DecodeError error = DecodeError::None;
    std::size_t bytesConsumed = 0;

    bool ok() const { return error == DecodeError::None; }
};

std::size_t encodedCommandSizeV1(const Command &command);
bool encodeCommandV1(const Command &command, std::vector<std::uint8_t> &output);
DecodeResult decodeCommandV1(const std::uint8_t *data, std::size_t size, Command &command);

} // namespace evolution
