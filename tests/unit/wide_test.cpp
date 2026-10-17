// Compares the kernel's operations on values of any width (kernel/wide.h)
// with the valsem reference (valsem/value.h) on random values of widths 1
// to 260, with and without X and Z bits (VALUE_SEMANTICS.md 3).

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "fuzz.h"
#include "kirimasim/kernel/w4ops.h"
#include "kirimasim/kernel/wide.h"
#include "kirimasim/valsem/value.h"

namespace {

namespace k = kirimasim::kernel;
namespace kw = kirimasim::kernel::wide;
namespace v = kirimasim::valsem;
using k::W4;
using Wide = std::vector<W4>;

v::Value ref(const Wide& x, unsigned w, bool s) {
    std::vector<std::uint64_t> a, b;
    for (const W4& c : x) {
        a.push_back(c.a);
        b.push_back(c.b);
    }
    return v::fromWords(a, b, w, s);
}

Wide fromRef(const v::Value& r) {
    std::vector<std::uint64_t> a, b;
    v::toWords(r, a, b);
    Wide out(a.size());
    for (std::size_t i = 0; i < a.size(); ++i)
        out[i] = {a[i], b[i]};
    return out;
}

/// Random values that often hit 0, 1, all ones, the sign bit, small
/// numbers and a few X or Z bits.
class Gen {
public:
    Wide next(unsigned w) {
        const unsigned n = kw::chunks(w);
        Wide x(n, W4{0, 0});
        const unsigned kind = static_cast<unsigned>(rng_() % 8);
        for (unsigned k = 0; k < n; ++k) {
            std::uint64_t a = 0;
            switch (kind) {
                case 0: a = 0; break;
                case 1: a = k == 0 ? 1 : 0; break;
                case 2: a = ~std::uint64_t{0}; break;
                case 3: a = k + 1 == n ? std::uint64_t{1} << ((w - 1) % 64) : 0; break;
                case 4: a = k == 0 ? rng_() % 8 : 0; break;
                case 5: a = ~std::uint64_t{0} - (k == 0 ? rng_() % 4 : 0); break;
                default: a = rng_(); break;
            }
            x[k].a = a;
        }
        switch (rng_() % 4) {
            case 0:
                for (unsigned k = 0; k < n; ++k)
                    x[k].b = rng_() & rng_() & rng_();
                break;
            case 1: {
                const unsigned i = static_cast<unsigned>(rng_() % w);
                x[i / 64].b = std::uint64_t{1} << (i % 64);
                break;
            }
            default: break;
        }
        // Bits above w are 0.
        for (unsigned k = 0; k < n; ++k) {
            const unsigned lo = 64 * k;
            const std::uint64_t m = w - lo >= 64 ? ~std::uint64_t{0} : k::mask(w - lo);
            x[k].a &= m;
            x[k].b &= m;
        }
        return x;
    }
    unsigned width() { return 1 + static_cast<unsigned>(rng_() % 260); }
    bool flip() { return (rng_() & 1U) != 0; }
    std::uint64_t raw() { return rng_(); }

private:
    std::mt19937_64 rng_{kirimasim::test::fuzzSeed(0x77696465ULL)};
};

std::uint64_t rounds() { return 4'000 * kirimasim::test::fuzzScale(); }

::testing::AssertionResult same(const Wide& fast, const v::Value& slow, const std::string& what) {
    const v::Value got = ref(fast, slow.width(), slow.isSigned);
    if (got == slow) return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure()
           << what << ": fast " << v::toBinary(got) << " reference " << v::toBinary(slow);
}

::testing::AssertionResult same1(W4 fast, const v::Value& slow, const std::string& what) {
    return same(Wide{fast}, slow, what);
}

TEST(Wide, Binary) {
    Gen g;
    SCOPED_TRACE(kirimasim::test::fuzzSetting(kirimasim::test::fuzzSeed(0x77696465ULL)));
    for (std::uint64_t n = 0, end = rounds(); n < end; ++n) {
        const unsigned w = g.width();
        const bool s = g.flip();
        const Wide x = g.next(w);
        const Wide y = g.next(w);
        const v::Value rx = ref(x, w, s);
        const v::Value ry = ref(y, w, s);
        const std::string ctx = "w=" + std::to_string(w) + (s ? " signed " : " unsigned ") +
                                v::toBinary(rx) + ", " + v::toBinary(ry);
        Wide r(kw::chunks(w));
        kw::bitAnd(r.data(), x.data(), y.data(), w);
        ASSERT_TRUE(same(r, v::bitAnd(rx, ry), "and " + ctx));
        kw::bitOr(r.data(), x.data(), y.data(), w);
        ASSERT_TRUE(same(r, v::bitOr(rx, ry), "or " + ctx));
        kw::bitXor(r.data(), x.data(), y.data(), w);
        ASSERT_TRUE(same(r, v::bitXor(rx, ry), "xor " + ctx));
        kw::bitXnor(r.data(), x.data(), y.data(), w);
        ASSERT_TRUE(same(r, v::bitXnor(rx, ry), "xnor " + ctx));
        kw::add(r.data(), x.data(), y.data(), w);
        ASSERT_TRUE(same(r, v::add(rx, ry), "add " + ctx));
        kw::sub(r.data(), x.data(), y.data(), w);
        ASSERT_TRUE(same(r, v::sub(rx, ry), "sub " + ctx));
        kw::mul(r.data(), x.data(), y.data(), w);
        ASSERT_TRUE(same(r, v::mul(rx, ry), "mul " + ctx));
        kw::div(r.data(), x.data(), y.data(), w, s);
        ASSERT_TRUE(same(r, v::div(rx, ry), "div " + ctx));
        kw::mod(r.data(), x.data(), y.data(), w, s);
        ASSERT_TRUE(same(r, v::mod(rx, ry), "mod " + ctx));
        ASSERT_TRUE(same1(kw::lt(x.data(), y.data(), w, s), v::lt(rx, ry), "lt " + ctx));
        ASSERT_TRUE(same1(kw::le(x.data(), y.data(), w, s), v::le(rx, ry), "le " + ctx));
        ASSERT_TRUE(same1(kw::gt(x.data(), y.data(), w, s), v::gt(rx, ry), "gt " + ctx));
        ASSERT_TRUE(same1(kw::ge(x.data(), y.data(), w, s), v::ge(rx, ry), "ge " + ctx));
        ASSERT_TRUE(same1(kw::eq(x.data(), y.data(), w), v::eq(rx, ry), "eq " + ctx));
        ASSERT_TRUE(same1(kw::ne(x.data(), y.data(), w), v::ne(rx, ry), "ne " + ctx));
        ASSERT_TRUE(same1(kw::ceq(x.data(), y.data(), w), v::ceq(rx, ry), "ceq " + ctx));
        ASSERT_TRUE(same1(kw::cne(x.data(), y.data(), w), v::cne(rx, ry), "cne " + ctx));
        ASSERT_TRUE(same1(kw::weq(x.data(), y.data(), w), v::weq(rx, ry), "weq " + ctx));
        ASSERT_TRUE(same1(kw::wne(x.data(), y.data(), w), v::wne(rx, ry), "wne " + ctx));
        for (const bool ignoreX : {false, true})
            ASSERT_TRUE(same1(kw::caseMatch(x.data(), y.data(), w, ignoreX),
                              v::caseMatch(rx, ry, ignoreX), "case match " + ctx));
    }
}

TEST(Wide, Unary) {
    Gen g;
    SCOPED_TRACE(kirimasim::test::fuzzSetting(kirimasim::test::fuzzSeed(0x77696465ULL)));
    for (std::uint64_t n = 0, end = rounds(); n < end; ++n) {
        const unsigned w = g.width();
        const bool s = g.flip();
        const Wide x = g.next(w);
        const v::Value rx = ref(x, w, s);
        const std::string ctx = "w=" + std::to_string(w) + " " + v::toBinary(rx);
        Wide r(kw::chunks(w));
        kw::bitNot(r.data(), x.data(), w);
        ASSERT_TRUE(same(r, v::bitNot(rx), "not " + ctx));
        kw::neg(r.data(), x.data(), w);
        ASSERT_TRUE(same(r, v::neg(rx), "neg " + ctx));
        ASSERT_TRUE(same1(kw::redAnd(x.data(), w), v::redAnd(rx), "&" + ctx));
        ASSERT_TRUE(same1(kw::redOr(x.data(), w), v::redOr(rx), "|" + ctx));
        ASSERT_TRUE(same1(kw::redXor(x.data(), w), v::redXor(rx), "^" + ctx));
        ASSERT_TRUE(same1(kw::truth(x.data(), w), v::truth(rx), "truth " + ctx));
        // Conversions to a random width.
        const unsigned to = g.width();
        Wide c(kw::chunks(to));
        kw::resize(c.data(), to, x.data(), w, s);
        const v::Value want =
            to <= w ? v::trunc(rx, to, s) : (s ? v::sext(rx, to, s) : v::zext(rx, to, s));
        ASSERT_TRUE(same(c, want, "resize to " + std::to_string(to) + " " + ctx));
        kw::to2(r.data(), x.data(), w);
        ASSERT_TRUE(same(r, v::to2(rx), "to2 " + ctx));
    }
}

TEST(Wide, ShiftsSelectsAndPower) {
    Gen g;
    SCOPED_TRACE(kirimasim::test::fuzzSetting(kirimasim::test::fuzzSeed(0x77696465ULL)));
    for (std::uint64_t n = 0, end = rounds(); n < end; ++n) {
        const unsigned w = g.width();
        const bool s = g.flip();
        const Wide x = g.next(w);
        const v::Value rx = ref(x, w, s);
        // A shift amount of 1 to 80 bits, usually small.
        const unsigned aw = 1 + static_cast<unsigned>(g.raw() % 80);
        Wide amount = g.next(aw);
        if (g.raw() % 2 == 0) {
            for (W4& c : amount)
                c = {0, 0};
            amount[0].a = g.raw() % (w + 3);
            if (aw < 64) amount[0].a &= k::mask(aw);
        }
        const v::Value ra = ref(amount, aw, false);
        const std::string ctx =
            "w=" + std::to_string(w) + " " + v::toBinary(rx) + " by " + v::toBinary(ra);
        Wide r(kw::chunks(w));
        kw::shl(r.data(), x.data(), w, amount.data(), aw);
        ASSERT_TRUE(same(r, v::shl(rx, ra), "<< " + ctx));
        kw::shr(r.data(), x.data(), w, amount.data(), aw);
        ASSERT_TRUE(same(r, v::shr(rx, ra), ">> " + ctx));
        kw::ashr(r.data(), x.data(), w, amount.data(), aw, s);
        ASSERT_TRUE(same(r, v::ashr(rx, ra), ">>> " + ctx));
        // A part, partly outside the value.
        const unsigned pw = 1 + static_cast<unsigned>(g.raw() % 100);
        const std::int64_t lsb = static_cast<std::int64_t>(g.raw() % (w + 20)) - 10;
        Wide p(kw::chunks(pw));
        kw::bits(p.data(), pw, x.data(), w, lsb);
        v::Value want = v::filled(v::Logic::X, pw, false);
        for (unsigned i = 0; i < pw; ++i) {
            const std::int64_t at = lsb + i;
            if (at >= 0 && at < w) want.bits[i] = rx.bits[static_cast<std::size_t>(at)];
        }
        ASSERT_TRUE(same(p, want,
                         "part " + std::to_string(lsb) + "+:" + std::to_string(pw) + " of " + ctx));
        // Powers with small exponents.
        const unsigned yw = 1 + static_cast<unsigned>(g.raw() % 8);
        const bool ys = g.flip();
        const Wide y = g.next(yw);
        const v::Value ry = ref(y, yw, ys);
        kw::pow(r.data(), x.data(), w, s, y.data(), yw, ys);
        ASSERT_TRUE(same(r, v::pow(rx, ry), "** " + v::toBinary(ry) + " " + ctx));
    }
}

TEST(Wide, Reals) {
    Wide x(3);
    // 2^130 + 1 rounds to 2^130; -(2^100) comes back.
    x = {W4{1, 0}, W4{0, 0}, W4{4, 0}};
    EXPECT_EQ(kw::toReal(x.data(), 131, false), std::ldexp(1.0, 130));
    Wide r(2);
    kw::fromReal(r.data(), 100, -5.5);
    EXPECT_EQ(kw::toReal(r.data(), 100, true), -6.0);
    kw::fromReal(r.data(), 100, 1e30);  // does not fit in 64 bits: 0 (VALUE_SEMANTICS.md 7.4)
    EXPECT_EQ(r[0].a, 0U);
    EXPECT_EQ(r[1].a, 0U);
}

}  // namespace
