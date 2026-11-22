// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/run_policy_abi.h"
#include "abi_boundary.h"
#include "abi_views.h"
#include "kirimasim/kernel/context.h"
#include "service_tables.h"
namespace kirimasim::kernel {
namespace {
using U32 = std::uint32_t;
using Ticket = kirima_resource;
constexpr U32 ok = KIRIMA_VALUE_OK, invalid = KIRIMA_VALUE_INVALID;
RunDiagnostics* policy(kirima_context* c, Ticket t) {
    return c && t.owner == c ? fromC(c).modelResources().get<RunDiagnostics>(t.serial) : nullptr;
}
U32 create(kirima_context* c, Ticket* out) {
    if (!c || !out) return invalid;
    *out = {c, fromC(c).modelResources().reserve<RunDiagnostics>().id};
    return ok;
}
U32 release(kirima_context* c, Ticket t) {
    return c && t.owner == c && fromC(c).modelResources().release<RunDiagnostics>(t.serial)
               ? ok
               : invalid;
}
U32 first(kirima_context* c, Ticket t, std::uint64_t id, U32* out) {
    auto* p = policy(c, t);
    if (!p || !out) return invalid;
    *out = p->firstInstruction(id);
    return ok;
}
SourcePosition pos(kirima_position p) { return {p.file, p.line, p.column}; }
U32 firstHeap(kirima_context* c, Ticket t, kirima_position site, U32* out) {
    auto* p = policy(c, t);
    if (!p || !out) return invalid;
    *out = p->firstHeap(pos(site));
    return ok;
}
U32 occurrence(kirima_context* c, Ticket t, U32 check, kirima_position site, U32* out) {
    auto* p = policy(c, t);
    if (!p || !out || check > KIRIMA_CHECK_CONDITION) return invalid;
    *out = p->occurrence(static_cast<SourceCheck>(check), pos(site));
    return ok;
}
U32 counts(kirima_context* c, Ticket t, U32 check, std::uint64_t* total, std::uint64_t* sites) {
    auto* p = policy(c, t);
    if (!p || !total || !sites || check > KIRIMA_CHECK_CONDITION) return invalid;
    auto r = p->counts(static_cast<SourceCheck>(check));
    *total = r.occurrences;
    *sites = r.positions;
    return ok;
}
U32 diagnostic(RuntimeDiagnostic d, U32* out, kirima_byte_sink sink, void* frame) {
    if (!out || !sink) return invalid;
    *out = static_cast<U32>(d.action);
    sink(frame, abi::bytesOf(d.text));
    return ok;
}
U32 heap(kirima_heap_status s, U32* out, kirima_byte_sink sink, void* f) {
    if (s.error > KIRIMA_HEAP_RESOURCE || s.warning > KIRIMA_HEAP_QUEUE_WRITE) return invalid;
    return diagnostic(
        heapDiagnostic({static_cast<HeapError>(s.error), static_cast<HeapWarning>(s.warning)}), out,
        sink, f);
}
U32 object(U32 error, U32* out, kirima_byte_sink sink, void* f) {
    if (error > KIRIMA_OBJ_CAST) return invalid;
    return diagnostic(objectDiagnostic(static_cast<ObjectError>(error)), out, sink, f);
}
U32 condition(kirima_bytes kind, U32 error, kirima_byte_sink sink, void* f) {
    if (!abi::valid(kind) || error > 1 || !sink) return invalid;
    sink(f, abi::bytesOf(
                conditionDiagnostic(abi::view(kind), error ? CondXMode::Error : CondXMode::Warn)));
    return ok;
}
U32 init(U32 mode, U32 four, std::uint64_t* state, kirima_value_out out) {
    if (mode > KIRIMA_INIT_RANDOM || four > 1 || !state || !out.words || !out.width ||
        out.count != wide::chunks(out.width))
        return invalid;
    Rng rng{*state};
    fillSourceDefault({out.words, out.count}, out.width, four != 0,
                      static_cast<InitValuesMode>(mode), rng);
    *state = rng.state;
    return ok;
}
U32 replicate(kirima_context* c, Ticket t, kirima_bytes text, kirima_index idx,
              kirima_byte_sink sink, void* f, U32* warn) {
    auto* p = policy(c, t);
    if (!p || !abi::valid(text) || idx.known > 1 || idx.reserved || !sink || !warn) return invalid;
    const auto n = idx.known ? std::optional{idx.value} : std::nullopt;
    auto s = replicateString(abi::view(text), n);
    *warn = (!n || *n < 0) && p->firstReplication();
    sink(f, abi::bytesOf(s));
    return ok;
}
U32 select(kirima_context* c, W4 cond, std::uint64_t yes, std::uint64_t no, std::uint64_t* out) {
    if (!c || !out) return invalid;
    *out = selectString(fromC(c).strings(), cond, {yes, 0}, {no, 0}).a;
    return ok;
}
U32 write(kirima_bytes name, kirima_bytes contents, U32* out) {
    if (!out || !abi::valid(name) || !abi::valid(contents) ||
        abi::view(name).find('\0') != std::string_view::npos)
        return invalid;
    *out = static_cast<U32>(writeMemoryFile(std::string(abi::view(name)), abi::view(contents)));
    return ok;
}
}  // namespace
const void* runPolicyService() noexcept {
    static constexpr kirima_run_policy_api table{Boundary<create>::call,
                                                 release,
                                                 Boundary<first>::call,
                                                 Boundary<firstHeap>::call,
                                                 Boundary<occurrence>::call,
                                                 counts,
                                                 heap,
                                                 object,
                                                 Boundary<condition>::call,
                                                 init,
                                                 Boundary<replicate>::call,
                                                 select,
                                                 Boundary<write>::call};
    return &table;
}
}  // namespace kirimasim::kernel
