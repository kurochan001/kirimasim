// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/task_abi.h"

#include "abi_boundary.h"
#include "abi_scan_value.h"
#include "abi_views.h"
#include "kirimasim/kernel/memfile.h"
#include "service_tables.h"
#include <algorithm>

namespace kirimasim::kernel {
namespace {
using U32 = std::uint32_t;
bool named(kirima_bytes text) { return abi::valid(text) && text.size; }
bool path(kirima_bytes text) {
    return abi::valid(text) && abi::view(text).find('\0') == std::string_view::npos;
}
bool validVariable(Context& ctx, const kirima_dump_var& v) {
    if (!named(v.name) || !abi::valid(v.range) || v.type > KIRIMA_DUMP_SUPPLY1 || !v.width ||
        v.short_real > 1 || v.slot >= ctx.store().size())
        return false;
    const auto& l = ctx.store().layout(SlotId{v.slot});
    if (l.elements != 1) return false;
    if (v.type == KIRIMA_DUMP_REAL || v.type == KIRIMA_DUMP_REALTIME)
        return l.domain == Domain::Real;
    if (v.short_real) return false;
    if (v.type == KIRIMA_DUMP_EVENT) return l.domain == Domain::Handle;
    return (l.domain == Domain::Four || l.domain == Domain::Two) && v.width == l.width;
}
U32 setup(kirima_context* c, const kirima_wave_setup* s) {
    if (!c || !s || fromC(c).dump().configured() || s->glitches > 1 ||
        (s->scope_count && !s->scopes) || (s->var_count && !s->vars) ||
        (s->filter_count && !s->filters) || !abi::valid(s->date) || !abi::valid(s->version) ||
        !abi::valid(s->timescale) || !path(s->filename))
        return KIRIMA_VALUE_INVALID;
    auto& ctx = fromC(c);
    DumpSetup native;
    native.date = abi::view(s->date);
    native.version = abi::view(s->version);
    native.timescale = abi::view(s->timescale);
    native.waveFile = abi::view(s->filename);
    native.glitches = s->glitches;
    for (const auto r : std::span(s->scopes, s->scope_count)) {
        if (!named(r.name) || !named(r.path) || r.kind > KIRIMA_DUMP_FUNCTION ||
            (r.parent != UINT32_MAX && r.parent >= native.scopes.size()))
            return KIRIMA_VALUE_INVALID;
        native.scopes.push_back({std::string(abi::view(r.name)), std::string(abi::view(r.path)),
                                 static_cast<DumpScopeKind>(r.kind), r.parent});
    }
    for (const auto v : std::span(s->vars, s->var_count)) {
        if (v.scope >= native.scopes.size() ||
            (!native.vars.empty() && native.vars.back().scope > v.scope) || !validVariable(ctx, v))
            return KIRIMA_VALUE_INVALID;
        native.vars.push_back({v.scope, std::string(abi::view(v.name)),
                               std::string(abi::view(v.range)), static_cast<DumpType>(v.type),
                               v.width, SlotId{v.slot}, v.short_real != 0});
    }
    for (auto f : std::span(s->filters, s->filter_count)) {
        if (!abi::valid(f)) return KIRIMA_VALUE_INVALID;
        native.scopePatterns.emplace_back(abi::view(f));
    }
    ctx.dump().setup(std::move(native));
    return KIRIMA_VALUE_OK;
}
U32 file(kirima_context* c, kirima_bytes name, kirima_bytes call, U32* out) {
    if (!c || !out || !path(name) || !abi::valid(call) || !fromC(c).dump().configured())
        return KIRIMA_VALUE_INVALID;
    *out = static_cast<U32>(
        fromC(c).dump().file(std::string(abi::view(name)), std::string(abi::view(call))));
    return KIRIMA_VALUE_OK;
}
U32 vars(kirima_context* c, U32 levels, const U32* scopes, U32 ns, const U32* vars, U32 nv, U32 all,
         U32* out) {
    if (!c || !out || all > 1 || (ns && !scopes) || (nv && !vars) || (all && (ns || nv)))
        return KIRIMA_VALUE_INVALID;
    auto& ctx = fromC(c);
    auto& dump = ctx.dump();
    const std::span scopeRows(scopes, ns), varRows(vars, nv);
    if (!dump.configured() ||
        std::ranges::any_of(scopeRows, [&](U32 i) { return i >= dump.scopeCount(); }) ||
        std::ranges::any_of(varRows, [&](U32 i) { return i >= dump.variableCount(); }))
        return KIRIMA_VALUE_INVALID;
    *out =
        static_cast<U32>(dump.vars(ctx.scheduler().time(), levels, scopeRows, varRows, all != 0));
    return KIRIMA_VALUE_OK;
}
U32 control(kirima_context* c, U32 op) {
    if (!c || !fromC(c).dump().configured()) return KIRIMA_VALUE_INVALID;
    auto& ctx = fromC(c);
    auto& d = ctx.dump();
    switch (op) {
        case KIRIMA_DUMP_OFF: d.off(); break;
        case KIRIMA_DUMP_ON: d.on(); break;
        case KIRIMA_DUMP_ALL: d.all(); break;
        case KIRIMA_DUMP_FLUSH: d.flush(); break;
        case KIRIMA_DUMP_FINISH: d.finish(ctx.scheduler()); break;
        default: return KIRIMA_VALUE_INVALID;
    }
    return KIRIMA_VALUE_OK;
}
U32 limit(kirima_context* c, std::uint64_t bytes) {
    if (!c || !fromC(c).dump().configured()) return KIRIMA_VALUE_INVALID;
    fromC(c).dump().limit(bytes);
    return KIRIMA_VALUE_OK;
}
U32 display(kirima_context* c, kirima_bytes value, U32 newline) {
    if (!c || !abi::valid(value) || newline > 1) return KIRIMA_VALUE_INVALID;
    fromC(c).scheduler().writeDisplay(std::string(abi::view(value)), newline != 0);
    return KIRIMA_VALUE_OK;
}
U32 read(kirima_bytes name, kirima_byte_sink sink, void* user, U32* out) {
    if (!path(name) || !out || !sink) return KIRIMA_VALUE_INVALID;
    bool readError = false;
    const auto content = readFile(std::string(abi::view(name)), &readError);
    *out = content ? KIRIMA_READ_OK : readError ? KIRIMA_READ_IO_ERROR : KIRIMA_READ_OPEN_ERROR;
    if (content) sink(user, abi::bytesOf(*content));
    return KIRIMA_VALUE_OK;
}
bool args(const kirima_bytes* fields, U32 count, std::vector<std::string_view>& out) {
    if (count && !fields) return false;
    out.reserve(count);
    for (auto a : std::span(fields, count)) {
        if (!abi::valid(a)) return false;
        out.push_back(abi::view(a));
    }
    return true;
}
U32 testPlusargs(const kirima_bytes* fields, U32 count, kirima_bytes prefix, U32* found) {
    std::vector<std::string_view> text;
    if (!found || !abi::valid(prefix) || !args(fields, count, text)) return KIRIMA_VALUE_INVALID;
    *found = plusargSuffix(text, abi::view(prefix)).has_value();
    return KIRIMA_VALUE_OK;
}
U32 valuePlusargs(kirima_context* c, const kirima_bytes* fields, U32 count, kirima_bytes format,
                  kirima_scan_target target, kirima_scan_sink assign, kirima_byte_sink diagnostic,
                  void* user, U32* found) {
    std::vector<std::string_view> text;
    const auto kind = abi::targetKind(target);
    if (!c || !found || !assign || !diagnostic || !abi::valid(format) ||
        kind == ScanTarget::Other || !args(fields, count, text))
        return KIRIMA_VALUE_INVALID;
    const auto plan = preparePlusargFormat(abi::view(format), kind);
    if (!plan.error.empty()) {
        *found = 0;
        diagnostic(user, abi::bytesOf(plan.error));
        return KIRIMA_VALUE_OK;
    }
    const auto suffix = plusargSuffix(text, plan.prefix);
    if (suffix) {
        std::vector<W4> words;
        abi::scanCells(fromC(c), target, kind, plusargValue(*suffix, plan.conv), words);
        assign(user, 0, words.data(), static_cast<U32>(words.size()));
    }
    *found = suffix.has_value();
    return KIRIMA_VALUE_OK;
}
}  // namespace
const void* taskService() noexcept {
    static constexpr kirima_task_api table{
        Boundary<setup>::call,   Boundary<file>::call,         Boundary<vars>::call,
        Boundary<control>::call, Boundary<limit>::call,        Boundary<display>::call,
        Boundary<read>::call,    Boundary<testPlusargs>::call, Boundary<valuePlusargs>::call};
    return &table;
}
}  // namespace kirimasim::kernel
