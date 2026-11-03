// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/module_path.h"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <tuple>

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/edge.h"

namespace kirimasim::kernel {

std::optional<ModulePathDelays> modulePathDelays(std::span<const std::uint64_t> values) noexcept {
    ModulePathDelays d{};
    switch (values.size()) {
        case 1: d.fill(values[0]); return d;
        case 2: d = {values[0], values[1], values[0], values[0], values[1], values[1]}; break;
        case 3: d = {values[0], values[1], values[2], values[0], values[2], values[1]}; break;
        case 6: std::copy(values.begin(), values.end(), d.begin()); break;
        case 12: std::copy(values.begin(), values.end(), d.begin()); return d;
        default: return std::nullopt;
    }
    d[6] = std::min(d[0], d[2]);
    d[7] = std::max(d[0], d[3]);
    d[8] = std::min(d[1], d[4]);
    d[9] = std::max(d[1], d[5]);
    d[10] = std::max(d[2], d[4]);
    d[11] = std::min(d[3], d[5]);
    return d;
}

std::optional<std::uint64_t> modulePathDelay(const ModulePathDelays& d, unsigned from,
                                             unsigned to) noexcept {
    constexpr unsigned index[4][4] = {{12, 0, 2, 6}, {1, 12, 4, 8}, {5, 3, 12, 11}, {9, 7, 10, 12}};
    if (from >= 4 || to >= 4 || from == to) return 0;
    return d[index[from][to]];
}

StrengthError Scheduler::configureModulePaths(std::span<const ModulePathSpec> paths,
                                              ModulePathEvaluator evaluator) noexcept {
    const auto& bank = ctx_.strengthDrivers();
    if (begun_ || modulePathsConfigured_ || !bank.configured_ || bank.started_)
        return StrengthError::InvalidTarget;
    if (paths.empty() || paths.size() >= UINT32_MAX) return StrengthError::InvalidShape;
    try {
        const auto memory = ctx_.heap().memoryResource();
        std::pmr::vector<ModulePathState> staged(memory);
        std::pmr::vector<PathSpan> origins(bank.origins_.size(), PathSpan{}, memory);
        std::pmr::vector<PathSpan> sources(store_.size(), PathSpan{}, memory);
        std::pmr::vector<std::uint32_t> order(memory);
        std::pmr::vector<ModulePathCondition> conditions(memory);
        staged.reserve(paths.size());
        conditions.reserve(static_cast<std::size_t>(std::ranges::count_if(
            paths, [](const auto& path) { return path.enable == ModulePathEnable::Condition; })));
        for (const auto& path : paths) {
            const auto& at = path.source;
            if (at.slot.index() >= store_.size() || path.origin >= origins.size())
                return StrengthError::InvalidTarget;
            const auto& layout = store_.layout(at.slot);
            if (layout.domain != Domain::Four || at.width != 1 || at.elem >= layout.elements ||
                at.lsb >= layout.width)
                return StrengthError::InvalidTarget;
            if (path.edge != ModulePathEdge::Any && path.edge != ModulePathEdge::Pos &&
                path.edge != ModulePathEdge::Neg)
                return StrengthError::InvalidValue;
            if (path.enable != ModulePathEnable::Always &&
                path.enable != ModulePathEnable::Condition &&
                path.enable != ModulePathEnable::IfNone)
                return StrengthError::InvalidValue;
            if (path.enable == ModulePathEnable::Always) {
                if (path.condition != kNoModulePathCondition ||
                    path.group != kNoModulePathCondition)
                    return StrengthError::InvalidTarget;
            } else {
                if (path.edge != ModulePathEdge::Any || path.group == kNoModulePathCondition)
                    return StrengthError::InvalidTarget;
                if (path.enable == ModulePathEnable::Condition) {
                    if (evaluator.read == nullptr || path.condition >= evaluator.count)
                        return StrengthError::InvalidTarget;
                    conditions.push_back({path.condition});
                } else if (path.condition != kNoModulePathCondition) {
                    return StrengthError::InvalidTarget;
                }
            }
            const auto& node = bank.nodes_[bank.origins_[path.origin].node];
            const auto& component = bank.components_[node.spec.component];
            if (node.count != 1 || component.count != 1 || component.edgeCount != 0 ||
                (node.spec.kind != NetKind::Wire && node.spec.kind != NetKind::Tri))
                return StrengthError::InvalidTarget;
            staged.push_back({path, 0, path.edge == ModulePathEdge::Any});
        }
        const auto key = [](const ModulePathState& p) {
            return std::tuple{p.spec.origin,      p.spec.source.slot.index(),
                              p.spec.source.elem, p.spec.source.lsb,
                              p.spec.edge,        p.spec.enable,
                              p.spec.condition};
        };
        std::sort(staged.begin(), staged.end(),
                  [&](const auto& a, const auto& b) { return key(a) < key(b); });
        for (std::uint32_t i = 0; i < staged.size(); ++i) {
            if (i != 0 && key(staged[i - 1]) == key(staged[i])) return StrengthError::InvalidTarget;
            auto& span = origins[staged[i].spec.origin];
            if (span.count++ == 0) span.first = i;
            order.push_back(i);
        }
        const auto pairKey = [](const ModulePathSpec& p) {
            return std::tuple{p.origin, p.source.slot, p.source.elem, p.source.lsb};
        };
        for (const auto& path : staged) {
            if (path.spec.enable == ModulePathEnable::Always) continue;
            for (const auto& other : staged) {
                const bool samePair = pairKey(path.spec) == pairKey(other.spec);
                if (other.spec.enable != ModulePathEnable::Always &&
                    (path.spec.group == other.spec.group) != samePair)
                    return StrengthError::InvalidTarget;
                if (samePair && path.spec.enable == ModulePathEnable::IfNone &&
                    (other.spec.enable == ModulePathEnable::Always ||
                     other.spec.edge != ModulePathEdge::Any))
                    return StrengthError::InvalidTarget;
            }
        }
        std::sort(conditions.begin(), conditions.end(),
                  [](const auto& a, const auto& b) { return a.id < b.id; });
        conditions.erase(std::unique(conditions.begin(), conditions.end(),
                                     [](const auto& a, const auto& b) { return a.id == b.id; }),
                         conditions.end());
        for (auto& path : staged)
            if (path.spec.enable == ModulePathEnable::Condition)
                path.conditionIndex = static_cast<std::uint32_t>(
                    std::lower_bound(conditions.begin(), conditions.end(), path.spec.condition,
                                     [](const auto& c, auto id) { return c.id < id; }) -
                    conditions.begin());
        std::sort(order.begin(), order.end(), [&](auto a, auto b) {
            return staged[a].spec.source.slot < staged[b].spec.source.slot;
        });
        for (std::uint32_t i = 0; i < order.size(); ++i) {
            auto& span = sources[staged[order[i]].spec.source.slot.index()];
            if (span.count++ == 0) span.first = i;
        }
        modulePaths_.swap(staged);
        modulePathOrigins_.swap(origins);
        modulePathSources_.swap(sources);
        modulePathSourceOrder_.swap(order);
        modulePathConditions_.swap(conditions);
        modulePathEvaluator_ = evaluator;
        modulePathsConfigured_ = true;
        return StrengthError::None;
    } catch (const std::bad_alloc&) {
        return StrengthError::ResourceLimit;
    } catch (const std::length_error&) {
        return StrengthError::ResourceLimit;
    }
}

void Scheduler::detachModulePathEvaluator(void* owner) noexcept {
    if (modulePathEvaluator_.user == owner) modulePathEvaluator_ = {};
}

void Scheduler::captureModulePathSources(SlotId slot, std::uint32_t elem, std::uint32_t lsb,
                                         std::uint32_t width, const std::uint64_t* old,
                                         const std::uint64_t* now) noexcept {
    if (slot.index() >= modulePathSources_.size()) return;
    const auto span = modulePathSources_[slot.index()];
    for (std::uint32_t i = 0; i < span.count; ++i) {
        auto& path = modulePaths_[modulePathSourceOrder_[span.first + i]];
        const auto& at = path.spec.source;
        if (at.elem != elem || at.lsb < lsb || (width != 0 && at.lsb - lsb >= width)) continue;
        const auto word = 2 * (at.lsb / 64);
        const auto bit = at.lsb % 64;
        const auto from = logicOf((old[word] >> bit) & 1U, (old[word + 1] >> bit) & 1U);
        const auto to = logicOf((now[word] >> bit) & 1U, (now[word + 1] >> bit) & 1U);
        if (from == to) continue;
        path.time = time_;
        path.eligible =
            path.spec.edge == ModulePathEdge::Any ||
            (path.spec.edge == ModulePathEdge::Pos ? isPosedge(from, to) : isNegedge(from, to));
    }
}

StrengthError Scheduler::driveModulePath(std::uint32_t origin, StrengthRange value) noexcept {
    auto& state = ctx_.strengthDrivers().origins_[origin].state;
    if (state.hasPending && state.pending == value) return StrengthError::None;
    if (!state.hasPending && state.committed == value) return StrengthError::None;
    if (state.generation == UINT64_MAX) {
        resourceLimit();
        return StrengthError::ResourceLimit;
    }
    const bool removed = state.hasPending;
    if (state.committed == value) {
        state.hasPending = false;
        ++state.generation;
        if (removed) pulseRemoved(PulseKind::StrengthDrive, origin);
        return StrengthError::None;
    }
    const auto before = strengthValue(state.committed), after = strengthValue(value);
    const auto from = logicOf(before.a, before.b), to = logicOf(after.a, after.b);
    bool eligible = false;
    std::uint64_t sourceTime = 0;
    std::optional<std::uint64_t> delay = 0;
    const auto span = modulePathOrigins_[origin];
    // Snapshot each evaluator once for this selection, never on a source event.
    for (std::uint32_t i = 0; i < span.count; ++i) {
        const auto index = modulePaths_[span.first + i].conditionIndex;
        if (index != kNoModulePathCondition) modulePathConditions_[index].evaluated = false;
    }
    for (std::uint32_t i = 0; i < span.count; ++i) {
        const auto index = modulePaths_[span.first + i].conditionIndex;
        if (index == kNoModulePathCondition) continue;
        auto& condition = modulePathConditions_[index];
        if (condition.evaluated) continue;
        W4 value{};
        if (modulePathEvaluator_.read == nullptr ||
            !modulePathEvaluator_.read(modulePathEvaluator_.user, condition.id, value)) {
            if (!ending_) fail("module-path condition evaluation failed");
            return StrengthError::InvalidValue;
        }
        condition.enabled = modulePathEnabled(value);
        condition.evaluated = true;
    }
    for (std::uint32_t i = 0; i < span.count; ++i) {
        const auto& path = modulePaths_[span.first + i];
        if (!path.eligible) continue;
        if (path.spec.enable == ModulePathEnable::Condition &&
            !modulePathConditions_[path.conditionIndex].enabled)
            continue;
        if (path.spec.enable == ModulePathEnable::IfNone) {
            bool enabled = false;
            for (std::uint32_t j = 0; j < span.count; ++j) {
                const auto& member = modulePaths_[span.first + j];
                if (member.spec.group == path.spec.group &&
                    member.spec.enable == ModulePathEnable::Condition)
                    enabled = enabled || modulePathConditions_[member.conditionIndex].enabled;
            }
            if (enabled) continue;
        }
        const auto selected = modulePathDelay(path.spec.delays, from, to);
        if (!eligible || path.time > sourceTime) {
            eligible = true;
            sourceTime = path.time;
            delay = selected;
        } else if (path.time == sourceTime) {
            if (selected && (!delay || *selected < *delay)) delay = selected;
        }
    }
    if (eligible && (!delay || *delay > UINT64_MAX - sourceTime)) {
        overflow_ = true;
        return StrengthError::InvalidValue;
    }
    const auto deadline = eligible ? std::max(time_, sourceTime + *delay) : time_;
    try {
        begin();
        const auto generation = state.generation + 1;
        schedule(Region::Active, {Kind::StrengthDrive, Region::Active, origin, generation},
                 deadline - time_);
        state.pending = value;
        state.generation = generation;
        state.hasPending = true;
        if (removed) pulseRemoved(PulseKind::StrengthDrive, origin);
        return StrengthError::None;
    } catch (const std::bad_alloc&) {
        resourceLimit();
        return StrengthError::ResourceLimit;
    } catch (const std::length_error&) {
        resourceLimit();
        return StrengthError::ResourceLimit;
    }
}
}  // namespace kirimasim::kernel
