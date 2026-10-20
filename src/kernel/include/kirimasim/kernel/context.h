// The execution context: the single owner of all simulation state.
//
// Nothing in the kernel is global. Several Contexts can exist in one process.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdio>

#include "kirimasim/kernel/counters.h"
#include "kirimasim/kernel/sim.h"
#include "kirimasim/kernel/store.h"
#include "kirimasim/kernel/strings.h"

namespace kirimasim::kernel {

class Context {
public:
    /// A Context that writes to standard output and standard error.
    Context() : Context(OutputDestinations{stdout, stderr}) {}
    /// A Context that writes where it is told (RUNTIME_ABI.md 9.3.10).
    explicit Context(OutputDestinations to) : output_(to), scheduler_(*this, store_, counters_) {}
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Counters& counters() { return counters_; }
    [[nodiscard]] const Counters& counters() const { return counters_; }
    Scheduler& scheduler() { return scheduler_; }
    [[nodiscard]] const Scheduler& scheduler() const { return scheduler_; }
    Output& output() { return output_; }
    Store& store() { return store_; }
    Strings& strings() { return strings_; }
    [[nodiscard]] const Strings& strings() const { return strings_; }
    [[nodiscard]] const Store& store() const { return store_; }

private:
    Counters counters_;
    Output output_;
    Store store_;
    Strings strings_;
    Scheduler scheduler_;
};

/// The C handle of a context (abi.h), and back.
inline kirima_context* toC(Context& c) { return reinterpret_cast<kirima_context*>(&c); }
inline Context& fromC(kirima_context* c) { return *reinterpret_cast<Context*>(c); }

}  // namespace kirimasim::kernel
