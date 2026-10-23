// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Formatted reading for $sscanf and $fscanf (SYSTEM_TASKS.md 6.3, 21.3.4.3).
//
// Every tier reads through these functions, so what a format matches does
// not depend on the tier. The caller turns each value into its target's
// type (SIR_SPEC.md 8).
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "kirimasim/kernel/w4.h"

namespace kirimasim::kernel {

/// One value that a conversion read and assigns.
struct ScanValue {
    /// b, o, h, d, c, u or z for an integral value; s for a string; f for a real
    /// (also %e and %g).
    char conv = 'd';
    W4 bits{0, 0};  ///< b, o, h, d, c: the value's 64 low bits.
    /// b, o, h, d, u, z: every bit read, least significant chunk first; for d the
    /// magnitude, with `negative` for a minus sign. bits is its low chunk
    /// (negated for d with a minus sign).
    std::vector<W4> wide;
    bool negative = false;
    bool fill = false;  ///< d: a lone x or z (in bits), which fills every bit.
    double real = 0;    ///< f
    std::string text;   ///< s
};

/// One conversion of a format: its letter (as in ScanValue::conv, %x as h)
/// and whether it assigns (no *).
struct ScanConversion {
    char conv = 'd';
    bool assigns = true;
};

/// The conversions of a format, in order. Returns false, with the offending
/// specifier in `bad`, for one that kirimasim does not know or support yet.
bool parseScanFormat(std::string_view format, std::vector<ScanConversion>& convs, std::string& bad);

/// The type of an output argument, independent of any tier's type table.
enum class ScanTarget { Integral, String, Real, Other };

struct ScanPlan {
    std::string format;
    std::string error;
    bool missingTargets = false;
};

/// Checks a run-time format against its output types. A missing target
/// cuts the format before that conversion: warn, and do not read it
/// (SYSTEM_TASKS.md 6.3). Extra targets are ignored.
ScanPlan prepareScanFormat(std::string_view format, std::span<const ScanTarget> targets);

struct PlusargPlan {
    std::string prefix;
    char conv = 'd';  ///< Normalized as ScanValue::conv.
    std::string error;
};

/// Splits a $value$plusargs format into its prefix and one conversion,
/// checking the output type (SYSTEM_TASKS.md 9, SIR_SPEC.md 8).
PlusargPlan preparePlusargFormat(std::string_view format, ScanTarget target);

/// Reads `input` by `format` (a format parseScanFormat accepts). Returns
/// the number of values assigned, which are put in `out` in order, or -1
/// when the input ends before the first conversion. NUL characters count
/// as white space except inside binary fields. widths names the bit width
/// of each assigning target; an absent width defaults to 32 bits. Explicit
/// binary field widths override it (SYSTEM_TASKS.md 6.3).
int scanString(std::string_view input, std::string_view format, std::vector<ScanValue>& out,
               std::span<const std::uint32_t> widths = {});

/// Where scanInput takes its characters from: get() gives the next one,
/// or -1 at the end of the input.
class ScanInput {
public:
    ScanInput() = default;
    ScanInput(const ScanInput&) = delete;
    ScanInput& operator=(const ScanInput&) = delete;
    virtual ~ScanInput() = default;
    virtual int get() = 0;
};

/// Reads as scanString does, from `input` ($fscanf; SYSTEM_TASKS.md 6.3),
/// where NUL is not white space. The characters it took from `input` to
/// look at but did not use are put in `unread`, in order, for the caller
/// to give back.
int scanInput(ScanInput& input, std::string_view format, std::vector<ScanValue>& out,
              std::string& unread, std::span<const std::uint32_t> widths = {});

/// The value of the rest of a plusarg after its name, read by the one
/// conversion of $value$plusargs (SYSTEM_TASKS.md 9): an empty rest gives 0
/// or the empty string; a rest the conversion cannot read whole gives x
/// (fill).
ScanValue plusargValue(std::string_view rest, char conv);

}  // namespace kirimasim::kernel
