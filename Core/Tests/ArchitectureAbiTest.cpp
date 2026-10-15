/*
** Architecture and fixed-width ABI guard.
**
** Evolution native
** pointers are now required to be 64-bit while serialization/network widths
** remain explicitly fixed and architecture-independent.
*/

#include "Precompiled/CppMacros.h"
#include "Lib/BaseType.h"
#include "Common/GameType.h"

#include <gtest/gtest.h>

namespace
{
void Expect_Size(const char *name, size_t expected, size_t actual)
{
	EXPECT_EQ(expected, actual) << name;
}
}

TEST(ArchitectureAbi, FixedWireWidths)
{
	Expect_Size("Int wire width", 4U, sizeof(Int));
	Expect_Size("UnsignedInt wire width", 4U, sizeof(UnsignedInt));
	Expect_Size("Short wire width", 2U, sizeof(Short));
	Expect_Size("UnsignedShort wire width", 2U, sizeof(UnsignedShort));
	Expect_Size("Int64 wire width", 8U, sizeof(Int64));
	Expect_Size("UnsignedInt64 wire width", 8U, sizeof(UnsignedInt64));
	Expect_Size("Real wire width", 4U, sizeof(Real));
	Expect_Size("ObjectID wire width", 4U, sizeof(ObjectID));
	Expect_Size("DrawableID wire width", 4U, sizeof(DrawableID));

#if defined(_WIN32)
	Expect_Size("Windows WideChar compatibility width", 2U, sizeof(WideChar));
#endif
}

TEST(ArchitectureAbi, NativePointerWidth)
{
	Expect_Size("Evolution native pointer width", 8U, sizeof(void *));
	Expect_Size("uintptr_t follows pointer width", sizeof(void *), sizeof(uintptr_t));

	int value = 7;
	void *pointer = &value;
	const uintptr_t bits = reinterpret_cast<uintptr_t>(pointer);
	void *roundTrip = reinterpret_cast<void *>(bits);
	EXPECT_EQ(roundTrip, pointer) << "pointer -> uintptr_t -> pointer round-trip failed";
}
