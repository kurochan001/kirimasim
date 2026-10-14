// The execution context: the single owner of all simulation state.
//
// Nothing in the kernel is global. Several Contexts can exist in one process.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include "haremasim/kernel/counters.h"

namespace haremasim::kernel {

class Context {
public:
    Counters& counters() { return counters_; }
    [[nodiscard]] const Counters& counters() const { return counters_; }

private:
    Counters counters_;
};

}  // namespace haremasim::kernel
