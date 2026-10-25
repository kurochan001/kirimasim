// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Shared automatic activations (RUNTIME_ABI 6; automatic-capture contract).
// C++20. Layouts and typed bindings contain no front-end or tier types.
#pragma once

#include <cstdint>
#include <memory_resource>
#include <span>
#include <vector>

#include "kirimasim/kernel/heap.h"
#include "kirimasim/kernel/id.h"
#include "kirimasim/kernel/managed_event.h"
#include "kirimasim/kernel/w4.h"

namespace kirimasim::kernel {

struct ActivationLayoutTag;
using ActivationLayoutId = Id<ActivationLayoutTag>;
struct ActivationToken {
    std::uint64_t bits = 0;
    friend bool operator==(ActivationToken, ActivationToken) = default;
};
enum class ActivationKind : std::uint8_t {
    Two,
    Four,
    Real,
    ShortReal,
    String,
    Chandle,
    Container,
    Cell,
    Event
};
struct ActivationDim {
    std::int64_t left = 0, right = 0;
};
struct ActivationField {
    ActivationKind kind = ActivationKind::Four;
    std::uint32_t width = 1;
    std::vector<ActivationDim> dims;
};
enum class ActivationError : std::uint8_t {
    None,
    InvalidLayout,
    InvalidBinding,
    OutOfRange,
    ReadOnly,
    InvalidValue,
    ResourceLimit
};
template <class T> struct ActivationResult {
    T value{};
    ActivationError error = ActivationError::None;
    explicit operator bool() const { return error == ActivationError::None; }
};
class ActivationStore;

/// A C++ owner of one lexical extent, not a language process. Leases must
/// not outlive their Context; reset invalidates earlier bindings and leases.
class ActivationLease {
public:
    ActivationLease() noexcept = default;
    ActivationLease(const ActivationLease&) = delete;
    ActivationLease& operator=(const ActivationLease&) = delete;
    ActivationLease(ActivationLease&& other) noexcept;
    ActivationLease& operator=(ActivationLease&& other) noexcept;
    ~ActivationLease();
    void reset() noexcept;
    [[nodiscard]] ActivationToken token() const noexcept { return token_; }
    explicit operator bool() const noexcept;

private:
    friend class ActivationStore;
    ActivationLease(ActivationStore* store, ActivationToken token, bool readonly = false) noexcept
        : store_(store), token_(token), readonly_(readonly) {}
    ActivationStore* store_ = nullptr;
    ActivationToken token_;
    bool readonly_ = false;
};

/// A nonowning typed view. Creating or copying a binding acquires no lease.
/// Keep an extent lease while source code, a child or a callback uses it.
/// The C++ store identity prevents cross-Context bindings; source values
/// carry opaque numbers, never this implementation pointer.
class ActivationBinding {
public:
    ActivationBinding() noexcept = default;
    [[nodiscard]] ActivationToken token() const noexcept { return token_; }
    [[nodiscard]] std::uint32_t field() const noexcept { return field_; }
    [[nodiscard]] bool readonly() const noexcept { return readonly_; }

private:
    friend class ActivationStore;
    const ActivationStore* store_ = nullptr;
    ActivationToken token_;
    std::uint32_t field_ = 0;
    bool readonly_ = false;
};
struct ActivationAccess {
    ActivationToken token;
    std::uint32_t field = 0, element = 0, lsb = 0, width = 0;
};
/// A statically resolved sensitivity range, including unexecuted branches.
struct ActivationPart {
    ActivationBinding binding;
    std::uint32_t firstElem = 0, numElems = 1, lsb = 0, width = 0;
};
struct ActivationRegion {
    ActivationToken token;
    std::uint32_t field = 0, firstElem = 0, numElems = 1, lsb = 0, width = 0;
    friend bool operator==(const ActivationRegion&, const ActivationRegion&) = default;
};
/// Observers may inspect but must not mutate or reset this store. They
/// must contain their own allocation failures and let no exception escape.
/// Changes arrive after complete normalization; source callbacks run later.
struct ActivationHook {
    void (*fn)(void*, const ActivationAccess&) noexcept = nullptr;
    void* frame = nullptr;
};
struct ActivationStats {
    std::uint64_t live = 0, leases = 0, words = 0, entries = 0;
};

class ActivationStore {
public:
    explicit ActivationStore(std::pmr::memory_resource* memory = std::pmr::new_delete_resource());
    ActivationStore(const ActivationStore&) = delete;
    ActivationStore& operator=(const ActivationStore&) = delete;
    ActivationResult<ActivationLayoutId>
    addLayout(std::span<const ActivationField> fields) noexcept;
    /// Safe internal zero contents are not source-language initialization;
    /// front emits each declaration default/initializer once on scope entry.
    ActivationResult<ActivationLease> create(ActivationLayoutId layout) noexcept;
    ActivationResult<ActivationLease> retain(const ActivationLease& lease) noexcept;
    ActivationResult<ActivationLease> retain(const ActivationBinding& binding) noexcept;
    ActivationResult<ActivationBinding> bind(const ActivationLease& lease, std::uint32_t field,
                                             bool readonly = false) const noexcept;
    ActivationResult<ActivationBinding> readOnly(const ActivationBinding& binding) const noexcept;
    /// Validate identity and bounds without reading values or firing hooks.
    ActivationResult<ActivationRegion> region(const ActivationPart& part) const noexcept;
    /// Caller-owned W4 chunks, low word first. Width 0 means the whole
    /// field element. Invalid operations change neither values nor buffers.
    ActivationError read(const ActivationBinding& binding, std::uint32_t element,
                         std::span<W4> value, std::uint32_t lsb = 0,
                         std::uint32_t width = 0) const noexcept;
    ActivationError write(const ActivationBinding& binding, std::uint32_t element,
                          std::span<const W4> value, std::uint32_t lsb = 0,
                          std::uint32_t width = 0) noexcept;
    ActivationHook setReadHook(ActivationHook hook) noexcept;
    ActivationHook setChangeHook(ActivationHook hook) noexcept;
    /// Enumerate only fields explicitly typed as container/cell roots;
    /// ordinary integers, strings and real bits never become heap roots.
    void visitRoots(void (*visitor)(void*, HeapRoot) noexcept, void* frame) const noexcept;
    [[nodiscard]] bool rootsChanged() const noexcept { return rootsChanged_; }
    void acknowledgeRoots() noexcept { rootsChanged_ = false; }
    /// Trace all live physical event fields, including unread retained arrays.
    /// Null and legacy static event handles need no managed-object root.
    ManagedEventError visitEventRoots(ManagedEventRoots& roots) const noexcept;
    [[nodiscard]] ManagedEventRootProvider eventRootProvider() const noexcept;
    [[nodiscard]] bool eventRootsChanged() const noexcept { return eventRootsChanged_; }
    void acknowledgeEventRoots() noexcept { eventRootsChanged_ = false; }
    [[nodiscard]] ActivationStats stats() const noexcept;
    /// Invalidate all bindings without resetting generations or layouts.
    /// Detach observations before resetting a running Context.
    void clear() noexcept;

private:
    friend class ActivationLease;
    struct Field {
        ActivationKind kind = ActivationKind::Four;
        std::uint32_t width = 1, elements = 1, chunks = 1;
        std::uint64_t offset = 0;
        std::pmr::vector<ActivationDim> dims;
        explicit Field(std::pmr::memory_resource* memory) : dims(memory) {}
    };
    struct Layout {
        std::pmr::vector<Field> fields;
        std::uint64_t words = 0;
        explicit Layout(std::pmr::memory_resource* memory) : fields(memory) {}
    };
    struct Record {
        ActivationLayoutId layout = kNone;
        std::uint32_t generation = 1, next = kNone;
        std::uint64_t leases = 0;
        std::pmr::vector<std::uint64_t> values;
        explicit Record(std::pmr::memory_resource* memory) : values(memory) {}
    };
    Record* find(ActivationToken token) noexcept;
    const Record* find(ActivationToken token) const noexcept;
    const Field* fieldOf(const ActivationBinding& binding) const noexcept;
    ActivationResult<ActivationLease> retainToken(ActivationToken token, bool readonly) noexcept;
    void release(ActivationToken token) noexcept;
    void reclaim(std::uint32_t index) noexcept;
    std::pmr::memory_resource* memory_;
    std::pmr::vector<Layout> layouts_;
    std::pmr::vector<Record> records_;
    std::uint32_t free_ = kNone;
    std::uint64_t live_ = 0, leases_ = 0, words_ = 0;
    bool rootsChanged_ = false, eventRootsChanged_ = false;
    ActivationHook readHook_, changeHook_;
};

}  // namespace kirimasim::kernel
