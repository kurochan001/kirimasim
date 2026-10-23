// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The execution context: the single owner of all simulation state.
//
// Nothing in the kernel is global. Several Contexts can exist in one process.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdint>
#include <cstdio>

#include "kirimasim/kernel/counters.h"
#include "kirimasim/kernel/dump.h"
#include "kirimasim/kernel/files.h"
#include "kirimasim/kernel/heap.h"
#include "kirimasim/kernel/sim.h"
#include "kirimasim/kernel/stack.h"
#include "kirimasim/kernel/store.h"
#include "kirimasim/kernel/strings.h"

namespace kirimasim::kernel {

class Context {
public:
    /// A Context that writes to standard output and standard error, and
    /// whose STDIN reads standard input.
    // kirimasim: banned-ok the default destinations (RUNTIME_ABI.md 9.3.10)
    Context() : Context(OutputDestinations{stdout, stderr}, stdin) {}
    /// A Context that writes where it is told (RUNTIME_ABI.md 9.3.10), and
    /// whose STDIN reads in (nothing for nullptr; SYSTEM_TASKS.md 6.3).
    explicit Context(OutputDestinations to, std::FILE* in = nullptr)
        : output_(to), files_(output_, in), dump_(output_, store_),
          scheduler_(*this, store_, counters_) {}
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Counters& counters() { return counters_; }
    [[nodiscard]] const Counters& counters() const { return counters_; }
    Scheduler& scheduler() { return scheduler_; }
    [[nodiscard]] const Scheduler& scheduler() const { return scheduler_; }
    Output& output() { return output_; }
    Files& files() { return files_; }
    Dump& dump() { return dump_; }
    Store& store() { return store_; }
    Heap& heap() { return heap_; }
    [[nodiscard]] const Heap& heap() const { return heap_; }
    Strings& strings() { return strings_; }
    [[nodiscard]] const Strings& strings() const { return strings_; }
    [[nodiscard]] const Store& store() const { return store_; }
    /// The seed of $random with no argument: one for the whole simulation,
    /// starting at 0 (SYSTEM_TASKS.md 12.3).
    std::int32_t& randomSeed() { return randomSeed_; }
    /// The stack of the thread running the design, which the tier reads
    /// when the run starts; each call of the design checks what is left
    /// (RUNTIME_ABI.md 5).
    StackRange& stack() { return stack_; }

private:
    Counters counters_;
    Output output_;
    Files files_;  ///< After output_, which it writes to.
    Store store_;
    Strings strings_;
    Heap heap_;
    Dump dump_;  ///< After output_ and store_, which it reads and writes.
    Scheduler scheduler_;
    std::int32_t randomSeed_ = 0;
    StackRange stack_;
};

/// The C handle of a context (abi.h), and back.
inline kirima_context* toC(Context& c) { return reinterpret_cast<kirima_context*>(&c); }
inline Context& fromC(kirima_context* c) { return *reinterpret_cast<Context*>(c); }

}  // namespace kirimasim::kernel
