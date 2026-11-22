// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/foreign_abi.h"
#include "abi_slot_view.h"
#include "abi_views.h"
#include "kirimasim/kernel/vpi.h"
#include "service_tables.h"
namespace kirimasim::kernel {
namespace {
using U32 = std::uint32_t;
constexpr U32 ok = KIRIMA_VALUE_OK, invalid = KIRIMA_VALUE_INVALID;
bool nativeType(kirima_foreign_type v, VpiType& out) {
    if (v.four_state > 1 || v.is_signed > 1 || v.real > 1 ||
        (v.real && (v.width != 64 || v.four_state)))
        return false;
    out = {v.width, v.four_state != 0, v.is_signed != 0, v.real != 0};
    return true;
}
kirima_foreign_type type(VpiType v) { return {v.width, v.four, v.signedValue, v.real}; }
kirima_foreign_access access(const VpiBinding& b) {
    return {b.first, b.second, b.generation, b.rootWidth, b.slot.lsb, b.slot.width};
}
bool readBinding(const VpiBinding& b, std::span<W4> out) {
    const auto* a = static_cast<const kirima_foreign_adapter*>(b.owner);
    return a->read(a->frame, access(b), {out.data(), static_cast<U32>(out.size())}) != 0;
}
bool writeBinding(const VpiBinding& b, std::span<const W4> words) {
    const auto* a = static_cast<const kirima_foreign_adapter*>(b.owner);
    return a->write(a->frame, access(b), {words.data(), static_cast<U32>(words.size())}) != 0;
}
bool binding(kirima_context* c, const kirima_foreign_binding& b, VpiType type, VpiBinding& out) {
    if (b.flags & ~15U) return false;
    out.slot = abi::bits(b.selection);
    out.isSlot = b.flags & KIRIMA_FOREIGN_SLOT;
    out.writable = b.flags & KIRIMA_FOREIGN_WRITABLE;
    out.net = b.flags & KIRIMA_FOREIGN_NET;
    out.netWritable = b.flags & KIRIMA_FOREIGN_WHOLE_NET;
    out.rootWidth = b.root_width;
    if (out.netWritable && (!out.net || !out.isSlot)) return false;
    if (out.net && out.writable) return false;
    if (out.isSlot) {
        if (b.adapter || !abi::valid(c, b.selection)) return false;
        const auto& l = fromC(c).store().layout(out.slot.slot);
        if (l.domain == Domain::Handle || out.rootWidth != l.width ||
            b.selection.width != type.width)
            return false;
        if (out.netWritable && (b.selection.lsb || b.selection.element || l.elements != 1 ||
                                b.selection.width != l.width))
            return false;
    } else if (b.adapter) {
        auto& a = *b.adapter;
        if (a.owner != c || !a.read || (out.writable && !a.write) || out.net ||
            out.slot.width != type.width || out.slot.lsb > out.rootWidth ||
            type.width > out.rootWidth - out.slot.lsb)
            return false;
        out.owner = b.adapter;
        out.first = b.first;
        out.second = b.second;
        out.generation = b.generation;
        out.read = readBinding;
        out.write = out.writable ? writeBinding : nullptr;
    } else if (out.writable || out.net)
        return false;
    return true;
}
bool args(kirima_context* c, const kirima_foreign_arg* src, U32 n, std::vector<VpiArgument>& out) {
    if (n && !src) return false;
    out.reserve(n);
    for (const auto& v : std::span(src, n)) {
        auto& a = out.emplace_back();
        if (!nativeType(v.type, a.value.type) || !v.type.width || !abi::valid(v.name) ||
            v.available > 1 || !binding(c, v.binding, a.value.type, a.binding) ||
            v.words.count != wide::chunks(v.type.width) || !v.words.data)
            return false;
        a.name = abi::view(v.name);
        a.available = v.available != 0;
        a.value.words.assign(v.words.data, v.words.data + v.words.count);
        if (v.declaration) {
            const auto& d = *v.declaration;
            if (d.integer > 1 || d.scalar > 1) return false;
            a.declaration = VpiDeclaration{d.left, d.right, d.scalar != 0, d.integer != 0};
        }
    }
    return true;
}
U32 state(kirima_context* c, U32* enabled, U32* stopped) {
    if (!c || !enabled || !stopped) return invalid;
    auto& v = fromC(c).vpi();
    *enabled = v.enabled();
    *stopped = v.lastCallStopped();
    return ok;
}
U32 routines(kirima_context* c, kirima_routine_sink emit, void* frame) {
    if (!c || !emit) return invalid;
    const auto list = fromC(c).vpi().routines();
    U32 row = 0;
    for (const auto& r : list)
        emit(frame, row++, abi::bytesOf(r.name), type(r.result), r.task);
    return ok;
}
U32 object(kirima_context* c, const kirima_foreign_object* v) {
    if (!c || !v || !abi::valid(v->name) || !abi::valid(v->file) || !abi::valid(v->scope) ||
        !abi::valid(v->parent) || !abi::valid(v->leaf) || v->scalar > 1 || v->indexed > 1 ||
        (v->dimension_count && !v->dimensions))
        return invalid;
    VpiObjectInfo o;
    if (!nativeType(v->type, o.type) || !binding(c, v->binding, o.type, o.binding)) return invalid;
    o.name = abi::view(v->name);
    o.file = abi::view(v->file);
    o.scopeName = abi::view(v->scope);
    o.parentName = abi::view(v->parent);
    o.leafName = abi::view(v->leaf);
    o.kind = v->kind;
    o.line = v->line;
    o.scalar = v->scalar != 0;
    o.indexed = v->indexed != 0;
    o.left = v->left;
    o.right = v->right;
    o.index = v->index;
    o.unit = v->unit;
    o.precision = v->precision;
    if (o.binding.netWritable && (o.kind != vpiNet || o.indexed || v->dimension_count))
        return invalid;
    o.dimensions.reserve(v->dimension_count);
    for (auto d : std::span(v->dimensions, v->dimension_count))
        o.dimensions.emplace_back(d.left, d.right);
    return fromC(c).vpi().object(std::move(o)) ? ok : invalid;
}
U32 compile(kirima_context* c, const kirima_foreign_site* site, const kirima_foreign_arg* src,
            U32 n, U32* accepted) {
    if (!c || !site || !accepted || !abi::valid(site->scope) || !abi::valid(site->file))
        return invalid;
    VpiType result;
    std::vector<VpiArgument> arguments;
    if (!nativeType(site->result, result) || !args(c, src, n, arguments)) return invalid;
    *accepted = fromC(c).vpi().compileCall(
        site->site, site->routine, std::string(abi::view(site->scope)), arguments, result,
        site->unit, site->precision, std::string(abi::view(site->file)), site->line);
    return ok;
}
U32 call(kirima_context* c, U32 site, const kirima_foreign_arg* src, U32 n,
         kirima_foreign_result emit, void* frame) {
    if (!c || !emit) return invalid;
    std::vector<VpiArgument> arguments;
    if (!args(c, src, n, arguments)) return invalid;
    auto& v = fromC(c).vpi();
    if (!v.acceptsArguments(site, arguments)) return invalid;
    const auto r = v.call(site, arguments);
    emit(frame, type(r.type), {r.words.data(), static_cast<U32>(r.words.size())});
    return ok;
}
U32 detach(kirima_context* c, kirima_foreign_adapter* a) {
    if (!c || !a || a->owner != c) return invalid;
    fromC(c).vpi().detachBindings(a);
    return ok;
}
U32 phase(kirima_context* c, U32 event) {
    if (!c) return invalid;
    auto& v = fromC(c).vpi();
    switch (event) {
        case KIRIMA_FOREIGN_BEGIN: v.beginRuntime(); break;
        case KIRIMA_FOREIGN_START: v.start(); break;
        case KIRIMA_FOREIGN_FINISH: v.finish(); break;
        default: return invalid;
    }
    return ok;
}
U32 error(kirima_context* c, kirima_byte_sink sink, void* frame) {
    if (!c || !sink) return invalid;
    sink(frame, abi::bytesOf(fromC(c).vpi().error()));
    return ok;
}
U32 report(kirima_context* c, kirima_bytes b) {
    if (!c || !abi::valid(b)) return invalid;
    fromC(c).vpi().reportError(std::string(abi::view(b)));
    return ok;
}
// Allocation after a VPI host callback or handle publication is terminal.
template <auto F> struct ForeignBoundary;
template <class... Args, U32 (*F)(kirima_context*, Args...)> struct ForeignBoundary<F> {
    static U32 call(kirima_context* c, Args... args) noexcept {
        try {
            return F(c, args...);
        } catch (const std::bad_alloc&) {
            fromC(c).scheduler().outOfMemory();
        } catch (const std::length_error&) {
            fromC(c).scheduler().outOfMemory();
        }
        return KIRIMA_VALUE_RESOURCE;
    }
};
}  // namespace
const void* foreignService() noexcept {
    static constexpr kirima_foreign_api table{state,
                                              ForeignBoundary<routines>::call,
                                              ForeignBoundary<object>::call,
                                              ForeignBoundary<compile>::call,
                                              ForeignBoundary<call>::call,
                                              detach,
                                              ForeignBoundary<phase>::call,
                                              ForeignBoundary<error>::call,
                                              ForeignBoundary<report>::call};
    return &table;
}
}  // namespace kirimasim::kernel
