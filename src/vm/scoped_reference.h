// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory_resource>
#include <vector>

#include "activation_environment.h"
#include "frame_layout.h"

namespace kirimasim::vm::detail {

/// Private, nonowning references to actual lexical allocations. Numbers
/// identify generation-checked descriptors, never a caller's mutable row.
/// Immutable plans and the store outlive the registry. A body drops
/// its tickets on scope unwind/return, independently of control-frame reuse.
class ScopedReferences {
public:
    using Error = kernel::ActivationError;
    template <class T> using Result = kernel::ActivationResult<T>;
    struct Selection {
        kernel::ActivationBinding binding;
        std::uint32_t element = 0, lsb = 0, width = 0;
    };
    struct Stats {
        std::size_t live = 0, entries = 0, retired = 0;
    };
    explicit ScopedReferences(kernel::ActivationStore& store,
                              std::pmr::memory_resource* memory = std::pmr::new_delete_resource());
    /// A shape is a decoded implementation table, not another IR or an ABI.
    Result<std::uint32_t> addShape(const ActivationPlan& plan) noexcept;
    Result<std::uint64_t> make(std::uint32_t shape, const ActivationOrigin& origin,
                               std::uint32_t element, std::uint32_t owner,
                               std::size_t depth) noexcept;
    Result<std::uint64_t> step(std::uint64_t token, std::uint64_t words, std::uint32_t owner,
                               std::size_t depth) noexcept;
    /// Strict selected-width access, or one remaining word for narrow refs.
    Result<Selection> resolve(std::uint64_t token, std::uint64_t words,
                              std::uint32_t width) const noexcept;
    Result<Selection> resolveWord(std::uint64_t token, std::uint64_t words) const noexcept;
    void release(std::uint32_t owner, std::size_t minimumDepth = 0) noexcept;
    [[nodiscard]] Stats stats() const noexcept { return {live_, entries_.size(), retired_}; }

private:
    struct Shape {
        const ActivationPlan* plan = nullptr;
        FrameLayout layout;
        std::pmr::vector<std::uint32_t> fields;
        std::pmr::vector<std::uint32_t> ordered;
        Shape(const ActivationPlan& source, std::pmr::memory_resource* memory)
            : plan(&source), layout(source.frame(), memory), fields(memory), ordered(memory) {}
    };
    struct Target {
        kernel::ActivationBinding binding;
        std::uint32_t shape = 0, row = 0;
        std::uint64_t word = 0;
    };
    struct Entry {
        Target target;
        std::size_t depth = 0;
        std::uint32_t generation = 1, next = sir::kNone;
        bool live = false;
    };
    const Entry* find(std::uint64_t token) const noexcept;
    Result<Target> target(const Target& source, std::uint64_t words) const noexcept;
    Result<Selection> selection(const Target& target, std::uint32_t width,
                                bool word) const noexcept;
    Result<std::uint64_t> insert(Target target, std::uint32_t owner, std::size_t depth);
    kernel::ActivationStore& store_;
    std::pmr::memory_resource* memory_;
    std::pmr::vector<Shape> shapes_;
    std::pmr::vector<Entry> entries_;
    std::pmr::map<std::uint32_t, std::pmr::vector<std::uint32_t>> owners_;
    std::uint32_t free_ = sir::kNone;
    std::size_t live_ = 0, retired_ = 0;
};

}  // namespace kirimasim::vm::detail
