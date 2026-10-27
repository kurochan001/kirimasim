// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "kirimasim/kernel/w4.h"

namespace kirimasim::vm::detail {

/// Typed native-stack SSA regions. Offsets survive growth of the word vector;
/// inactive words are cleared on every entry and unwind, never guessed by bits.
class ValueTemporaries {
public:
    enum class Kind : std::uint8_t { Container, Reference, String };
    struct Root {
        std::uint32_t offset;
        Kind kind;
    };
    explicit ValueTemporaries(std::vector<kernel::W4>& words) noexcept : words_(words) {}
    ValueTemporaries(const ValueTemporaries&) = delete;
    ValueTemporaries& operator=(const ValueTemporaries&) = delete;
    class Region {
    public:
        Region(ValueTemporaries& owner, std::size_t base, std::size_t words,
               std::span<const Root> roots) noexcept
            : owner_(owner), previous_(owner.top_), base_(base), words_(words), roots_(roots) {
            if (roots.empty()) return;
            valid_ = base <= owner.words_.size() && words <= owner.words_.size() - base;
            std::uint32_t last = 0;
            bool first = true;
            for (const auto root : roots) {
                valid_ = valid_ && root.offset < words && (first || last < root.offset);
                first = false;
                last = root.offset;
            }
            if (previous_ != nullptr)
                valid_ =
                    valid_ && previous_->valid_ && base >= previous_->base_ + previous_->words_;
            owner_.top_ = this;
            clear();
        }
        Region(const Region&) = delete;
        Region& operator=(const Region&) = delete;
        ~Region() {
            if (roots_.empty()) return;
            clear();
            owner_.top_ = previous_;
        }

    private:
        friend class ValueTemporaries;
        void clear() noexcept {
            if (!valid_ || base_ > owner_.words_.size() || words_ > owner_.words_.size() - base_)
                return;
            for (const auto root : roots_)
                owner_.words_[base_ + root.offset] = {0, 0};
        }
        ValueTemporaries& owner_;
        Region* previous_;
        std::size_t base_, words_;
        std::span<const Root> roots_;
        bool valid_ = true;
    };
    bool visit(bool (*visitor)(void*, Kind, kernel::W4) noexcept, void* frame) const noexcept {
        if (visitor == nullptr) return false;
        for (const auto* region = top_; region != nullptr; region = region->previous_) {
            if (!region->valid_ || region->base_ > words_.size() ||
                region->words_ > words_.size() - region->base_)
                return false;
            for (const auto root : region->roots_)
                if (!visitor(frame, root.kind, words_[region->base_ + root.offset])) return false;
        }
        return true;
    }

private:
    std::vector<kernel::W4>& words_;
    Region* top_ = nullptr;
};
}  // namespace kirimasim::vm::detail
