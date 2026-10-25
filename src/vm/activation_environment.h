// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <optional>
#include <span>
#include <vector>

#include "kirimasim/kernel/activation.h"
#include "kirimasim/sir/design.h"

namespace kirimasim::vm::detail {

/// A decoded SIR storage table, with kernel layouts and logical field IDs.
/// It selects no physical frame ABI. SIR tables and the owning Context must
/// outlive the plan and its environments, and remain immutable.
class ActivationPlan {
public:
    using Error = kernel::ActivationError;
    template <class T> using Result = kernel::ActivationResult<T>;
    ActivationPlan() = default;
    ActivationPlan(const ActivationPlan&) = delete;
    ActivationPlan& operator=(const ActivationPlan&) = delete;
    ActivationPlan(ActivationPlan&& other) noexcept;
    ActivationPlan& operator=(ActivationPlan&&) = delete;
    static Result<ActivationPlan>
    compile(kernel::ActivationStore& store, const std::vector<sir::FrameVar>& frame,
            const sir::ActivationInfo& info,
            std::pmr::memory_resource* memory = std::pmr::new_delete_resource()) noexcept;
    [[nodiscard]] bool valid() const noexcept { return store_ != nullptr; }
    [[nodiscard]] std::span<const sir::FrameVar> frame() const noexcept {
        return frame_ != nullptr ? std::span<const sir::FrameVar>(*frame_)
                                 : std::span<const sir::FrameVar>{};
    }
    [[nodiscard]] std::span<const sir::ActivationScope> scopes() const noexcept {
        return info_ != nullptr ? std::span<const sir::ActivationScope>(info_->scopes)
                                : std::span<const sir::ActivationScope>{};
    }

private:
    friend class ActivationEnvironment;
    friend class ScopedReferences;
    explicit ActivationPlan(std::pmr::memory_resource* memory) : layouts_(memory) {}
    kernel::ActivationStore* store_ = nullptr;
    const std::vector<sir::FrameVar>* frame_ = nullptr;
    const sir::ActivationInfo* info_ = nullptr;
    std::pmr::vector<kernel::ActivationLayoutId> layouts_;
};

/// Immutable origin of a promoted value. Captures preserve the declaring
/// plan/row too, so a ref never consults a returned owner's current frame.
class ActivationOrigin {
public:
    ActivationOrigin() = default;
    [[nodiscard]] bool valid() const noexcept { return plan_ != nullptr; }
    [[nodiscard]] kernel::ActivationBinding binding() const noexcept { return binding_; }
    [[nodiscard]] const ActivationPlan& plan() const noexcept { return *plan_; }
    [[nodiscard]] sir::FrameVarId row() const noexcept { return row_; }

private:
    friend class ActivationEnvironment;
    ActivationOrigin(kernel::ActivationBinding binding, const ActivationPlan& plan,
                     sir::FrameVarId row)
        : binding_(binding), plan_(&plan), row_(row) {}
    kernel::ActivationBinding binding_;
    const ActivationPlan* plan_ = nullptr;
    sir::FrameVarId row_ = sir::kNone;
};

/// A copied reference binding, never a copy of its referred-to value.
/// The interpreter resolves and validates the actual origin, effective
/// rights and lifetime in its private encoding. No borrowed ref gains a lease.
struct BorrowedReference {
    kernel::W4 token{};
    bool readonly = false;
    bool staticActual = false;
};
enum class CaptureJoin : std::uint8_t { All, Any, None };
using ResolveBorrowedReference =
    kernel::ActivationResult<BorrowedReference> (*)(void*, sir::FrameVarId) noexcept;

class ActivationEnvironment;
struct ActivationEnvironmentDelete {
    std::pmr::memory_resource* memory = std::pmr::new_delete_resource();
    void operator()(ActivationEnvironment* environment) const noexcept;
};

/// One executing body's automatic owners and imported lexical environment.
/// No PC, control stack, RNG or mutable SystemVerilog value lives here.
/// Creation does not initialize source declarations or formal parameters.
class ActivationEnvironment {
public:
    using Error = kernel::ActivationError;
    template <class T> using Result = kernel::ActivationResult<T>;
    using Pointer = std::unique_ptr<ActivationEnvironment, ActivationEnvironmentDelete>;
    using Children = std::pmr::vector<Pointer>;
    using Pins = std::pmr::vector<kernel::ActivationLease>;

    ActivationEnvironment(const ActivationEnvironment&) = delete;
    ActivationEnvironment& operator=(const ActivationEnvironment&) = delete;
    ~ActivationEnvironment();

    static Result<Pointer>
    root(kernel::ActivationStore& store, const ActivationPlan& plan,
         std::pmr::memory_resource* memory = std::pmr::new_delete_resource()) noexcept;
    /// All child environments are prepared before the caller starts any
    /// child or advances the parent's random stream. Failure destroys every
    /// provisional root/import and leaves the source environment unchanged.
    static Result<Children>
    children(kernel::ActivationStore& store, const ActivationPlan& parentPlan,
             const ActivationEnvironment* parent, std::span<const ActivationPlan* const> plans,
             CaptureJoin join, ResolveBorrowedReference resolve = nullptr,
             void* referenceFrame = nullptr,
             std::pmr::memory_resource* memory = std::pmr::new_delete_resource()) noexcept;

    Error enter(sir::ActivationScopeId scope) noexcept;
    Error leave(sir::ActivationScopeId scope) noexcept;
    /// Named control exits restore their saved depth. Return/kill uses
    /// finish(), after waits and callbacks have been detached by the caller.
    Error unwind(std::size_t depth) noexcept;
    void finish() noexcept;
    [[nodiscard]] Result<kernel::ActivationBinding> binding(sir::FrameVarId field) const noexcept;
    [[nodiscard]] Result<ActivationOrigin> origin(sir::FrameVarId field) const noexcept;
    [[nodiscard]] Result<BorrowedReference> reference(sir::FrameVarId field) const noexcept;
    /// Temporary callback ownership includes unread and empty extents too.
    [[nodiscard]] Result<Pins> pin() const noexcept;
    [[nodiscard]] std::span<const sir::ActivationScopeId> active() const noexcept {
        return std::span(active_).first(activeDepth_);
    }
    [[nodiscard]] std::size_t importedExtents() const noexcept { return imports_.size(); }
    void visitReferences(void (*visitor)(void*, BorrowedReference) noexcept,
                         void* frame) const noexcept;

private:
    struct Entry {
        ActivationOrigin origin;
        std::optional<BorrowedReference> reference;
    };
    ActivationEnvironment(kernel::ActivationStore& store, const ActivationPlan& plan,
                          std::pmr::memory_resource* memory);
    static Result<Pointer> allocate(kernel::ActivationStore& store, const ActivationPlan& plan,
                                    std::pmr::memory_resource* memory);
    Error enterOwner(sir::ActivationScopeId scope) noexcept;
    void unwindOwners(std::size_t depth) noexcept;
    Error import(const ActivationPlan& parentPlan, const ActivationEnvironment* parent,
                 CaptureJoin join, ResolveBorrowedReference resolve, void* referenceFrame);
    Error retain(Pins& pins, const kernel::ActivationLease& source) const noexcept;
    kernel::ActivationStore& store_;
    const ActivationPlan& plan_;
    std::pmr::memory_resource* memory_;
    std::pmr::vector<kernel::ActivationLease> owners_;
    Pins imports_;
    std::pmr::vector<sir::ActivationScopeId> active_;
    std::size_t activeDepth_ = 0;
    std::pmr::vector<Entry> entries_;
    bool finished_ = false;
};

}  // namespace kirimasim::vm::detail
