// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/heap.h"

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace kirimasim::kernel {
namespace {

struct HeapFault {
    HeapError error;
};
HeapStatus failure(HeapError error) { return {error, HeapWarning::None}; }

template <class T, class F> HeapResult<T> protectValue(F&& f) noexcept {
    try {
        return {f(), {}};
    } catch (const HeapFault& e) {
        return {{}, failure(e.error)};
    } catch (const std::bad_alloc&) {
        return {{}, failure(HeapError::ResourceLimit)};
    } catch (const std::length_error&) {
        return {{}, failure(HeapError::ResourceLimit)};
    }
}
template <class F> HeapStatus protect(F&& f) noexcept {
    try {
        return f();
    } catch (const HeapFault& e) {
        return failure(e.error);
    } catch (const std::bad_alloc&) {
        return failure(HeapError::ResourceLimit);
    } catch (const std::length_error&) {
        return failure(HeapError::ResourceLimit);
    }
}

std::optional<std::size_t> element(std::optional<std::int64_t> index, std::size_t count) {
    if (!index || *index < 0 || static_cast<std::uint64_t>(*index) >= count) return {};
    return static_cast<std::size_t>(*index);
}

}  // namespace

HeapHook Heap::setReadHook(HeapHook hook) noexcept { return std::exchange(readHook_, hook); }
HeapHook Heap::setChangeHook(HeapHook hook) noexcept { return std::exchange(changeHook_, hook); }
HeapHook Heap::setAllocationHook(HeapHook hook) noexcept {
    return std::exchange(allocationHook_, hook);
}
void Heap::recordRead(HeapRoot root) const noexcept {
    if (root.bits != 0 && readHook_.fn != nullptr) readHook_.fn(readHook_.frame, root);
}
void Heap::recordChange(HeapRoot root) const noexcept { recordChanges({&root, 1}); }
void Heap::recordChanges(std::span<const HeapRoot> roots) const noexcept {
    const bool previous = lastChange_;
    for (std::size_t i = 0; i < roots.size(); ++i) {
        lastChange_ = i + 1 == roots.size();
        if (roots[i].bits != 0 && changeHook_.fn != nullptr)
            changeHook_.fn(changeHook_.frame, roots[i]);
    }
    lastChange_ = previous;
}

template <class T> T* Heap::Pool<T>::get(std::uint64_t token) noexcept {
    const std::uint32_t low = static_cast<std::uint32_t>(token);
    if (low == 0 || low > entries.size()) return nullptr;
    Entry<T>& e = entries[low - 1];
    if (e.generation != token >> 32 || !e.value) return nullptr;
    return &*e.value;
}
template <class T> const T* Heap::Pool<T>::get(std::uint64_t token) const noexcept {
    const std::uint32_t low = static_cast<std::uint32_t>(token);
    if (low == 0 || low > entries.size()) return nullptr;
    const Entry<T>& e = entries[low - 1];
    if (e.generation != token >> 32 || !e.value) return nullptr;
    return &*e.value;
}
template <class T> std::uint64_t* Heap::Pool<T>::watcherCount(std::uint64_t token) noexcept {
    if (!get(token)) return nullptr;
    return &entries[static_cast<std::uint32_t>(token) - 1].watchers;
}
void Heap::watch(HeapRoot root, bool on) noexcept {
    auto* count = root.kind == HeapRoot::Kind::Container ? containers_.watcherCount(root.bits)
                                                         : cells_.watcherCount(root.bits);
    if (count != nullptr) {
        if (on)
            ++*count;
        else if (*count != 0)
            --*count;
    }
}
bool Heap::watched(HeapRoot root) noexcept {
    const auto* count = root.kind == HeapRoot::Kind::Container ? containers_.watcherCount(root.bits)
                                                               : cells_.watcherCount(root.bits);
    return count != nullptr && *count != 0;
}

template <class T>
std::uint64_t Heap::Pool<T>::add(T value, std::pmr::vector<std::uint32_t>& allocated) {
    std::uint32_t index = 0;
    if (freeCount == 0) {
        if (entries.size() >= std::numeric_limits<std::uint32_t>::max()) throw std::bad_alloc{};
        // Size the free-list storage BEFORE changing the pool, so
        // rollback and sweeping never need to allocate memory.
        if (free.size() < entries.size() + 1)
            free.resize(std::max(entries.size() + 1, free.size() * 2));
        index = static_cast<std::uint32_t>(entries.size());
        allocated.push_back(index);
        entries.emplace_back();
    } else {
        index = free[freeCount - 1];
        allocated.push_back(index);
        --freeCount;
    }
    Entry<T>& e = entries[index];
    e.value.emplace(std::move(value));
    return (std::uint64_t{e.generation} << 32) | (std::uint64_t{index} + 1);
}
template <class T> void Heap::Pool<T>::release(std::uint32_t index) noexcept {
    if (index >= entries.size() || !entries[index].value) return;
    Entry<T>& e = entries[index];
    e.value.reset();
    e.marked = false;
    e.watchers = 0;
    // Retire an entry rather than ever wrapping its generation.
    if (e.generation == std::numeric_limits<std::uint32_t>::max()) return;
    ++e.generation;
    free[freeCount++] = index;
}

Heap::Transaction::Transaction(Heap& h) : heap(h), containers(h.memory_), cells(h.memory_) {
    if (heap.collecting_) throw HeapFault{HeapError::InvalidType};
}
Heap::Transaction::~Transaction() {
    if (committed) {
        heap.changed_ = true;
        const std::uint64_t count = containers.size() + cells.size();
        heap.allocated_ =
            count > UINT64_MAX - heap.allocated_ ? UINT64_MAX : heap.allocated_ + count;
        if (heap.allocationHook_.fn != nullptr) {
            const auto announce = [&](const auto& pool, std::uint32_t index, HeapRoot::Kind kind) {
                const auto token = (std::uint64_t{pool.entries[index].generation} << 32) |
                                   (std::uint64_t{index} + 1);
                heap.allocationHook_.fn(heap.allocationHook_.frame, {kind, token});
            };
            for (const auto index : containers)
                announce(heap.containers_, index, HeapRoot::Kind::Container);
            for (const auto index : cells)
                announce(heap.cells_, index, HeapRoot::Kind::Cell);
        }
        return;
    }
    for (const std::uint32_t i : cells)
        heap.cells_.release(i);
    for (const std::uint32_t i : containers)
        heap.containers_.release(i);
}

Heap::Heap(std::pmr::memory_resource* memory)
    : memory_(memory), layouts_(memory), types_(memory), containers_(memory), cells_(memory) {}

const Heap::Layout* Heap::layoutOf(HeapLayoutId id) const noexcept {
    return id.index() < layouts_.size() ? &layouts_[id.index()] : nullptr;
}
const HeapType* Heap::typeOf(HeapTypeId id) const noexcept {
    return id.index() < types_.size() ? &types_[id.index()] : nullptr;
}
bool Heap::equivalentLayouts(HeapLayoutId a, HeapLayoutId b) const noexcept {
    const Layout* x = layoutOf(a);
    const Layout* y = layoutOf(b);
    return x && y && x->equivalent == y->equivalent;
}
bool Heap::equivalentTypes(HeapTypeId a, HeapTypeId b) const noexcept {
    const HeapType* x = typeOf(a);
    const HeapType* y = typeOf(b);
    return x && y && x->kind == y->kind && equivalentLayouts(x->element, y->element);
}
std::size_t Heap::words(HeapLayoutId layout) const noexcept {
    const Layout* l = layoutOf(layout);
    return l ? l->words.size() : 0;
}
HeapResult<HeapTypeId> Heap::actualType(ContainerToken value) const noexcept {
    if (value.bits == 0) return {kNone, {}};
    const Container* c = containers_.get(value.bits);
    if (!c) return {kNone, failure(HeapError::InvalidHandle)};
    return {c->type, {}};
}
HeapResult<HeapType> Heap::describeType(HeapTypeId type) const noexcept {
    const HeapType* t = typeOf(type);
    return t ? HeapResult<HeapType>{*t, {}}
             : HeapResult<HeapType>{{}, failure(HeapError::InvalidType)};
}
HeapResult<HeapWord> Heap::describeWord(HeapLayoutId layout, std::size_t word) const noexcept {
    const Layout* l = layoutOf(layout);
    return l && word < l->words.size() ? HeapResult<HeapWord>{l->words[word], {}}
                                       : HeapResult<HeapWord>{{}, failure(HeapError::InvalidType)};
}
HeapResult<HeapLayoutId> Heap::addLayout(std::span<const HeapWord> words) noexcept {
    return protectValue<HeapLayoutId>([&] {
        if (collecting_) throw HeapFault{HeapError::InvalidType};
        if (words.empty()) throw HeapFault{HeapError::InvalidType};
        for (const HeapWord& w : words) {
            if (w.string && (w.nested != kNone || w.mask != UINT64_MAX || w.fourState ||
                             w.initial.a != 0 || w.initial.b != 0))
                throw HeapFault{HeapError::InvalidType};
            if (w.mask == 0 || ((w.initial.a | w.initial.b) & ~w.mask) != 0 ||
                (!w.fourState && w.initial.b != 0))
                throw HeapFault{HeapError::InvalidType};
            if (w.nested != kNone && (!typeOf(w.nested) || w.mask != ~std::uint64_t{0} ||
                                      w.fourState || w.initial.a != 0 || w.initial.b != 0))
                throw HeapFault{HeapError::InvalidType};
        }
        if (layouts_.size() >= kNone) throw std::bad_alloc{};
        const HeapLayoutId id{static_cast<std::uint32_t>(layouts_.size())};
        HeapLayoutId equivalent = id;
        // Nested types must already exist, so their element shapes have
        // canonical IDs. Comparing a shape needs neither recursion nor a
        // runtime allocation, even for deep or branching container types.
        for (const Layout& old : layouts_) {
            if (old.words.size() != words.size()) continue;
            bool same = true;
            for (std::size_t i = 0; i < words.size(); ++i) {
                const HeapWord &a = old.words[i], &b = words[i];
                if (a.mask != b.mask || a.fourState != b.fourState || a.initial.a != b.initial.a ||
                    a.initial.b != b.initial.b || a.string != b.string ||
                    ((a.nested == kNone || b.nested == kNone)
                         ? a.nested != b.nested
                         : !equivalentTypes(a.nested, b.nested))) {
                    same = false;
                    break;
                }
            }
            if (same) {
                equivalent = old.equivalent;
                break;
            }
        }
        Layout l{std::pmr::vector<HeapWord>(words.begin(), words.end(), memory_), equivalent};
        layouts_.push_back(std::move(l));
        return id;
    });
}
HeapResult<HeapTypeId> Heap::addType(HeapType type) noexcept {
    return protectValue<HeapTypeId>([&] {
        if (collecting_) throw HeapFault{HeapError::InvalidType};
        if (!layoutOf(type.element) ||
            (type.kind != ContainerKind::DynamicArray && type.kind != ContainerKind::Queue) ||
            (type.kind == ContainerKind::DynamicArray && type.maxElements != 0))
            throw HeapFault{HeapError::InvalidType};
        if (types_.size() >= kNone) throw std::bad_alloc{};
        const HeapTypeId id{static_cast<std::uint32_t>(types_.size())};
        types_.push_back(type);
        return id;
    });
}
HeapError Heap::check(HeapTypeId type, ContainerToken value) const noexcept {
    if (!typeOf(type)) return HeapError::InvalidType;
    if (value.bits == 0) return HeapError::None;
    const Container* c = containers_.get(value.bits);
    return c && c->type == type ? HeapError::None : HeapError::InvalidHandle;
}
HeapError Heap::checkData(HeapLayoutId layout, std::span<const W4> data) const noexcept {
    const Layout* l = layoutOf(layout);
    if (!l || data.size() != l->words.size()) return HeapError::InvalidType;
    for (std::size_t i = 0; i < data.size(); ++i)
        if (l->words[i].nested != kNone) {
            if (data[i].b != 0) return HeapError::InvalidHandle;
            if (data[i].a != 0) {
                const Container* c = containers_.get(data[i].a);
                if (!c || !equivalentTypes(l->words[i].nested, c->type))
                    return HeapError::InvalidHandle;
            }
        }
    return HeapError::None;
}
void Heap::defaults(HeapLayoutId layout, std::span<W4> out) const noexcept {
    const auto& ws = layouts_[layout.index()].words;
    for (std::size_t i = 0; i < ws.size(); ++i)
        out[i] = ws[i].initial;
}

// Iterative deep copying also handles very deep nested container types
// without consuming the native call stack. Parents are referred to by token,
// never by a pointer that allocation could invalidate.
ContainerToken Heap::clone(Transaction& tx, HeapTypeId type, ContainerToken source,
                           std::uint64_t count) {
    struct Job {
        HeapTypeId type;
        ContainerToken source;
        std::uint64_t count;
        CellToken parent;
        std::size_t word;
    };
    std::pmr::vector<Job> jobs(memory_);
    jobs.push_back({type, source, count, {}, 0});
    ContainerToken result;
    while (!jobs.empty()) {
        Job job = jobs.back();
        jobs.pop_back();
        recordRead(HeapRoot::container(job.source));
        const HeapType& t = types_[job.type.index()];
        const Layout& l = layouts_[t.element.index()];
        if (t.maxElements != 0 && job.count > t.maxElements) {
            job.count = t.maxElements;
            tx.truncated = true;
        }
        // Include cell storage and value words, rather than only the token
        // vector, in the overflow check. All later size_t conversions fit.
        if (l.words.size() >
            (std::numeric_limits<std::size_t>::max() - sizeof(Entry<Cell>) - sizeof(CellToken)) /
                sizeof(W4))
            throw std::bad_alloc{};
        const std::size_t bytes =
            sizeof(Entry<Cell>) + sizeof(CellToken) + l.words.size() * sizeof(W4);
        if (job.count > std::numeric_limits<std::size_t>::max() / bytes) throw std::bad_alloc{};
        ContainerToken made;
        if (job.count != 0) {
            Container c{job.type, std::pmr::vector<CellToken>(memory_)};
            c.cells.reserve(static_cast<std::size_t>(job.count));
            made.bits = containers_.add(std::move(c), tx.containers);
            const Container* src = containers_.get(job.source.bits);
            const std::size_t copied =
                src ? std::min(src->cells.size(), static_cast<std::size_t>(job.count)) : 0;
            for (std::size_t i = 0; i < job.count; ++i) {
                std::pmr::vector<W4> data(l.words.size(), memory_);
                if (i < copied) {
                    // Reacquire after every allocation into either pool.
                    const CellToken old = containers_.get(job.source.bits)->cells[i];
                    const auto& oldWords = cells_.get(old.bits)->words;
                    std::copy(oldWords.begin(), oldWords.end(), data.begin());
                    recordRead(HeapRoot::cell(old));
                } else {
                    defaults(t.element, data);
                }
                const CellToken cell{cells_.add(Cell{t.element, std::move(data)}, tx.cells)};
                containers_.get(made.bits)->cells.push_back(cell);
                for (std::size_t w = 0; w < l.words.size(); ++w) {
                    const HeapTypeId child = l.words[w].nested;
                    if (child == kNone) continue;
                    const ContainerToken old{cells_.get(cell.bits)->words[w].a};
                    if (old.bits != 0) {
                        const Container* oldContainer = containers_.get(old.bits);
                        jobs.push_back({child, old, oldContainer->cells.size(), cell, w});
                    }
                }
            }
        }
        if (job.parent.bits != 0)
            cells_.get(job.parent.bits)->words[job.word] = {made.bits, 0};
        else
            result = made;
    }
    return result;
}
std::pmr::vector<W4> Heap::cloneWords(Transaction& tx, HeapLayoutId layout,
                                      std::span<const W4> data) {
    if (const HeapError e = checkData(layout, data); e != HeapError::None) throw HeapFault{e};
    const auto& ws = layouts_[layout.index()].words;
    std::pmr::vector<W4> out(data.begin(), data.end(), memory_);
    for (std::size_t i = 0; i < ws.size(); ++i) {
        const HeapWord& w = ws[i];
        if (w.nested != kNone && out[i].a != 0) {
            const ContainerToken old{out[i].a};
            const std::size_t count = containers_.get(old.bits)->cells.size();
            out[i] = {clone(tx, w.nested, old, count).bits, 0};
        } else if (w.fourState) {
            out[i].a &= w.mask;
            out[i].b &= w.mask;
        } else {
            out[i] = {out[i].a & ~out[i].b & w.mask, 0};
        }
    }
    return out;
}
CellToken Heap::cloneCell(Transaction& tx, HeapLayoutId layout, std::span<const W4> data) {
    auto words = cloneWords(tx, layout, data);
    return {cells_.add(Cell{layout, std::move(words)}, tx.cells)};
}
HeapResult<ContainerToken> Heap::construct(HeapTypeId type, std::optional<std::int64_t> size,
                                           ContainerToken initializer) noexcept {
    bool truncated = false;
    auto result = protectValue<ContainerToken>([&] {
        const HeapType* t = typeOf(type);
        if (!t || t->kind != ContainerKind::DynamicArray) throw HeapFault{HeapError::InvalidType};
        if (!size || *size < 0) throw HeapFault{HeapError::InvalidSize};
        if (initializer.bits != 0) {
            const Container* src = containers_.get(initializer.bits);
            if (!src || !equivalentLayouts(types_[src->type.index()].element, t->element))
                throw HeapFault{HeapError::InvalidHandle};
        }
        Transaction tx(*this);
        const ContainerToken out = clone(tx, type, initializer, static_cast<std::uint64_t>(*size));
        truncated = tx.truncated;
        tx.committed = true;
        return out;
    });
    if (result && truncated) result.status.warning = HeapWarning::BoundExceeded;
    return result;
}
HeapResult<ContainerToken> Heap::copy(HeapTypeId destination, ContainerToken source) noexcept {
    std::uint64_t count = 0;
    const HeapType* dst = typeOf(destination);
    if (!dst) return {{}, failure(HeapError::InvalidType)};
    if (source.bits != 0) {
        const Container* src = containers_.get(source.bits);
        if (!src || !equivalentLayouts(types_[src->type.index()].element, dst->element))
            return {{}, failure(HeapError::InvalidHandle)};
        count = src->cells.size();
    }
    bool truncated = false;
    auto out = protectValue<ContainerToken>([&] {
        Transaction tx(*this);
        const ContainerToken result = clone(tx, destination, source, count);
        truncated = tx.truncated;
        tx.committed = true;
        return result;
    });
    if (out && truncated) out.status.warning = HeapWarning::BoundExceeded;
    return out;
}
HeapResult<ContainerToken> Heap::snapshot(ContainerToken source) noexcept {
    if (source.bits == 0) return {};
    const Container* c = containers_.get(source.bits);
    if (!c) return {{}, failure(HeapError::InvalidHandle)};
    return copy(c->type, source);
}
HeapResult<ContainerToken> Heap::fromValues(HeapTypeId type, std::span<const W4> values) noexcept {
    bool truncated = false;
    auto out = protectValue<ContainerToken>([&] {
        const HeapType* t = typeOf(type);
        if (!t) throw HeapFault{HeapError::InvalidType};
        const std::size_t n = words(t->element);
        if (values.size() % n != 0) throw HeapFault{HeapError::InvalidType};
        const std::size_t count = values.size() / n;
        for (std::size_t i = 0; i < count; ++i)
            if (const HeapError e = checkData(t->element, values.subspan(i * n, n));
                e != HeapError::None)
                throw HeapFault{e};
        const std::size_t keep = t->maxElements != 0 && count > t->maxElements
                                     ? static_cast<std::size_t>(t->maxElements)
                                     : count;
        truncated = keep != count;
        Transaction tx(*this);
        tx.truncated = truncated;
        ContainerToken result;
        if (keep != 0) {
            Container c{type, std::pmr::vector<CellToken>(memory_)};
            c.cells.reserve(keep);
            result.bits = containers_.add(std::move(c), tx.containers);
            for (std::size_t i = 0; i < keep; ++i) {
                const CellToken cell = cloneCell(tx, t->element, values.subspan(i * n, n));
                containers_.get(result.bits)->cells.push_back(cell);
            }
        }
        truncated = tx.truncated;
        tx.committed = true;
        return result;
    });
    if (out && truncated) out.status.warning = HeapWarning::BoundExceeded;
    return out;
}
HeapResult<std::uint64_t> Heap::size(HeapTypeId type, ContainerToken value) const noexcept {
    if (const HeapError e = check(type, value); e != HeapError::None) return {0, failure(e)};
    const Container* c = containers_.get(value.bits);
    recordRead(HeapRoot::container(value));
    return {c ? c->cells.size() : 0, {}};
}
HeapResult<CellToken> Heap::cell(HeapTypeId type, ContainerToken value,
                                 std::optional<std::int64_t> index) const noexcept {
    if (const HeapError e = check(type, value); e != HeapError::None) return {{}, failure(e)};
    const Container* c = containers_.get(value.bits);
    recordRead(HeapRoot::container(value));
    if (!c) return {};
    const auto at = element(index, c->cells.size());
    return {at ? c->cells[*at] : CellToken{}, {}};
}
HeapStatus Heap::readCell(HeapLayoutId layout, CellToken cell, std::span<W4> out) const noexcept {
    if (!layoutOf(layout) || words(layout) != out.size()) return failure(HeapError::InvalidType);
    if (cell.bits == 0) {
        defaults(layout, out);
        return {};
    }
    const Cell* c = cells_.get(cell.bits);
    if (!c || c->layout != layout) return failure(HeapError::InvalidHandle);
    std::copy(c->words.begin(), c->words.end(), out.begin());
    recordRead(HeapRoot::cell(cell));
    return {};
}
HeapStatus Heap::read(HeapTypeId type, ContainerToken value, std::optional<std::int64_t> index,
                      std::span<W4> out) const noexcept {
    const auto found = cell(type, value, index);
    if (!found) return found.status;
    return readCell(types_[type.index()].element, found.value, out);
}
HeapStatus Heap::writeCell(HeapLayoutId layout, CellToken cell, std::span<const W4> data) noexcept {
    bool different = false;
    const auto status = protect([&] {
        if (const HeapError e = checkData(layout, data); e != HeapError::None) return failure(e);
        if (cell.bits == 0) return HeapStatus{};
        const Cell* c = cells_.get(cell.bits);
        if (!c || c->layout != layout) return failure(HeapError::InvalidHandle);
        Transaction tx(*this);
        auto copied = cloneWords(tx, layout, data);
        const auto& old = cells_.get(cell.bits)->words;
        different = !std::equal(old.begin(), old.end(), copied.begin(),
                                [](W4 a, W4 b) { return a.a == b.a && a.b == b.b; });
        cells_.get(cell.bits)->words.swap(copied);
        tx.committed = true;
        return HeapStatus{HeapError::None, tx.warning()};
    });
    if (status && different) recordChange(HeapRoot::cell(cell));
    return status;
}
HeapStatus Heap::swapValues(HeapTypeId type, ContainerToken value,
                            std::optional<std::int64_t> first,
                            std::optional<std::int64_t> second) noexcept {
    if (collecting_) return failure(HeapError::InvalidType);
    if (const auto error = check(type, value); error != HeapError::None) return failure(error);
    if (value.bits == 0) return {};
    const auto& container = *containers_.get(value.bits);
    const auto a = element(first, container.cells.size());
    const auto b = element(second, container.cells.size());
    if (!a || !b || *a == *b) return {};
    const auto firstCell = container.cells[*a], secondCell = container.cells[*b];
    auto& lhs = cells_.get(firstCell.bits)->words;
    auto& rhs = cells_.get(secondCell.bits)->words;
    const bool different = !std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                                       [](W4 x, W4 y) { return x.a == y.a && x.b == y.b; });
    if (!different) return {};
    lhs.swap(rhs);
    changed_ = true;
    const std::array roots{HeapRoot::cell(firstCell), HeapRoot::cell(secondCell)};
    recordChanges(roots);
    return {};
}

