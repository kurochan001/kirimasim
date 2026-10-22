// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Formatted reading for $sscanf and $fscanf (SYSTEM_TASKS.md 6.3).

#include "kirimasim/kernel/scan.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace kirimasim::kernel {

namespace {

/// White space of a format and of the input (21.3.4.3).
bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/// The characters a scan reads: a string, or what a ScanInput gives, taken
/// one at a time as the scan looks at them, so that a file loses only what
/// was looked at.
class Chars {
public:
    explicit Chars(std::string_view text) : text_(text) {}
    explicit Chars(ScanInput& in) : in_(&in) {}

    /// Whether there is a character at i, reading up to it from a ScanInput.
    bool has(std::size_t i) {
        while (in_ != nullptr && !end_ && taken_.size() <= i) {
            const int c = in_->get();
            if (c < 0)
                end_ = true;
            else
                taken_.push_back(static_cast<char>(c));
        }
        return i < (in_ != nullptr ? taken_.size() : text_.size());
    }
    [[nodiscard]] char at(std::size_t i) const { return in_ != nullptr ? taken_[i] : text_[i]; }
    /// White space of the input: NUL too for a string ($sscanf; 21.3.4.3).
    [[nodiscard]] bool space(std::size_t i) const {
        return isSpace(at(i)) || (in_ == nullptr && at(i) == '\0');
    }
    /// What was taken from a ScanInput from i on.
    [[nodiscard]] std::string from(std::size_t i) const {
        return i < taken_.size() ? taken_.substr(i) : std::string();
    }

private:
    std::string_view text_;
    ScanInput* in_ = nullptr;
    std::string taken_;
    bool end_ = false;
};

bool isDigit(char c) { return c >= '0' && c <= '9'; }

/// One specifier: %[*][width]conv. width 0 is none.
struct Spec {
    bool assigns = true;
    std::size_t width = 0;
    char conv = 'd';
};

/// Parses the specifier that starts at f[i] (the %); leaves i on its last
/// character. conv is '%' for %%.
bool parseSpec(std::string_view f, std::size_t& i, Spec& s, std::string& bad) {
    ++i;
    if (i < f.size() && f[i] == '*') {
        s.assigns = false;
        ++i;
    }
    while (i < f.size() && isDigit(f[i]))
        s.width = s.width * 10 + static_cast<std::size_t>(f[i++] - '0');
    if (i >= f.size()) {
        bad = "%";
        return false;
    }
    char c = f[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (c == 'x') c = 'h';
    if (c == 'e' || c == 'g') c = 'f';
    switch (c) {
        case '%':
        case 'b':
        case 'o':
        case 'h':
        case 'd':
        case 'c':
        case 's':
        case 'f': s.conv = c; return true;
        default: bad = std::string("%") + f[i]; return false;
    }
}

/// The value of a digit of base 2, 8 or 16 with x and z (one bit per bit
/// of the digit); false for a character that is not one.
bool radixDigit(char c, unsigned bits, W4& d) {
    const std::uint64_t all = (std::uint64_t{1} << bits) - 1;
    if (c == 'x' || c == 'X') {
        d = {all, all};
        return true;
    }
    if (c == 'z' || c == 'Z' || c == '?') {
        d = {0, all};
        return true;
    }
    unsigned v = 0;
    if (isDigit(c))
        v = static_cast<unsigned>(c - '0');
    else if (c >= 'a' && c <= 'f')
        v = static_cast<unsigned>(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F')
        v = static_cast<unsigned>(c - 'A' + 10);
    else
        return false;
    if (v >= (1U << bits)) return false;
    d = {v, 0};
    return true;
}

/// Reads one field of a conversion at input[in], at most `limit`
/// characters (any number with no width). Returns false on a matching
/// failure.
bool readField(Chars& input, std::size_t& in, const Spec& s, ScanValue& v) {
    const std::size_t limit = s.width == 0 ? SIZE_MAX : s.width;
    const std::size_t start = in;
    const auto more = [&] { return in - start < limit && input.has(in); };
    v.conv = s.conv;
    switch (s.conv) {
        case 'c': v.bits = {static_cast<unsigned char>(input.at(in++)), 0}; return true;
        case 's':
            while (more() && !input.space(in))
                v.text.push_back(input.at(in++));
            return !v.text.empty();
        case 'd': {
            // An optional sign and decimal digits with _, or one x, z or ?.
            if (more() && std::string_view("xXzZ?").find(input.at(in)) != std::string_view::npos) {
                const bool x = input.at(in) == 'x' || input.at(in) == 'X';
                v.bits = {x ? ~std::uint64_t{0} : 0, ~std::uint64_t{0}};
                v.fill = true;
                ++in;
                return true;
            }
            bool negative = false;
            if (more() && (input.at(in) == '+' || input.at(in) == '-'))
                negative = input.at(in++) == '-';
            bool digits = false;
            std::vector<W4> n{{0, 0}};
            while (more() && (isDigit(input.at(in)) || input.at(in) == '_')) {
                if (input.at(in) != '_') {
                    // n = n * 10 + digit, chunk by chunk, in 32-bit halves.
                    auto carry = static_cast<std::uint64_t>(input.at(in) - '0');
                    for (W4& c : n) {
                        const std::uint64_t lo = (c.a & 0xffffffffU) * 10 + carry;
                        const std::uint64_t hi = (c.a >> 32) * 10 + (lo >> 32);
                        c.a = (hi << 32) | (lo & 0xffffffffU);
                        carry = hi >> 32;
                    }
                    if (carry != 0) n.push_back({carry, 0});
                    digits = true;
                }
                ++in;
            }
            v.bits = {negative ? ~n[0].a + 1 : n[0].a, 0};
            v.wide = std::move(n);
            v.negative = negative;
            return digits;
        }
        case 'f': {
            // C's form: a sign, digits with an optional point, an optional
            // exponent with its own digits.
            std::string text;
            if (more() && (input.at(in) == '+' || input.at(in) == '-'))
                text.push_back(input.at(in++));
            bool digits = false;
            while (more() && isDigit(input.at(in))) {
                text.push_back(input.at(in++));
                digits = true;
            }
            if (more() && input.at(in) == '.') {
                text.push_back(input.at(in++));
                while (more() && isDigit(input.at(in))) {
                    text.push_back(input.at(in++));
                    digits = true;
                }
            }
            if (!digits) return false;
            if (more() && (input.at(in) == 'e' || input.at(in) == 'E')) {
                std::size_t k = in + 1;
                std::string exp(1, input.at(in));
                if (k - start < limit && input.has(k) && (input.at(k) == '+' || input.at(k) == '-'))
                    exp.push_back(input.at(k++));
                const std::size_t first = k;
                while (k - start < limit && input.has(k) && isDigit(input.at(k)))
                    exp.push_back(input.at(k++));
                if (k > first) {
                    text += exp;
                    in = k;
                }
            }
            v.real = std::strtod(text.c_str(), nullptr);
            return true;
        }
        default: {
            // b, o, h: digits of the base and x, z, ? and _.
            const unsigned bits = s.conv == 'b' ? 1 : s.conv == 'o' ? 3 : 4;
            bool digits = false;
            std::vector<W4> acc{{0, 0}};
            while (more()) {
                W4 d{0, 0};
                if (input.at(in) == '_') {
                    ++in;
                    continue;
                }
                if (!radixDigit(input.at(in), bits, d)) break;
                // acc = acc << bits | d, chunk by chunk.
                const W4 top = acc.back();
                for (std::size_t k = acc.size(); k-- > 0;) {
                    const W4 below = k > 0 ? acc[k - 1] : d;
                    acc[k] = {(acc[k].a << bits) | (k > 0 ? below.a >> (64 - bits) : d.a),
                              (acc[k].b << bits) | (k > 0 ? below.b >> (64 - bits) : d.b)};
                }
                const W4 out{top.a >> (64 - bits), top.b >> (64 - bits)};
                if (out.a != 0 || out.b != 0) acc.push_back(out);
                digits = true;
                ++in;
            }
            v.bits = acc[0];
            v.wide = std::move(acc);
            return digits;
        }
    }
}

}  // namespace

bool parseScanFormat(std::string_view format, std::vector<ScanConversion>& convs,
                     std::string& bad) {
    for (std::size_t i = 0; i < format.size(); ++i) {
        if (format[i] != '%') continue;
        Spec s;
        if (!parseSpec(format, i, s, bad)) return false;
        if (s.conv != '%') convs.push_back({s.conv, s.assigns});
    }
    return true;
}

ScanPlan prepareScanFormat(std::string_view format, std::span<const ScanTarget> targets) {
    ScanPlan plan;
    plan.format = format;
    std::size_t target = 0;
    for (std::size_t i = 0; i < format.size(); ++i) {
        if (format[i] != '%') continue;
        const std::size_t start = i;
        Spec s;
        std::string bad;
        if (!parseSpec(format, i, s, bad)) {
            plan.error = "format specifier " + bad;
            return plan;
        }
        if (s.conv == '%' || !s.assigns) continue;
        if (target == targets.size()) {
            plan.missingTargets = true;
            plan.format.resize(start);
            return plan;
        }
        const ScanTarget type = targets[target++];
        const bool valid = s.conv == 'f' ? type == ScanTarget::Real
                           : s.conv == 's'
                               ? type == ScanTarget::Integral || type == ScanTarget::String
                               : type == ScanTarget::Integral;
        if (!valid) {
            plan.error = std::string("%") + s.conv + " into a target of this type";
            return plan;
        }
    }
    return plan;
}

namespace {

/// scanString and scanInput: the number of values assigned, or -1 when the
/// input ends before the first conversion (21.3.4.3); `in` ends at the
/// first character not used.
int scan(Chars& input, std::size_t& in, std::string_view format, std::vector<ScanValue>& out) {
    int count = 0;
    bool converted = false;
    const auto skipSpace = [&] {
        while (input.has(in) && input.space(in))
            ++in;
    };
    for (std::size_t i = 0; i < format.size(); ++i) {
        const char c = format[i];
        if (isSpace(c) || c == '\0') {
            skipSpace();
            continue;
        }
        if (c != '%') {
            if (!input.has(in)) return converted ? count : -1;
            if (input.at(in) != c) return count;
            ++in;
            continue;
        }
        Spec s;
        std::string bad;
        if (!parseSpec(format, i, s, bad)) return count;
        if (s.conv != 'c') skipSpace();
        if (!input.has(in)) return converted ? count : -1;
        if (s.conv == '%') {
            if (input.at(in) != '%') return count;
            ++in;
            continue;
        }
        ScanValue v;
        if (!readField(input, in, s, v)) return count;
        converted = true;
        if (s.assigns) {
            out.push_back(std::move(v));
            ++count;
        }
    }
    return count;
}

}  // namespace

int scanString(std::string_view input, std::string_view format, std::vector<ScanValue>& out) {
    Chars chars(input);
    std::size_t in = 0;
    return scan(chars, in, format, out);
}

int scanInput(ScanInput& input, std::string_view format, std::vector<ScanValue>& out,
              std::string& unread) {
    Chars chars(input);
    std::size_t in = 0;
    const int n = scan(chars, in, format, out);
    unread = chars.from(in);
    return n;
}

ScanValue plusargValue(std::string_view rest, char conv) {
    ScanValue v;
    v.conv = conv;
    if (conv == 's') {
        v.text = std::string(rest);
        return v;
    }
    if (rest.empty()) return v;  // 0, 0.0
    Spec s;
    s.conv = conv == 'c' ? 'd' : conv;
    std::size_t in = 0;
    Chars chars(rest);
    if (readField(chars, in, s, v) && in == rest.size()) return v;
    v = ScanValue{};
    v.conv = conv;
    v.bits = {~std::uint64_t{0}, ~std::uint64_t{0}};
    v.fill = true;
    return v;
}

}  // namespace kirimasim::kernel
