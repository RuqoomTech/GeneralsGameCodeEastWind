#include "Common/RandomValue.h"
#include "Common/crc.h"
#include "GameLogic/FPUControl.h"
#include "GameLogic/LogicRandomValue.h"

#include <fenv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <string>
#include <vector>

namespace {

struct SimulationState
{
    UnsignedInt frame;
    Int score;
    Int energy;
    UnsignedInt events;
    Real position;
    Real velocity;
};

struct Checkpoint
{
    UnsignedInt frame;
    UnsignedInt logicalCRC;
    UnsignedInt rngCRC;
};

const UnsignedInt kSeed = 0x12345678U;
const UnsignedInt kEndFrame = 12000U;
const UnsignedInt kCheckpointFrames[] = { 0U, 1U, 10U, 100U, 1000U, 5000U, 10000U, kEndFrame };

UnsignedInt RealBits(Real value)
{
    UnsignedInt bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void CRCField(CRC &crc, const void *data, Int bytes)
{
    crc.computeCRC(data, bytes);
}

UnsignedInt ComputeLogicalCRC(const SimulationState &state)
{
    // Never hash the native object representation. Fold only explicit fixed-width
    // simulation fields, in protocol order, so native padding/pointers cannot enter.
    CRC crc;
    CRCField(crc, &state.frame, sizeof(state.frame));
    CRCField(crc, &state.score, sizeof(state.score));
    CRCField(crc, &state.energy, sizeof(state.energy));
    CRCField(crc, &state.events, sizeof(state.events));
    const UnsignedInt positionBits = RealBits(state.position);
    const UnsignedInt velocityBits = RealBits(state.velocity);
    CRCField(crc, &positionBits, sizeof(positionBits));
    CRCField(crc, &velocityBits, sizeof(velocityBits));
    const UnsignedInt rngCRC = GetGameLogicRandomSeedCRC();
    CRCField(crc, &rngCRC, sizeof(rngCRC));
    return crc.get();
}

void AdvanceFrame(SimulationState &state)
{
    // This mirrors the deterministic constraints of GameLogic::update(): reset the
    // FP environment, consume only GameLogic RNG, and retain Real as float32.
    setFPMode();

    const Int impulse = GameLogicRandomValue(-2500, 2500);
    const Real jitter = GameLogicRandomValueReal(-0.25f, 0.25f);
    const Int scoreDelta = GameLogicRandomValue(-20, 35);
    const Int energyDelta = GameLogicRandomValue(-7, 11);

    state.velocity = state.velocity * 0.984375f
        + static_cast<Real>(impulse) * 0.0009765625f
        + jitter;
    state.position = state.position + state.velocity * 0.03125f;
    state.score += scoreDelta;
    state.energy += energyDelta;
    if (state.energy < -5000)
        state.energy = -5000;
    else if (state.energy > 5000)
        state.energy = 5000;

    state.events = (state.events << 5) | (state.events >> 27);
    state.events ^= static_cast<UnsignedInt>(impulse);
    state.events += static_cast<UnsignedInt>(scoreDelta * 17 + energyDelta * 31);
    ++state.frame;
}

bool IsCheckpoint(UnsignedInt frame)
{
    for (size_t i = 0; i < sizeof(kCheckpointFrames) / sizeof(kCheckpointFrames[0]); ++i) {
        if (kCheckpointFrames[i] == frame)
            return true;
    }
    return false;
}

std::vector<Checkpoint> RunTimeline()
{
    InitRandom(kSeed);
    setFPMode();

    SimulationState state;
    state.frame = 0;
    state.score = 1250;
    state.energy = 300;
    state.events = 0x13579bdfU;
    state.position = 17.25f;
    state.velocity = -0.5f;

    std::vector<Checkpoint> result;
    if (IsCheckpoint(state.frame))
        result.push_back(Checkpoint{ state.frame, ComputeLogicalCRC(state), GetGameLogicRandomSeedCRC() });

    while (state.frame < kEndFrame) {
        AdvanceFrame(state);
        if (IsCheckpoint(state.frame))
            result.push_back(Checkpoint{ state.frame, ComputeLogicalCRC(state), GetGameLogicRandomSeedCRC() });
    }
    return result;
}

void EmitTimeline(FILE *out, const std::vector<Checkpoint> &timeline)
{
    fprintf(out, "step04d-headless-v1 seed=0x%08x end=%u\n",
        static_cast<unsigned int>(kSeed), static_cast<unsigned int>(kEndFrame));
    for (size_t i = 0; i < timeline.size(); ++i) {
        fprintf(out, "%u 0x%08x 0x%08x\n",
            static_cast<unsigned int>(timeline[i].frame),
            static_cast<unsigned int>(timeline[i].logicalCRC),
            static_cast<unsigned int>(timeline[i].rngCRC));
    }
}

bool ParseUnsigned(const std::string &token, UnsignedInt *value)
{
    char *end = nullptr;
    const unsigned long parsed = strtoul(token.c_str(), &end, 0);
    if (end == token.c_str() || *end != '\0')
        return false;
    *value = static_cast<UnsignedInt>(parsed);
    return true;
}

bool VerifyFixture(const char *path, const std::vector<Checkpoint> &actual)
{
    std::ifstream in(path);
    if (!in) {
        fprintf(stderr, "Cannot open Step 04D timeline fixture: %s\n", path);
        return false;
    }

    std::string header;
    std::getline(in, header);
    if (header != "step04d-headless-v1 seed=0x12345678 end=12000") {
        fprintf(stderr, "Unexpected Step 04D timeline fixture header: %s\n", header.c_str());
        return false;
    }

    std::vector<Checkpoint> expected;
    std::string frameToken;
    std::string crcToken;
    std::string rngToken;
    while (in >> frameToken >> crcToken >> rngToken) {
        Checkpoint cp = {};
        if (!ParseUnsigned(frameToken, &cp.frame)
            || !ParseUnsigned(crcToken, &cp.logicalCRC)
            || !ParseUnsigned(rngToken, &cp.rngCRC)) {
            fprintf(stderr, "Invalid Step 04D fixture row near frame token '%s'.\n", frameToken.c_str());
            return false;
        }
        expected.push_back(cp);
    }

    if (expected.size() != actual.size()) {
        fprintf(stderr, "Step 04D timeline count mismatch: expected %lu, got %lu.\n",
            static_cast<unsigned long>(expected.size()), static_cast<unsigned long>(actual.size()));
        return false;
    }

    for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i].frame != actual[i].frame
            || expected[i].logicalCRC != actual[i].logicalCRC
            || expected[i].rngCRC != actual[i].rngCRC) {
            fprintf(stderr,
                "Step 04D timeline mismatch at frame %u: expected crc=0x%08x rng=0x%08x, got crc=0x%08x rng=0x%08x.\n",
                static_cast<unsigned int>(expected[i].frame),
                static_cast<unsigned int>(expected[i].logicalCRC),
                static_cast<unsigned int>(expected[i].rngCRC),
                static_cast<unsigned int>(actual[i].logicalCRC),
                static_cast<unsigned int>(actual[i].rngCRC));
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    // Prove the production FP reset repairs a deliberately perturbed host rounding mode.
    if (fesetround(FE_DOWNWARD) != 0) {
        fprintf(stderr, "Unable to perturb floating-point rounding mode for Step 04D guard.\n");
        return 1;
    }
    setFPMode();
    if (fegetround() != FE_TONEAREST) {
        fprintf(stderr, "setFPMode() did not restore FE_TONEAREST.\n");
        return 1;
    }

    const std::vector<Checkpoint> timeline = RunTimeline();

    if (argc == 2 && strcmp(argv[1], "--emit") == 0) {
        EmitTimeline(stdout, timeline);
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "--verify") == 0) {
        if (!VerifyFixture(argv[2], timeline))
            return 1;
        puts("Step 04D headless deterministic timeline matched the checked-in fixture.");
        return 0;
    }

    fprintf(stderr, "Usage: %s --emit | --verify <fixture>\n", argv[0]);
    return 2;
}
