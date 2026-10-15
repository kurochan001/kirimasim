// The SIR text format (SIR_SPEC.md section 9). Printing only; reading the
// text back comes later in P1, when the grammar is fixed.

#pragma once

#include <string>

#include "haremasim/sir/design.h"

namespace haremasim::sir {

/// Prints the design in the SIR text format. The output depends only on
/// the design, so the same SIR always gives the same text.
std::string toText(const Design& design);

/// Prints a constant as a Verilog-style literal: 8'h0f, 4'b1x0z.
std::string constantText(const Constant& c);

}  // namespace haremasim::sir
