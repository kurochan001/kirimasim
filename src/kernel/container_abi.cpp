// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/container_abi.h"

#include "abi_boundary.h"
#include "kirimasim/kernel/context.h"
#include "service_tables.h"
#include <span>
#include <vector>

namespace kirimasim::kernel {
namespace {
using U32 = std::uint32_t;
using Token = kirima_heap_token;
using Status = kirima_heap_status;
static_assert(static_cast<U32>(HeapError::None) == KIRIMA_HEAP_OK &&
              static_cast<U32>(HeapError::InvalidType) == KIRIMA_HEAP_TYPE &&
              static_cast<U32>(HeapError::InvalidHandle) == KIRIMA_HEAP_HANDLE &&
              static_cast<U32>(HeapError::InvalidSize) == KIRIMA_HEAP_SIZE &&
              static_cast<U32>(HeapError::ResourceLimit) == KIRIMA_HEAP_RESOURCE);
static_assert(static_cast<U32>(HeapWarning::None) == KIRIMA_HEAP_QUIET &&
              static_cast<U32>(HeapWarning::InvalidIndex) == KIRIMA_HEAP_INDEX &&
              static_cast<U32>(HeapWarning::EmptyPop) == KIRIMA_HEAP_EMPTY &&
              static_cast<U32>(HeapWarning::BoundExceeded) == KIRIMA_HEAP_BOUND &&
              static_cast<U32>(HeapWarning::InvalidQueueWrite) == KIRIMA_HEAP_QUEUE_WRITE);
U32 publish(HeapStatus status, Status* out) {
    *out = {static_cast<U32>(status.error), static_cast<U32>(status.warning)};
    return KIRIMA_VALUE_OK;
}
bool valid(const kirima_context* c, Token t, U32 kind) {
    if (t.reserved) return false;
    if (!t.bits) return !t.owner && !t.kind;
    return t.owner == c && t.kind == kind;
}
Token token(const kirima_context* c, std::uint64_t bits, U32 kind) {
    return bits ? Token{c, bits, kind, 0} : Token{};
}
bool valid(kirima_index i) { return i.known <= 1 && !i.reserved; }
std::optional<std::int64_t> index(kirima_index i) {
    return i.known ? std::optional{i.value} : std::nullopt;
}
template <class T> bool buffer(T w) { return w.data || !w.count; }
template <class T> auto words(T w) { return std::span(w.data, w.count); }
template <class T> U32 publishId(HeapResult<T> id, U32* out, Status* status) {
    if (id) *out = id.value.index();
    return publish(id.status, status);
}
template <class T>
U32 publishToken(const kirima_context* c, HeapResult<T> value, U32 kind, Token* out,
                 Status* status) {
    if (value) *out = token(c, value.value.bits, kind);
    return publish(value.status, status);
}
U32 addLayout(kirima_context* c, const kirima_heap_word* input, U32 count, U32* out,
              Status* status) {
    if (!c || !status || !out || (count && !input)) return KIRIMA_VALUE_INVALID;
    std::vector<HeapWord> layout;
    layout.reserve(count);
    for (const auto w : std::span(input, count)) {
        if (w.four_state > 1 || w.string_edge > 1) return KIRIMA_VALUE_INVALID;
        layout.push_back({w.mask, w.four_state != 0, w.initial, HeapTypeId{w.nested_type},
                          w.string_edge != 0, ObjectClassId{w.object_class}});
    }
    return publishId(fromC(c).heap().addLayout(layout), out, status);
}
U32 addType(kirima_context* c, kirima_heap_type t, U32* out, Status* status) {
    if (!c || !out || !status || t.kind > KIRIMA_QUEUE) return KIRIMA_VALUE_INVALID;
    const HeapType type{t.kind == KIRIMA_QUEUE ? ContainerKind::Queue : ContainerKind::DynamicArray,
                        HeapLayoutId{t.element_layout}, t.capacity};
    return publishId(fromC(c).heap().addType(type), out, status);
}
U32 describeType(kirima_context* c, U32 type, kirima_heap_type* out, Status* status) {
    if (!c || !out || !status) return KIRIMA_VALUE_INVALID;
    const auto r = fromC(c).heap().describeType(HeapTypeId{type});
    if (r)
        *out = {r.value.kind == ContainerKind::Queue ? KIRIMA_QUEUE : KIRIMA_DYNAMIC_ARRAY,
                r.value.element.index(), r.value.maxElements};
    return publish(r.status, status);
}
U32 describeWord(kirima_context* c, U32 layout, U32 at, kirima_heap_word* out, Status* status) {
    if (!c || !out || !status) return KIRIMA_VALUE_INVALID;
    const auto r = fromC(c).heap().describeWord(HeapLayoutId{layout}, at);
    if (r)
        *out = {r.value.mask,   r.value.initial,        r.value.fourState,
                r.value.string, r.value.nested.index(), r.value.object.index()};
    return publish(r.status, status);
}
U32 wordCount(kirima_context* c, U32 layout, std::uint64_t* out) {
    if (!c || !out) return KIRIMA_VALUE_INVALID;
    *out = fromC(c).heap().words(HeapLayoutId{layout});
    return KIRIMA_VALUE_OK;
}
U32 actualType(kirima_context* c, Token t, U32* out, Status* status) {
    if (!c || !out || !status || !valid(c, t, KIRIMA_CONTAINER)) return KIRIMA_VALUE_INVALID;
    return publishId(fromC(c).heap().actualType({t.bits}), out, status);
}
U32 construct(kirima_context* c, U32 type, kirima_index size, Token init, kirima_fresh_word fresh,
              void* user, Token* out, Status* status) {
    if (!c || !out || !status || !valid(size) || !valid(c, init, KIRIMA_CONTAINER))
        return KIRIMA_VALUE_INVALID;
    struct Fresh {
        kirima_fresh_word fn;
        void* user;
    } source{fresh, user};
    HeapFreshInit next;
    if (fresh)
        next = {[](void* p) noexcept {
                    const auto& s = *static_cast<Fresh*>(p);
                    return s.fn(s.user);
                },
                &source};
    return publishToken(c,
                        fromC(c).heap().construct(HeapTypeId{type}, index(size), {init.bits}, next),
                        KIRIMA_CONTAINER, out, status);
}
U32 copy(kirima_context* c, U32 type, Token t, Token* out, Status* status) {
    if (!c || !out || !status || !valid(c, t, KIRIMA_CONTAINER)) return KIRIMA_VALUE_INVALID;
    return publishToken(c, fromC(c).heap().copy(HeapTypeId{type}, {t.bits}), KIRIMA_CONTAINER, out,
                        status);
}
U32 snapshot(kirima_context* c, Token t, Token* out, Status* status) {
    if (!c || !out || !status || !valid(c, t, KIRIMA_CONTAINER)) return KIRIMA_VALUE_INVALID;
    return publishToken(c, fromC(c).heap().snapshot({t.bits}), KIRIMA_CONTAINER, out, status);
}
U32 fromWords(kirima_context* c, U32 type, kirima_words_in input, Token* out, Status* status) {
    if (!c || !out || !status || !buffer(input)) return KIRIMA_VALUE_INVALID;
    return publishToken(c, fromC(c).heap().fromValues(HeapTypeId{type}, words(input)),
                        KIRIMA_CONTAINER, out, status);
}
U32 size(kirima_context* c, U32 type, Token t, std::uint64_t* out, Status* status) {
    if (!c || !out || !status || !valid(c, t, KIRIMA_CONTAINER)) return KIRIMA_VALUE_INVALID;
    const auto r = fromC(c).heap().size(HeapTypeId{type}, {t.bits});
    if (r) *out = r.value;
    return publish(r.status, status);
}
U32 read(kirima_context* c, U32 type, Token t, kirima_index i, kirima_words_out out,
         Status* status) {
    if (!c || !status || !buffer(out) || !valid(i) || !valid(c, t, KIRIMA_CONTAINER))
        return KIRIMA_VALUE_INVALID;
    return publish(fromC(c).heap().read(HeapTypeId{type}, {t.bits}, index(i), words(out)), status);
}
U32 cell(kirima_context* c, U32 type, Token t, kirima_index i, Token* out, Status* status) {
    if (!c || !status || !out || !valid(i) || !valid(c, t, KIRIMA_CONTAINER))
        return KIRIMA_VALUE_INVALID;
    return publishToken(c, fromC(c).heap().cell(HeapTypeId{type}, {t.bits}, index(i)), KIRIMA_CELL,
                        out, status);
}
template <auto Operation, class Buffer>
U32 cellValue(kirima_context* c, U32 layout, Token t, Buffer value, Status* status) {
    if (!c || !status || !buffer(value) || !valid(c, t, KIRIMA_CELL)) return KIRIMA_VALUE_INVALID;
    return publish(
        (fromC(c).heap().*Operation)(HeapLayoutId{layout}, CellToken{t.bits}, words(value)),
        status);
}
U32 storeNested(kirima_context* c, U32 layout, Token cell, Token value, Status* status) {
    if (!c || !status || !valid(c, cell, KIRIMA_CELL) || !valid(c, value, KIRIMA_CONTAINER))
        return KIRIMA_VALUE_INVALID;
    return publish(fromC(c).heap().storeNested(HeapLayoutId{layout}, {cell.bits}, {value.bits}),
                   status);
}
// Queue edits can allocate the first container. Publish its new token only
// after the same transaction that changed its elements succeeds.
template <auto Operation, class... Args>
U32 edit(kirima_context* c, U32 type, Token* t, Status* status, Args... args) {
    if (!c || !t || !status || !valid(c, *t, KIRIMA_CONTAINER)) return KIRIMA_VALUE_INVALID;
    ContainerToken container{t->bits};
    const auto r = (fromC(c).heap().*Operation)(HeapTypeId{type}, container, args...);
    if (r) *t = token(c, container.bits, KIRIMA_CONTAINER);
    return publish(r, status);
}
template <auto Operation>
U32 indexedWrite(kirima_context* c, U32 type, Token* t, kirima_index i, kirima_words_in input,
                 Status* status) {
    if (!valid(i) || !buffer(input)) return KIRIMA_VALUE_INVALID;
    return edit<Operation>(c, type, t, status, index(i), words(input));
}
template <auto Operation, class Buffer>
U32 endEdit(kirima_context* c, U32 type, Token* t, U32 front, Buffer value, Status* status) {
    if (front > 1 || !buffer(value)) return KIRIMA_VALUE_INVALID;
    return edit<Operation>(c, type, t, status, front != 0, words(value));
}
U32 erase(kirima_context* c, U32 type, Token* t, kirima_index i, Status* status) {
    if (!valid(i)) return KIRIMA_VALUE_INVALID;
    return edit<&Heap::erase>(c, type, t, status, index(i));
}
U32 clear(kirima_context* c, U32 type, Token* t, Status* status) {
    return edit<&Heap::clear>(c, type, t, status);
}
U32 swapValues(kirima_context* c, U32 type, Token t, kirima_index a, kirima_index b,
               Status* status) {
    if (!c || !status || !valid(c, t, KIRIMA_CONTAINER) || !valid(a) || !valid(b))
        return KIRIMA_VALUE_INVALID;
    return publish(fromC(c).heap().swapValues(HeapTypeId{type}, {t.bits}, index(a), index(b)),
                   status);
}
U32 locate(kirima_context* c, U32 type, Token t, const kirima_heap_key_part* parts, U32 count,
           kirima_words_in key, kirima_words_out cell, kirima_heap_position* out, Status* status) {
    if (!c || !status || !out || !valid(c, t, KIRIMA_CONTAINER) || (count && !parts) ||
        !buffer(key) || !buffer(cell))
        return KIRIMA_VALUE_INVALID;
    std::vector<KeyPart> shape;
    shape.reserve(count);
    for (const auto p : std::span(parts, count)) {
        if (p.signed_integral > 1 || p.string_key > 1) return KIRIMA_VALUE_INVALID;
        shape.push_back({p.width, p.signed_integral != 0, p.string_key != 0});
    }
    auto& ctx = fromC(c);
    const auto r = ctx.heap().locateKey(HeapTypeId{type}, {t.bits}, ctx.strings(), shape,
                                        words(key), words(cell));
    if (r) *out = {r.value.index, r.value.found, r.value.valid};
    return publish(r.status, status);
}
bool decodeRoot(kirima_context* c, Token t, HeapRoot& r) {
    if (!valid(c, t, t.kind)) return false;
    switch (t.kind) {
        case KIRIMA_CONTAINER: r = HeapRoot::container({t.bits}); return true;
        case KIRIMA_CELL: r = HeapRoot::cell({t.bits}); return true;
        case KIRIMA_OBJECT: r = HeapRoot::object({t.bits, &fromC(c).heap()}); return true;
        default: return false;
    }
}
bool roots(kirima_context* c, const Token* input, U32 count, std::vector<HeapRoot>& out) {
    if (count && !input) return false;
    out.resize(count);
    for (U32 i = 0; i < count; ++i)
        if (!decodeRoot(c, input[i], out[i])) return false;
    return true;
}
U32 reaches(kirima_context* c, const Token* input, U32 count, Token target, U32* out,
            Status* status) {
    std::vector<HeapRoot> graph;
    HeapRoot node;
    if (!c || !out || !status || !decodeRoot(c, target, node) || !roots(c, input, count, graph))
        return KIRIMA_VALUE_INVALID;
    const auto r = fromC(c).heap().reaches(graph, node);
    if (r) *out = r.value;
    return publish(r.status, status);
}
U32 collect(kirima_context* c, const Token* input, U32 count, kirima_string_mark mark, void* user,
            Status* status) {
    struct Publication {
        std::vector<HeapRoot> graph;
        bool failed = false;
        static void add(void* p, HeapRoot root) noexcept {
            auto& s = *static_cast<Publication*>(p);
            if (s.failed) return;
            try {
                s.graph.push_back(root);
            } catch (const std::bad_alloc&) {
                s.failed = true;
            } catch (const std::length_error&) {
                s.failed = true;
            }
        }
    } publication;
    if (!c || !status || !fromC(c).scheduler().permitsTransientCollection() ||
        !roots(c, input, count, publication.graph))
        return KIRIMA_VALUE_INVALID;
    auto& ctx = fromC(c);
    ctx.activations().visitRoots(Publication::add, &publication);
    if (publication.failed) return KIRIMA_VALUE_RESOURCE;
    struct Marker {
        kirima_string_mark fn;
        void* user;
    } marker{mark, user};
    HeapStringRoots strings;
    if (mark)
        strings = {[](void* p, std::uint64_t h) noexcept {
                       const auto& m = *static_cast<Marker*>(p);
                       return m.fn(m.user, h) != 0;
                   },
                   &marker};
    return publish(ctx.heap().collect(publication.graph, strings), status);
}
U32 stats(kirima_context* c, kirima_heap_stats* out) {
    if (!c || !out) return KIRIMA_VALUE_INVALID;
    const auto& h = fromC(c).heap();
    const auto s = h.stats();
    *out = {s.containers,        s.cells,          s.containerEntries, s.cellEntries,
            h.needsCollection(), h.underPressure()};
    return KIRIMA_VALUE_OK;
}
}  // namespace
const void* containerService() noexcept {
    static constexpr kirima_container_api table{Boundary<addLayout>::call,
                                                Boundary<addType>::call,
                                                describeType,
                                                describeWord,
                                                wordCount,
                                                actualType,
                                                construct,
                                                copy,
                                                snapshot,
                                                fromWords,
                                                size,
                                                read,
                                                indexedWrite<&Heap::write>,
                                                cell,
                                                cellValue<&Heap::readCell, kirima_words_out>,
                                                cellValue<&Heap::writeCell, kirima_words_in>,
                                                storeNested,
                                                indexedWrite<&Heap::insert>,
                                                endEdit<&Heap::push, kirima_words_in>,
                                                endEdit<&Heap::pop, kirima_words_out>,
                                                erase,
                                                clear,
                                                swapValues,
                                                Boundary<locate>::call,
                                                Boundary<reaches>::call,
                                                Boundary<collect>::call,
                                                stats};
    return &table;
}
}  // namespace kirimasim::kernel
