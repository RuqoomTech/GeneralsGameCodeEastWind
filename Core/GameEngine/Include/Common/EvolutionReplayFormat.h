/*
** Command & Conquer Generals Evolution
** Versioned replay framing for Evolution command records.
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Common/EvolutionCommandCodec.h"

namespace evolution
{

static constexpr std::uint32_t REPLAY_MAGIC_V1 = 0x31525645U; // "EVR1" in little-endian byte order.
static constexpr std::uint16_t REPLAY_FORMAT_VERSION_V1 = 1;
static constexpr std::size_t REPLAY_HEADER_BYTES_V1 = 16;
static constexpr std::size_t REPLAY_COMMAND_RECORD_HEADER_BYTES_V1 = 12;

struct ReplayHeader
{
    std::uint32_t flags = 0;
};

struct ReplayCommandRecord
{
    std::uint32_t frame = 0;
    std::int32_t playerIndex = -1;
    Command command;
};

enum class ReplayDecodeError
{
    None,
    Truncated,
    InvalidMagic,
    UnsupportedFormatVersion,
    UnsupportedCommandCodecVersion,
    RecordSizeMismatch,
    InvalidCommand,
};

struct ReplayDecodeResult
{
    ReplayDecodeError error = ReplayDecodeError::None;
    std::size_t bytesConsumed = 0;
    bool ok() const { return error == ReplayDecodeError::None; }
};

void encodeReplayHeaderV1(const ReplayHeader &header, std::vector<std::uint8_t> &output);
ReplayDecodeResult decodeReplayHeaderV1(const std::uint8_t *data, std::size_t size, ReplayHeader &header);

bool encodeReplayCommandRecordV1(const ReplayCommandRecord &record, std::vector<std::uint8_t> &output);
ReplayDecodeResult decodeReplayCommandRecordV1(
    const std::uint8_t *data,
    std::size_t size,
    ReplayCommandRecord &record);

} // namespace evolution
