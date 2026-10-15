#include "haremasim/kernel/format.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace haremasim::kernel {

namespace {

/// The (a, b) pair of bit i.
struct Bit {
    bool a = false;
    bool b = false;
};

Bit bitOf(const IntValue& v, std::uint32_t i) {
    const std::size_t word = i / 64;
    const unsigned shift = i % 64;
    if (!v.fourState) return {((v.words[word] >> shift) & 1U) != 0, false};
    return {((v.words[2 * word] >> shift) & 1U) != 0, ((v.words[2 * word + 1] >> shift) & 1U) != 0};
}

bool hasUnknown(const IntValue& v) {
    for (std::uint32_t i = 0; i < v.width; ++i)
        if (bitOf(v, i).b) return true;
    return false;
}

/// The a bits as words, least significant first.
std::vector<std::uint64_t> aWords(const IntValue& v) {
    std::vector<std::uint64_t> w((v.width + 63) / 64, 0);
    for (std::uint32_t i = 0; i < v.width; ++i)
        if (bitOf(v, i).a) w[i / 64] |= std::uint64_t{1} << (i % 64);
    return w;
}

/// Decimal digits of a non-negative number held in words (consumed).
std::string decimal(std::vector<std::uint64_t> n) {
    std::string out;
    const auto isZero = [&] {
        return std::all_of(n.begin(), n.end(), [](std::uint64_t w) { return w == 0; });
    };
    if (isZero()) return "0";
    while (!isZero()) {
        // Divide by 10 one 32-bit half at a time, most significant first.
        std::uint64_t rem = 0;
        for (std::size_t i = n.size(); i-- > 0;) {
            const std::uint64_t hi = (rem << 32) | (n[i] >> 32);
            const std::uint64_t qhi = hi / 10;
            rem = hi % 10;
            const std::uint64_t lo = (rem << 32) | (n[i] & 0xffffffffU);
            const std::uint64_t qlo = lo / 10;
            rem = lo % 10;
            n[i] = (qhi << 32) | qlo;
        }
        out.push_back(static_cast<char>('0' + rem));
    }
    std::reverse(out.begin(), out.end());
    return out;
}

/// 2^n as words.
std::vector<std::uint64_t> powerOfTwo(std::uint32_t n, std::size_t words) {
    std::vector<std::uint64_t> w(std::max<std::size_t>(words, n / 64 + 1), 0);
    w[n / 64] = std::uint64_t{1} << (n % 64);
    return w;
}

/// The automatic width of %d: the digits of the largest value of the
/// width, with the sign for signed values (3.3).
std::size_t autoDecimalWidth(std::uint32_t width, bool isSigned) {
    if (isSigned) return 1 + decimal(powerOfTwo(width - 1, 1)).size();
    // 2^w - 1 has as many digits as 2^w unless 2^w is a power of ten,
    // which it never is for w >= 1.
    return decimal(powerOfTwo(width, 1)).size();
}

std::string padLeft(std::string s, std::size_t width, char fill) {
    if (s.size() < width) s.insert(0, width - s.size(), fill);
    return s;
}

std::string stripLeadingZeros(std::string_view s) {
    const std::size_t first = s.find_first_not_of('0');
    if (first == std::string_view::npos) return "0";
    return std::string(s.substr(first));
}

/// A digit of %b, %o or %h for `n` bits from bit `lsb` (3.4).
char groupDigit(const IntValue& v, std::uint32_t lsb, std::uint32_t n) {
    unsigned value = 0;
    bool anyX = false;
    bool anyZ = false;
    bool allX = true;
    bool allZ = true;
    for (std::uint32_t k = 0; k < n; ++k) {
        const Bit b = bitOf(v, lsb + k);
        const bool x = b.a && b.b;
        const bool z = !b.a && b.b;
        anyX = anyX || x;
        anyZ = anyZ || z;
        allX = allX && x;
        allZ = allZ && z;
        if (b.a) value |= 1U << k;
    }
    if (allX) return 'x';
    if (allZ) return 'z';
    if (anyX) return 'X';
    if (anyZ) return 'Z';
    return "0123456789abcdef"[value];
}

std::string radixDigits(const IntValue& v, std::uint32_t bitsPerDigit) {
    std::string out;
    for (std::uint32_t lsb = 0; lsb < v.width; lsb += bitsPerDigit)
        out.push_back(groupDigit(v, lsb, std::min(bitsPerDigit, v.width - lsb)));
    std::reverse(out.begin(), out.end());
    return out;
}

std::string formatDecimal(const IntValue& v, int width) {
    std::string digits;
    if (hasUnknown(v)) {
        bool allX = true;
        bool allZ = true;
        bool anyX = false;
        for (std::uint32_t i = 0; i < v.width; ++i) {
            const Bit b = bitOf(v, i);
            allX = allX && b.a && b.b;
            allZ = allZ && !b.a && b.b;
            anyX = anyX || (b.a && b.b);
        }
        digits = allX ? "x" : allZ ? "z" : anyX ? "X" : "Z";
    } else {
        std::vector<std::uint64_t> n = aWords(v);
        const bool negative = v.isSigned && bitOf(v, v.width - 1).a;
        if (negative) {
            // Two's complement within the width: invert and add 1.
            for (auto& w : n)
                w = ~w;
            for (auto& w : n)
                if (++w != 0) break;
            const unsigned top = v.width % 64;
            if (top != 0) n.back() &= (std::uint64_t{1} << top) - 1;
        }
        digits = (negative ? "-" : "") + decimal(std::move(n));
    }
    const std::size_t w =
        width < 0 ? autoDecimalWidth(v.width, v.isSigned) : static_cast<std::size_t>(width);
    return padLeft(std::move(digits), w, ' ');
}

std::string formatChars(const IntValue& v, int width) {
    // Bytes from the most significant end; leading zero bytes are not
    // printed (3.5).
    std::string bytes;
    const std::uint32_t n = (v.width + 7) / 8;
    for (std::uint32_t k = n; k-- > 0;) {
        unsigned c = 0;
        for (std::uint32_t i = 0; i < 8 && k * 8 + i < v.width; ++i)
            if (bitOf(v, k * 8 + i).a) c |= 1U << i;
        if (c == 0 && bytes.empty()) continue;
        bytes.push_back(static_cast<char>(c));
    }
    const std::size_t w = width < 0 ? n : static_cast<std::size_t>(width);
    return padLeft(std::move(bytes), w, ' ');
}

}  // namespace

