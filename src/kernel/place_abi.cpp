// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/place_abi.h"
#include "abi_boundary.h"
#include "abi_slot_view.h"
#include "kirimasim/kernel/frame_value.h"
#include "kirimasim/kernel/wide.h"
#include "service_tables.h"
#include <bit>
namespace kirimasim::kernel {
namespace {
using U32 = std::uint32_t;
using Ticket = kirima_resource;
using Rights = kirima_place_rights;
using Info = kirima_place_info;
using Window = kirima_ref_window;
constexpr U32 ok = KIRIMA_REF_OK, invalid = KIRIMA_REF_INVALID, bounds = KIRIMA_REF_BOUNDS;
template <class T> T* get(kirima_context* c, Ticket t) {
    return c && t.owner == c ? fromC(c).modelResources().get<T>(t.serial) : nullptr;
}
bool rights(kirima_context* c, Rights r) {
    return c && r.read_only <= 1 && r.local <= 1 &&
           (r.actual_type == UINT32_MAX || fromC(c).heap().describeType(HeapTypeId{r.actual_type}));
}
U32 objStatus(ObjectError e) {
    switch (e) {
        case ObjectError::None: return ok;
        case ObjectError::ResourceLimit: return KIRIMA_REF_RESOURCE;
        case ObjectError::ConstWrite: return KIRIMA_REF_READONLY;
        case ObjectError::InvalidValue: return KIRIMA_REF_VALUE;
        default: return invalid;
    }
}
U32 heapStatus(HeapStatus s) {
    if (s) return ok;
    return s.error == HeapError::ResourceLimit ? KIRIMA_REF_RESOURCE : invalid;
}
std::span<W4> resolve(const ModelOrdinary& f) {
    kirima_words_out v{};
    if (!f.provider->resolve(f.provider->frame, f.id, &v) || !v.data) return {};
    return {v.data, v.count};
}
U32 acquire(kirima_context* c, ModelPlaceSource source, U32 fieldWidth, Info info, Ticket* out) {
    auto& ctx = fromC(c);
    auto& resources = ctx.modelResources();
    auto t = resources.reserve<ModelPlace>();
    auto& p = t.entry;
    p.source = source;
    p.fieldWidth = fieldWidth;
    p.info = info;
    U32 result = ok;
    if (auto* f = std::get_if<ModelOrdinary>(&p.source)) {
        p.framePinned = f->provider->retain(f->provider->frame, f->id) != 0;
        if (!p.framePinned)
            result = invalid;
        else {
            const auto words = resolve(*f);
            const auto end = std::uint64_t(info.window.first_element) + info.window.elements;
            if (f->offset > words.size() ||
                end > (words.size() - f->offset) / wide::chunks(fieldWidth))
                result = bounds;
        }
    } else if (auto* a = std::get_if<ActivationBinding>(&p.source)) {
        auto lease = ctx.activations().retain(*a);
        result = static_cast<U32>(lease.error);
        if (lease) p.activation = std::move(lease.value);
    } else if (auto* f = std::get_if<ModelField>(&p.source)) {
        auto pin = ctx.heap().pinObject(f->field.object);
        result = objStatus(pin.error);
        if (pin) p.object = std::move(pin.value);
    }
    if (result != ok) {
        resources.release<ModelPlace>(t.id);
        return result;
    }
    *out = {c, t.id};
    return ok;
}
Info info(U32 kind, U32 elements, U32 width, Rights r) {
    return {{0, elements, 0, width}, kind, r.read_only, r.local, r.actual_type};
}
U32 slot(kirima_context* c, U32 row, Rights r, Ticket* out) {
    if (!out || !rights(c, r) || !abi::slot(c, row)) return invalid;
    auto l = fromC(c).store().layout(SlotId{row});
    return acquire(c, SlotId{row}, l.width, info(KIRIMA_PLACE_SLOT, l.elements, l.width, r), out);
}
U32 ordinary(kirima_context* c, kirima_frame_provider* p, kirima_frame_identity id,
             std::uint64_t offset, U32 n, U32 width, Rights r, Ticket* out) {
    if (!out || !rights(c, r) || !p || p->owner != c || !p->retain || !p->release || !p->resolve ||
        !id.generation || !width || !n)
        return invalid;
    return acquire(c, ModelOrdinary{p, id, offset}, width, info(KIRIMA_PLACE_ORDINARY, n, width, r),
                   out);
}
U32 lexical(kirima_context* c, kirima_reference ref, Rights r, Ticket* out) {
    if (!out || !rights(c, r) || ref.owner != c) return invalid;
    const auto a = fromC(c).references().part(ref.ticket);
    if (!a) return static_cast<U32>(a.error);
    const auto& b = a.value;
    r.read_only |= b.binding.readonly();
    auto i = info(KIRIMA_PLACE_LEXICAL, b.numElems, b.width, r);
    i.window.first_element = b.firstElem;
    i.window.lsb = b.lsb;
    return acquire(c, b.binding, b.width, i, out);
}
U32 cell(kirima_context* c, kirima_heap_token t, U32 layout, U32 width, Rights r, Ticket* out) {
    if (!out || !rights(c, r) || t.owner != c || t.kind != KIRIMA_CELL || t.reserved || !t.bits ||
        !width)
        return invalid;
    auto& h = fromC(c).heap();
    HeapLayoutId id{layout};
    auto chunks = h.words(id);
    if (!chunks || chunks != wide::chunks(width)) return bounds;
    bool packed = true;
    for (U32 i = 0; i < chunks; ++i) {
        auto d = h.describeWord(id, i);
        if (!d) return invalid;
        const auto& w = d.value;
        packed = packed && !w.string && w.nested == kNone && w.object == kNone &&
                 w.mask == mask(i + 1 == chunks ? width - 64 * i : 64U);
    }
    if (!h.validCell(id, {t.bits})) return invalid;
    return acquire(c, ModelCell{id, {t.bits}, packed}, width, info(KIRIMA_PLACE_CELL, 1, width, r),
                   out);
}
U32 object(kirima_context* c, Ticket field, U32 width, Rights r, Ticket* out) {
    const auto* f = get<ModelObjectField>(c, field);
    if (!out || !rights(c, r) || !f || !width) return invalid;
    auto bits = f->bits;
    if (bits) {
        if (width != bits->width) return bounds;
    } else {
        auto selected = fromC(c).heap().selectObjectBits(f->field, 0, width);
        if (selected)
            bits = selected.value;
        else if (std::uint64_t(width) != std::uint64_t(f->field.count) * 64)
            return bounds;
    }
    r.read_only |= f->field.readonly;
    return acquire(c, ModelField{f->field, bits}, width, info(KIRIMA_PLACE_OBJECT, 1, width, r),
                   out);
}
U32 select(kirima_context* c, Ticket h, Window w, U32 ro, Ticket* out) {
    const auto* p = get<ModelPlace>(c, h);
    if (!p || !out || ro > 1) return invalid;
    auto i = p->info;
    const auto parent = i.window;
    if (!w.elements || w.first_element >= parent.elements ||
        w.elements > parent.elements - w.first_element || w.lsb >= parent.width ||
        w.width > parent.width - w.lsb)
        return bounds;
    w.width = w.width ? w.width : parent.width - w.lsb;
    const bool sub = w.width != parent.width || w.lsb != 0;
    if (sub) {
        if (auto s = std::get_if<SlotId>(&p->source)) {
            auto d = fromC(c).store().layout(*s).domain;
            if (d == Domain::Real || d == Domain::Handle) return bounds;
        }
        if (auto f = std::get_if<ModelField>(&p->source); f && !f->bits) return bounds;
        if (auto cell = std::get_if<ModelCell>(&p->source); cell && !cell->packed) return bounds;
    }
    w.first_element += parent.first_element;
    w.lsb += parent.lsb;
    i.window = w;
    i.read_only |= ro;
    if (auto a = std::get_if<ActivationBinding>(&p->source)) {
        const auto valid =
            fromC(c).activations().region({*a, w.first_element, w.elements, w.lsb, w.width});
        if (!valid) return static_cast<U32>(valid.error);
    }
    return acquire(c, p->source, p->fieldWidth, i, out);
}
U32 describe(kirima_context* c, Ticket h, Info* out) {
    auto* p = get<ModelPlace>(c, h);
    if (!p || !out) return invalid;
    *out = p->info;
    return ok;
}
U32 release(kirima_context* c, Ticket h) {
    if (!c || h.owner != c || !fromC(c).modelResources().release<ModelPlace>(h.serial))
        return invalid;
    return ok;
}
template <bool Write, class Span>
U32 transfer(kirima_context* c, ModelPlace& p, U32 element, Span data, const ObjectStage* stage) {
    auto& ctx = fromC(c);
    const auto w = p.info.window;
    if (element >= w.elements || data.size() != wide::chunks(w.width)) return bounds;
    if (Write && p.info.read_only) return KIRIMA_REF_READONLY;
    element += w.first_element;
    if (auto* a = std::get_if<ActivationBinding>(&p.source)) {
        if constexpr (Write)
            return static_cast<U32>(
                ctx.scheduler().writeActivation(*a, element, data, w.lsb, w.width));
        else
            return static_cast<U32>(ctx.activations().read(*a, element, data, w.lsb, w.width));
    }
    if (auto* f = std::get_if<ModelOrdinary>(&p.source)) {
        auto words = resolve(*f);
        const auto stride = wide::chunks(p.fieldWidth);
        if (f->offset > words.size() ||
            std::uint64_t(element) + 1 > (words.size() - f->offset) / stride)
            return invalid;
        auto v = FrameValue{words.subspan(f->offset + std::uint64_t(element) * stride, stride),
                            p.fieldWidth, p.info.read_only != 0};
        if constexpr (Write)
            return static_cast<U32>(v.write(data, w.lsb, w.width));
        else
            return static_cast<U32>(v.read(data, w.lsb, w.width));
    }
    if (auto* cell = std::get_if<ModelCell>(&p.source)) {
        auto& heap = ctx.heap();
        if (!w.lsb && w.width == p.fieldWidth) {
            if constexpr (Write)
                return heapStatus(heap.writeCell(cell->layout, cell->token, data));
            else
                return heapStatus(heap.readCell(cell->layout, cell->token, data));
        }
        std::vector<W4> full(wide::chunks(p.fieldWidth));
        auto status = heap.readCell(cell->layout, cell->token, full);
        if (!status) return heapStatus(status);
        FrameValue v(full, p.fieldWidth);
        if constexpr (Write) {
            auto e = v.write(data, w.lsb, w.width);
            if (e != ActivationError::None) return static_cast<U32>(e);
            return heapStatus(heap.writeCell(cell->layout, cell->token, full));
        } else
            return static_cast<U32>(v.read(data, w.lsb, w.width));
    }
    if (auto* f = std::get_if<ModelField>(&p.source)) {
        auto& h = ctx.heap();
        if (f->bits) {
            auto view = h.selectObjectBits(*f->bits, w.lsb, w.width);
            if (!view) return objStatus(view.error);
            if constexpr (Write)
                return objStatus(h.writeObject(view.value, data, stage));
            else
                return objStatus(h.readObject(view.value, data));
        }
        if constexpr (Write)
            return objStatus(h.writeObject(f->field, data, stage));
        else
            return objStatus(h.readObject(f->field, data));
    }
    const auto id = *std::get_if<SlotId>(&p.source);
    auto& store = ctx.store();
    const auto& layout = store.layout(id);
    const auto four = layout.domain == Domain::Four;
    if constexpr (Write) {
        std::vector<std::uint64_t> packed;
        packed.reserve(data.size() * (four ? 2 : 1));
        for (auto x : data) {
            if (!four && x.b) return KIRIMA_REF_VALUE;
            packed.push_back(x.a);
            if (four) packed.push_back(x.b);
        }
        ctx.scheduler().write({id, element, w.lsb, w.width}, packed.data());
        return ok;
    } else {
        auto* raw = store.element(id, element);
        std::vector<W4> full(wide::chunks(layout.width));
        for (std::size_t i = 0; i < full.size(); ++i)
            full[i] = four ? W4{raw[i * 2], raw[i * 2 + 1]} : W4{raw[i], 0};
        return static_cast<U32>(FrameValue{full, layout.width}.read(data, w.lsb, w.width));
    }
}
U32 read(kirima_context* c, Ticket h, U32 e, kirima_value_out out) {
    auto* p = get<ModelPlace>(c, h);
    if (!p || !out.words || out.width != p->info.window.width) return invalid;
    return transfer<false>(c, *p, e, std::span(out.words, out.count), nullptr);
}
U32 write(kirima_context* c, Ticket h, U32 e, kirima_value_in v, Ticket init) {
    auto* p = get<ModelPlace>(c, h);
    const auto* stage = get<ObjectStage>(c, init);
    if (!p || !v.words || v.width != p->info.window.width ||
        ((init.owner || init.serial) && (!stage || p->info.kind != KIRIMA_PLACE_OBJECT)))
        return invalid;
    return transfer<true>(c, *p, e, std::span(v.words, v.count), stage);
}
U32 region(kirima_context* c, kirima_reference ref, kirima_lexical_region* out) {
    if (!c || !out || ref.owner != c) return invalid;
    auto& ctx = fromC(c);
    auto p = ctx.references().part(ref.ticket);
    if (!p) return static_cast<U32>(p.error);
    auto a = ctx.activations().region(p.value);
    if (!a) return static_cast<U32>(a.error);
    const auto& r = a.value;
    *out = {r.token.bits,
            r.field,
            p.value.binding.readonly(),
            {r.firstElem, r.numElems, r.lsb, r.width}};
    return ok;
}
U32 frames(kirima_context* c, kirima_frame_visitor fn, void* u, U32* done) {
    if (!c || !fn || !done) return invalid;
    *done = fromC(c).modelResources().visit<ModelPlace>([&](const ModelPlace& p) {
        auto* f = std::get_if<ModelOrdinary>(&p.source);
        return !f || fn(u, f->provider, f->id) != 0;
    });
    return ok;
}
U32 attach(kirima_context* c, kirima_run_hooks* hooks, Ticket* out) {
    if (!c || !out || !hooks || hooks->owner != c) return invalid;
    auto& ctx = fromC(c);
    auto& r = ctx.modelResources();
    auto t = r.reserve<ModelHooks>();
    SafePointHook safe;
    PulseHook pulse;
    if (hooks->safe_point)
        safe = {[](void* arg) noexcept {
                    auto& h = *static_cast<kirima_run_hooks*>(arg);
                    h.safe_point(h.frame);
                },
                hooks};
    if (hooks->pulse)
        pulse = {[](void* arg, PulseKind k, U32 id, std::uint64_t time) noexcept {
                     auto& h = *static_cast<kirima_run_hooks*>(arg);
                     h.pulse(h.frame, static_cast<U32>(k), id, time);
                 },
                 hooks};
    auto& s = ctx.scheduler();
    t.entry = {s.setSafePointHook(safe), s.setPulseHook(pulse), r.hookTop};
    r.hookTop = t.id;
    *out = {c, t.id};
    return ok;
}
U32 detach(kirima_context* c, Ticket h) {
    auto* hooks = get<ModelHooks>(c, h);
    if (!hooks) return invalid;
    auto& ctx = fromC(c);
    auto& r = ctx.modelResources();
    if (r.hookTop != h.serial) return invalid;
    ctx.scheduler().setSafePointHook(hooks->previousSafe);
    ctx.scheduler().setPulseHook(hooks->previousPulse);
    r.hookTop = hooks->previous;
    r.release<ModelHooks>(h.serial);
    return ok;
}
template <auto F> constexpr auto guard = Boundary<F, KIRIMA_REF_RESOURCE>::call;
}  // namespace
const void* placeService() noexcept {
    static constexpr kirima_place_api table{
        guard<slot>,   guard<ordinary>, guard<lexical>, guard<cell>,  guard<object>,
        guard<select>, describe,        guard<read>,    guard<write>, release,
        region,        frames,          guard<attach>,  detach};
    return &table;
}
}  // namespace kirimasim::kernel
