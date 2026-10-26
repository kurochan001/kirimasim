// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "kirimasim/kernel/managed_event.h"
#include "kirimasim/kernel/w4.h"

namespace kirimasim::vm::detail {

/// Owns typed event values while argument/result buffers are unpublished.
/// The first managed value needs no allocation. Legacy/null values need no
/// registry pin. Only the tier's immutable event-parameter plan selects words.
class EventTransfers {
public:
    explicit EventTransfers(kernel::ManagedEvents& events,
                            std::pmr::memory_resource* memory = std::pmr::new_delete_resource())
        : events_(events), extra_(memory) {}
    EventTransfers(const EventTransfers&) = delete;
    EventTransfers& operator=(const EventTransfers&) = delete;

    kernel::ManagedEventError retain(std::span<const kernel::W4> values) noexcept {
        const auto before = size();
        for (const auto value : values) {
            if ((value.a >> 32) == 0) continue;
            auto pin = events_.retain({value.a});
            if (!pin) {
                rollback(before);
                return pin.error;
            }
            if (!firstUsed_) {
                first_ = std::move(pin.value);
                firstUsed_ = true;
                continue;
            }
            try {
                extra_.push_back(std::move(pin.value));
            } catch (const std::bad_alloc&) {
                rollback(before);
                return kernel::ManagedEventError::ResourceLimit;
            } catch (const std::length_error&) {
                rollback(before);
                return kernel::ManagedEventError::ResourceLimit;
            }
        }
        return kernel::ManagedEventError::None;
    }

    kernel::ManagedEventError
    retainArguments(std::span<const std::vector<kernel::W4>> values,
                    std::span<const std::uint32_t> eventParameters) noexcept {
        const auto before = size();
        std::uint32_t previous = 0;
        bool first = true;
        for (const auto index : eventParameters) {
            if (index >= values.size() || (!first && index <= previous)) {
                rollback(before);
                return kernel::ManagedEventError::InvalidBackend;
            }
            const auto error = retain(values[index]);
            if (error != kernel::ManagedEventError::None) {
                rollback(before);
                return error;
            }
            previous = index;
            first = false;
        }
        return kernel::ManagedEventError::None;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return extra_.size() + (firstUsed_ ? 1U : 0U);
    }
    void clear() noexcept { rollback(0); }

private:
    void rollback(std::size_t retained) noexcept {
        while (size() > retained) {
            if (!extra_.empty())
                extra_.pop_back();
            else {
                first_.reset();
                firstUsed_ = false;
            }
        }
    }
    kernel::ManagedEvents& events_;
    kernel::ManagedEventPin first_;
    std::pmr::vector<kernel::ManagedEventPin> extra_;
    bool firstUsed_ = false;
};

}  // namespace kirimasim::vm::detail