bool parseFormat(std::string_view format, std::vector<FormatPiece>& pieces, std::string& error) {
    std::string literal;
    const auto flush = [&] {
        if (literal.empty()) return;
        FormatPiece p;
        p.text = std::move(literal);
        pieces.push_back(std::move(p));
        literal.clear();
    };
    for (std::size_t i = 0; i < format.size(); ++i) {
        const char c = format[i];
        if (c != '%') {
            literal.push_back(c);
            continue;
        }
        if (++i >= format.size()) {
            error = "the format ends with a lone %";
            return false;
        }
        if (format[i] == '%') {
            literal.push_back('%');
            continue;
        }
        int width = -1;
        if (format[i] >= '0' && format[i] <= '9') {
            width = 0;
            while (i < format.size() && format[i] >= '0' && format[i] <= '9') {
                width = width * 10 + (format[i] - '0');
                if (width > 100000) {
                    error = "the format width is too large";
                    return false;
                }
                ++i;
            }
        }
        if (i >= format.size()) {
            error = "the format ends inside a specifier";
            return false;
        }
        char conv = format[i];
        if (conv >= 'A' && conv <= 'Z') conv = static_cast<char>(conv - 'A' + 'a');
        if (conv == 'x') conv = 'h';
        switch (conv) {
            case 'b':
            case 'o':
            case 'h':
            case 'd':
            case 'c':
            case 's':
            case 't': break;
            default:
                error = std::string("format specifier %") + format[i] + " is not supported yet";
                return false;
        }
        flush();
        FormatPiece p;
        p.isSpec = true;
        p.spec = {conv, width};
        pieces.push_back(std::move(p));
    }
    flush();
    return true;
}

std::string formatInt(const IntValue& v, FormatSpec spec) {
    switch (spec.conv) {
        case 'd': return formatDecimal(v, spec.width);
        case 'c': {
            unsigned c = 0;
            for (std::uint32_t i = 0; i < 8 && i < v.width; ++i)
                if (bitOf(v, i).a) c |= 1U << i;
            return padLeft(std::string(1, static_cast<char>(c)),
                           spec.width < 0 ? 1 : static_cast<std::size_t>(spec.width), ' ');
        }
        case 's': return formatChars(v, spec.width);
        default: break;
    }
    const std::uint32_t bits = spec.conv == 'b' ? 1 : spec.conv == 'o' ? 3 : 4;
    std::string digits = radixDigits(v, bits);
    if (spec.width < 0) return digits;  // the automatic width keeps leading zeros
    return padLeft(stripLeadingZeros(digits), static_cast<std::size_t>(spec.width), '0');
}

std::string formatTime(const IntValue& ticks, int precision, const TimeFormat& tf, int width) {
    const std::size_t w = static_cast<std::size_t>(width >= 0 ? width : std::max(tf.minWidth, 0));
    if (hasUnknown(ticks)) return padLeft(formatDecimal(ticks, 0) + tf.suffix, w, ' ');
    const std::uint64_t t = ticks.words[0];  // a time fits in 64 bits
    // The value in units of tf.unit, with tf.decimals digits after the
    // point, as a whole number: t * 10^k.
    const int k = precision - tf.unit + tf.decimals;
    std::string digits;
    if (k >= 0) {
        digits = std::to_string(t);
        if (t != 0) digits.append(static_cast<std::size_t>(k), '0');
    } else if (k < -19) {
        digits = "0";  // t < 2^64 < 0.5 * 10^20
    } else {
        std::uint64_t div = 1;
        for (int i = 0; i < -k; ++i)
            div *= 10;
        const std::uint64_t q = t / div;
        const std::uint64_t r = t % div;
        digits = std::to_string(q + (r >= div - r ? 1 : 0));  // half away from zero
    }
    if (tf.decimals > 0) {
        const auto d = static_cast<std::size_t>(tf.decimals);
        digits = padLeft(std::move(digits), d + 1, '0');
        digits.insert(digits.size() - d, ".");
    }
    return padLeft(digits + tf.suffix, w, ' ');
}

}  // namespace haremasim::kernel
