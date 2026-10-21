// The generator of $urandom (SYSTEM_TASKS.md 13): SplitMix64 against its
// published first value, FNV-1a against its published values, and the
// bounds of $urandom_range.

#include <cstdint>

#include <gtest/gtest.h>

#include "kirimasim/kernel/urandom.h"

namespace {

using kirimasim::kernel::initialState;
using kirimasim::kernel::Rng;
using kirimasim::kernel::urandomRange;

TEST(Urandom, SplitMix64) {
    Rng g{0};
    EXPECT_EQ(g.next(), 0xe220a8397b1dcdafULL);
    Rng h{0};
    EXPECT_EQ(h.draw(), 0xe220a839U);
    // A child takes a whole step as its state.
    Rng p{0};
    EXPECT_EQ(p.child().state, 0xe220a8397b1dcdafULL);
}

TEST(Urandom, InitialStateHashesTheName) {
    EXPECT_EQ(initialState(0, ""), 0xcbf29ce484222325ULL);
    EXPECT_EQ(initialState(0, "a"), 0xaf63dc4c8601ec8cULL);
    EXPECT_EQ(initialState(5, "a"), 0xaf63dc4c8601ec8cULL ^ 5U);
}

TEST(Urandom, RangeStaysInsideAndSwaps) {
    Rng g{7};
    for (int i = 0; i < 1000; ++i) {
        const std::uint32_t v = urandomRange(g, 7, 3);
        EXPECT_GE(v, 3U);
        EXPECT_LE(v, 7U);
        const std::uint32_t w = urandomRange(g, 3, 7);
        EXPECT_GE(w, 3U);
        EXPECT_LE(w, 7U);
    }
    // One value: no choice; the whole range: the random number itself.
    EXPECT_EQ(urandomRange(g, 9, 9), 9U);
    Rng a{11};
    Rng b{11};
    EXPECT_EQ(urandomRange(a, 0xffffffffU, 0), b.draw());
}

}  // namespace
