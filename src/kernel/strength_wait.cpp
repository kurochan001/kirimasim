// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Internal strength sensitivity shares the ordinary process wait lifetime
// (approved MOS/switch contract 6/7, SCHEDULER_SPEC 9).
#include "kirimasim/kernel/context.h"

#include <new>
#include <stdexcept>

namespace kirimasim::kernel {
StrengthError Scheduler::waitEventStrength(ProcessId process, std::span<const WaitItem> items,
                                           std::span<const SlotPart> strengths) noexcept {
    if (process.index() >= processes_.size() || processes_[process.index()].done)
        return StrengthError::InvalidTarget;
    auto& storage = ctx_.strengths();
    std::pmr::vector<StrengthWatchToken> staged(ctx_.heap().memoryResource());
    const auto cancel = [&]() noexcept {
        for (const auto token : staged)
            storage.cancel(token);
    };
    try {
        std::uint64_t count = 0;
        for (const auto& part : strengths) {
            SlotBits bit{part.slot, 0, part.lsb, part.width};
            if (!storage.normalize(bit)) return StrengthError::InvalidTarget;
            const auto elements = store_.layout(part.slot).elements;
            const auto n = part.numElems == 0 ? elements : part.numElems;
            if (part.firstElem > elements || n > elements - part.firstElem)
                return StrengthError::InvalidTarget;
            count += n;
            if (count > UINT32_MAX) {
                resourceLimit();
                return StrengthError::ResourceLimit;
            }
        }
        staged.reserve(static_cast<std::size_t>(count));
        const auto generation = processes_[process.index()].gen + 1;
        for (const auto& part : strengths) {
            const auto n = part.numElems == 0 ? store_.layout(part.slot).elements : part.numElems;
            for (std::uint32_t e = 0; e < n; ++e) {
                const auto result =
                    storage.subscribeImpl({part.slot, part.firstElem + e, part.lsb, part.width},
                                          nullptr, nullptr, this, process.index(), generation);
                if (result.error != StrengthError::None) {
                    cancel();
                    if (result.error == StrengthError::ResourceLimit) resourceLimit();
                    return result.error;
                }
                staged.push_back(result.token);  // All token capacity was staged first.
            }
        }
        waitEvent(process, items);
        processes_[process.index()].strengthWaits.swap(staged);
        return StrengthError::None;
    } catch (const std::bad_alloc&) {
        cancel();
        resourceLimit();
        return StrengthError::ResourceLimit;
    } catch (const std::length_error&) {
        cancel();
        resourceLimit();
        return StrengthError::ResourceLimit;
    }
}

void Scheduler::strengthChanged(std::uint32_t process, std::uint64_t generation) noexcept {
    if (process >= processes_.size()) return;
    const auto& p = processes_[process];
    if (p.done || p.gen != generation) return;
    try {
        wake(ProcessId{process}, true);
    } catch (const std::bad_alloc&) {
        resourceLimit();
    } catch (const std::length_error&) {
        resourceLimit();
    }
}
}  // namespace kirimasim::kernel
