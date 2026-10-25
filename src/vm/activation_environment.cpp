// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "activation_environment.h"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <utility>

namespace kirimasim::vm::detail {
namespace {
using Error = kernel::ActivationError;
template <class T> using Result = kernel::ActivationResult<T>;

Result<kernel::ActivationField> fieldOf(const sir::FrameVar& row) {
    kernel::ActivationField field;
    if (sir::isIntegral(row.type)) {
        field.kind =
            sir::isFourState(row.type) ? kernel::ActivationKind::Four : kernel::ActivationKind::Two;
        field.width = row.type.width;
    } else {
        field.width = row.type.kind == sir::TypeKind::ShortReal ? 32 : 64;
        switch (row.type.kind) {
            case sir::TypeKind::Event:
                if (row.type != sir::Type{sir::TypeKind::Event, 0})
                    return {{}, Error::InvalidLayout};
                field.kind = kernel::ActivationKind::Event;
                break;
            case sir::TypeKind::Real: field.kind = kernel::ActivationKind::Real; break;
            case sir::TypeKind::ShortReal: field.kind = kernel::ActivationKind::ShortReal; break;
            case sir::TypeKind::String: field.kind = kernel::ActivationKind::String; break;
            case sir::TypeKind::Chandle: field.kind = kernel::ActivationKind::Chandle; break;
            case sir::TypeKind::DynArray:
            case sir::TypeKind::Queue: field.kind = kernel::ActivationKind::Container; break;
            default: return {{}, Error::InvalidLayout};
        }
    }
    for (const auto& dim : row.dims)
        field.dims.push_back({dim.left, dim.right});
    return {std::move(field), Error::None};
}

bool sameShape(const sir::FrameVar& a, const sir::FrameVar& b) {
    return a.type == b.type && a.target == b.target && a.declType == b.declType &&
           a.refStatic == b.refStatic && a.dims.size() == b.dims.size() &&
           std::equal(
               a.dims.begin(), a.dims.end(), b.dims.begin(),
               [](const auto& x, const auto& y) { return x.left == y.left && x.right == y.right; });
}
}  // namespace

ActivationPlan::ActivationPlan(ActivationPlan&& other) noexcept
    : store_(std::exchange(other.store_, nullptr)), frame_(std::exchange(other.frame_, nullptr)),
      info_(std::exchange(other.info_, nullptr)), layouts_(std::move(other.layouts_)) {}

ActivationPlan::Result<ActivationPlan>
ActivationPlan::compile(kernel::ActivationStore& store, const std::vector<sir::FrameVar>& frame,
                        const sir::ActivationInfo& info,
                        std::pmr::memory_resource* memory) noexcept {
    try {
        if (frame.size() >= sir::kNone || info.scopes.size() >= sir::kNone)
            return {{}, Error::InvalidLayout};
        ActivationPlan plan(memory);
        // Verify the whole local graph before registering any kernel layouts.
        std::pmr::vector<unsigned char> colors(info.scopes.size(), 0, memory);
        std::pmr::vector<sir::ActivationScopeId> path(memory);
        std::pmr::vector<unsigned> membership(frame.size(), 0, memory);
        for (std::size_t i = 0; i < info.scopes.size(); ++i) {
            const auto& scope = info.scopes[i];
            if (scope.parent != sir::kNone && scope.parent.index() >= info.scopes.size())
                return {{}, Error::InvalidLayout};
            for (const auto id : scope.fields) {
                if (id.index() >= frame.size()) return {{}, Error::InvalidLayout};
                const auto& row = frame[id.index()];
                if (row.home != sir::FrameHome::Scoped || row.activationScope.index() != i ||
                    membership[id.index()]++ != 0 || !fieldOf(row))
                    return {{}, Error::InvalidLayout};
            }
            path.clear();
            auto at = sir::ActivationScopeId{static_cast<std::uint32_t>(i)};
            while (at != sir::kNone && colors[at.index()] == 0) {
                colors[at.index()] = 1;
                path.push_back(at);
                at = info.scopes[at.index()].parent;
                if (at != sir::kNone && at.index() >= info.scopes.size())
                    return {{}, Error::InvalidLayout};
            }
            if (at != sir::kNone && colors[at.index()] == 1) return {{}, Error::InvalidLayout};
            for (const auto id : path)
                colors[id.index()] = 2;
        }
        if (info.root != sir::kNone && (info.root.index() >= info.scopes.size() ||
                                        info.scopes[info.root.index()].parent != sir::kNone))
            return {{}, Error::InvalidLayout};
        for (std::size_t i = 0; i < frame.size(); ++i) {
            const auto& row = frame[i];
            if (row.home != sir::FrameHome::Ordinary && row.home != sir::FrameHome::Scoped &&
                row.home != sir::FrameHome::Capture)
                return {{}, Error::InvalidLayout};
            if ((row.home == sir::FrameHome::Scoped) != (membership[i] == 1))
                return {{}, Error::InvalidLayout};
            if (row.home != sir::FrameHome::Scoped && row.activationScope != sir::kNone)
                return {{}, Error::InvalidLayout};
            if ((row.home == sir::FrameHome::Capture) != (row.captureSource != sir::kNone))
                return {{}, Error::InvalidLayout};
            if (row.refStatic && row.type.kind != sir::TypeKind::Ref)
                return {{}, Error::InvalidLayout};
            if ((row.home == sir::FrameHome::Scoped && (row.readonly || row.refStatic)) ||
                (row.home == sir::FrameHome::Ordinary && row.readonly &&
                 row.type.kind != sir::TypeKind::Ref))
                return {{}, Error::InvalidLayout};
        }
        plan.layouts_.reserve(info.scopes.size());
        for (const auto& scope : info.scopes) {
            std::pmr::vector<kernel::ActivationField> fields(memory);
            fields.reserve(scope.fields.size());
            for (const auto id : scope.fields)
                fields.push_back(std::move(fieldOf(frame[id.index()]).value));
            const auto layout = store.addLayout(fields);
            if (!layout) return {{}, layout.error};
            plan.layouts_.push_back(layout.value);
        }
        plan.store_ = &store;
        plan.frame_ = &frame;
        plan.info_ = &info;
        return {std::move(plan), Error::None};
    } catch (const std::bad_alloc&) {
        return {{}, Error::ResourceLimit};
    } catch (const std::length_error&) {
        return {{}, Error::ResourceLimit};
    }
}

ActivationEnvironment::ActivationEnvironment(kernel::ActivationStore& store,
                                             const ActivationPlan& plan,
                                             std::pmr::memory_resource* memory)
    : store_(store), plan_(plan), memory_(memory), owners_(memory), imports_(memory),
      active_(memory), entries_(memory) {
    owners_.resize(plan.info_->scopes.size());
    active_.resize(owners_.size());
    entries_.resize(plan.frame_->size());
}
ActivationEnvironment::~ActivationEnvironment() { finish(); }

void ActivationEnvironmentDelete::operator()(ActivationEnvironment* environment) const noexcept {
    if (environment == nullptr) return;
    environment->~ActivationEnvironment();
    memory->deallocate(environment, sizeof(ActivationEnvironment), alignof(ActivationEnvironment));
}

ActivationEnvironment::Result<ActivationEnvironment::Pointer>
ActivationEnvironment::allocate(kernel::ActivationStore& store, const ActivationPlan& plan,
                                std::pmr::memory_resource* memory) {
    if (!plan.valid() || plan.store_ != &store) return {{}, Error::InvalidLayout};
    void* storage = memory->allocate(sizeof(ActivationEnvironment), alignof(ActivationEnvironment));
    ActivationEnvironment* created = nullptr;
    try {
        created = new (storage) ActivationEnvironment(store, plan, memory);
    } catch (...) {
        memory->deallocate(storage, sizeof(ActivationEnvironment), alignof(ActivationEnvironment));
        throw;
    }
    Pointer environment(created, ActivationEnvironmentDelete{memory});
    if (plan.info_->root != sir::kNone) {
        const auto error = environment->enterOwner(plan.info_->root);
        if (error != Error::None) return {{}, error};
    }
    return {std::move(environment), Error::None};
}

ActivationEnvironment::Result<ActivationEnvironment::Pointer>
ActivationEnvironment::root(kernel::ActivationStore& store, const ActivationPlan& plan,
                            std::pmr::memory_resource* memory) noexcept {
    if (!plan.valid() || !plan.info_->extents.empty() || plan.info_->inheritCaptures ||
        std::any_of(plan.frame().begin(), plan.frame().end(),
                    [](const auto& row) { return row.home == sir::FrameHome::Capture; }))
        return {{}, Error::InvalidLayout};
    try {
        return allocate(store, plan, memory);
    } catch (const std::bad_alloc&) {
        return {{}, Error::ResourceLimit};
    } catch (const std::length_error&) {
        return {{}, Error::ResourceLimit};
    }
}

ActivationEnvironment::Error
ActivationEnvironment::enterOwner(sir::ActivationScopeId scope) noexcept {
    if (finished_ || scope.index() >= owners_.size() || owners_[scope.index()] ||
        activeDepth_ >= active_.size())
        return Error::InvalidBinding;
    const auto parent =
        activeDepth_ == 0 ? sir::ActivationScopeId{sir::kNone} : active_[activeDepth_ - 1];
    if (plan_.info_->scopes[scope.index()].parent != parent) return Error::InvalidBinding;
    auto owner = store_.create(plan_.layouts_[scope.index()]);
    if (!owner) return owner.error;
    const auto& fields = plan_.info_->scopes[scope.index()].fields;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto field = fields[i];
        const auto binding = store_.bind(owner.value, static_cast<std::uint32_t>(i),
                                         (*plan_.frame_)[field.index()].readonly);
        if (!binding) {
            for (const auto id : fields)
                entries_[id.index()] = {};
            return binding.error;
        }
        entries_[field.index()].origin = ActivationOrigin(binding.value, plan_, field);
    }
    owners_[scope.index()] = std::move(owner.value);
    active_[activeDepth_++] = scope;  // Sized for every local scope at construction.
    return Error::None;
}
ActivationEnvironment::Error ActivationEnvironment::enter(sir::ActivationScopeId scope) noexcept {
    return scope == plan_.info_->root ? Error::InvalidBinding : enterOwner(scope);
}
ActivationEnvironment::Error ActivationEnvironment::leave(sir::ActivationScopeId scope) noexcept {
    if (finished_ || scope == plan_.info_->root || activeDepth_ == 0 ||
        active_[activeDepth_ - 1] != scope)
        return Error::InvalidBinding;
    return unwind(activeDepth_ - 1);
}
ActivationEnvironment::Error ActivationEnvironment::unwind(std::size_t depth) noexcept {
    if (finished_ || depth > activeDepth_ || (plan_.info_->root != sir::kNone && depth == 0))
        return Error::InvalidBinding;
    unwindOwners(depth);
    return Error::None;
}
void ActivationEnvironment::unwindOwners(std::size_t depth) noexcept {
    while (activeDepth_ > depth) {
        const auto scope = active_[--activeDepth_];
        for (const auto id : plan_.info_->scopes[scope.index()].fields)
            entries_[id.index()] = {};
        owners_[scope.index()].reset();
    }
}
void ActivationEnvironment::finish() noexcept {
    if (finished_) return;
    unwindOwners(0);
    entries_.clear();
    imports_.clear();
    finished_ = true;
}

