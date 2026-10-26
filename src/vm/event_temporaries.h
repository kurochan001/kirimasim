// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "kirimasim/kernel/managed_event.h"
#include "kirimasim/kernel/wide.h"

namespace kirimasim::vm::detail {

/// Typed SSA regions on the native execution stack. Region descriptors borrow
/// immutable word offsets, never pointers into the growing temporary vector.
/// This publishes only event values; other root providers remain necessary.
class EventTemporaries {
public:
    explicit EventTemporaries(std::vector<kernel::W4>& words) noexcept : words_(words) {}
    EventTemporaries(const EventTemporaries&) = delete;
    EventTemporaries& operator=(const EventTemporaries&) = delete;

    class Region {
    public:
        Region(EventTemporaries& owner, std::size_t base, std::size_t words,
               std::span<const std::uint32_t> events) noexcept
            : owner_(owner), previous_(owner.top_), base_(base), words_(words), events_(events) {
            // Event-free bodies add no descriptor or per-word work to the path.
            if (events_.empty()) return;
            valid_ = base_ <= owner_.words_.size() && words_ <= owner_.words_.size() - base_;
            std::uint32_t last = 0;
            bool first = true;
            for (const auto offset : events_) {
                valid_ = valid_ && offset < words_ && (first || last < offset);
                last = offset;
                first = false;
            }
            if (previous_ != nullptr)
                valid_ =
                    valid_ && previous_->valid_ && base_ >= previous_->base_ + previous_->words_;
            owner_.top_ = this;
            clear();
        }
        Region(const Region&) = delete;
        Region& operator=(const Region&) = delete;
        ~Region() {
            if (events_.empty()) return;
            clear();
            owner_.top_ = previous_;
        }

    private:
        friend class EventTemporaries;
        void clear() noexcept {
            if (!valid_ || base_ > owner_.words_.size() || words_ > owner_.words_.size() - base_)
                return;
            for (const auto offset : events_)
                owner_.words_[base_ + offset] = {0, 0};
        }
        EventTemporaries& owner_;
        Region* previous_;
        std::size_t base_, words_;
        std::span<const std::uint32_t> events_;
        bool valid_ = true;
    };

    [[nodiscard]] kernel::ManagedEventRootProvider provider() const noexcept {
        return {this, &enumerate};
    }

private:
    static kernel::ManagedEventError enumerate(const void* raw,
                                               kernel::ManagedEventRoots& roots) noexcept {
        const auto& self = *static_cast<const EventTemporaries*>(raw);
        for (const auto* region = self.top_; region != nullptr; region = region->previous_) {
            if (!region->valid_ || region->base_ > self.words_.size() ||
                region->words_ > self.words_.size() - region->base_)
                return kernel::ManagedEventError::InvalidBackend;
            for (const auto offset : region->events_) {
                const auto bits = self.words_[region->base_ + offset].a;
                // Generation zero encodes permanent legacy objects or null.
                if ((bits >> 32) == 0) continue;
                const auto error = roots.add({bits});
                if (error != kernel::ManagedEventError::None) return error;
            }
        }
        return kernel::ManagedEventError::None;
    }
    std::vector<kernel::W4>& words_;
    Region* top_ = nullptr;
};

}  // namespace kirimasim::vm::detail
