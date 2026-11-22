// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/object_abi.h"
#include "abi_boundary.h"
#include "abi_views.h"
#include "kirimasim/kernel/context.h"
#include "service_tables.h"

namespace kirimasim::kernel {
namespace {
using U32 = std::uint32_t;
using Token = kirima_heap_token;
using Ticket = kirima_resource;
U32 code(ObjectError error) { return static_cast<U32>(error); }
static_assert(static_cast<U32>(ObjectError::CastFailure) == KIRIMA_OBJ_CAST &&
              static_cast<U32>(ObjectError::ResourceLimit) == KIRIMA_OBJ_RESOURCE);
bool valid(kirima_context* c, Token t) {
    return c && !t.reserved &&
           (t.bits ? t.owner == c && t.kind == KIRIMA_OBJECT : !t.owner && !t.kind);
}
ObjectToken native(kirima_context* c, Token t) {
    return {t.bits, t.bits ? &fromC(c).heap() : nullptr};
}
Token model(kirima_context* c, ObjectToken t) {
    return t.bits ? Token{c, t.bits, KIRIMA_OBJECT, 0} : Token{};
}
template <class T> T* entry(kirima_context* c, Ticket t) {
    return c && t.owner == c ? fromC(c).modelResources().get<T>(t.serial) : nullptr;
}
template <class Native, class Out> U32 scalar(ObjectResult<Native> value, Out* out) {
    if (value) *out = value.value;
    return code(value.error);
}
U32 addClasses(kirima_context* c, const kirima_class_descriptor* classes, U32 count) {
    if (!c || (count && !classes)) return KIRIMA_OBJ_ABI;
    struct Row {
        std::vector<ObjectClassId> interfaces;
        std::vector<ObjectField> fields;
    };
    std::vector<Row> backing(count);
    std::vector<ObjectClass> descriptors;
    descriptors.reserve(count);
    for (U32 i = 0; i < count; ++i) {
        const auto& t = classes[i];
        if (!abi::valid(t.name) || t.abstract_class > 1 || t.interface_class > 1 ||
            (t.interface_count && !t.interfaces) || (t.field_count && !t.fields) ||
            (t.method_count && !t.virtual_methods))
            return KIRIMA_OBJ_ABI;
        auto& row = backing[i];
        for (auto id : std::span(t.interfaces, t.interface_count))
            row.interfaces.emplace_back(id);
        for (auto f : std::span(t.fields, t.field_count)) {
            if (f.constant > 1 || f.has_initializer > 1) return KIRIMA_OBJ_ABI;
            row.fields.push_back({HeapLayoutId{f.layout}, f.constant != 0, f.has_initializer != 0});
        }
        descriptors.push_back({abi::view(t.name),
                               ObjectClassId{t.base},
                               row.interfaces,
                               row.fields,
                               {t.virtual_methods, t.method_count},
                               t.abstract_class != 0,
                               t.interface_class != 0,
                               ObjectClassId{t.weak_target}});
    }
    return code(fromC(c).heap().addClasses(descriptors));
}
template <class Acquire> U32 pinned(kirima_context* c, kirima_pinned_object* out, Acquire acquire) {
    auto& resources = fromC(c).modelResources();
    auto slot = resources.reserve<ObjectPin>();
    auto r = acquire();
    if (!r || !r.value) {
        resources.release<ObjectPin>(slot.id);
        if (r) *out = {};
    } else {
        slot.entry = std::move(r.value);
        *out = {model(c, slot.entry.token()), {c, slot.id}};
    }
    return code(r.error);
}
U32 create(kirima_context* c, U32 type, std::uint64_t* rng, kirima_pinned_object* out) {
    if (!c || !rng || !out) return KIRIMA_OBJ_ABI;
    Rng state{*rng};
    const auto status =
        pinned(c, out, [&] { return fromC(c).heap().newObject(ObjectClassId{type}, state); });
    *rng = state.state;
    return status;
}
template <auto Operation> U32 acquire(kirima_context* c, Token t, kirima_pinned_object* out) {
    if (!valid(c, t) || !out) return KIRIMA_OBJ_ABI;
    return pinned(c, out, [&] { return (fromC(c).heap().*Operation)(native(c, t)); });
}
U32 info(kirima_context* c, Token t, kirima_object_info* out) {
    if (!valid(c, t) || !out) return KIRIMA_OBJ_ABI;
    const auto r = fromC(c).heap().objectInfo(native(c, t));
    if (r)
        *out = {r.value.dynamicClass.index(), r.value.constructed, r.value.identity,
                r.value.ordinal, r.value.randomState};
    return code(r.error);
}
U32 identity(kirima_context* c, Token t, std::uint64_t* out) {
    if (!valid(c, t) || !out) return KIRIMA_OBJ_ABI;
    return scalar(fromC(c).heap().objectId(native(c, t)), out);
}
U32 equal(kirima_context* c, Token a, Token b, U32* out) {
    if (!valid(c, a) || !valid(c, b) || !out) return KIRIMA_OBJ_ABI;
    return scalar(fromC(c).heap().equalObjects(native(c, a), native(c, b)), out);
}
U32 truth(kirima_context* c, Token a, U32* out) {
    if (!valid(c, a) || !out) return KIRIMA_OBJ_ABI;
    return scalar(fromC(c).heap().objectTruth(native(c, a)), out);
}
U32 select(kirima_context* c, W4 condition, Token a, Token b, Token* out) {
    if (!valid(c, a) || !valid(c, b) || !out) return KIRIMA_OBJ_ABI;
    const auto r = fromC(c).heap().selectObject(condition, native(c, a), native(c, b));
    if (r) *out = model(c, r.value);
    return code(r.error);
}
U32 cast(kirima_context* c, Token t, U32 source, U32 destination, U32 required, U32* out) {
    if (!valid(c, t) || required > 1 || !out) return KIRIMA_OBJ_ABI;
    return scalar(fromC(c).heap().castObject(native(c, t), ObjectClassId{source},
                                             ObjectClassId{destination}, required != 0),
                  out);
}
U32 seed(kirima_context* c, Token t, U32 seed) {
    if (!valid(c, t)) return KIRIMA_OBJ_ABI;
    return code(fromC(c).heap().seedObject(native(c, t), seed));
}
U32 method(kirima_context* c, Token t, U32 view, U32 slot, U32* out) {
    if (!valid(c, t) || !out) return KIRIMA_OBJ_ABI;
    return scalar(fromC(c).heap().virtualMethod(native(c, t), ObjectClassId{view}, slot), out);
}
U32 toWord(kirima_context* c, Token t, W4* out) {
    if (!valid(c, t) || !out) return KIRIMA_OBJ_ABI;
    const auto value = native(c, t);
    auto& h = fromC(c).heap();
    const auto check = h.objectTruth(value);
    if (check) *out = h.objectWord(value);
    return code(check.error);
}
U32 fromWord(kirima_context* c, W4 word, U32 view, Token* out) {
    if (!c || !out) return KIRIMA_OBJ_ABI;
    const auto r = fromC(c).heap().wordObject(word, ObjectClassId{view});
    if (r) *out = model(c, r.value);
    return code(r.error);
}
U32 bindField(kirima_context* c, ObjectRef field, std::optional<ObjectBitRef> bits, Ticket* out) {
    auto& ctx = fromC(c);
    auto slot = ctx.modelResources().reserve<ModelObjectField>();
    auto pin = ctx.heap().pinObject(field.object);
    if (!pin) {
        ctx.modelResources().release<ModelObjectField>(slot.id);
        return code(pin.error);
    }
    slot.entry = {std::move(pin.value), field, bits};
    *out = {c, slot.id};
    return KIRIMA_OBJ_OK;
}
U32 bind(kirima_context* c, Token t, U32 declaring, U32 index, U32 readonly, Ticket* out) {
    if (!valid(c, t) || readonly > 1 || !out) return KIRIMA_OBJ_ABI;
    const auto field =
        fromC(c).heap().objectRef(native(c, t), {ObjectClassId{declaring}, index}, readonly != 0);
    if (!field) return code(field.error);
    return bindField(c, field.value, std::nullopt, out);
}
U32 selectWords(kirima_context* c, Ticket id, std::uint64_t first, std::uint64_t count,
                Ticket* out) {
    const auto* src = entry<ModelObjectField>(c, id);
    if (!src || !out) return KIRIMA_OBJ_ABI;
    auto field = src->field;
    if (src->bits || !count || first > field.count || count > field.count - first)
        return KIRIMA_OBJ_FIELD;
    field.first += static_cast<std::size_t>(first);
    field.count = static_cast<std::size_t>(count);
    return bindField(c, field, std::nullopt, out);
}
U32 selectBits(kirima_context* c, Ticket id, std::uint64_t lsb, std::uint64_t width, Ticket* out) {
    const auto* src = entry<ModelObjectField>(c, id);
    if (!src || !out) return KIRIMA_OBJ_ABI;
    const auto& h = fromC(c).heap();
    const auto result = src->bits ? h.selectObjectBits(*src->bits, lsb, width)
                                  : h.selectObjectBits(src->field, lsb, width);
    if (!result) return code(result.error);
    return bindField(c, src->field, result.value, out);
}
U32 read(kirima_context* c, Ticket id, kirima_words_out out) {
    const auto* src = entry<ModelObjectField>(c, id);
    if (!src || (out.count && !out.data)) return KIRIMA_OBJ_ABI;
    const auto& h = fromC(c).heap();
    const std::span words(out.data, out.count);
    return code(src->bits ? h.readObject(*src->bits, words) : h.readObject(src->field, words));
}
U32 write(kirima_context* c, Ticket id, kirima_words_in value, Ticket stage) {
    const auto* src = entry<ModelObjectField>(c, id);
    const auto* init = entry<ObjectStage>(c, stage);
    if (!src || (value.count && !value.data) || ((stage.serial || stage.owner) && !init))
        return KIRIMA_OBJ_ABI;
    auto& h = fromC(c).heap();
    const std::span words(value.data, value.count);
    return code(src->bits ? h.writeObject(*src->bits, words, init)
                          : h.writeObject(src->field, words, init));
}
U32 beginStage(kirima_context* c, Token t, U32 declaring, Ticket* out) {
    if (!valid(c, t) || !out) return KIRIMA_OBJ_ABI;
    auto& ctx = fromC(c);
    auto slot = ctx.modelResources().reserve<ObjectStage>();
    auto r = ctx.heap().beginObjectStage(native(c, t), ObjectClassId{declaring});
    if (r) {
        slot.entry = std::move(r.value);
        *out = {c, slot.id};
    } else
        ctx.modelResources().release<ObjectStage>(slot.id);
    return code(r.error);
}
U32 beginBody(kirima_context* c, Ticket id) {
    auto* stage = entry<ObjectStage>(c, id);
    if (!stage) return KIRIMA_OBJ_ABI;
    return code(fromC(c).heap().beginObjectBody(*stage));
}
U32 finishStage(kirima_context* c, Ticket id) {
    auto* stage = entry<ObjectStage>(c, id);
    if (!stage) return KIRIMA_OBJ_ABI;
    const auto err = fromC(c).heap().finishObjectStage(*stage);
    if (err == ObjectError::None) fromC(c).modelResources().release<ObjectStage>(id.serial);
    return code(err);
}
U32 newWeak(kirima_context* c, U32 type, Token target, std::uint64_t* rng,
            kirima_pinned_object* out) {
    if (!valid(c, target) || !rng || !out) return KIRIMA_OBJ_ABI;
    Rng state{*rng};
    const auto status = pinned(c, out, [&] {
        return fromC(c).heap().newWeakObject(ObjectClassId{type}, native(c, target), state);
    });
    *rng = state.state;
    return status;
}
U32 clearWeak(kirima_context* c, Token weak) {
    if (!valid(c, weak)) return KIRIMA_OBJ_ABI;
    return code(fromC(c).heap().clearWeakObject(native(c, weak)));
}
U32 release(kirima_context* c, Ticket id) {
    if (!c || id.owner != c) return KIRIMA_OBJ_ABI;
    auto& r = fromC(c).modelResources();
    return r.release<ObjectPin>(id.serial) || r.release<ObjectStage>(id.serial) ||
                   r.release<ModelObjectField>(id.serial)
               ? KIRIMA_OBJ_OK
               : KIRIMA_OBJ_ABI;
}
template <auto F> constexpr auto guard = Boundary<F, KIRIMA_OBJ_RESOURCE>::call;
}  // namespace
const void* objectService() noexcept {
    static constexpr kirima_object_api table{guard<addClasses>,
                                             guard<create>,
                                             guard<acquire<&Heap::copyObject>>,
                                             guard<acquire<&Heap::pinObject>>,
                                             info,
                                             identity,
                                             equal,
                                             truth,
                                             select,
                                             cast,
                                             seed,
                                             method,
                                             toWord,
                                             fromWord,
                                             guard<bind>,
                                             guard<selectWords>,
                                             guard<selectBits>,
                                             read,
                                             write,
                                             guard<beginStage>,
                                             beginBody,
                                             finishStage,
                                             guard<newWeak>,
                                             guard<acquire<&Heap::lockWeakObject>>,
                                             clearWeak,
                                             release};
    return &table;
}
}  // namespace kirimasim::kernel
