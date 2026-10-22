// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Formatting of values for $display and its relatives (SYSTEM_TASKS.md 3).
//
// Every tier formats through these functions, so the output does not
// depend on the tier. Constant formats are parsed once when a design is
// loaded; runtime string-format calls parse the value at each call.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kirimasim::kernel {

/// An integral value to format, laid out like a slot element (store.h):
/// the a and b words interleaved when four-state, a words only otherwise.
struct IntValue {
    const std::uint64_t* words = nullptr;
    std::uint32_t width = 1;
    bool fourState = false;
    bool isSigned = false;
};

/// One format specifier: %<flags><width><conv>, conv in lower case. width
/// is -1 when none is written (the automatic width, 3.3) and 0 for %0d and
/// the like.
struct FormatSpec {
    char conv = 'd';
    int width = -1;
    /// %e, %f and %g: the whole specifier in the form of C's printf, with
    /// its flags, width and precision (3.6). Empty for the others.
    std::string cformat;
    /// The flags of the other formats, which the standard does not define
    /// (3.12): - puts the value at the left of its field, and 0 (a written
    /// width that starts with 0) pads %d with 0.
    bool left = false;
    bool zero = false;
};

/// A piece of a parsed format: literal text, or a specifier. %m and %l
/// take no argument; the others take one.
struct FormatPiece {
    bool isSpec = false;
    std::string text;  ///< Literal text (with %% already turned into %).
    FormatSpec spec;
};

/// Splits a format string into pieces. Returns false, with the reason in
/// `error`, for a specifier kirimasim does not know or support yet.
bool parseFormat(std::string_view format, std::vector<FormatPiece>& pieces, std::string& error);

/// Formats an integral value with %b, %o, %h, %d, %c or %s (3.2 to 3.5).
std::string formatInt(const IntValue& v, const FormatSpec& spec);

/// Formats a real with %e, %f or %g as C does; infinities and NaN are
/// "inf", "-inf" and "nan" whatever the sign of NaN (3.6). An empty
/// cformat is plain %g, the form of a real without a specifier (3.6).
std::string formatReal(double v, const FormatSpec& spec);

/// The settings of $timeformat (5.4): the unit as a power of ten of a
/// second, digits after the decimal point, the suffix and the minimum
/// width.
struct TimeFormat {
    int unit = -9;
    int decimals = 0;
    std::string suffix;
    int minWidth = 20;
};

/// Formats a value of the string type with %s: its characters, in the
/// written width if there is one (3.5, 3.12).
std::string formatString(std::string_view text, const FormatSpec& spec);

/// Formats a time given in ticks of the global precision `precision` with
/// %t (3.7). `width` overrides the minimum width when it is not -1; `left`
/// is the - flag (3.12).
std::string formatTime(const IntValue& ticks, int precision, const TimeFormat& tf, int width,
                       bool left = false);

/// Formats a real time given in ticks of the global precision with %t
/// (3.7): rounded once to tf.decimals digits of tf.unit, half away from
/// zero, on the shortest decimal text that reads back as the value (the
/// rounding of real delays, SCHEDULER_SPEC.md 1).
std::string formatTimeReal(double ticks, int precision, const TimeFormat& tf, int width,
                           bool left = false);

/// A borrowed argument of a runtime string-format call. Integral storage
/// has the same layout as IntValue; all views live through the call.
struct FormatValue {
    enum class Kind { Integral, Real, String, Empty };
    Kind kind = Kind::Integral;
    IntValue integral;
    double real = 0;
    std::string_view text;
};

struct StringFormatResult {
    std::string text;
    std::string error;
    bool argumentMismatch = false;
};

/// $sformat/$sformatf (SYSTEM_TASKS.md 3.13): only format is interpreted;
/// missing values print nothing, extra values are ignored, and either
/// sets argumentMismatch for the caller's warning. tickScale converts the
/// caller's time unit to global-precision ticks for %t.
StringFormatResult formatStringCall(std::string_view format, std::span<const FormatValue> values,
                                    std::string_view scope, std::string_view cell,
                                    std::uint64_t tickScale, int precision, const TimeFormat& tf);

}  // namespace kirimasim::kernel
