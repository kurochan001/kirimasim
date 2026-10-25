// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

#include "kirimasim/kernel/activation.h"
#include "kirimasim/kernel/sim.h"
#include "kirimasim/kernel/wide.h"

namespace kirimasim::vm::detail {

/// A short-lived value access, never an owner or a second mutable copy.
/// Logical frame and reference adapters resolve a place before using it.
/// Writes of a whole value or selection commit once, including wide values.
class FrameValue {
public:
    FrameValue(std::span<kernel::W4> words, std::uint32_t width, bool readonly = false)
        : words_(words), width_(width), readonly_(readonly) {}
    FrameValue(kernel::ActivationStore& store, kernel::Scheduler& scheduler,
               kernel::ActivationBinding binding, std::uint32_t element, std::uint32_t width,
               std::uint32_t physicalLsb = 0)
        : store_(&store), scheduler_(&scheduler), binding_(binding), element_(element),
          width_(width), physicalLsb_(physicalLsb) {}
    static FrameValue reference(kernel::W4 token) {
        FrameValue result{{}, 64, true};
        result.reference_ = token;
        result.isReference_ = true;
        return result;
    }

    [[nodiscard]] kernel::ActivationError read(std::span<kernel::W4> out, std::uint32_t lsb = 0,
                                               std::uint32_t width = 0) const noexcept {
        if (!valid(out.size(), lsb, width)) return kernel::ActivationError::OutOfRange;
        if (isReference_) {
            if (lsb != 0 || (width != 0 && width != 64)) return kernel::ActivationError::OutOfRange;
            out[0] = reference_;
            return kernel::ActivationError::None;
        }
        if (store_ != nullptr)
            return store_->read(binding_, element_, out, physicalLsb_ + lsb,
                                width == 0 ? width_ : width);
        if (lsb == 0 && (width == 0 || width == width_))
            std::copy_n(words_.begin(), chunks(width_), out.begin());
        else
            kernel::wide::bits(out.data(), width == 0 ? width_ : width, words_.data(), width_, lsb);
        return kernel::ActivationError::None;
    }

    [[nodiscard]] kernel::ActivationError write(std::span<const kernel::W4> value,
                                                std::uint32_t lsb = 0,
                                                std::uint32_t width = 0) const noexcept {
        if (!valid(value.size(), lsb, width)) return kernel::ActivationError::OutOfRange;
        if (isReference_) return kernel::ActivationError::ReadOnly;
        if (store_ != nullptr)
            return scheduler_->writeActivation(binding_, element_, value, physicalLsb_ + lsb,
                                               width == 0 ? width_ : width);
        if (readonly_) return kernel::ActivationError::ReadOnly;
        if (lsb == 0 && (width == 0 || width == width_))
            std::copy_n(value.begin(), chunks(width_), words_.begin());
        else
            kernel::wide::put(words_.data(), lsb, value.data(), width == 0 ? width_ : width);
        return kernel::ActivationError::None;
    }

private:
    static std::uint32_t chunks(std::uint32_t width) noexcept {
        return width / 64 + (width % 64 != 0 ? 1U : 0U);
    }
    [[nodiscard]] bool valid(std::size_t count, std::uint32_t lsb,
                             std::uint32_t width) const noexcept {
        const auto n = width == 0 ? width_ : width;
        return width_ != 0 && lsb <= width_ && n <= width_ - lsb &&
               lsb <= UINT32_MAX - physicalLsb_ && count >= chunks(n) &&
               (store_ != nullptr || isReference_ || words_.size() >= chunks(width_));
    }
    std::span<kernel::W4> words_;
    kernel::ActivationStore* store_ = nullptr;
    kernel::Scheduler* scheduler_ = nullptr;
    kernel::ActivationBinding binding_;
    std::uint32_t element_ = 0, width_ = 0;
    std::uint32_t physicalLsb_ = 0;
    kernel::W4 reference_{};
    bool readonly_ = false;
    bool isReference_ = false;
};

}  // namespace kirimasim::vm::detail
