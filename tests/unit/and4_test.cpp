// Compares the fast four-state AND in the kernel with the one-bit-at-a-time
// reference in valsem.

#include <cstdint>
#include <random>

#include <gtest/gtest.h>

#include "kirimasim/kernel/w4.h"
#include "kirimasim/valsem/logic.h"

namespace {

using kirimasim::kernel::W4;
using kirimasim::valsem::Logic;

Logic bitAt(W4 w, int i) {
    const bool a = (w.a >> i) & 1U;
    const bool b = (w.b >> i) & 1U;
    if (!b) return a ? Logic::One : Logic::Zero;
    return a ? Logic::X : Logic::Z;
}

W4 fromLogic(Logic v) {
    switch (v) {
        case Logic::Zero: return {0, 0};
        case Logic::One: return {1, 0};
        case Logic::Z: return {0, 1};
        case Logic::X: return {1, 1};
    }
    return {0, 0};
}

const char* name(Logic v) {
    switch (v) {
        case Logic::Zero: return "0";
        case Logic::One: return "1";
        case Logic::Z: return "z";
        case Logic::X: return "x";
    }
    return "?";
}

constexpr Logic kAll[] = {Logic::Zero, Logic::One, Logic::Z, Logic::X};

TEST(And4, MatchesReferenceForAllOneBitPairs) {
    for (const Logic x : kAll) {
        for (const Logic y : kAll) {
            const W4 r = kirimasim::kernel::and4(fromLogic(x), fromLogic(y));
            EXPECT_EQ(bitAt(r, 0), kirimasim::valsem::logicAnd(x, y))
                << name(x) << " & " << name(y);
        }
    }
}

TEST(And4, MatchesReferenceForRandomWords) {
    // A fixed seed keeps the test reproducible.
    std::mt19937_64 rng(0x6861'7265'6d61'7369ULL);  // NOLINT(bugprone-random-generator-seed)
    for (int n = 0; n < 10'000; ++n) {
        W4 x{rng(), rng()};
        W4 y{rng(), rng()};
        // Every fourth pair has no X or Z, so the two-state path is covered too.
        if (n % 4 == 0) x.b = y.b = 0;
        const W4 r = kirimasim::kernel::and4(x, y);
        for (int i = 0; i < 64; ++i) {
            ASSERT_EQ(bitAt(r, i), kirimasim::valsem::logicAnd(bitAt(x, i), bitAt(y, i)))
                << "pair " << n << ", bit " << i;
        }
    }
}

}  // namespace
