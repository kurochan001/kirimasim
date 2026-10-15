// The execution context: the single owner of all simulation state.
//
// Nothing in the kernel is global. Several Contexts can exist in one process.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include "haremasim/kernel/counters.h"
#include "haremasim/kernel/sim.h"
#include "haremasim/kernel/store.h"

namespace haremasim::kernel {

class Context {
public:
    Context() : scheduler_(*this, store_, counters_) {}
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Counters& counters() { return counters_; }
    [[nodiscard]] const Counters& counters() const { return counters_; }
    Scheduler& scheduler() { return scheduler_; }
    [[nodiscard]] const Scheduler& scheduler() const { return scheduler_; }
    Output& output() { return output_; }
    Store& store() { return store_; }
    [[nodiscard]] const Store& store() const { return store_; }

private:
    Counters counters_;
    Output output_;
    Store store_;
    Scheduler scheduler_;
};

/// The C handle of a context (abi.h), and back.
inline hs_context* toC(Context& c) { return reinterpret_cast<hs_context*>(&c); }
inline Context& fromC(hs_context* c) { return *reinterpret_cast<Context*>(c); }

}  // namespace haremasim::kernel
