// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Approved whole-net contract 2-6: physical capabilities, temporary public
// deposits and the same force ownership used by HDL statements.
#include "kirimasim/kernel/context.h"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <vector>

namespace kirimasim::kernel {
StrengthPublication Scheduler::writeNet(SlotBits target, std::span<const W4> value,
                                        NetWriteKind kind) noexcept {
    auto& bank = ctx_.strengthDrivers();
    if (kind != NetWriteKind::Deposit && kind != NetWriteKind::Force &&
        kind != NetWriteKind::Release)
        return {StrengthError::InvalidValue};
    if (!bank.started_ || target.slot.index() >= store_.size() ||
        !ctx_.strengths().registered(target.slot))
        return {StrengthError::InvalidTarget};
    if (bank.forcing_ || ctx_.strengths().publishing_) return {StrengthError::Reentrant};
    const auto layout = store_.layout(target.slot);
    if (layout.domain != Domain::Four || layout.elements != 1 || target.elem != 0 ||
        target.lsb != 0 || (target.width != 0 && target.width != layout.width) ||
        layout.width > bank.nodes_.size())
        return {StrengthError::InvalidTarget};
    target.width = layout.width;
    if (kind != NetWriteKind::Release && value.size() != (target.width - 1) / 64 + 1)
        return {StrengthError::InvalidShape};
    const auto savedWalking = walking_;
    const auto savedDraining = draining_;
    try {
        std::vector<std::uint32_t> nodes(target.width, UINT32_MAX);
        std::optional<std::uint32_t> component;
        for (std::uint32_t n = 0; n < bank.nodes_.size(); ++n) {
            const auto& at = bank.nodes_[n].spec.at;
            if (at.slot == target.slot && at.elem == 0) {
                if (at.lsb >= target.width || nodes[at.lsb] != UINT32_MAX)
                    return {StrengthError::InvalidTarget};
                const auto owner = bank.nodes_[n].spec.component;
                if (component && *component != owner) return {StrengthError::InvalidTarget};
                component = owner;
                if (kind == NetWriteKind::Deposit &&
                    bank.components_[bank.nodes_[n].spec.component].edgeCount != 0)
                    return {StrengthError::UnsupportedHold};
                nodes[at.lsb] = n;
            }
        }
        if (std::ranges::find(nodes, UINT32_MAX) != nodes.end())
            return {StrengthError::InvalidTarget};
        if (takeStrengthInterrupt())
            return {reason_ == EndReason::ResourceLimit ? StrengthError::ResourceLimit
                                                        : StrengthError::Interrupted};
        if (kind == NetWriteKind::Deposit) {
            std::vector<std::optional<StrengthRange>> before;
            std::vector<StrengthRange> after;
            std::vector<StrengthUpdate> updates;
            before.reserve(nodes.size());
            after.reserve(nodes.size());
            updates.reserve(nodes.size());
            for (std::uint32_t bit = 0; bit < target.width; ++bit) {
                const auto n = nodes[bit];
                const W4 scalar{(value[bit / 64].a >> (bit % 64)) & 1U,
                                (value[bit / 64].b >> (bit % 64)) & 1U};
                W4 packed;
                static_cast<void>(makeStrengthValue({&packed, 1}, 1, {&scalar, 1}, 6, 6));
                before.push_back(bank.deposits_[n]);
                after.push_back(bank.forces_[n].value_or(
                    strengthRangeAt({&packed, 1}, 0).value_or(StrengthRange{})));
            }
            for (std::uint32_t bit = 0; bit < target.width; ++bit)
                updates.push_back({bank.nodes_[nodes[bit]].spec.at, {&after[bit], 1}});
            for (std::uint32_t bit = 0; bit < target.width; ++bit)
                if (!bank.forces_[nodes[bit]]) bank.setDeposit(nodes[bit], after[bit]);
            beginWalk();
            const auto result = publishStrengths(updates);
            if (!result.committed)
                for (std::uint32_t bit = 0; bit < target.width; ++bit)
                    bank.setDeposit(nodes[bit], before[bit]);
            endWalk();
            return result;
        }
        std::vector<StrengthForceUpdate> updates;
        updates.reserve(nodes.size());
        for (std::uint32_t bit = 0; bit < target.width; ++bit) {
            std::optional<W4> scalar;
            if (kind == NetWriteKind::Force)
                scalar = W4{(value[bit / 64].a >> (bit % 64)) & 1U,
                            (value[bit / 64].b >> (bit % 64)) & 1U};
            updates.push_back({nodes[bit], scalar});
        }
        markHeld(target.slot, true, false, true);
        beginWalk();
        const auto result = setStrengthForces(updates);
        if (result.committed) {
            auto& held = held_[heldOf_[target.slot.index()]];
            constexpr ProcessId owner{UINT32_MAX - 1};
            const auto index = static_cast<int>(Hold::Force);
            std::fill(held.owner[index].begin(), held.owner[index].end(),
                      kind == NetWriteKind::Force ? owner : kNoHolder);
            if (kind == NetWriteKind::Force)
                for (std::size_t word = 0; word < value.size(); ++word) {
                    held.value[index][word * 2] = value[word].a;
                    held.value[index][word * 2 + 1] = value[word].b;
                }
        }
        endWalk();
        return result;
    } catch (const std::bad_alloc&) {
        outOfMemory();
    } catch (const std::length_error&) {
        outOfMemory();
    }
    walking_ = savedWalking;
    draining_ = savedDraining;
    if (savedWalking == 0 && !savedDraining) lateChanges_.clear();
    return {StrengthError::ResourceLimit};
}
}  // namespace kirimasim::kernel
