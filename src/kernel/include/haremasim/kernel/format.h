// Formatting of values for $display and its relatives (SYSTEM_TASKS.md 3).
//
// Every tier formats through these functions, so the output does not
// depend on the tier. A format string is parsed once, when a design is
// loaded, into pieces; running a display only walks the pieces.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace haremasim::kernel {

/// An integral value to format, laid out like a slot element (store.h):
/// the a and b words interleaved when four-state, a words only otherwise.
struct IntValue {
    const std::uint64_t* words = nullptr;
    std::uint32_t width = 1;
    bool fourState = false;
    bool isSigned = false;
};

/// One format specifier: %<width><conv>, conv in lower case. width is -1
/// when none is written (the automatic width, 3.3) and 0 for %0d and the
/// like.
struct FormatSpec {
    char conv = 'd';
    int width = -1;
};

/// A piece of a parsed format: literal text, or a specifier that takes
/// one argument.
struct FormatPiece {
    bool isSpec = false;
    std::string text;  ///< Literal text (with %% already turned into %).
    FormatSpec spec;
};

/// Splits a format string into pieces. Returns false, with the reason in
/// `error`, for a specifier haremasim does not know or support yet.
bool parseFormat(std::string_view format, std::vector<FormatPiece>& pieces, std::string& error);

/// Formats an integral value with %b, %o, %h, %d, %c or %s (3.2 to 3.5).
std::string formatInt(const IntValue& v, FormatSpec spec);

/// The settings of $timeformat (5.4): the unit as a power of ten of a
/// second, digits after the decimal point, the suffix and the minimum
/// width.
struct TimeFormat {
    int unit = -9;
    int decimals = 0;
    std::string suffix;
    int minWidth = 20;
};

/// Formats a time given in ticks of the global precision `precision` with
/// %t (3.7). `width` overrides the minimum width when it is not -1.
std::string formatTime(const IntValue& ticks, int precision, const TimeFormat& tf, int width);

}  // namespace haremasim::kernel
