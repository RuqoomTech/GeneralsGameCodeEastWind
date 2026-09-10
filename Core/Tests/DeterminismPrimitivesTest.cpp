#include "Common/RandomValue.h"
#include "Common/crc.h"
#include "GameLogic/LogicRandomValue.h"

#if defined(RTS_ENGINE_DETERMINISM_TEST)
#include "Common/Xfer.h"
#include "Common/XferSave.h"
#include "Common/XferCRC.h"
#include "GameLogic/Damage.h"
#include "GameNetwork/NetworkDefs.h"
#include <stddef.h>
#include <vector>
#endif

#include <stdio.h>
#include <string.h>

namespace {

int g_failures = 0;

void Expect_Unsigned(const char *case_name, UnsignedInt expected, UnsignedInt actual)
{
    if (expected != actual) {
        fprintf(stderr, "%s: expected 0x%08x, got 0x%08x\n",
            case_name, static_cast<unsigned int>(expected), static_cast<unsigned int>(actual));
        ++g_failures;
    }
}

void Expect_Int(const char *case_name, Int expected, Int actual)
{
    if (expected != actual) {
        fprintf(stderr, "%s: expected %d, got %d\n",
            case_name, static_cast<int>(expected), static_cast<int>(actual));
        ++g_failures;
    }
}

UnsignedInt Real_Bits(Real value)
{
    UnsignedInt bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

Real Real_From_Bits(UnsignedInt bits)
{
    Real value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

void Expect_Real_Bits(const char *case_name, UnsignedInt expected_bits, Real actual)
{
    Expect_Unsigned(case_name, expected_bits, Real_Bits(actual));
}

void Check_Buffer(const char *case_name, const void *data, Int size, UnsignedInt expected)
{
    CRC crc;
    crc.computeCRC(data, size);
    Expect_Unsigned(case_name, expected, crc.get());
}

void Check_Logic_RNG_Sequence(
    const char *case_name,
    UnsignedInt seed,
    UnsignedInt expected_initial_crc,
    const Int *expected_values,
    const UnsignedInt *expected_state_crcs,
    Int count)
{
    InitRandom(seed);
    Expect_Unsigned(case_name, seed, GetGameLogicRandomSeed());
    Expect_Unsigned("RNG initial state CRC", expected_initial_crc, GetGameLogicRandomSeedCRC());

    for (Int i = 0; i < count; ++i) {
        const Int value = GameLogicRandomValue(0, 1000);
        Expect_Int("RNG sequence value", expected_values[i], value);
        Expect_Unsigned("RNG sequence state CRC", expected_state_crcs[i], GetGameLogicRandomSeedCRC());
    }

    // The replay/base seed is metadata for initialization; drawing values must not mutate it.
    Expect_Unsigned("RNG base seed remains stable", seed, GetGameLogicRandomSeed());
}


#if defined(RTS_ENGINE_DETERMINISM_TEST)

class CaptureXfer : public Xfer
{
public:
    CaptureXfer()
    {
        m_xferMode = XFER_SAVE;
    }

    virtual void open(AsciiString identifier) override
    {
        (void)identifier;
        m_bytes.clear();
    }

    virtual void close() override {}
    virtual Int beginBlock() override { return 0; }
    virtual void endBlock() override {}
    virtual void skip(Int dataSize) override { (void)dataSize; }
    virtual void xferSnapshot(Snapshot *snapshot) override { (void)snapshot; }

    const std::vector<UnsignedByte> &bytes() const { return m_bytes; }

protected:
    virtual void xferImplementation(void *data, Int dataSize) override
    {
        if (data == nullptr || dataSize <= 0) {
            return;
        }

        const UnsignedByte *begin = static_cast<const UnsignedByte *>(data);
        m_bytes.insert(m_bytes.end(), begin, begin + dataSize);
    }

private:
    std::vector<UnsignedByte> m_bytes;
};

class CaptureSaveXfer : public XferSave
{
public:
    const std::vector<UnsignedByte> &bytes() const { return m_bytes; }

protected:
    virtual void xferImplementation(void *data, Int dataSize) override
    {
        if (data == nullptr || dataSize <= 0) {
            return;
        }

        const UnsignedByte *begin = static_cast<const UnsignedByte *>(data);
        m_bytes.insert(m_bytes.end(), begin, begin + dataSize);
    }

private:
    std::vector<UnsignedByte> m_bytes;
};

void Expect_Size(const char *case_name, size_t expected, size_t actual)
{
    if (expected != actual) {
        fprintf(stderr, "%s: expected %lu byte(s), got %lu byte(s)\n",
            case_name, static_cast<unsigned long>(expected), static_cast<unsigned long>(actual));
        ++g_failures;
    }
}

void Expect_Bytes(const char *case_name, const std::vector<UnsignedByte> &actual,
    const UnsignedByte *expected, size_t expected_size)
{
    if (actual.size() != expected_size ||
        (expected_size != 0 && memcmp(&actual[0], expected, expected_size) != 0)) {
        fprintf(stderr, "%s: serialized bytes differ (expected %lu byte(s), got %lu byte(s))\n",
            case_name, static_cast<unsigned long>(expected_size),
            static_cast<unsigned long>(actual.size()));
        ++g_failures;
    }
}

void Check_Xfer_Primitive_Characterization()
{
    // Xfer's primitive methods intentionally serialize the native in-memory bytes.
    // The legacy/reference Windows runtime is little-endian, and save/replay compatibility
    // therefore depends on these widths and byte sequences remaining stable.
    Expect_Size("XferVersion width", 1U, sizeof(XferVersion));
    Expect_Size("Byte width", 1U, sizeof(Byte));
    Expect_Size("UnsignedByte width", 1U, sizeof(UnsignedByte));
    Expect_Size("Bool width", 1U, sizeof(Bool));
    Expect_Size("Int width", 4U, sizeof(Int));
    Expect_Size("Int64 width", 8U, sizeof(Int64));
    Expect_Size("UnsignedInt width", 4U, sizeof(UnsignedInt));
    Expect_Size("Short width", 2U, sizeof(Short));
    Expect_Size("UnsignedShort width", 2U, sizeof(UnsignedShort));
    Expect_Size("Real width", 4U, sizeof(Real));

    CaptureXfer xfer;

    XferVersion version = 3;
    Byte byte_value = static_cast<Byte>(0x7f);
    UnsignedByte unsigned_byte_value = 0xa5U;
    Bool bool_true = true;
    Bool bool_false = false;
    Int int_value = static_cast<Int>(0x12345678);
    Int64 int64_value = static_cast<Int64>(0x0123456789abcdefLL);
    UnsignedInt unsigned_int_value = 0x89abcdefU;
    Short short_value = static_cast<Short>(0x1234);
    UnsignedShort unsigned_short_value = 0xfedcU;
    Real real_value = 1.0f;
    UnsignedByte user_bytes[] = { 0xdeU, 0xadU, 0xbeU, 0xefU };

    xfer.xferVersion(&version, 3);
    xfer.xferByte(&byte_value);
    xfer.xferUnsignedByte(&unsigned_byte_value);
    xfer.xferBool(&bool_true);
    xfer.xferBool(&bool_false);
    xfer.xferInt(&int_value);
    xfer.xferInt64(&int64_value);
    xfer.xferUnsignedInt(&unsigned_int_value);
    xfer.xferShort(&short_value);
    xfer.xferUnsignedShort(&unsigned_short_value);
    xfer.xferReal(&real_value);
    xfer.xferUser(user_bytes, sizeof(user_bytes));

    const UnsignedByte expected[] = {
        0x03,
        0x7f,
        0xa5,
        0x01,
        0x00,
        0x78, 0x56, 0x34, 0x12,
        0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01,
        0xef, 0xcd, 0xab, 0x89,
        0x34, 0x12,
        0xdc, 0xfe,
        0x00, 0x00, 0x80, 0x3f,
        0xde, 0xad, 0xbe, 0xef
    };

    Expect_Bytes("Xfer primitive little-endian byte stream",
        xfer.bytes(), expected, sizeof(expected));
}

void Check_Xfer_Snapshot_Characterization()
{
    // Exercise a real production Snapshot implementation through XferSave's
    // production snapshot dispatch. DamageInfoOutput is deliberately small but
    // determinism-relevant: its serialized order is version, dealt, clipped, no-effect.
    DamageInfoOutput output;
    output.m_actualDamageDealt = 1.0f;
    output.m_actualDamageClipped = -2.0f;
    output.m_noEffect = true;

    CaptureSaveXfer xfer;
    xfer.xferSnapshot(&output);

    const UnsignedByte expected[] = {
        0x01,
        0x00, 0x00, 0x80, 0x3f,
        0x00, 0x00, 0x00, 0xc0,
        0x01
    };

    Expect_Bytes("DamageInfoOutput snapshot field order",
        xfer.bytes(), expected, sizeof(expected));
}

void Check_Xfer_CRC_Characterization()
{
    // XferCRC is the production state-CRC transport. Lock both its 32-bit
    // network-order folding and its partial-tail behavior across separate xfer calls.
    XferCRC xfer;
    xfer.open("determinism-characterization");
    Expect_Unsigned("XferCRC initial state", 0x00000000U, xfer.getCRC());

    UnsignedInt word = 0x12345678U;
    xfer.xferUnsignedInt(&word);
    Expect_Unsigned("XferCRC 32-bit word", 0x12345678U, xfer.getCRC());

    UnsignedShort short_value = 0x9abcU;
    xfer.xferUnsignedShort(&short_value);
    Expect_Unsigned("XferCRC 16-bit tail", 0xe002adf0U, xfer.getCRC());

    UnsignedByte byte_value = 0xefU;
    xfer.xferUnsignedByte(&byte_value);
    Expect_Unsigned("XferCRC 8-bit tail", 0xb0065ae1U, xfer.getCRC());

    Real real_value = 1.0f;
    xfer.xferReal(&real_value);
    Expect_Unsigned("XferCRC Real word", 0xa08db4c2U, xfer.getCRC());
}

void Check_ABI_And_Replay_Characterization()
{
    // These are compatibility/reference Win32 ABI assumptions that feed raw network/replay
    // formats. They are intentionally strict: x64/Evolution must introduce explicit wire
    // layouts instead of silently inheriting different native sizes.
    Expect_Size("reference pointer width", 4U, sizeof(void *));
    Expect_Size("WideChar replay width", 2U, sizeof(WideChar));
    Expect_Size("ObjectID replay width", 4U, sizeof(ObjectID));
    Expect_Size("DrawableID replay width", 4U, sizeof(DrawableID));
    Expect_Size("Coord3D replay width", 12U, sizeof(Coord3D));
    Expect_Size("ICoord2D replay width", 8U, sizeof(ICoord2D));
    Expect_Size("IRegion2D replay width", 16U, sizeof(IRegion2D));
    Expect_Size("GameMessage::Type replay width", 4U, sizeof(GameMessage::Type));
    Expect_Size("GameMessageArgumentDataType replay width", 4U, sizeof(GameMessageArgumentDataType));
    Expect_Size("GameMessageArgumentType union width", 16U, sizeof(GameMessageArgumentType));

    Expect_Int("network message enum base", 1000, static_cast<Int>(GameMessage::MSG_BEGIN_NETWORK_MESSAGES));
    Expect_Int("logic CRC message enum", 1093, static_cast<Int>(GameMessage::MSG_LOGIC_CRC));
    Expect_Int("network message enum end", 1999, static_cast<Int>(GameMessage::MSG_END_NETWORK_MESSAGES));
    Expect_Int("integer replay argument tag", 0, static_cast<Int>(ARGUMENTDATATYPE_INTEGER));
    Expect_Int("location replay argument tag", 6, static_cast<Int>(ARGUMENTDATATYPE_LOCATION));
    Expect_Int("wide-char replay argument tag", 10, static_cast<Int>(ARGUMENTDATATYPE_WIDECHAR));

    Expect_Size("TransportMessageHeader size", 6U, sizeof(TransportMessageHeader));
    Expect_Size("TransportMessageHeader crc offset", 0U, offsetof(TransportMessageHeader, crc));
    Expect_Size("TransportMessageHeader magic offset", 4U, offsetof(TransportMessageHeader, magic));

    // GameMessage's native size currently participates in command-packet capacity. Locking
    // this catches STL/ABI/compiler drift before it silently changes the network packet shape.
    Expect_Size("GameMessage Win32 ABI size", 36U, sizeof(GameMessage));
    Expect_Int("commands per retail command packet", 28, numCommandsPerCommandPacket);
    Expect_Size("CommandPacket frame offset", 0U, offsetof(CommandPacket, m_frame));
    Expect_Size("CommandPacket count offset", 4U, offsetof(CommandPacket, m_numCommands));
    Expect_Size("CommandPacket command offset", 6U, offsetof(CommandPacket, m_commands));
    Expect_Size("CommandPacket retail size", 1014U, sizeof(CommandPacket));

    // Known replay command-record fixture following RecorderClass::writeToFile():
    // frame 0x11223344, MSG_LOGIC_CRC, player 2, one INTEGER argument 0x89abcdef.
    const UnsignedByte replay_logic_crc_record[] = {
        0x44, 0x33, 0x22, 0x11,
        0x45, 0x04, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x01,
        0x00, 0x01,
        0xef, 0xcd, 0xab, 0x89
    };
    CRC replay_crc;
    replay_crc.computeCRC(replay_logic_crc_record, sizeof(replay_logic_crc_record));
    Expect_Unsigned("replay logic-CRC record byte checkpoint", 0x01b254dbU, replay_crc.get());

    XferCRC xfer_crc;
    xfer_crc.open("replay-record-checkpoint");
    xfer_crc.xferUser(const_cast<UnsignedByte *>(replay_logic_crc_record), sizeof(replay_logic_crc_record));
    Expect_Unsigned("replay record XferCRC checkpoint", 0xc1d0db75U, xfer_crc.getCRC());
}

#endif

void Check_Fast_Float_Characterization()
{
    // Lock the legacy bit-level trunc/floor/ceil behavior before compiler migration.
    // Some edge behavior differs from std::floor/std::ceil and is intentionally preserved.
    struct FloatCase {
        UnsignedInt input;
        UnsignedInt trunc_value;
        UnsignedInt floor_value;
        UnsignedInt ceil_value;
    };
    const FloatCase cases[] = {
        { 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U },
        { 0x80000000U, 0x00000000U, 0x00000000U, 0x00000000U },
        { 0x3f000000U, 0x00000000U, 0x00000000U, 0x3f800000U },
        { 0xbf000000U, 0x00000000U, 0xbf800000U, 0x00000000U },
        { 0x3f800000U, 0x3f800000U, 0x3f800000U, 0x40000000U },
        { 0xbf800000U, 0xbf800000U, 0xc0000000U, 0xbf800000U },
        { 0x3fc00000U, 0x3f800000U, 0x3f800000U, 0x40000000U },
        { 0xbfc00000U, 0xbf800000U, 0xc0000000U, 0xbf800000U },
        { 0x41280000U, 0x41200000U, 0x41200000U, 0x41300000U },
        { 0xc1280000U, 0xc1200000U, 0xc1300000U, 0xc1200000U }
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const Real input = Real_From_Bits(cases[i].input);
        Expect_Real_Bits("fast_float_trunc bits", cases[i].trunc_value, fast_float_trunc(input));
        Expect_Real_Bits("fast_float_floor bits", cases[i].floor_value, fast_float_floor(input));
        Expect_Real_Bits("fast_float_ceil bits", cases[i].ceil_value, fast_float_ceil(input));
    }
}

void Check_CRC_Characterization()
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
    Expect_Unsigned("incremental prefix", 0x00000004U, incremental.get());
    incremental.computeCRC(sequential_bytes + 3, 5);
    Expect_Unsigned("incremental final", 0x000000f7U, incremental.get());

    CRC no_op;
    no_op.computeCRC(one_byte, sizeof(one_byte));
    no_op.computeCRC(0, 8);
    Expect_Unsigned("null input is a no-op", 0x00000001U, no_op.get());
    no_op.computeCRC(one_byte, 0);
    Expect_Unsigned("zero length is a no-op", 0x00000001U, no_op.get());
    no_op.computeCRC(one_byte, -1);
    Expect_Unsigned("negative length is a no-op", 0x00000001U, no_op.get());
    no_op.clear();
    Expect_Unsigned("clear resets CRC", 0x00000000U, no_op.get());

    CRC split_carry;
    split_carry.computeCRC(carry_bytes, 13);
    split_carry.computeCRC(carry_bytes + 13, 19);
    Expect_Unsigned("split carry-heavy input", 0xfbf7ef5dU, split_carry.get());
}

void Check_Game_Logic_RNG_Characterization()
{
    // These vectors exercise the production six-word game-logic RNG state through
    // InitRandom(), GameLogicRandomValue(), and GetGameLogicRandomSeedCRC().
    const Int seed_zero_values[] = {
        135, 422, 430, 435, 906, 472, 619, 848, 652, 67
    };
    const UnsignedInt seed_zero_crcs[] = {
        0xaac177a1U, 0xb498d7f9U, 0xcceac7a1U, 0x52aa9529U, 0x9b4762d1U,
        0xdcc28ff9U, 0x48d4c3e1U, 0x53658f29U, 0x928dd411U, 0x87732929U
    };

    const Int seed_pattern_values[] = {
        823, 209, 704, 299, 632, 971, 682, 948, 750, 591
    };
    const UnsignedInt seed_pattern_crcs[] = {
        0xa87859d4U, 0xb904045cU, 0x6f39fa84U, 0x76b75e0cU, 0x45b82a34U,
        0x9eef5cdcU, 0xb898b5e4U, 0xc4405f5cU, 0x83b98f64U, 0x4d01640cU
    };

    Check_Logic_RNG_Sequence(
        "RNG base seed zero",
        0x00000000U,
        0x4c71fff9U,
        seed_zero_values,
        seed_zero_crcs,
        sizeof(seed_zero_values) / sizeof(seed_zero_values[0]));

    Check_Logic_RNG_Sequence(
        "RNG patterned base seed",
        0x12345678U,
        0x933b34acU,
        seed_pattern_values,
        seed_pattern_crcs,
        sizeof(seed_pattern_values) / sizeof(seed_pattern_values[0]));

    // Lock signed-range modulo/add behavior independently from the 0..1000 vectors.
    const Int signed_values[] = { -17, -21, 65, 2, 66, -65 };
    InitRandom(0x12345678U);
    for (Int i = 0; i < static_cast<Int>(sizeof(signed_values) / sizeof(signed_values[0])); ++i) {
        Expect_Int("RNG signed-range value", signed_values[i], GameLogicRandomValue(-100, 100));
    }
    Expect_Unsigned("RNG signed-range final state CRC", 0x9eef5cdcU, GetGameLogicRandomSeedCRC());

    // Retail-compatible behavior consumes one logic RNG value even when lo == hi.
    InitRandom(0x12345678U);
    Expect_Unsigned("RNG equal-range initial state CRC", 0x933b34acU, GetGameLogicRandomSeedCRC());
    Expect_Int("RNG equal range return", 7, GameLogicRandomValue(7, 7));
    Expect_Unsigned("RNG equal range advances state", 0xa87859d4U, GetGameLogicRandomSeedCRC());

    // With RETAIL_COMPATIBLE_CRC enabled, the historical Unchanged API delegates to
    // the normal logic RNG and therefore advances the production seed state.
    InitRandom(0x12345678U);
    Expect_Int("RNG retail unchanged return", 823, GameLogicRandomValueUnchanged(0, 1000));
    Expect_Unsigned("RNG retail unchanged advances state", 0xa87859d4U, GetGameLogicRandomSeedCRC());

    // Reinitializing the same seed must reproduce the first value and first state transition.
    InitRandom(0x12345678U);
    Expect_Int("RNG reset first value", 823, GameLogicRandomValue(0, 1000));
    Expect_Unsigned("RNG reset first state CRC", 0xa87859d4U, GetGameLogicRandomSeedCRC());
    InitRandom(0x12345678U);
    Expect_Int("RNG reset reproduced value", 823, GameLogicRandomValue(0, 1000));
    Expect_Unsigned("RNG reset reproduced state CRC", 0xa87859d4U, GetGameLogicRandomSeedCRC());

    // Real-valued RNG is compared by IEEE-754 bits, not formatted decimal output.
    const UnsignedInt real_bits[] = {
        0x3f05a8b2U, 0x3e966700U, 0x3f4735c4U,
        0xbe1c38d4U, 0xbe2afd10U, 0x3f0ab9c6U
    };
    const UnsignedInt real_state_crcs[] = {
        0xa87859d4U, 0xb904045cU, 0x6f39fa84U,
        0x76b75e0cU, 0x45b82a34U, 0x9eef5cdcU
    };
    InitRandom(0x12345678U);
    for (size_t i = 0; i < sizeof(real_bits) / sizeof(real_bits[0]); ++i) {
        Expect_Real_Bits("RNG real value bits", real_bits[i], GameLogicRandomValueReal(-1.0f, 1.0f));
        Expect_Unsigned("RNG real state CRC", real_state_crcs[i], GetGameLogicRandomSeedCRC());
    }

    // Unlike the retail-compatible integer equal-range path, non-positive real delta returns
    // immediately and must not consume RNG state.
    InitRandom(0x12345678U);
    Expect_Real_Bits("RNG real equal-range return", 0x40e00000U, GameLogicRandomValueReal(7.0f, 7.0f));
    Expect_Unsigned("RNG real equal-range state unchanged", 0x933b34acU, GetGameLogicRandomSeedCRC());
}

} // namespace

int main()
{
    Check_Fast_Float_Characterization();
    Check_CRC_Characterization();
    Check_Game_Logic_RNG_Characterization();
#if defined(RTS_ENGINE_DETERMINISM_TEST)
    Check_Xfer_Primitive_Characterization();
    Check_Xfer_Snapshot_Characterization();
    Check_Xfer_CRC_Characterization();
    Check_ABI_And_Replay_Characterization();
#endif

    if (g_failures != 0) {
        fprintf(stderr, "%d determinism primitive test(s) failed.\n", g_failures);
        return 1;
    }

#if defined(RTS_ENGINE_DETERMINISM_TEST)
    puts("Step 01 determinism guard passed: float helpers, CRC/RNG, Xfer/XferCRC, snapshot, ABI, and replay checkpoints.");
#else
    puts("Determinism float-helper, CRC, and game-logic RNG characterization tests passed.");
#endif
    return 0;
}
