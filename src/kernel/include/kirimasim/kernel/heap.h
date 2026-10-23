// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Context-owned typed containers and stable cells (RUNTIME_ABI.md 6).
// Tokens are local to one Context; they are never addresses into vectors.
// C++20, including the interface used by generated code.
#pragma once

#include <cstdint>
#include <memory_resource>
#include <optional>
#include <span>
#include <vector>

#include "kirimasim/kernel/id.h"
#include "kirimasim/kernel/w4.h"

namespace kirimasim::kernel {

struct HeapLayoutTag;
using HeapLayoutId = Id<HeapLayoutTag>;
struct HeapTypeTag;
using HeapTypeId = Id<HeapTypeTag>;

template <class Tag> struct HeapToken {
    std::uint64_t bits = 0;
    friend bool operator==(HeapToken, HeapToken) = default;
};
struct ContainerTokenTag;
using ContainerToken = HeapToken<ContainerTokenTag>;
struct CellTokenTag;
using CellToken = HeapToken<CellTokenTag>;

enum class ContainerKind : std::uint8_t { DynamicArray, Queue };
enum class HeapError : std::uint8_t {
    None,
    InvalidType,
    InvalidHandle,
    InvalidSize,
    ResourceLimit
};
enum class HeapWarning : std::uint8_t { None, InvalidIndex, EmptyPop, BoundExceeded };

struct HeapStatus {
    HeapError error = HeapError::None;
    HeapWarning warning = HeapWarning::None;
    explicit operator bool() const { return error == HeapError::None; }
};
template <class T> struct HeapResult {
    T value{};
    HeapStatus status{};
    explicit operator bool() const { return static_cast<bool>(status); }
};

/// One word of a flattened element. Nested containers name a previously
/// registered container type; other handles (strings, for example) are
/// immutable values or opaque handles, not tracing edges of this heap.
struct HeapWord {
    std::uint64_t mask = ~std::uint64_t{0};
    bool fourState = false;
    W4 initial{0, 0};
    HeapTypeId nested = kNone;
};
struct HeapType {
    ContainerKind kind = ContainerKind::DynamicArray;
    HeapLayoutId element;
    std::uint64_t maxElements = 0;  ///< Queue capacity; 0: unbounded. Dynamic arrays: 0.
};
struct HeapRoot {
    enum class Kind : std::uint8_t { Container, Cell };
    Kind kind = Kind::Container;
    std::uint64_t bits = 0;
    static HeapRoot container(ContainerToken t) { return {Kind::Container, t.bits}; }
    static HeapRoot cell(CellToken t) { return {Kind::Cell, t.bits}; }
};
struct HeapStats {
    std::uint64_t containers = 0;
    std::uint64_t cells = 0;
    std::uint64_t containerEntries = 0;
    std::uint64_t cellEntries = 0;
};

class Heap {
public:
    /// The optional allocation source also permits deterministic allocation
    /// failure checks. It must outlive this heap. No allocation at construction.
    explicit Heap(std::pmr::memory_resource* memory = std::pmr::new_delete_resource());
    Heap(const Heap&) = delete;
    Heap& operator=(const Heap&) = delete;

    HeapResult<HeapLayoutId> addLayout(std::span<const HeapWord> words) noexcept;
    HeapResult<HeapTypeId> addType(HeapType type) noexcept;
    [[nodiscard]] std::size_t words(HeapLayoutId layout) const noexcept;

    /// A missing (X/Z) or negative construction size gives InvalidSize.
    /// Construction and whole-value copying always make independent cells,
    /// even on same-size resize and self-assignment. On failure, no token is
    /// published and all newly allocated objects are rolled back.
    /// Equivalent nested queue types may differ in capacity. Each level uses
    /// its destination capacity; any retained value that is truncated reports
    /// BoundExceeded. Source-language type checking remains the tier's job.
    HeapResult<ContainerToken> construct(HeapTypeId type, std::optional<std::int64_t> size,
                                         ContainerToken initializer = {}) noexcept;
    HeapResult<ContainerToken> copy(HeapTypeId destination, ContainerToken source) noexcept;
    /// Capture the actual source type and bounds, without a destination-type
    /// conversion. Used before later expression side effects can replace it.
    HeapResult<ContainerToken> snapshot(ContainerToken source) noexcept;
    HeapResult<ContainerToken> fromValues(HeapTypeId type, std::span<const W4> values) noexcept;
    HeapResult<std::uint64_t> size(HeapTypeId type, ContainerToken value) const noexcept;

