// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/strength_drivers.h"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <tuple>

#include "kirimasim/kernel/context.h"

namespace kirimasim::kernel {
namespace {
bool validRange(StrengthRange value) noexcept {
    return value.lo >= -7 && value.hi <= 7 && value.lo <= value.hi;
}
bool explicitRoots(NetKind kind) noexcept {
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

std::optional<StrengthOriginState> StrengthDrivers::origin(std::uint32_t id) const noexcept {
    if (id >= origins_.size()) return std::nullopt;
    return origins_[id].state;
}

StrengthError StrengthDrivers::configure(const StrengthStorage& storage,
                                         std::span<const StrengthNodeSpec> nodes,
                                         std::span<const StrengthOriginSpec> origins) noexcept {
    if (configured_) return StrengthError::InvalidTarget;
    if (nodes.empty() || nodes.size() >= UINT32_MAX || origins.size() >= UINT32_MAX)
        return StrengthError::InvalidShape;
    try {
        // Build privately: failed validation/allocation leaves registration
        // retryable, with no published bits, pending events or partial table.
        StrengthDrivers staged(memory_);
        staged.nodes_.reserve(nodes.size());
        staged.values_.resize(nodes.size());
        staged.updates_.resize(nodes.size());
        std::pmr::vector<std::uint32_t> byTarget(memory_);
        byTarget.reserve(nodes.size());
        std::uint32_t maxComponent = 0;
        for (std::uint32_t i = 0; i < nodes.size(); ++i) {
            const auto& node = nodes[i];
            StrengthRange initial;
            if (node.at.width != 1 || storage.read(node.at, {&initial, 1}) != StrengthError::None)
                return StrengthError::InvalidTarget;
            if (!explicitRoots(node.kind)) return StrengthError::InvalidValue;
            if (node.component >= nodes.size()) return StrengthError::InvalidShape;
            maxComponent = std::max(maxComponent, node.component);
            staged.nodes_.push_back({node});
            byTarget.push_back(i);
        }
        const auto key = [&](std::uint32_t i) {
            const auto& at = nodes[i].at;
            return std::tuple{at.slot.index(), at.elem, at.lsb};
        };
        std::sort(byTarget.begin(), byTarget.end(),
                  [&](std::uint32_t a, std::uint32_t b) { return key(a) < key(b); });
        for (std::size_t i = 1; i < byTarget.size(); ++i)
            if (key(byTarget[i - 1]) == key(byTarget[i])) return StrengthError::InvalidTarget;
        staged.components_.resize(std::size_t{maxComponent} + 1);
        staged.componentNodes_.resize(nodes.size());
        for (const auto& node : nodes)
            ++staged.components_[node.component].count;
        std::uint32_t firstNode = 0;
        for (auto& component : staged.components_) {
            if (component.count == 0) return StrengthError::InvalidShape;
            component.first = firstNode;
            firstNode += component.count;
            component.count = 0;
        }
        for (std::uint32_t n = 0; n < nodes.size(); ++n) {
            auto& component = staged.components_[nodes[n].component];
            staged.componentNodes_[component.first + component.count++] = n;
        }
        staged.origins_.reserve(origins.size());
        staged.order_.resize(origins.size());
        for (const auto& origin : origins) {
            if (origin.node >= nodes.size()) return StrengthError::InvalidTarget;
            if (!validRange(origin.initial)) return StrengthError::InvalidValue;
            staged.origins_.push_back({origin.node, {origin.initial, {}, 0, false}});
            ++staged.nodes_[origin.node].count;
        }
        std::uint32_t first = 0;
        for (auto& node : staged.nodes_) {
            if (node.spec.kind == NetKind::Uwire && node.count > 1)
                return StrengthError::InvalidTarget;
            node.first = first;
            first += node.count;
            node.count = 0;
        }
        for (std::uint32_t i = 0; i < origins.size(); ++i) {
            auto& node = staged.nodes_[origins[i].node];
            staged.order_[node.first + node.count++] = i;
        }
        nodes_.swap(staged.nodes_);
        origins_.swap(staged.origins_);
        order_.swap(staged.order_);
        components_.swap(staged.components_);
        componentNodes_.swap(staged.componentNodes_);
        values_.swap(staged.values_);
        updates_.swap(staged.updates_);
        configured_ = true;
        return StrengthError::None;
    } catch (const std::bad_alloc&) {
        return StrengthError::ResourceLimit;
    } catch (const std::length_error&) {
        return StrengthError::ResourceLimit;
    }
}

StrengthError
Scheduler::configureStrengthDrivers(std::span<const StrengthNodeSpec> nodes,
                                    std::span<const StrengthOriginSpec> origins) noexcept {
    return ctx_.strengthDrivers().configure(ctx_.strengths(), nodes, origins);
}

StrengthError Scheduler::queueStrengthComponent(std::uint32_t component) noexcept {
    auto& bank = ctx_.strengthDrivers();
    if (bank.components_[component].queued) return StrengthError::None;
    try {
        begin();
        schedule(Region::Active, {Kind::StrengthResolve, Region::Active, component, 0}, 0);
        bank.components_[component].queued = true;
        return StrengthError::None;
    } catch (const std::bad_alloc&) {
        resourceLimit();
        return StrengthError::ResourceLimit;
    } catch (const std::length_error&) {
        resourceLimit();
        return StrengthError::ResourceLimit;
    }
}

StrengthError Scheduler::startStrengthDrivers() noexcept {
    auto& bank = ctx_.strengthDrivers();
    if (!bank.configured_ || bank.started_) return StrengthError::InvalidTarget;
    for (std::uint32_t c = 0; c < bank.components_.size(); ++c) {
        const auto error = queueStrengthComponent(c);
        if (error != StrengthError::None) return error;
    }
    bank.started_ = true;
    return StrengthError::None;
}

StrengthError Scheduler::driveStrength(std::uint32_t origin, StrengthRange value) noexcept {
    auto& bank = ctx_.strengthDrivers();
    if (!bank.started_ || origin >= bank.origins_.size()) return StrengthError::InvalidTarget;
    if (!validRange(value)) return StrengthError::InvalidValue;
    auto& row = bank.origins_[origin];
    auto& state = row.state;
    if (state.hasPending && state.generation == UINT64_MAX) {
        resourceLimit();
        return StrengthError::ResourceLimit;
    }
    if (state.committed != value) {
        const auto error = queueStrengthComponent(bank.nodes_[row.node].spec.component);
        if (error != StrengthError::None) return error;
    }
    if (state.hasPending) ++state.generation;
    state.hasPending = false;
    state.committed = value;
    return StrengthError::None;
}

StrengthError Scheduler::driveStrengthDelayed(std::uint32_t origin, StrengthRange value,
                                              std::uint64_t rise, std::uint64_t fall,
                                              std::uint64_t off) noexcept {
    auto& bank = ctx_.strengthDrivers();
    if (!bank.started_ || origin >= bank.origins_.size()) return StrengthError::InvalidTarget;
    if (!validRange(value)) return StrengthError::InvalidValue;
    auto& state = bank.origins_[origin].state;
    if (state.hasPending && state.pending == value) return StrengthError::None;
    if (!state.hasPending && state.committed == value) return StrengthError::None;
    if (state.generation == UINT64_MAX) {
        resourceLimit();
        return StrengthError::ResourceLimit;
    }
    if (state.committed == value) {
        state.hasPending = false;
        ++state.generation;
        return StrengthError::None;
    }
    const auto projected = strengthValue(value);
    const auto delay = projected.b ? (projected.a ? std::min({rise, fall, off}) : off)
                                   : (projected.a ? rise : fall);
    try {
        begin();
        // schedule detects time overflow before inserting an event. Do not
        // replace the pending interval when no future event could be queued.
        if (delay > std::numeric_limits<std::uint64_t>::max() - time_) {
            overflow_ = true;
            return StrengthError::InvalidValue;
        }
        const auto generation = state.generation + 1;
        schedule(delay == 0 ? Region::Inactive : Region::Active,
                 {Kind::StrengthDrive, Region::Active, origin, generation}, delay);
        state.pending = value;
        state.generation = generation;
        state.hasPending = true;
        return StrengthError::None;
    } catch (const std::bad_alloc&) {
        resourceLimit();
        return StrengthError::ResourceLimit;
    } catch (const std::length_error&) {
        resourceLimit();
        return StrengthError::ResourceLimit;
    }
}

void Scheduler::deliverStrengthDrive(std::uint32_t origin, std::uint64_t generation) noexcept {
    auto& bank = ctx_.strengthDrivers();
    auto& row = bank.origins_[origin];
    auto& state = row.state;
    if (!state.hasPending || state.generation != generation) return;
    const auto error = queueStrengthComponent(bank.nodes_[row.node].spec.component);
    if (error != StrengthError::None) return;
    state.committed = state.pending;
    state.hasPending = false;
}

void Scheduler::resolveStrengthComponent(std::uint32_t component) noexcept {
    auto& bank = ctx_.strengthDrivers();
    bank.components_[component].queued = false;
    std::uint32_t used = 0;
    // No callbacks until the whole input snapshot has been solved. The
    // preallocated output buffer is not an origin and never feeds resolution.
    const auto& part = bank.components_[component];
    for (std::uint32_t j = 0; j < part.count; ++j) {
        if (takeInterrupt()) return;
        const auto n = bank.componentNodes_[part.first + j];
        const auto& node = bank.nodes_[n];
        auto states = detail::stateBit(0, 1);
        for (std::uint32_t i = 0; i < node.count; ++i) {
            if (takeInterrupt()) return;
            const auto& range = bank.origins_[bank.order_[node.first + i]].state.committed;
            states = detail::addRange(states, range.lo, range.hi);
        }
        bank.values_[n] = detail::finishRange(node.spec.kind, states);
        bank.updates_[used++] = {node.spec.at, {&bank.values_[n], 1}};
    }
    // Publication copies all intervals before callbacks may drive origins
    // and queue a subsequent Active solve. There is no recursive solve.
    const auto result = publishStrengths(std::span(bank.updates_).first(used));
    if (result.error != StrengthError::None && result.error != StrengthError::ResourceLimit)
        requestEnd(EndReason::Internal, 1);
}
}  // namespace kirimasim::kernel
