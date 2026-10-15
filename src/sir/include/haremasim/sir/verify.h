// The SIR verifier (SIR_SPEC.md section 10).
//
// All the rules: 1 (types), 2 (slot ranges and table references), 3
// (control-flow shape, block arguments, reachability), 4 (SSA, and no temp
// used after a wait), 5 (where waits may appear), 6 (write sets and write
// restrictions), 7 (automatic variables; mostly kept by the form of SIR,
// since NBAs and event controls take only slots), 8 (unused high bits of
// constants), 9 (source locations), 10 (type, frame, subroutine and process
// tables, and the time precision) and 11 (one index per array dimension).

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
