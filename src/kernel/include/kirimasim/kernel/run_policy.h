// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/heap.h"
#include "kirimasim/kernel/strings.h"
#include "kirimasim/kernel/urandom.h"
#include <array>
#include <map>
#include <set>
#include <utility>
namespace kirimasim::kernel {
enum class CondXMode : std::uint8_t { Off, Warn, Error };
enum class InitValuesMode : std::uint8_t { X, Zero, One, Random };
enum class DiagnosticAction : std::uint8_t { Quiet, Warn, Design, Resource, Internal };
struct RuntimeDiagnostic {
    DiagnosticAction action;
    std::string_view text;
};
RuntimeDiagnostic heapDiagnostic(HeapStatus) noexcept;
RuntimeDiagnostic objectDiagnostic(ObjectError) noexcept;
std::string conditionDiagnostic(std::string_view kind, CondXMode mode);
enum class SourceCheck : std::uint8_t { Bounds, Condition };
using SourcePosition = std::array<std::uint32_t, 3>;
struct CheckCounts {
    std::uint64_t occurrences = 0, positions = 0;
};
class RunDiagnostics {
public:
    bool firstInstruction(std::uint64_t id) { return warned_.insert(id).second; }
    bool firstHeap(SourcePosition site) { return heapWarned_.insert(site).second; }
    bool firstReplication() noexcept { return !std::exchange(replication_, true); }
    bool occurrence(SourceCheck check, SourcePosition position);
    CheckCounts counts(SourceCheck check) const noexcept;

private:
    bool replication_ = false;
    std::set<std::uint64_t> warned_;
    std::set<SourcePosition> heapWarned_;
    std::array<std::map<SourcePosition, std::uint64_t>, 2> counts_;
};
std::uint64_t sourceInitialBits(InitValuesMode, Rng&) noexcept;
void fillDefault(std::span<W4>, std::uint32_t width, bool fourState) noexcept;
void fillSourceDefault(std::span<W4>, std::uint32_t width, bool fourState, InitValuesMode,
                       Rng&) noexcept;
std::string replicateString(std::string_view text, std::optional<std::int64_t> count);
W4 selectString(const Strings&, W4 condition, W4 yes, W4 no) noexcept;
enum class MemoryWrite : std::uint8_t { Done, OpenError, WriteError };
MemoryWrite writeMemoryFile(const std::string& name, std::string_view bytes) noexcept;
}  // namespace kirimasim::kernel
