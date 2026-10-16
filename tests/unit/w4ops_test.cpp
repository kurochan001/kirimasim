// Compares the kernel's fast one-word operations (kernel/w4ops.h) with the
// valsem reference (valsem/value.h) on random values of every width from 1
// to 64, with and without X and Z bits (VALUE_SEMANTICS.md 3).

#include <cstdint>
#include <random>
#include <string>

#include <gtest/gtest.h>

#include "fuzz.h"
#include "kirimasim/kernel/w4ops.h"
#include "kirimasim/valsem/value.h"

namespace {

namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;
using k::W4;

v::Value ref(W4 x, unsigned w, bool s) { return v::fromWords({x.a}, {x.b}, w, s); }

std::string show(W4 x, unsigned w) { return v::toBinary(ref(x, w, false)); }

/// Random values that often hit the interesting cases: 0, 1, all ones, the
/// sign bit, small numbers, and a few X or Z bits.
class Gen {
public:
    W4 next(unsigned w) {
        const std::uint64_t m = k::mask(w);
        std::uint64_t a = 0;
        switch (rng_() % 8) {
            case 0: a = 0; break;
            case 1: a = 1; break;
            case 2: a = m; break;
            case 3: a = std::uint64_t{1} << (w - 1); break;
            case 4: a = rng_() % 8; break;
            case 5: a = m - (rng_() % 4); break;
            default: a = rng_(); break;
        }
        a &= m;
        std::uint64_t b = 0;
        switch (rng_() % 4) {
            case 0: b = rng_() & rng_() & rng_(); break;          // a few unknown bits
            case 1: b = std::uint64_t{1} << (rng_() % w); break;  // one unknown bit
            default: break;                                       // two-state
        }
        return {a, b & m};
    }
    unsigned width() { return 1 + static_cast<unsigned>(rng_() % 64); }
    bool flip() { return (rng_() & 1U) != 0; }

private:
    std::mt19937_64 rng_{kirimasim::test::fuzzSeed(0x77346f7073ULL)};
};

/// Rounds of each fuzz test: 20000 times HS_FUZZ_SCALE (fuzz.h).
std::uint64_t rounds() { return 20'000 * kirimasim::test::fuzzScale(); }

/// Whether the fast result equals the reference, with a message if not.
::testing::AssertionResult same(W4 fast, const v::Value& slow, const std::string& what) {
    const v::Value got = ref(fast, slow.width(), slow.isSigned);
    if (got == slow) return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure()
           << what << ": fast " << v::toBinary(got) << " reference " << v::toBinary(slow);
}

TEST(W4Ops, Binary) {
    Gen g;
    SCOPED_TRACE(kirimasim::test::fuzzSetting(kirimasim::test::fuzzSeed(0x77346f7073ULL)));
    for (std::uint64_t n = 0, end = rounds(); n < end; ++n) {
        const unsigned w = g.width();
        const bool s = g.flip();
        const W4 x = g.next(w);
        const W4 y = g.next(w);
        const v::Value rx = ref(x, w, s);
        const v::Value ry = ref(y, w, s);
        const std::string ctx = "w=" + std::to_string(w) + (s ? " signed " : " unsigned ") +
                                show(x, w) + ", " + show(y, w);
        ASSERT_TRUE(same(k::and4(x, y), v::bitAnd(rx, ry), "and " + ctx));
        ASSERT_TRUE(same(k::or4(x, y), v::bitOr(rx, ry), "or " + ctx));
        ASSERT_TRUE(same(k::xor4(x, y), v::bitXor(rx, ry), "xor " + ctx));
        ASSERT_TRUE(same(k::xnor4(x, y, w), v::bitXnor(rx, ry), "xnor " + ctx));
        ASSERT_TRUE(same(k::add4(x, y, w), v::add(rx, ry), "add " + ctx));
        ASSERT_TRUE(same(k::sub4(x, y, w), v::sub(rx, ry), "sub " + ctx));
        ASSERT_TRUE(same(k::mul4(x, y, w), v::mul(rx, ry), "mul " + ctx));
        ASSERT_TRUE(same(k::div4(x, y, w, s), v::div(rx, ry), "div " + ctx));
        ASSERT_TRUE(same(k::mod4(x, y, w, s), v::mod(rx, ry), "mod " + ctx));
        ASSERT_TRUE(same(k::lt4(x, y, w, s), v::lt(rx, ry), "lt " + ctx));
        ASSERT_TRUE(same(k::le4(x, y, w, s), v::le(rx, ry), "le " + ctx));
        ASSERT_TRUE(same(k::gt4(x, y, w, s), v::gt(rx, ry), "gt " + ctx));
        ASSERT_TRUE(same(k::ge4(x, y, w, s), v::ge(rx, ry), "ge " + ctx));
        ASSERT_TRUE(same(k::eq4(x, y), v::eq(rx, ry), "eq " + ctx));
        ASSERT_TRUE(same(k::ne4(x, y), v::ne(rx, ry), "ne " + ctx));
        ASSERT_TRUE(same(k::ceq4(x, y), v::ceq(rx, ry), "ceq " + ctx));
        ASSERT_TRUE(same(k::cne4(x, y), v::cne(rx, ry), "cne " + ctx));
        ASSERT_TRUE(same(k::weq4(x, y), v::weq(rx, ry), "weq " + ctx));
        ASSERT_TRUE(same(k::wne4(x, y), v::wne(rx, ry), "wne " + ctx));
        ASSERT_TRUE(same(k::caseMatch4(x, y, false), v::caseMatch(rx, ry, false), "casez " + ctx));
        ASSERT_TRUE(same(k::caseMatch4(x, y, true), v::caseMatch(rx, ry, true), "casex " + ctx));
        ASSERT_TRUE(same(k::land4(x, y), v::land(rx, ry), "land " + ctx));
        ASSERT_TRUE(same(k::lor4(x, y), v::lor(rx, ry), "lor " + ctx));
    }
}

TEST(W4Ops, Unary) {
    Gen g;
    SCOPED_TRACE(kirimasim::test::fuzzSetting(kirimasim::test::fuzzSeed(0x77346f7073ULL)));
    for (std::uint64_t n = 0, end = rounds(); n < end; ++n) {
        const unsigned w = g.width();
        const bool s = g.flip();
        const W4 x = g.next(w);
        const v::Value rx = ref(x, w, s);
        const std::string ctx = "w=" + std::to_string(w) + " " + show(x, w);
        ASSERT_TRUE(same(k::not4(x, w), v::bitNot(rx), "not " + ctx));
        ASSERT_TRUE(same(k::neg4(x, w), v::neg(rx), "neg " + ctx));
        ASSERT_TRUE(same(k::truth4(x), v::truth(rx), "truth " + ctx));
        ASSERT_TRUE(same(k::lnot4(x), v::lnot(rx), "lnot " + ctx));
        ASSERT_TRUE(same(k::redAnd4(x, w), v::redAnd(rx), "rand " + ctx));
        ASSERT_TRUE(same(k::redNand4(x, w), v::redNand(rx), "rnand " + ctx));
        ASSERT_TRUE(same(k::redOr4(x), v::redOr(rx), "ror " + ctx));
        ASSERT_TRUE(same(k::redNor4(x), v::redNor(rx), "rnor " + ctx));
        ASSERT_TRUE(same(k::redXor4(x), v::redXor(rx), "rxor " + ctx));
        ASSERT_TRUE(same(k::redXnor4(x), v::redXnor(rx), "rxnor " + ctx));
        ASSERT_TRUE(same(k::to2(x), v::to2(rx), "to2 " + ctx));
        const unsigned to = w + static_cast<unsigned>(n % (65 - w));  // w..64
        ASSERT_TRUE(same(x, v::zext(rx, to, s), "zext " + ctx));
        ASSERT_TRUE(same(k::sext4(x, w, to), v::sext(rx, to, s), "sext " + ctx));
        const unsigned narrow = 1 + static_cast<unsigned>(n % w);  // 1..w
        ASSERT_TRUE(same(k::trunc4(x, narrow), v::trunc(rx, narrow, s), "trunc " + ctx));
    }
}

TEST(W4Ops, ShiftsAndPower) {
    Gen g;
    SCOPED_TRACE(kirimasim::test::fuzzSetting(kirimasim::test::fuzzSeed(0x77346f7073ULL)));
    for (std::uint64_t n = 0, end = rounds(); n < end; ++n) {
        const unsigned w = g.width();
        const unsigned aw = g.width();  // the amount or exponent has its own width
        const bool s = g.flip();
        const bool as = g.flip();
        const W4 x = g.next(w);
        W4 amount = g.next(aw);
        if (n % 3 == 0) amount = {amount.a % (w + 3), amount.b};  // around the width
        const v::Value rx = ref(x, w, s);
        const v::Value ra = ref(amount, aw, as);
        const std::string ctx =
            "w=" + std::to_string(w) + " " + show(x, w) + " by " + show(amount, aw);
        ASSERT_TRUE(same(k::shl4(x, amount, w), v::shl(rx, ra), "shl " + ctx));
        ASSERT_TRUE(same(k::shr4(x, amount, w), v::shr(rx, ra), "shr " + ctx));
        ASSERT_TRUE(same(k::ashr4(x, amount, w, s), v::ashr(rx, ra), "ashr " + ctx));
        ASSERT_TRUE(same(k::pow4(x, amount, w, s, aw, as), v::pow(rx, ra), "pow " + ctx));
    }
}

TEST(W4Ops, Select) {
    Gen g;
    SCOPED_TRACE(kirimasim::test::fuzzSetting(kirimasim::test::fuzzSeed(0x77346f7073ULL)));
    for (std::uint64_t n = 0, end = rounds(); n < end; ++n) {
        const unsigned w = g.width();
        const unsigned cw = 1 + static_cast<unsigned>(n % 3);
        const W4 c = g.next(cw);
        const W4 x = g.next(w);
        // Equal sides now and then, so the "equal values" rule is exercised.
        const W4 y = n % 5 == 0 ? x : g.next(w);
        ASSERT_TRUE(same(k::select4(c, x, y, w),
                         v::select(ref(c, cw, false), ref(x, w, false), ref(y, w, false)),
                         "select " + show(c, cw) + " ? " + show(x, w) + " : " + show(y, w)));
    }
}

}  // namespace
