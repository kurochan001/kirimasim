// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/sim.h"
#include "kirimasim/sir/design.h"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace kirimasim::driver {
using RaceFile = std::unique_ptr<std::FILE, int (*)(std::FILE*)>;

class RaceTrace {
public:
    explicit RaceTrace(std::FILE* file) : file_(file) {}
    kernel::RaceHook hook() { return {&write, this}; }
    bool flush() const;

private:
    static void write(void*, const kernel::RaceEvent&) noexcept;
    std::FILE* file_;
    bool failed_ = false;
};

/// Snapshot seekable standard input without advancing its shared file offset.
/// Pipes and terminals are recorded lazily as the baseline consumes them.
struct RaceInput {
    std::int64_t offset = -1;
    bool ended = false;
};
bool snapshotInput(std::FILE* input, std::FILE* tape, RaceInput& state);

/// Compare each fresh process with the recorded deterministic run. The parent's
/// file is rewound for each seed; the event stream never accumulates in memory.
bool exploreRaces(const std::string& executable, const std::vector<std::string>& arguments,
                  const sir::Design& design, std::FILE* baseline, std::FILE* input,
                  std::uint64_t runs, std::uint64_t seed, const std::string& directory,
                  RaceInput state);
}  // namespace kirimasim::driver
