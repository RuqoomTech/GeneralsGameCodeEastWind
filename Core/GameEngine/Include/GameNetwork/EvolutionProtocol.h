/*
** Command & Conquer Generals Evolution
** Versioned outer framing for Evolution multiplayer packets.
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Common/EvolutionCommandCodec.h"

namespace evolution
{

static constexpr std::uint32_t NETWORK_MAGIC_V1 = 0x314E5645U; // "EVN1" in little-endian byte order.
static constexpr std::uint16_t NETWORK_PROTOCOL_VERSION_V1 = 1;
static constexpr std::size_t NETWORK_HEADER_BYTES_V1 = 20;
static constexpr std::size_t NETWORK_COMMAND_BATCH_HEADER_BYTES_V1 = 4;
static constexpr std::size_t NETWORK_COMMAND_RECORD_HEADER_BYTES_V1 = 8;
static constexpr std::size_t MAX_NETWORK_COMMANDS_V1 = 256;

enum class NetworkPacketType : std::uint16_t
{
    CommandBatch = 1,
    KeepAlive = 2,
    Disconnect = 3,
};

struct NetworkPacketHeader
{
    NetworkPacketType packetType = NetworkPacketType::CommandBatch;
    std::uint32_t sequence = 0;
    std::uint32_t frame = 0;
};

struct NetworkCommandRecord
{
    std::uint8_t playerId = 0;
    std::uint16_t commandId = 0;
    Command command;
};

enum class NetworkDecodeError
{
    None,
    Truncated,
    InvalidMagic,
    UnsupportedVersion,
    InvalidPacketType,
    TooManyCommands,
    ReservedFieldNonZero,
    InvalidCommand,
    PayloadSizeMismatch,
};

struct NetworkDecodeResult
{
    NetworkDecodeError error = NetworkDecodeError::None;
    bool ok() const { return error == NetworkDecodeError::None; }
};

bool encodeCommandBatchV1(const std::vector<NetworkCommandRecord> &commands, std::vector<std::uint8_t> &output);
NetworkDecodeResult decodeCommandBatchV1(
    const std::uint8_t *data,
    std::size_t size,
    std::vector<NetworkCommandRecord> &commands);

bool encodeNetworkPacketV1(
    const NetworkPacketHeader &header,
    const std::uint8_t *payload,
    std::size_t payloadSize,
    std::vector<std::uint8_t> &output);

NetworkDecodeResult decodeNetworkPacketV1(
    const std::uint8_t *data,
    std::size_t size,
    NetworkPacketHeader &header,
    const std::uint8_t *&payload,
    std::size_t &payloadSize);

} // namespace evolution
