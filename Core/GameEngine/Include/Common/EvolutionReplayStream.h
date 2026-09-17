/*
** Command & Conquer Generals Evolution
** Runtime file bridge for the versioned EVR1 command stream.
*/

#pragma once

#include "Common/EvolutionReplayFormat.h"

class File;
class GameMessage;

namespace evolution
{

enum class ReplayStreamReadStatus
{
    Record,
    EndOfFile,
    Error,
};

class EvolutionReplayStream
{
public:
    EvolutionReplayStream() = default;
    ~EvolutionReplayStream();

    bool openForWrite(const char *path);
    bool openForRead(const char *path);
    void close();
    bool flush();
    bool isOpen() const { return m_file != nullptr; }

    bool writeGameMessage(std::uint32_t frame, std::int32_t playerIndex, const GameMessage &message);
    ReplayStreamReadStatus readRecord(ReplayCommandRecord &record);

private:
    File *m_file = nullptr;
    bool m_writing = false;
    ReplaySequenceState m_sequence;
};

} // namespace evolution
