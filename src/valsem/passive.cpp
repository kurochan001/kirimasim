// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/valsem/passive.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace kirimasim::valsem {
Conduction passiveConduction(Logic control, bool activeHigh) {
    switch (control) {
        case Logic::Zero: return activeHigh ? Conduction::Off : Conduction::On;
        case Logic::One: return activeHigh ? Conduction::On : Conduction::Off;
        case Logic::X:
        case Logic::Z: return Conduction::Uncertain;
    }
    return Conduction::Uncertain;
}

namespace {
bool core(NetKind kind) {
    switch (kind) {
        case NetKind::Wire:
        case NetKind::Tri:
        case NetKind::Wand:
        case NetKind::Triand:
        case NetKind::Wor:
        case NetKind::Trior:
        case NetKind::Uwire: return true;
        default: return false;
    }
}
}  // namespace
std::optional<std::vector<Signal>> resolvePassive(std::span<const NetKind> kinds,
                                                  std::span<const PassiveRoot> roots,
                                                  std::span<const PassiveEdge> edges) {
    if (kinds.empty() || !std::ranges::all_of(kinds, core)) return std::nullopt;
    for (const auto& root : roots)
        if (root.node >= kinds.size() || root.value.lo < -7 || root.value.hi > 7 ||
            root.value.lo > root.value.hi)
            return std::nullopt;
    for (const auto& edge : edges)
        if (edge.first >= kinds.size() || edge.second >= kinds.size() ||
            !transferStrength(edge.transfer, {1, 1}) ||
            (edge.state != Conduction::Off && edge.state != Conduction::On &&
             edge.state != Conduction::Uncertain))
            return std::nullopt;
    // Independent slow reachability check for static uwire ownership.
    for (std::size_t n = 0; n < kinds.size(); ++n) {
        if (kinds[n] != NetKind::Uwire) continue;
        std::vector<bool> seen(kinds.size());
        std::vector<std::size_t> work{n};
        seen[n] = true;
        while (!work.empty()) {
            const auto from = work.back();
            work.pop_back();
            for (const auto& edge : edges) {
                if (edge.first != from && edge.second != from) continue;
                const auto to = edge.first == from ? edge.second : edge.first;
                if (!seen[to]) {
                    seen[to] = true;
                    work.push_back(to);
                }
            }
        }
        unsigned count = 0;
        for (const auto& root : roots)
            if (seen[root.node]) ++count;
        if (count > 1) return std::nullopt;
    }
    std::vector<Signal> result(kinds.size(), {8, -8});
    std::vector<bool> world(edges.size());
    std::vector<int> choices;
    for (const auto& root : roots)
        choices.push_back(root.value.lo);
    bool moreWorlds = true;
    while (moreWorlds) {
        for (std::size_t r = 0; r < roots.size(); ++r)
            choices[r] = roots[r].value.lo;
        bool morePoints = true;
        while (morePoints) {
            std::vector<std::vector<Signal>> contributions(kinds.size());
            for (std::size_t r = 0; r < roots.size(); ++r) {
                std::vector<int> best(kinds.size());
                std::vector<bool> visited(kinds.size());
                const auto visit = [&](auto&& self, std::uint32_t from, int rank) -> void {
                    best[from] = std::max(best[from], rank);
                    visited[from] = true;
                    for (std::size_t e = 0; e < edges.size(); ++e) {
                        const auto& edge = edges[e];
                        if (edge.state == Conduction::Off ||
                            (edge.state == Conduction::Uncertain && !world[e]))
                            continue;
                        if (edge.first != from && edge.second != from) continue;
                        const auto to = edge.first == from ? edge.second : edge.first;
                        if (visited[to]) continue;
                        const auto mapped = transferStrength(edge.transfer, {rank, rank});
                        if (mapped) self(self, to, mapped->lo);
                    }
                    visited[from] = false;
                };
                const int point = choices[r];
                visit(visit, roots[r].node, point < 0 ? -point : point);
                for (std::size_t n = 0; n < kinds.size(); ++n) {
                    const int value = point < 0 ? -best[n] : best[n];
                    contributions[n].push_back({value, value});
                }
            }
            for (std::size_t n = 0; n < kinds.size(); ++n) {
                const auto value = resolveSignals(kinds[n], contributions[n]);
                result[n].lo = std::min(result[n].lo, value.lo);
                result[n].hi = std::max(result[n].hi, value.hi);
            }
            morePoints = false;
            for (std::size_t r = 0; r < roots.size(); ++r) {
                if (choices[r] < roots[r].value.hi) {
                    ++choices[r];
                    morePoints = true;
                    break;
                }
                choices[r] = roots[r].value.lo;
            }
        }
        moreWorlds = false;
        for (std::size_t e = 0; e < edges.size(); ++e) {
            if (edges[e].state != Conduction::Uncertain) continue;
            world[e] = !world[e];
            if (world[e]) {
                moreWorlds = true;
                break;
            }
        }
    }
    return result;
}
}  // namespace kirimasim::valsem
