// Bit ranges inside arrays of 64-bit words.
//
// A value of w bits is held in ceil(w / 64) words, least significant word
// first. A four-state value interleaves its halves: a0, b0, a1, b1, ...
// (SIR_SPEC.md 3.2), so the a words have a stride of 2 starting at index 0
// and the b words a stride of 2 starting at index 1.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdint>

#include "kirimasim/kernel/w4ops.h"

namespace kirimasim::kernel {

/// Words needed for w bits.
inline constexpr std::uint32_t wordsFor(std::uint32_t w) { return (w + 63) / 64; }

/// Bit i of a word array with the given stride.
inline bool bitAt(const std::uint64_t* words, unsigned stride, std::uint64_t i) {
    return ((words[(i / 64) * stride] >> (i % 64)) & 1U) != 0;
}

/// Reads `n` (1 to 64) bits starting at bit `lsb`.
inline std::uint64_t readField(const std::uint64_t* words, unsigned stride, std::uint64_t lsb,
                               unsigned n) {
    const std::uint64_t w = lsb / 64;
    const unsigned s = static_cast<unsigned>(lsb % 64);
    std::uint64_t v = words[w * stride] >> s;
    if (s != 0 && s + n > 64) v |= words[(w + 1) * stride] << (64 - s);
    return v & mask(n);
}

/// Writes the low `n` (1 to 64) bits of v starting at bit `lsb`.
inline void writeField(std::uint64_t* words, unsigned stride, std::uint64_t lsb, unsigned n,
                       std::uint64_t v) {
    const std::uint64_t w = lsb / 64;
    const unsigned s = static_cast<unsigned>(lsb % 64);
    const std::uint64_t m = mask(n);
    v &= m;
    std::uint64_t& lo = words[w * stride];
    lo = (lo & ~(m << s)) | (v << s);
    if (s != 0 && s + n > 64) {
        std::uint64_t& hi = words[(w + 1) * stride];
        const unsigned done = 64 - s;
        hi = (hi & ~(m >> done)) | (v >> done);
    }
}

/// Copies `width` bits from bit `slsb` of src to bit `dlsb` of dst.
inline void copyBits(std::uint64_t* dst, unsigned dstride, std::uint64_t dlsb,
                     const std::uint64_t* src, unsigned sstride, std::uint64_t slsb,
                     std::uint64_t width) {
    while (width != 0) {
        const unsigned n = width < 64 ? static_cast<unsigned>(width) : 64U;
        writeField(dst, dstride, dlsb, n, readField(src, sstride, slsb, n));
        dlsb += n;
        slsb += n;
        width -= n;
    }
}

/// Whether `width` bits from bit `xl` of x equal those from bit `yl` of y.
inline bool equalBits(const std::uint64_t* x, unsigned xs, std::uint64_t xl, const std::uint64_t* y,
                      unsigned ys, std::uint64_t yl, std::uint64_t width) {
    while (width != 0) {
        const unsigned n = width < 64 ? static_cast<unsigned>(width) : 64U;
        if (readField(x, xs, xl, n) != readField(y, ys, yl, n)) return false;
        xl += n;
        yl += n;
        width -= n;
    }
    return true;
}

}  // namespace kirimasim::kernel
