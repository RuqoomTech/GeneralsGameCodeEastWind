/*
** Step 04A architecture-width migration guard.
**
** The x86 compatibility runtime and the x64 modernization lane must agree on
** fixed serialization/network widths while allowing native pointers/size_t to
** follow the target architecture.
*/

#include "Utility/CppMacros.h"
#include "Lib/BaseType.h"
#include "Common/GameType.h"

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
}

int main()
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
	Expect_Size("uintptr_t follows pointer width", sizeof(void *), sizeof(uintptr_t));

#if defined(_WIN32)
	Expect_Size("Windows WideChar compatibility width", 2U, sizeof(WideChar));
# if defined(_WIN64)
	Expect_Size("Win64 pointer width", 8U, sizeof(void *));
# else
	Expect_Size("Win32 pointer width", 4U, sizeof(void *));
# endif
#endif

	int value = 7;
	void *pointer = &value;
	const uintptr_t bits = reinterpret_cast<uintptr_t>(pointer);
	void *roundTrip = reinterpret_cast<void *>(bits);
	if (roundTrip != pointer) {
		std::fprintf(stderr, "pointer -> uintptr_t -> pointer round-trip failed\n");
		++g_failures;
	}

	if (g_failures != 0) {
		std::fprintf(stderr, "%d architecture migration guard(s) failed.\n", g_failures);
		return 1;
	}

	std::printf("Step 04A architecture guard passed: native pointer width=%lu, fixed wire IDs remain 32-bit.\n",
		static_cast<unsigned long>(sizeof(void *) * 8U));
	return 0;
}