HeapStatus Heap::storeNested(HeapLayoutId layout, CellToken cell, ContainerToken value) noexcept {
    if (collecting_) return failure(HeapError::InvalidType);
    const Layout* l = layoutOf(layout);
    if (!l || l->words.size() != 1 || l->words[0].nested == kNone)
        return failure(HeapError::InvalidType);
    if (cell.bits == 0) return {};
    Cell* c = cells_.get(cell.bits);
    if (!c || c->layout != layout) return failure(HeapError::InvalidHandle);
    if (const HeapError e = check(l->words[0].nested, value); e != HeapError::None)
        return failure(e);
    const bool different = c->words[0].a != value.bits;
    c->words[0] = {value.bits, 0};
    changed_ = true;
    if (different) recordChange(HeapRoot::cell(cell));
    return {};
}
HeapStatus Heap::write(HeapTypeId type, ContainerToken& value, std::optional<std::int64_t> index,
                       std::span<const W4> data) noexcept {
    if (const HeapError e = check(type, value); e != HeapError::None) return failure(e);
    const HeapType& t = types_[type.index()];
    if (const HeapError e = checkData(t.element, data); e != HeapError::None) return failure(e);
    const auto count = size(type, value).value;
    if (t.kind == ContainerKind::Queue && index && *index >= 0 &&
        static_cast<std::uint64_t>(*index) == count)
        return insert(type, value, index, data);
    const auto target = cell(type, value, index);
    return writeCell(t.element, target.value, data);
}
HeapStatus Heap::insert(HeapTypeId type, ContainerToken& value, std::optional<std::int64_t> index,
                        std::span<const W4> data) noexcept {
    bool inserted = false;
    const auto status = protect([&] {
        if (const HeapError e = check(type, value); e != HeapError::None) return failure(e);
        const HeapType& t = types_[type.index()];
        if (t.kind != ContainerKind::Queue) return failure(HeapError::InvalidType);
        if (const HeapError e = checkData(t.element, data); e != HeapError::None) return failure(e);
        Container* c = containers_.get(value.bits);
        const std::size_t count = c ? c->cells.size() : 0;
        if (!index || *index < 0 || static_cast<std::uint64_t>(*index) > count)
            return HeapStatus{HeapError::None, HeapWarning::InvalidIndex};
        const std::size_t at = static_cast<std::size_t>(*index);
        if (t.maxElements != 0 && at >= t.maxElements)
            return HeapStatus{HeapError::None, HeapWarning::BoundExceeded};
        Transaction tx(*this);
        ContainerToken destination = value;
        if (!c) {
            Container empty{type, std::pmr::vector<CellToken>(memory_)};
            empty.cells.reserve(1);
            destination.bits = containers_.add(std::move(empty), tx.containers);
        } else {
            c->cells.reserve(count + 1);
        }
        const CellToken added = cloneCell(tx, t.element, data);
        auto& elements = containers_.get(destination.bits)->cells;
        elements.insert(elements.begin() + static_cast<std::ptrdiff_t>(at), added);
        const bool truncated = t.maxElements != 0 && elements.size() > t.maxElements;
        if (truncated) elements.resize(static_cast<std::size_t>(t.maxElements));
        tx.truncated = tx.truncated || truncated;
        value = destination;
        tx.committed = true;
        inserted = true;
        return HeapStatus{HeapError::None, tx.warning()};
    });
    if (status && inserted) recordChange(HeapRoot::container(value));
    return status;
}
HeapStatus Heap::push(HeapTypeId type, ContainerToken& value, bool front,
                      std::span<const W4> data) noexcept {
    const auto count = size(type, value);
    if (!count) return count.status;
    if (count.value > std::numeric_limits<std::int64_t>::max())
        return failure(HeapError::ResourceLimit);
    return insert(type, value, front ? 0 : static_cast<std::int64_t>(count.value), data);
}
HeapStatus Heap::pop(HeapTypeId type, ContainerToken& value, bool front,
                     std::span<W4> out) noexcept {
    bool removed = false;
    const auto status = protect([&] {
        if (const HeapError e = check(type, value); e != HeapError::None) return failure(e);
        const HeapType& t = types_[type.index()];
        if (t.kind != ContainerKind::Queue || out.size() != words(t.element))
            return failure(HeapError::InvalidType);
        const Container* c = containers_.get(value.bits);
        if (!c || c->cells.empty()) {
            defaults(t.element, out);
            return HeapStatus{HeapError::None, HeapWarning::EmptyPop};
        }
        const CellToken old = front ? c->cells.front() : c->cells.back();
        Transaction tx(*this);
        const auto copied = cloneWords(tx, t.element, cells_.get(old.bits)->words);
        auto& elements = containers_.get(value.bits)->cells;
        if (front)
            elements.erase(elements.begin());
        else
            elements.pop_back();
        std::copy(copied.begin(), copied.end(), out.begin());
        tx.committed = true;
        removed = true;
        return HeapStatus{HeapError::None, tx.warning()};
    });
    if (status && removed) recordChange(HeapRoot::container(value));
    return status;
}
HeapStatus Heap::erase(HeapTypeId type, ContainerToken& value,
                       std::optional<std::int64_t> index) noexcept {
    if (collecting_) return failure(HeapError::InvalidType);
    if (const HeapError e = check(type, value); e != HeapError::None) return failure(e);
    if (types_[type.index()].kind != ContainerKind::Queue) return failure(HeapError::InvalidType);
    Container* c = containers_.get(value.bits);
    const auto at = element(index, c ? c->cells.size() : 0);
    if (!at) return {HeapError::None, HeapWarning::InvalidIndex};
    c->cells.erase(c->cells.begin() + static_cast<std::ptrdiff_t>(*at));
    changed_ = true;
    recordChange(HeapRoot::container(value));
    return {};
}
HeapStatus Heap::clear(HeapTypeId type, ContainerToken& value) noexcept {
    if (collecting_) return failure(HeapError::InvalidType);
    if (const HeapError e = check(type, value); e != HeapError::None) return failure(e);
    changed_ = changed_ || value.bits != 0;
    value = {};
    return {};
}
HeapResult<bool> Heap::reaches(std::span<const HeapRoot> roots, HeapRoot target) const noexcept {
    return protectValue<bool>([&] {
        std::pmr::vector<HeapRoot> work(roots.begin(), roots.end(), memory_);
        std::pmr::vector<HeapRoot> visited(memory_);
        while (!work.empty()) {
            const auto root = work.back();
            work.pop_back();
            if (root.bits == 0) continue;
            if (std::find(visited.begin(), visited.end(), root) != visited.end()) continue;
            visited.push_back(root);
            if (root.kind == HeapRoot::Kind::Container) {
                const auto* c = containers_.get(root.bits);
                if (!c) throw HeapFault{HeapError::InvalidHandle};
                if (root == target) return true;
                for (const auto childCell : c->cells)
                    work.push_back(HeapRoot::cell(childCell));
            } else if (root.kind == HeapRoot::Kind::Cell) {
                const auto* c = cells_.get(root.bits);
                if (!c) throw HeapFault{HeapError::InvalidHandle};
                if (root == target) return true;
                const auto& words = layouts_[c->layout.index()].words;
                for (std::size_t i = 0; i < words.size(); ++i)
                    if (words[i].nested != kNone)
                        work.push_back(HeapRoot::container(ContainerToken{c->words[i].a}));
            } else {
                throw HeapFault{HeapError::InvalidType};
            }
        }
        return false;
    });
}

