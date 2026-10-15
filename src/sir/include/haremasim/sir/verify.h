// The SIR verifier (SIR_SPEC.md section 10).
//
// Implemented so far: 1 (types), 2 (slot ranges), 3 (control-flow shape and
// reachability), 4 (SSA), 5 (where waits may appear), 8 (unused high bits
// of constants) and 9 (source locations). Rules 6 (write restrictions) and
// 7 (automatic variables) need the write sets and frames that come with
// the front end.

#pragma once

#include <string>
#include <vector>

#include "haremasim/sir/design.h"

namespace haremasim::sir {

/// One violation found by the verifier.
struct VerifyError {
    std::string where;    ///< Process and block, or table and row.
    std::string message;  ///< What is wrong.
    SourceLoc loc;        ///< Source location, when there is one.
};

/// Checks the design and returns every violation found (empty if none).
std::vector<VerifyError> verify(const Design& design);

}  // namespace haremasim::sir
