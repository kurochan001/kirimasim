// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/run_policy.h"
#include "kirimasim/kernel/w4ops.h"
#include "kirimasim/kernel/wide.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
namespace kirimasim::kernel {
RuntimeDiagnostic heapDiagnostic(HeapStatus status) noexcept {
    using A = DiagnosticAction;
    switch (status.error) {
        case HeapError::None: break;
        case HeapError::ResourceLimit: return {A::Resource, "container resource limit"};
        case HeapError::InvalidSize:
            return {A::Design, "dynamic-array size is negative or contains X/Z bits"};
        default: return {A::Internal, "invalid typed container operand or reference"};
    }
    switch (status.warning) {
        case HeapWarning::None: return {A::Quiet, {}};
        case HeapWarning::BoundExceeded:
            return {A::Warn, "queue operation exceeds its declared bound"};
        case HeapWarning::EmptyPop: return {A::Warn, "pop from an empty queue"};
        case HeapWarning::InvalidQueueWrite:
            return {A::Warn, "write to an invalid queue index is ignored"};
        default: return {A::Warn, "queue method index is invalid"};
    }
}
RuntimeDiagnostic objectDiagnostic(ObjectError e) noexcept {
    using A = DiagnosticAction;
    switch (e) {
        case ObjectError::None: return {A::Quiet, {}};
        case ObjectError::ResourceLimit: return {A::Resource, "class object resource limit"};
        case ObjectError::Null: return {A::Design, "null class handle access"};
        case ObjectError::ConstWrite:
            return {A::Design, "instance const property write is not permitted or is repeated"};
        case ObjectError::CastFailure: return {A::Design, "class $cast failed"};
        default: return {A::Internal, "invalid typed class operand or constructor state"};
    }
}
std::string conditionDiagnostic(std::string_view kind, CondXMode mode) {
    return std::string(kind) + (kind == "case" ? " expression" : " condition") +
           " contains X/Z bits (--cond-x=" + (mode == CondXMode::Error ? "error)" : "warn)");
}
bool RunDiagnostics::occurrence(SourceCheck check, SourcePosition position) {
    auto& count = counts_[static_cast<unsigned>(check)][position];
    if (count == UINT64_MAX) return false;
    return ++count == 1;
}
CheckCounts RunDiagnostics::counts(SourceCheck check) const noexcept {
    const auto& sites = counts_[static_cast<unsigned>(check)];
    CheckCounts out{0, sites.size()};
    for (const auto& [position, count] : sites)
        out.occurrences += std::min(count, UINT64_MAX - out.occurrences);
    return out;
}
std::uint64_t sourceInitialBits(InitValuesMode mode, Rng& rng) noexcept {
    switch (mode) {
        case InitValuesMode::One: return UINT64_MAX;
        case InitValuesMode::Random: return rng.next();
        default: return 0;
    }
}
void fillDefault(std::span<W4> out, std::uint32_t width, bool four) noexcept {
    if (out.size() == 1) {
        out[0] = four ? allX(width) : W4{};
        return;
    }
    if (four && width)
        wide::allX(out.data(), width);
    else
        std::fill(out.begin(), out.end(), W4{});
}
void fillSourceDefault(std::span<W4> out, std::uint32_t width, bool four, InitValuesMode mode,
                       Rng& rng) noexcept {
    if (mode == InitValuesMode::X || !four) {
        fillDefault(out, width, four);
        return;
    }
    for (auto& word : out) {
        const auto n = std::min(64U, width);
        word = {sourceInitialBits(mode, rng) & mask(n), 0};
        width -= n;
    }
}
std::string replicateString(std::string_view text, std::optional<std::int64_t> count) {
    if (!count || *count <= 0 || text.empty()) return {};
    std::string result;
    if (std::uint64_t(*count) > result.max_size() / text.size())
        throw std::length_error("string replication");
    result.reserve(static_cast<std::size_t>(*count) * text.size());
    for (std::int64_t i = 0; i < *count; ++i)
        result += text;
    return result;
}
W4 selectString(const Strings& strings, W4 condition, W4 yes, W4 no) noexcept {
    auto c = truth4(condition);
    if (!c.b) return (c.a & 1U) ? yes : no;
    return strings.get(yes.a) == strings.get(no.a) ? yes : W4{};
}
MemoryWrite writeMemoryFile(const std::string& name, std::string_view bytes) noexcept {
    auto* f = std::fopen(name.c_str(), "w");
    if (!f) return MemoryWrite::OpenError;
    const bool written = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    const bool closed = std::fclose(f) == 0;
    return written && closed ? MemoryWrite::Done : MemoryWrite::WriteError;
}
}  // namespace kirimasim::kernel