HeapStatus Heap::collect(std::span<const HeapRoot> roots, HeapStringRoots strings) noexcept {
    if (collecting_) return failure(HeapError::InvalidType);
    struct Collection {
        bool& active;
        explicit Collection(bool& flag) : active(flag) { active = true; }
        ~Collection() { active = false; }
    } collection(collecting_);
    return protect([&] {
        std::uint64_t live = 0;
        for (auto& e : containers_.entries)
            e.marked = false;
        for (auto& e : cells_.entries)
            e.marked = false;
        std::pmr::vector<HeapRoot> work(roots.begin(), roots.end(), memory_);
        while (!work.empty()) {
            const HeapRoot root = work.back();
            work.pop_back();
            if (root.kind != HeapRoot::Kind::Container && root.kind != HeapRoot::Kind::Cell)
                return failure(HeapError::InvalidType);
            if (root.bits == 0) continue;
            if (root.kind == HeapRoot::Kind::Container) {
                const Container* c = containers_.get(root.bits);
                if (!c) return failure(HeapError::InvalidHandle);
                auto& entry = containers_.entries[static_cast<std::uint32_t>(root.bits) - 1];
                if (entry.marked) continue;
                entry.marked = true;
                ++live;
                for (const CellToken cell : c->cells)
                    work.push_back(HeapRoot::cell(cell));
            } else if (root.kind == HeapRoot::Kind::Cell) {
                const Cell* c = cells_.get(root.bits);
                if (!c) return failure(HeapError::InvalidHandle);
                auto& entry = cells_.entries[static_cast<std::uint32_t>(root.bits) - 1];
                if (entry.marked) continue;
                entry.marked = true;
                ++live;
                const auto& ws = layouts_[c->layout.index()].words;
                for (std::size_t i = 0; i < ws.size(); ++i) {
                    if (ws[i].string &&
                        (strings.mark == nullptr || !strings.mark(strings.frame, c->words[i].a)))
                        return failure(HeapError::InvalidHandle);
                    if (ws[i].nested != kNone)
                        work.push_back(HeapRoot::container(ContainerToken{c->words[i].a}));
                }
            } else {
                return failure(HeapError::InvalidType);
            }
        }
        for (std::size_t i = 0; i < containers_.entries.size(); ++i)
            if (containers_.entries[i].value && !containers_.entries[i].marked)
                containers_.release(static_cast<std::uint32_t>(i));
        for (std::size_t i = 0; i < cells_.entries.size(); ++i)
            if (cells_.entries[i].value && !cells_.entries[i].marked)
                cells_.release(static_cast<std::uint32_t>(i));
        changed_ = false;
        allocated_ = 0;
        nextCollection_ =
            std::max(std::uint64_t{1024}, live > UINT64_MAX / 2 ? UINT64_MAX : live * 2);
        return HeapStatus{};
    });
}
HeapStats Heap::stats() const noexcept {
    HeapStats out{0, 0, containers_.entries.size(), cells_.entries.size()};
    for (const auto& e : containers_.entries)
        if (e.value) ++out.containers;
    for (const auto& e : cells_.entries)
        if (e.value) ++out.cells;
    return out;
}

}  // namespace kirimasim::kernel
