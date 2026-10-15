/*
** Coordinate operator regression test.
**
** The neutron outer-radius correction uses unary coordinate operators imported
** from upstream. Exercise those operators directly without changing the
** deterministic floating-point characterization path.
*/

#include "Precompiled/CppMacros.h"
#include "Lib/BaseType.h"

#include <gtest/gtest.h>

namespace
{
// Exact comparison on purpose: negation must not change any bit of the value.
void expect_real(const char *name, Real expected, Real actual)
{
    EXPECT_EQ(expected, actual) << name;
}

void expect_int(const char *name, Int expected, Int actual)
{
    EXPECT_EQ(expected, actual) << name;
}
}

TEST(CoordinateOps, RealCoordinates)
{
    Coord3D c3 = { 1.5f, -2.0f, 3.25f };
    const Coord3D nc3 = -c3;
    const Coord3D pc3 = +c3;
    expect_real("Coord3D -x", -1.5f, nc3.x);
    expect_real("Coord3D -y", 2.0f, nc3.y);
    expect_real("Coord3D -z", -3.25f, nc3.z);
    expect_real("Coord3D +x", 1.5f, pc3.x);

    Coord2D c2 = { -4.0f, 5.0f };
    const Coord2D nc2 = -c2;
    expect_real("Coord2D -x", 4.0f, nc2.x);
    expect_real("Coord2D -y", -5.0f, nc2.y);
}

TEST(CoordinateOps, IntegerCoordinates)
{
    ICoord3D i3 = { 7, -8, 9 };
    const ICoord3D ni3 = -i3;
    expect_int("ICoord3D -x", -7, ni3.x);
    expect_int("ICoord3D -y", 8, ni3.y);
    expect_int("ICoord3D -z", -9, ni3.z);

    ICoord2D i2 = { -10, 11 };
    const ICoord2D ni2 = -i2;
    expect_int("ICoord2D -x", 10, ni2.x);
    expect_int("ICoord2D -y", -11, ni2.y);
}
