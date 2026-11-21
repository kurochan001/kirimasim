// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "kirimasim/sir/verified.h"

namespace kirimasim::analysis {

inline constexpr std::uint32_t kTableSchema = 1;
using Digest = std::array<std::uint8_t, 32>;

/// SHA-256 of the supplied bytes, including embedded zeroes.
[[nodiscard]] Digest contentDigest(std::string_view bytes);

/// Identity of the canonical SIR and the producer's pass/options bytes. Layout
/// and producer IDs include their revisions. The loader recomputes the SIR digest.
struct TableKey {
    std::uint32_t sirVersion = 0;
    std::uint32_t schemaVersion = 0;
    Digest design{};
    Digest options{};
    std::string layout;
    std::string producer;
    bool operator==(const TableKey&) const = default;
};

/// Hash canonical SIR and the exact deterministic options/pass bytes.
std::optional<TableKey> makeTableKey(const sir::VerifiedDesign& design, std::string_view options,
                                     std::string layout, std::string producer);

enum class SiteKind : std::uint8_t { Slot, BlockParameter, Result, Operand };
struct ValueSite {
    SiteKind kind = SiteKind::Slot;
    sir::Owner::Kind owner = sir::Owner::Kind::None;
    std::uint32_t row = 0;  ///< Slot ID or body ID within its owner kind.
    std::uint32_t block = 0;
    std::uint32_t instruction = 0;
    std::uint32_t index = 0;  ///< Block parameter or operand position.
    auto operator<=>(const ValueSite&) const = default;
};

enum class NoXProof : std::uint8_t { Unknown, DeclaredTwoState, Dataflow };
enum class PreserveReason : std::uint8_t { Waveform, Foreign, Force, Explicit };
struct ValueFact {
    ValueSite site;
    std::uint32_t width = 0;
    NoXProof noX = NoXProof::Unknown;
    std::vector<PreserveReason> preserve;  ///< Sorted unique reasons, empty if none reported.
};
struct IslandMember {
    sir::ProcessId process;
    std::uint32_t island;
    std::uint32_t level;
    std::vector<sir::ProcessId> dependencies;
};
enum class ExclusionReason : std::uint8_t {
    Kind,
    Delay,
    MidEventControl,
    SideEffectCall,
    NetResolver,
    HeapValue,
    SystemTask,
    Dpi,
    ReportQueue,
    OverrideTarget,
    Program,
    Scc,
    RuntimeIndex,
    UnknownEntry,
    CodegenUnsupported
};
struct IslandExclusion {
    sir::ProcessId process;
    std::vector<ExclusionReason> reasons;  ///< Nonempty, sorted and unique.
};
struct TableData {
    TableKey key;
    std::vector<ValueFact> values;          ///< Strictly increasing site keys.
    std::vector<IslandMember> members;      ///< Strictly increasing process IDs.
    std::vector<IslandExclusion> excluded;  ///< Sorted IDs disjoint from members.
};

/// Bind only trusted producer output. Identity, types, positions and dependency
/// order are checked here. Proof construction (including island eligibility)
/// belongs to the producing analysis. A missing table or row means unknown.
class BoundTables {
public:
    static std::optional<BoundTables> bind(const sir::VerifiedDesign& design,
                                           const TableKey& expected, TableData data);
    [[nodiscard]] const ValueFact* value(const ValueSite& site) const;
    [[nodiscard]] const IslandMember* member(sir::ProcessId process) const;
    [[nodiscard]] const IslandExclusion* exclusion(sir::ProcessId process) const;

private:
    explicit BoundTables(TableData data);
    TableData data_;
};

}  // namespace kirimasim::analysis