    /// Reads expose values, never mutable storage. A nested token read here
    /// is borrowed: copy it before placing it in another mutable variable.
    /// Unknown/out-of-range indices read the declared default and writes do
    /// nothing. A queue write at its current size appends (7.4.5).
    HeapStatus read(HeapTypeId type, ContainerToken value, std::optional<std::int64_t> index,
                    std::span<W4> out) const noexcept;
    HeapStatus write(HeapTypeId type, ContainerToken& value, std::optional<std::int64_t> index,
                     std::span<const W4> data) noexcept;
    HeapResult<CellToken> cell(HeapTypeId type, ContainerToken value,
                               std::optional<std::int64_t> index) const noexcept;
    HeapStatus readCell(HeapLayoutId layout, CellToken cell, std::span<W4> out) const noexcept;
    HeapStatus writeCell(HeapLayoutId layout, CellToken cell, std::span<const W4> data) noexcept;

    /// Queue movement preserves retained cell tokens. Removed cells remain
    /// usable through rooted references until an exact collection frees them.
    HeapStatus insert(HeapTypeId type, ContainerToken& value, std::optional<std::int64_t> index,
                      std::span<const W4> data) noexcept;
    HeapStatus push(HeapTypeId type, ContainerToken& value, bool front,
                    std::span<const W4> data) noexcept;
    HeapStatus pop(HeapTypeId type, ContainerToken& value, bool front, std::span<W4> out) noexcept;
    HeapStatus erase(HeapTypeId type, ContainerToken& value,
                     std::optional<std::int64_t> index) noexcept;
    HeapStatus clear(HeapTypeId type, ContainerToken& value) noexcept;

    /// Caller supplies ALL roots at a permitted safe point, outside DPI.
    /// Marking failure leaves all objects alive. Tokens include generations;
    /// reclaimed entries can be reused without making old tokens valid again.
    HeapStatus collect(std::span<const HeapRoot> roots) noexcept;
    [[nodiscard]] HeapStats stats() const noexcept;

private:
    struct Layout {
        std::pmr::vector<HeapWord> words;
        HeapLayoutId equivalent;  ///< First layout of this shape, ignoring nested queue bounds.
    };
    struct Cell {
        HeapLayoutId layout;
        std::pmr::vector<W4> words;
    };
    struct Container {
        HeapTypeId type;
        std::pmr::vector<CellToken> cells;
    };
    template <class T> struct Entry {
        std::optional<T> value;
        std::uint32_t generation = 1;
        bool marked = false;
    };
    template <class T> struct Pool {
        explicit Pool(std::pmr::memory_resource* memory) : entries(memory), free(memory) {}
        std::pmr::vector<Entry<T>> entries;
        std::pmr::vector<std::uint32_t> free;
        std::size_t freeCount = 0;
        T* get(std::uint64_t token) noexcept;
        const T* get(std::uint64_t token) const noexcept;
        std::uint64_t add(T value, std::pmr::vector<std::uint32_t>& allocated);
        void release(std::uint32_t index) noexcept;
    };
    struct Transaction {
        explicit Transaction(Heap& heap);
        ~Transaction();
        Heap& heap;
        std::pmr::vector<std::uint32_t> containers;
        std::pmr::vector<std::uint32_t> cells;
        bool committed = false;
        bool truncated = false;
        [[nodiscard]] HeapWarning warning() const noexcept {
            return truncated ? HeapWarning::BoundExceeded : HeapWarning::None;
        }
    };

    std::pmr::memory_resource* memory_;
    std::pmr::vector<Layout> layouts_;
    std::pmr::vector<HeapType> types_;
    Pool<Container> containers_;
    Pool<Cell> cells_;

    const HeapType* typeOf(HeapTypeId id) const noexcept;
    const Layout* layoutOf(HeapLayoutId id) const noexcept;
    bool equivalentLayouts(HeapLayoutId a, HeapLayoutId b) const noexcept;
    bool equivalentTypes(HeapTypeId a, HeapTypeId b) const noexcept;
    HeapError check(HeapTypeId type, ContainerToken value) const noexcept;
    HeapError checkData(HeapLayoutId layout, std::span<const W4> data) const noexcept;
    CellToken cloneCell(Transaction& tx, HeapLayoutId layout, std::span<const W4> data);
    ContainerToken clone(Transaction& tx, HeapTypeId type, ContainerToken source,
                         std::uint64_t count);
    std::pmr::vector<W4> cloneWords(Transaction& tx, HeapLayoutId layout, std::span<const W4> data);
    void defaults(HeapLayoutId layout, std::span<W4> out) const noexcept;
};

}  // namespace kirimasim::kernel