ActivationEnvironment::Result<kernel::ActivationBinding>
ActivationEnvironment::binding(sir::FrameVarId field) const noexcept {
    const auto value = origin(field);
    return value ? Result<kernel::ActivationBinding>{value.value.binding(), Error::None}
                 : Result<kernel::ActivationBinding>{{}, value.error};
}
ActivationEnvironment::Result<ActivationOrigin>
ActivationEnvironment::origin(sir::FrameVarId field) const noexcept {
    if (finished_ || field.index() >= entries_.size()) return {{}, Error::InvalidBinding};
    const auto value = entries_[field.index()].origin;
    if (!value.valid()) return {{}, Error::InvalidBinding};
    const auto region = store_.region({value.binding()});
    return region ? Result<ActivationOrigin>{value, Error::None}
                  : Result<ActivationOrigin>{{}, region.error};
}
ActivationEnvironment::Result<BorrowedReference>
ActivationEnvironment::reference(sir::FrameVarId field) const noexcept {
    if (finished_ || field.index() >= entries_.size()) return {{}, Error::InvalidBinding};
    const auto& reference = entries_[field.index()].reference;
    if (!reference) return {{}, Error::InvalidBinding};
    return {*reference, Error::None};
}

ActivationEnvironment::Error
ActivationEnvironment::retain(Pins& pins, const kernel::ActivationLease& source) const noexcept {
    // Every local scope creates a new token. Imported extents are distinct
    // from those new tokens and are copied once per source extent, not per
    // captured field. These lists are therefore disjoint by construction.
    auto pin = store_.retain(source);
    if (!pin) return pin.error;
    try {
        pins.push_back(std::move(pin.value));
        return Error::None;
    } catch (const std::bad_alloc&) {
        return Error::ResourceLimit;
    } catch (const std::length_error&) {
        return Error::ResourceLimit;
    }
}

