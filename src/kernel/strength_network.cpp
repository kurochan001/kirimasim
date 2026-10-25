// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Exact passive worlds, physical-origin path identity and destination-kind
// resolution (approved MOS/switch contract 1/3/6/7). No cached readout is a root.
#include "kirimasim/kernel/strength_drivers.h"

#include <algorithm>
#include <numeric>

#include "kirimasim/kernel/context.h"

namespace kirimasim::kernel {

std::optional<StrengthEdgeState> StrengthDrivers::edge(std::uint32_t id) const noexcept {
    if (id >= edges_.size()) return std::nullopt;
    return edges_[id].state;
}

StrengthError StrengthDrivers::configureEdges(std::span<const StrengthEdgeSpec> edges) {
    if (edges.empty()) return StrengthError::None;
    if (edges.size() >= UINT32_MAX || edges.size() > SIZE_MAX / 2 || nodes_.size() > SIZE_MAX / 8)
        return StrengthError::ResourceLimit;
    edges_.reserve(edges.size());
    componentEdges_.resize(edges.size());
    adjacencyOffsets_.resize(nodes_.size() + 1);
    adjacency_.resize(edges.size() * 2);
    routes_.resize(nodes_.size() * 8);
    inQueue_.resize(nodes_.size());
    queue_.resize(nodes_.size());
    choices_.resize(edges.size());
    states_.resize(nodes_.size());
    std::pmr::vector<std::uint32_t> parent(memory_), roots(memory_);
    parent.resize(nodes_.size());
    roots.resize(nodes_.size());
    std::iota(parent.begin(), parent.end(), 0U);
    const auto find = [&](std::uint32_t node) {
        auto root = node;
        while (parent[root] != root)
            root = parent[root];
        while (parent[node] != node) {
            const auto next = parent[node];
            parent[node] = root;
            node = next;
        }
        return root;
    };
    for (const auto& edge : edges) {
        if (edge.first >= nodes_.size() || edge.second >= nodes_.size())
            return StrengthError::InvalidTarget;
        if (nodes_[edge.first].spec.component != nodes_[edge.second].spec.component)
            return StrengthError::InvalidShape;
        if (!transferStrengthPoint(edge.transfer, 1) ||
            (edge.initial != StrengthConduction::Off && edge.initial != StrengthConduction::On &&
             edge.initial != StrengthConduction::Uncertain))
            return StrengthError::InvalidValue;
        edges_.push_back({edge, {edge.initial}});
        ++components_[nodes_[edge.first].spec.component].edgeCount;
        ++adjacencyOffsets_[std::size_t{edge.first} + 1];
        ++adjacencyOffsets_[std::size_t{edge.second} + 1];
        const auto a = find(edge.first), b = find(edge.second);
        parent[a] = b;
    }
    // Potential connectivity, independent of the initial conduction state.
    // Count physical origins once, not distinct paths through a channel.
    for (const auto& origin : origins_)
        ++roots[find(origin.node)];
    for (std::uint32_t n = 0; n < nodes_.size(); ++n)
        if (nodes_[n].spec.kind == NetKind::Uwire && roots[find(n)] > 1)
            return StrengthError::InvalidTarget;
    std::uint32_t first = 0;
    for (auto& component : components_) {
        component.edgeFirst = first;
        first += component.edgeCount;
        component.edgeCount = 0;
    }
    for (std::size_t n = 1; n < adjacencyOffsets_.size(); ++n)
        adjacencyOffsets_[n] += adjacencyOffsets_[n - 1];
    std::pmr::vector<std::size_t> next(adjacencyOffsets_, memory_);
    for (std::uint32_t e = 0; e < edges.size(); ++e) {
        const auto& edge = edges[e];
        auto& component = components_[nodes_[edge.first].spec.component];
        componentEdges_[component.edgeFirst + component.edgeCount++] = e;
        adjacency_[next[edge.first]++] = e;
        adjacency_[next[edge.second]++] = e;
    }
    return StrengthError::None;
}

bool Scheduler::solvePassiveStrengthComponent(std::uint32_t component) noexcept {
    auto& bank = ctx_.strengthDrivers();
    const auto& part = bank.components_[component];
    bool nonzero = false;
    for (std::uint32_t j = 0; j < part.count; ++j) {
        if (takeStrengthInterrupt()) return false;
        const auto n = bank.componentNodes_[part.first + j];
        bank.values_[n] = {7, -7};  // accumulator, never exposed before completion.
        const auto& node = bank.nodes_[n];
        for (std::uint32_t r = 0; r < node.count; ++r) {
            const auto value = bank.origins_[bank.order_[node.first + r]].state.committed;
            nonzero = nonzero || value != StrengthRange{};
        }
    }
    if (!nonzero) {
        // With no nonzero root, every possible graph is floating, even a ring.
        for (std::uint32_t j = 0; j < part.count; ++j)
            bank.values_[bank.componentNodes_[part.first + j]] = {};
        return true;
    }
    for (std::uint32_t e = 0; e < part.edgeCount; ++e)
        bank.choices_[bank.componentEdges_[part.edgeFirst + e]] = 0;
    bool more = true;
    while (more) {
        if (takeStrengthInterrupt()) return false;
        for (std::uint32_t j = 0; j < part.count; ++j)
            bank.states_[bank.componentNodes_[part.first + j]] = detail::stateBit(0, 1);
        for (std::uint32_t j = 0; j < part.count; ++j) {
            const auto source = bank.componentNodes_[part.first + j];
            const auto& originNode = bank.nodes_[source];
            for (std::uint32_t r = 0; r < originNode.count; ++r) {
                if (takeStrengthInterrupt()) return false;
                const auto range = bank.origins_[bank.order_[originNode.first + r]].state.committed;
                if (range == StrengthRange{}) continue;
                // One eight-entry magnitude map per destination, rebuilt for
                // this physical origin and this fixed graph. Queue membership
                // is bounded by the node count; route improvements are finite.
                for (std::uint32_t k = 0; k < part.count; ++k) {
                    const auto n = bank.componentNodes_[part.first + k];
                    std::fill_n(bank.routes_.begin() + std::size_t{n} * 8, 8, 0);
                    bank.inQueue_[n] = 0;
                }
                for (std::uint8_t rank = 1; rank <= 7; ++rank)
                    bank.routes_[std::size_t{source} * 8 + rank] = rank;
                std::size_t head = 0, tail = 1 % bank.queue_.size(), queued = 1;
                bank.queue_[0] = source;
                bank.inQueue_[source] = 1;
                while (queued != 0) {
                    if (takeStrengthInterrupt()) return false;
                    const auto from = bank.queue_[head];
                    head = (head + 1) % bank.queue_.size();
                    --queued;
                    bank.inQueue_[from] = 0;
                    for (auto at = bank.adjacencyOffsets_[from];
                         at < bank.adjacencyOffsets_[std::size_t{from} + 1]; ++at) {
                        if (takeStrengthInterrupt()) return false;
                        const auto e = bank.adjacency_[at];
                        const auto& edge = bank.edges_[e];
                        if (edge.state.committed == StrengthConduction::Off ||
                            (edge.state.committed == StrengthConduction::Uncertain &&
                             bank.choices_[e] == 0))
                            continue;
                        const auto to =
                            edge.spec.first == from ? edge.spec.second : edge.spec.first;
                        bool changed = false;
                        for (std::uint8_t rank = 1; rank <= 7; ++rank) {
                            const auto carried = bank.routes_[std::size_t{from} * 8 + rank];
                            const auto mapped = transferStrengthPoint(edge.spec.transfer, carried);
                            if (!mapped) {
                                requestEnd(EndReason::Internal, 1);
                                return false;
                            }
                            const auto value = static_cast<std::uint8_t>(*mapped);
                            auto& best = bank.routes_[std::size_t{to} * 8 + rank];
                            if (value > best) {
                                best = value;
                                changed = true;
                            }
                        }
                        if (changed && bank.inQueue_[to] == 0) {
                            bank.queue_[tail] = to;
                            tail = (tail + 1) % bank.queue_.size();
                            ++queued;
                            bank.inQueue_[to] = 1;
                        }
                    }
                }
                for (std::uint32_t k = 0; k < part.count; ++k) {
                    if (takeStrengthInterrupt()) return false;
                    const auto n = bank.componentNodes_[part.first + k];
                    const auto states = bank.states_[n];
                    std::uint32_t combined = 0;
                    // Keep exact point images: do not fill holes by taking an
                    // origin/path hull before resolution in this fixed world.
                    for (int point = -7; point <= 7; ++point) {
                        if (point < range.lo || point > range.hi) continue;
                        const auto rank = static_cast<unsigned>(point < 0 ? -point : point);
                        const int mapped = bank.routes_[std::size_t{n} * 8 + rank];
                        const int value = point < 0 ? -mapped : mapped;
                        combined |= detail::addRange(states, value, value);
                    }
                    bank.states_[n] = combined;
                }
            }
        }
        for (std::uint32_t j = 0; j < part.count; ++j) {
            if (takeStrengthInterrupt()) return false;
            const auto n = bank.componentNodes_[part.first + j];
            const auto value = detail::finishRange(bank.nodes_[n].spec.kind, bank.states_[n]);
            bank.values_[n].lo = std::min(bank.values_[n].lo, value.lo);
            bank.values_[n].hi = std::max(bank.values_[n].hi, value.hi);
        }
        // A carry chain, not an integer world mask: no 32/64-device cutoff.
        more = false;
        for (std::uint32_t j = 0; j < part.edgeCount; ++j) {
            const auto e = bank.componentEdges_[part.edgeFirst + j];
            if (bank.edges_[e].state.committed != StrengthConduction::Uncertain) continue;
            bank.choices_[e] ^= 1;
            if (bank.choices_[e] != 0) {
                more = true;
                break;
            }
        }
    }
    return true;
}
}  // namespace kirimasim::kernel
