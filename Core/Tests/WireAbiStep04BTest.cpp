/*
** Step 04B wire/replay ABI guard.
**
** Runtime C++ object layout is allowed to change with the native architecture.
** Network/replay capacities and scalar fields are not.
*/

#include "Utility/CppMacros.h"
#include "Common/GameType.h"
#include "GameNetwork/NetworkDefs.h"

#include <cstdio>

namespace
{
int g_failures = 0;

void Expect_Size(const char *name, size_t expected, size_t actual)
{
    if (expected != actual) {
        std::fprintf(stderr, "%s: expected %lu, got %lu\n",
            name, static_cast<unsigned long>(expected), static_cast<unsigned long>(actual));
        ++g_failures;
    }
}

void Expect_Int(const char *name, long expected, long actual)
{
    if (expected != actual) {
        std::fprintf(stderr, "%s: expected %ld, got %ld\n", name, expected, actual);
        ++g_failures;
    }
}
}

int main()
{
    // Replay/network scalar contracts: these are protocol widths, not native widths.
    Expect_Size("frame wire width", 4U, sizeof(UnsignedInt));
    Expect_Size("command-count wire width", 2U, sizeof(UnsignedShort));
    Expect_Size("ObjectID wire width", 4U, sizeof(ObjectID));
    Expect_Size("DrawableID wire width", 4U, sizeof(DrawableID));
    Expect_Size("Real wire width", 4U, sizeof(Real));

    // Frozen legacy byte capacity. Most importantly, none of these values is derived
    // from sizeof(GameMessage), which is an architecture-dependent runtime class.
    Expect_Int("legacy command bytes", 1008, LEGACY_COMMAND_PACKET_COMMAND_BYTES);
    Expect_Int("legacy max command count", 28, LEGACY_COMMAND_PACKET_MAX_COMMANDS);
    Expect_Int("compat command count alias", 28, numCommandsPerCommandPacket);
    Expect_Size("legacy CommandPacket byte layout", 1014U, sizeof(CommandPacket));
    Expect_Size("TransportMessageHeader byte layout", 6U, sizeof(TransportMessageHeader));

    if (g_failures != 0) {
        std::fprintf(stderr, "%d Step 04B wire/replay ABI guard(s) failed.\n", g_failures);
        return 1;
    }

    std::printf("Step 04B wire/replay ABI guard passed: fixed protocol widths and frozen command-packet byte capacity are architecture-independent.\n");
    return 0;
}