ActivationEnvironment::Error ActivationEnvironment::import(const ActivationPlan& parentPlan,
                                                           const ActivationEnvironment* parent,
                                                           CaptureJoin join,
                                                           ResolveBorrowedReference resolve,
                                                           void* referenceFrame) {
    if (!parentPlan.valid() || parentPlan.store_ != &store_ ||
        (parent != nullptr && (&parent->plan_ != &parentPlan || parent->finished_)))
        return Error::InvalidBinding;
    const auto& info = *plan_.info_;
    const auto scopes =
        parent != nullptr ? parent->active() : std::span<const sir::ActivationScopeId>{};
    if (!std::equal(info.extents.begin(), info.extents.end(), scopes.begin(), scopes.end()))
        return Error::InvalidBinding;
    if ((!parentPlan.info_->extents.empty() || parentPlan.info_->inheritCaptures) &&
        !info.inheritCaptures)
        return Error::InvalidBinding;
    if (parent != nullptr) {
        for (const auto id : info.extents) {
            const auto error = retain(imports_, parent->owners_[id.index()]);
            if (error != Error::None) return error;
        }
        if (info.inheritCaptures)
            for (const auto& lease : parent->imports_) {
                const auto error = retain(imports_, lease);
                if (error != Error::None) return error;
            }
    }
    for (std::size_t i = 0; i < plan_.frame_->size(); ++i) {
        const auto& row = (*plan_.frame_)[i];
        if (row.home != sir::FrameHome::Capture) continue;
        if (row.captureSource.index() >= parentPlan.frame_->size()) return Error::InvalidBinding;
        const auto& source = (*parentPlan.frame_)[row.captureSource.index()];
        if (!sameShape(row, source) || (source.readonly && !row.readonly))
            return Error::InvalidBinding;
        if (source.home == sir::FrameHome::Capture && !info.inheritCaptures)
            return Error::InvalidBinding;
        if (row.type.kind == sir::TypeKind::Ref) {
            if (!source.refStatic && join != CaptureJoin::All) return Error::InvalidBinding;
            auto ref = source.home == sir::FrameHome::Capture && parent != nullptr
                           ? parent->reference(row.captureSource)
                       : source.home == sir::FrameHome::Ordinary && resolve != nullptr
                           ? resolve(referenceFrame, row.captureSource)
                           : Result<BorrowedReference>{{}, Error::InvalidBinding};
            if (!ref) return ref.error;
            if ((row.refStatic && !ref.value.staticActual) || (ref.value.readonly && !row.readonly))
                return Error::InvalidBinding;
            ref.value.readonly = ref.value.readonly || row.readonly;
            entries_[i].reference = ref.value;
        } else {
            if (source.home == sir::FrameHome::Ordinary || parent == nullptr)
                return Error::InvalidBinding;
            auto sourceOrigin = parent->origin(row.captureSource);
            if (!sourceOrigin) return sourceOrigin.error;
            auto value =
                Result<kernel::ActivationBinding>{sourceOrigin.value.binding(), Error::None};
            if (!value) return value.error;
            if (value.value.readonly() && !row.readonly) return Error::InvalidBinding;
            if (row.readonly) value = store_.readOnly(value.value);
            if (!value) return value.error;
            entries_[i].origin = sourceOrigin.value;
            entries_[i].origin.binding_ = value.value;
        }
    }
    return Error::None;
}

