/*
** Command & Conquer Generals Evolution
** Runtime file bridge for the versioned EVR1 command stream.
*/

#include "PreRTS.h"

#include "Common/EvolutionReplayStream.h"

#include "Common/EvolutionGameMessageAdapter.h"
#include "Common/FileSystem.h"
#include "Common/MessageStream.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace evolution
{
namespace
{
std::uint32_t readU32LE(const std::uint8_t *data)
{
    return static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8U) |
        (static_cast<std::uint32_t>(data[2]) << 16U) |
        (static_cast<std::uint32_t>(data[3]) << 24U);
}

bool writeAll(File *file, const std::vector<std::uint8_t> &bytes)
{
    return file != nullptr &&
        bytes.size() <= static_cast<std::size_t>(std::numeric_limits<Int>::max()) &&
        file->write(bytes.data(), static_cast<Int>(bytes.size())) == static_cast<Int>(bytes.size());
}
} // namespace

EvolutionReplayStream::~EvolutionReplayStream()
{
    close();
}

bool EvolutionReplayStream::openForWrite(const char *path)
{
    close();
    if (path == nullptr || TheFileSystem == nullptr)
        return false;

    m_file = TheFileSystem->openFile(path, File::WRITE | File::BINARY);
    if (m_file == nullptr)
        return false;

    ReplayHeader header;
    std::vector<std::uint8_t> bytes;
    encodeReplayHeaderV1(header, bytes);
    if (!writeAll(m_file, bytes))
    {
        close();
        return false;
    }

    m_writing = true;
    return true;
}

bool EvolutionReplayStream::openForRead(const char *path)
{
    close();
    if (path == nullptr || TheFileSystem == nullptr)
        return false;

    m_file = TheFileSystem->openFile(path, File::READ | File::BINARY);
    if (m_file == nullptr)
        return false;

    std::uint8_t headerBytes[REPLAY_HEADER_BYTES_V1] = {};
    if (m_file->read(headerBytes, static_cast<Int>(sizeof(headerBytes))) != static_cast<Int>(sizeof(headerBytes)))
    {
        close();
        return false;
    }

    ReplayHeader header;
    if (!decodeReplayHeaderV1(headerBytes, sizeof(headerBytes), header).ok())
    {
        close();
        return false;
    }

    m_writing = false;
    return true;
}

void EvolutionReplayStream::close()
{
    if (m_file != nullptr)
    {
        m_file->close();
        m_file = nullptr;
    }
    m_writing = false;
}

bool EvolutionReplayStream::flush()
{
    return m_file != nullptr && m_writing && m_file->flush();
}

bool EvolutionReplayStream::writeGameMessage(
    std::uint32_t frame,
    std::int32_t playerIndex,
    const GameMessage &message)
{
    if (m_file == nullptr || !m_writing)
        return false;

    ReplayCommandRecord record;
    record.frame = frame;
    record.playerIndex = playerIndex;
    if (!gameMessageToEvolutionCommand(message, record.command))
        return false;

    std::vector<std::uint8_t> bytes;
    return encodeReplayCommandRecordV1(record, bytes) && writeAll(m_file, bytes);
}

ReplayStreamReadStatus EvolutionReplayStream::readRecord(ReplayCommandRecord &record)
{
    if (m_file == nullptr || m_writing)
        return ReplayStreamReadStatus::Error;

    std::uint8_t recordHeader[REPLAY_COMMAND_RECORD_HEADER_BYTES_V1] = {};
    const Int headerRead = m_file->read(recordHeader, static_cast<Int>(sizeof(recordHeader)));
    if (headerRead == 0)
        return ReplayStreamReadStatus::EndOfFile;
    if (headerRead != static_cast<Int>(sizeof(recordHeader)))
        return ReplayStreamReadStatus::Error;

    const std::uint32_t payloadBytes = readU32LE(recordHeader + 8U);
    if (payloadBytes > MAX_ENCODED_COMMAND_BYTES_V1)
        return ReplayStreamReadStatus::Error;

    std::vector<std::uint8_t> bytes(sizeof(recordHeader) + payloadBytes);
    memcpy(bytes.data(), recordHeader, sizeof(recordHeader));
    if (payloadBytes != 0 &&
        m_file->read(bytes.data() + sizeof(recordHeader), static_cast<Int>(payloadBytes)) != static_cast<Int>(payloadBytes))
        return ReplayStreamReadStatus::Error;

    const ReplayDecodeResult result = decodeReplayCommandRecordV1(bytes.data(), bytes.size(), record);
    if (!result.ok() || result.bytesConsumed != bytes.size())
        return ReplayStreamReadStatus::Error;
    return ReplayStreamReadStatus::Record;
}

} // namespace evolution
