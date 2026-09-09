#include "Common/crc.h"

#include <stdio.h>
#include <string.h>

namespace {

int g_failures = 0;

void Expect_CRC(const char *case_name, UnsignedInt expected, UnsignedInt actual)
{
    if (expected != actual) {
        fprintf(stderr, "%s: expected 0x%08x, got 0x%08x\n",
            case_name, static_cast<unsigned int>(expected), static_cast<unsigned int>(actual));
        ++g_failures;
    }
}

void Check_Buffer(const char *case_name, const void *data, Int size, UnsignedInt expected)
{
    CRC crc;
    crc.computeCRC(data, size);
    Expect_CRC(case_name, expected, crc.get());
}

} // namespace

int main()
{
    // These are characterization vectors for the production Common/crc.h primitive.
    // They intentionally lock the current rotate/add/carry byte-processing behavior.
    const UnsignedByte one_byte[] = { 0x01 };
    const UnsignedByte sequential_bytes[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
    const char generals_text[] = "Generals";
    const UnsignedByte high_bit_bytes[] = { 0xff, 0x80, 0x7f, 0x00, 0xaa, 0x55 };

    UnsignedByte carry_bytes[32];
    for (Int i = 0; i < 32; ++i) {
        carry_bytes[i] = static_cast<UnsignedByte>(i * 37 + 11);
    }

    Check_Buffer("empty buffer", sequential_bytes, 0, 0x00000000U);
    Check_Buffer("one byte", one_byte, sizeof(one_byte), 0x00000001U);
    Check_Buffer("sequential bytes", sequential_bytes, sizeof(sequential_bytes), 0x000000f7U);
    Check_Buffer("ASCII bytes", generals_text, strlen(generals_text), 0x0000572fU);
    Check_Buffer("high-bit bytes", high_bit_bytes, sizeof(high_bit_bytes), 0x00002d81U);
    Check_Buffer("carry-heavy bytes", carry_bytes, sizeof(carry_bytes), 0xfbf7ef5dU);

    CRC incremental;
    incremental.computeCRC(sequential_bytes, 3);
    Expect_CRC("incremental prefix", 0x00000004U, incremental.get());
    incremental.computeCRC(sequential_bytes + 3, 5);
    Expect_CRC("incremental final", 0x000000f7U, incremental.get());

    CRC no_op;
    no_op.computeCRC(one_byte, sizeof(one_byte));
    no_op.computeCRC(0, 8);
    Expect_CRC("null input is a no-op", 0x00000001U, no_op.get());
    no_op.computeCRC(one_byte, 0);
    Expect_CRC("zero length is a no-op", 0x00000001U, no_op.get());
    no_op.computeCRC(one_byte, -1);
    Expect_CRC("negative length is a no-op", 0x00000001U, no_op.get());
    no_op.clear();
    Expect_CRC("clear resets CRC", 0x00000000U, no_op.get());

    CRC split_carry;
    split_carry.computeCRC(carry_bytes, 13);
    split_carry.computeCRC(carry_bytes + 13, 19);
    Expect_CRC("split carry-heavy input", 0xfbf7ef5dU, split_carry.get());

    if (g_failures != 0) {
        fprintf(stderr, "%d determinism primitive test(s) failed.\n", g_failures);
        return 1;
    }

    puts("Determinism CRC primitive characterization tests passed.");
    return 0;
}