ActivationEnvironment::Result<ActivationEnvironment::Children>
ActivationEnvironment::children(kernel::ActivationStore& store, const ActivationPlan& parentPlan,
                                const ActivationEnvironment* parent,
                                std::span<const ActivationPlan* const> plans, CaptureJoin join,
                                ResolveBorrowedReference resolve, void* referenceFrame,
                                std::pmr::memory_resource* memory) noexcept {
    Children result(memory);
    try {
        if (!parentPlan.valid() || parentPlan.store_ != &store ||
            (parent != nullptr && (&parent->plan_ != &parentPlan || parent->finished_)) ||
            (join != CaptureJoin::All && join != CaptureJoin::Any && join != CaptureJoin::None))
            return {{}, Error::InvalidBinding};
        if (parent == nullptr &&
            (!parentPlan.info_->scopes.empty() || !parentPlan.info_->extents.empty() ||
             parentPlan.info_->inheritCaptures ||
             std::any_of(parentPlan.frame().begin(), parentPlan.frame().end(),
                         [](const auto& row) { return row.home == sir::FrameHome::Capture; })))
            return {{}, Error::InvalidBinding};
        result.reserve(plans.size());
        for (const auto* plan : plans) {
            if (plan == nullptr) return {{}, Error::InvalidLayout};
            auto child = allocate(store, *plan, memory);
            if (!child) return {{}, child.error};
            const auto error =
                child.value->import(parentPlan, parent, join, resolve, referenceFrame);
            if (error != Error::None) return {{}, error};
            result.push_back(std::move(child.value));
        }
        return {std::move(result), Error::None};
    } catch (const std::bad_alloc&) {
        return {{}, Error::ResourceLimit};
    } catch (const std::length_error&) {
        return {{}, Error::ResourceLimit};
    }
}

ActivationEnvironment::Result<ActivationEnvironment::Pins>
ActivationEnvironment::pin() const noexcept {
    if (finished_) return {{}, Error::InvalidBinding};
    Pins result(memory_);
    for (const auto id : active()) {
        const auto error = retain(result, owners_[id.index()]);
        if (error != Error::None) return {{}, error};
    }
    for (const auto& lease : imports_) {
        const auto error = retain(result, lease);
        if (error != Error::None) return {{}, error};
    }
    return {std::move(result), Error::None};
}
void ActivationEnvironment::visitReferences(void (*visitor)(void*, BorrowedReference) noexcept,
                                            void* frame) const noexcept {
    if (visitor == nullptr || finished_) return;
    for (const auto& entry : entries_)
        if (entry.reference) visitor(frame, *entry.reference);
}

}  // namespace kirimasim::vm::detail
