/*
** Wire/replay ABI guard.
**
** Runtime C++ object layout is allowed to change with the native architecture.
** Network/replay capacities and scalar fields are not.
*/

#include "Precompiled/CppMacros.h"
#include "Common/GameType.h"
#include "GameNetwork/NetworkDefs.h"

#include <gtest/gtest.h>

namespace
{
void Expect_Size(const char *name, size_t expected, size_t actual)
{
    EXPECT_EQ(expected, actual) << name;
}

void Expect_Int(const char *name, long expected, long actual)
{
    EXPECT_EQ(expected, actual) << name;
}
}

TEST(WireAbi, ProtocolScalarWidths)
{
    // Replay/network scalar contracts: these are protocol widths, not native widths.
    Expect_Size("frame wire width", 4U, sizeof(UnsignedInt));
    Expect_Size("command-count wire width", 2U, sizeof(UnsignedShort));
    Expect_Size("ObjectID wire width", 4U, sizeof(ObjectID));
    Expect_Size("DrawableID wire width", 4U, sizeof(DrawableID));
    Expect_Size("Real wire width", 4U, sizeof(Real));
}

TEST(WireAbi, FrozenLegacyCommandPacket)
{
    // Frozen legacy byte capacity. Most importantly, none of these values is derived
    // from sizeof(GameMessage), which is an architecture-dependent runtime class.
    Expect_Int("legacy command bytes", 1008, LEGACY_COMMAND_PACKET_COMMAND_BYTES);
    Expect_Int("legacy max command count", 28, LEGACY_COMMAND_PACKET_MAX_COMMANDS);
    Expect_Int("compat command count alias", 28, numCommandsPerCommandPacket);
    Expect_Size("legacy CommandPacket byte layout", 1014U, sizeof(CommandPacket));
    Expect_Size("TransportMessageHeader byte layout", 6U, sizeof(TransportMessageHeader));
}
