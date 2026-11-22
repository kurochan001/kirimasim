// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/analysis/islands.h"

#include "island_candidates.h"
#include "kirimasim/sir/text_v1.h"
#include <algorithm>
#include <array>
#include <map>
#include <numeric>
#include <queue>
#include <sstream>
#include <tuple>

namespace kirimasim::analysis {
namespace {
constexpr char kOptions[] = "static-islands";
constexpr char kLayout[] = "logical-slot-range-v1";
constexpr char kProducer[] = "islands-v1";
using Graph = std::vector<std::vector<std::uint32_t>>;
using Index = std::uint32_t;

Graph dependencies(const sir::Design& d, const std::vector<IslandCandidate>& rows) {
    struct Reader {
        Index process;
        sir::SlotRange range;
    };
    std::vector<std::vector<Reader>> readers(d.slots.size());
    for (Index i = 0; i < rows.size(); ++i)
        if (rows[i].reasons.empty())
            for (const auto r : rows[i].inputs)
                readers[r.slot.index()].push_back({i, r});
    Graph next(rows.size());
    for (Index i = 0; i < rows.size(); ++i) {
        if (!rows[i].reasons.empty()) continue;
        for (auto r : rows[i].immediate)
            for (const auto& reader : readers[r.slot.index()])
                if (sir::overlaps(d.slot(r.slot), r, reader.range))
                    next[i].push_back(reader.process);
        std::ranges::sort(next[i]);
        next[i].erase(std::unique(next[i].begin(), next[i].end()), next[i].end());
    }
    return next;
}

// Two iterative depth-first passes. A downstream tail of a cycle remains
// eligible, even when the only path to that tail crosses an excluded process.
void excludeCycles(const Graph& next, const Graph& previous, std::vector<IslandCandidate>& rows) {
    std::vector<bool> seen(next.size());
    std::vector<Index> finished;
    std::vector<std::pair<Index, std::size_t>> stack;
    for (Index root = 0; root < next.size(); ++root) {
        if (seen[root] || !rows[root].reasons.empty()) continue;
        seen[root] = true;
        stack.emplace_back(root, 0);
        while (!stack.empty()) {
            auto& [node, cursor] = stack.back();
            if (cursor == next[node].size()) {
                finished.push_back(node);
                stack.pop_back();
            } else {
                const auto child = next[node][cursor++];
                if (!seen[child]) {
                    seen[child] = true;
                    stack.emplace_back(child, 0);
                }
            }
        }
    }
    std::vector<Index> component;
    while (!finished.empty()) {
        const auto root = finished.back();
        finished.pop_back();
        if (!seen[root]) continue;
        component.assign(1, root);
        seen[root] = false;
        for (std::size_t i = 0; i < component.size(); ++i)
            for (auto node : previous[component[i]])
                if (seen[node]) {
                    seen[node] = false;
                    component.push_back(node);
                }
        if (component.size() > 1 || std::ranges::binary_search(next[root], root))
            for (auto node : component)
                rows[node].reasons.push_back(ExclusionReason::Scc);
    }
}

class Components {
public:
    explicit Components(std::size_t n) : parent_(n), size_(n, 1) {
        std::iota(parent_.begin(), parent_.end(), 0U);
    }
    Index root(Index p) {
        while (parent_[p] != p) {
            parent_[p] = parent_[parent_[p]];
            p = parent_[p];
        }
        return p;
    }
    void join(Index a, Index b) {
        a = root(a);
        b = root(b);
        if (a == b) return;
        if (size_[a] < size_[b]) std::swap(a, b);
        parent_[b] = a;
        size_[a] += size_[b];
    }

private:
    std::vector<Index> parent_, size_;
};

constexpr std::array kReasons = {"kind",
                                 "delay",
                                 "mid-event-control",
                                 "side-effect-call",
                                 "net-resolver",
                                 "heap-value",
                                 "system-task",
                                 "dpi",
                                 "report-queue",
                                 "override-target",
                                 "program",
                                 "scc",
                                 "runtime-index",
                                 "unknown-entry",
                                 "codegen-unsupported"};
static_assert(kReasons.size() == static_cast<std::size_t>(ExclusionReason::CodegenUnsupported) + 1);
}  // namespace

bool matchesIslandAnalysis(const sir::VerifiedDesign& verified, const TableData& table) {
    const TableKey expected{sir::kTextV1Version,     kTableSchema, table.key.design,
                            contentDigest(kOptions), kLayout,      kProducer};
    return BoundTables::bind(verified, expected, table).has_value();
}

std::optional<TableData> analyzeIslands(const sir::VerifiedDesign& verified) {
    const auto key = makeTableKey(verified, kOptions, kLayout, kProducer);
    if (!key) return {};
    TableData out;
    out.key = *key;
    const auto& d = verified.design();
    auto rows = islandCandidates(d);
    const auto next = dependencies(d, rows);
    Graph previous(rows.size());
    for (Index p = 0; p < rows.size(); ++p)
        for (auto child : next[p])
            previous[child].push_back(p);
    excludeCycles(next, previous, rows);
    Components groups(rows.size());
    std::vector<Index> degree(rows.size()), level(rows.size());
    std::queue<Index> ready;
    using Clock = std::tuple<sir::SlotId, Index, Index, Index, Index, sir::Edge>;
    std::map<Clock, Index> clocks;
    for (Index p = 0; p < rows.size(); ++p) {
        if (!rows[p].reasons.empty()) continue;
        for (auto parent : previous[p])
            if (rows[parent].reasons.empty()) {
                ++degree[p];
                groups.join(p, parent);
            }
        if (degree[p] == 0) ready.push(p);
        const auto& process = d.processes[p];
        if (process.kind != sir::ProcessKind::Ff && process.kind != sir::ProcessKind::FfPlain)
            continue;
        for (const auto& trigger : process.triggers) {
            if (trigger.watch != sir::kNone) continue;
            const auto r = sir::physicalRange(d, trigger.range);
            const auto& slot = d.slot(r.slot);
            auto [it, inserted] = clocks.try_emplace(
                {r.slot, r.firstElem,
                 r.numElems ? r.numElems : static_cast<Index>(sir::elementCount(slot)), r.lsb,
                 r.width ? r.width : slot.type.width, trigger.edge},
                p);
            if (!inserted) groups.join(p, it->second);
        }
    }
    while (!ready.empty()) {
        const auto p = ready.front();
        ready.pop();
        for (auto child : next[p])
            if (rows[child].reasons.empty()) {
                level[child] = std::max(level[child], level[p] + 1);
                if (--degree[child] == 0) ready.push(child);
            }
    }
    std::vector<Index> numbers(rows.size(), UINT32_MAX);
    Index count = 0;
    for (Index p = 0; p < rows.size(); ++p) {
        const sir::ProcessId id{p};
        if (!rows[p].reasons.empty()) {
            out.excluded.push_back({id, rows[p].reasons});
            continue;
        }
        auto& number = numbers[groups.root(p)];
        if (number == UINT32_MAX) number = count++;
        IslandMember member{id, number, level[p], {}};
        for (auto parent : previous[p])
            if (rows[parent].reasons.empty()) member.dependencies.emplace_back(parent);
        out.members.push_back(std::move(member));
    }
    return out;
}

std::string islandReport(const sir::Design& design, const TableData* table,
                         std::span<const std::uint32_t> unsupportedIslands) {
    std::size_t m = 0, e = 0;
    Index islands = 0;
    std::array<std::size_t, kReasons.size()> counts{};
    std::ostringstream out;
    for (Index p = 0; p < design.processes.size(); ++p) {
        out << "process " << p << ' ' << design.processes[p].name;
        if (table && m < table->members.size() && table->members[m].process.index() == p) {
            const auto& row = table->members[m++];
            islands = std::max(islands, row.island + 1);
            out << " island=" << row.island << " level=" << row.level << " predecessors=";
            for (std::size_t i = 0; i < row.dependencies.size(); ++i) {
                if (i) out << ',';
                out << row.dependencies[i].index();
            }
            if (std::ranges::binary_search(unsupportedIslands, row.island)) {
                out << " fallback=codegen-unsupported";
                ++counts[static_cast<std::size_t>(ExclusionReason::CodegenUnsupported)];
            }
        } else {
            out << " excluded=";
            if (table && e < table->excluded.size() && table->excluded[e].process.index() == p) {
                const auto& reasons = table->excluded[e++].reasons;
                for (std::size_t i = 0; i < reasons.size(); ++i) {
                    const auto code = static_cast<std::size_t>(reasons[i]);
                    if (i) out << ',';
                    out << kReasons[code];
                    ++counts[code];
                }
            } else {
                out << "unknown-entry";
                ++counts[static_cast<std::size_t>(ExclusionReason::UnknownEntry)];
            }
        }
        out << '\n';
    }
    out << "islands " << islands << "\nmembers " << m << '\n';
    for (std::size_t i = 0; i < counts.size(); ++i)
        out << "reason " << kReasons[i] << ' ' << counts[i] << '\n';
    return out.str();
}
}  // namespace kirimasim::analysis
