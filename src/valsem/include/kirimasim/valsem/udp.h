// Reference semantics of user-defined primitives (VALUE_SEMANTICS.md 5.9):
// the rows of a table matched symbol by symbol, as table 29-1 defines the
// symbols. The kernel's compiled tables (kernel/udp.h) are tested against
// these.

#pragma once

#include <string>
#include <vector>

#include "kirimasim/valsem/logic.h"

namespace kirimasim::valsem {

/// A row of a UDP table as slang normalizes it: one symbol per input, with
/// an edge written "(vw)" or as r, f, p, n or *; the state symbol (0 for a
/// combinational UDP); and the output symbol, '-' for no change.
struct UdpRow {
    std::string inputs;
    char state = 0;
    char output = 'x';
};

/// A combinational UDP: the output of the first row whose symbols match
/// the inputs, x if none does. An input z reads as x (29.2, 29.4).
Logic udpOutput(const std::vector<UdpRow>& rows, const std::vector<Logic>& inputs);

/// A sequential UDP: the next state when input k has changed from `from`
/// to inputs[k], the others at their values. A level row that matches
/// decides; else an edge row on input k that matches; else x (29.5, 29.6,
/// 29.9).
Logic udpNext(const std::vector<UdpRow>& rows, Logic state, const std::vector<Logic>& inputs,
              std::size_t k, Logic from);

/// A sequential UDP whose inputs changed from `prev` to `cur`: each changed
/// input in port order, each seeing the changes before it (VALUE_SEMANTICS.md
/// 5.9).
Logic udpStep(const std::vector<UdpRow>& rows, Logic state, const std::vector<Logic>& prev,
              const std::vector<Logic>& cur);

}  // namespace kirimasim::valsem
