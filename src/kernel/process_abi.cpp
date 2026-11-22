// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/process_abi.h"

#include <string>
#include <string_view>

#include "abi_boundary.h"
#include "kirimasim/kernel/context.h"
#include "value_services.h"

namespace kirimasim::kernel {
namespace {
static_assert(static_cast<unsigned>(KIRIMA_PROCESS_RESOURCE) == KIRIMA_VALUE_RESOURCE);
bool valid(kirima_context* c, kirima_process h) {
    return c && h.owner == c && !h.reserved &&
           fromC(c).scheduler().processSerial(ProcessId{h.index}) == h.serial;
}
bool valid(kirima_bytes data) {
    return (data.data || data.size == 0) && data.size <= std::string{}.max_size();
}
std::string copy(kirima_bytes data) {
    return {data.data ? data.data : "", static_cast<std::size_t>(data.size)};
}
std::uint32_t snapshot(kirima_context* c, ProcessId id, kirima_process* out) {
    const auto serial = fromC(c).scheduler().processSerial(id);
    if (!serial) return KIRIMA_PROCESS_RESOURCE;
    *out = {c, *serial, id.index(), 0};
    return KIRIMA_PROCESS_OK;
}
std::uint32_t add(kirima_context* c, std::uint32_t kind, kirima_resume_fn fn, void* frame,
                  kirima_process* out) {
    if (!c || !out || !fn || kind > KIRIMA_NET_RESOLVER) return KIRIMA_PROCESS_INVALID;
    auto& sched = fromC(c).scheduler();
    const auto id =
        kind == KIRIMA_NET_RESOLVER
            ? sched.addNetResolver(fn, frame)
            : sched.addProcess(fn, frame,
                               kind == KIRIMA_ACTIVE_PROCESS ? Group::Active : Group::Reactive);
    return snapshot(c, id, out);
}
std::uint32_t addProgram(kirima_context* c, kirima_program* out) {
    if (!c || !out) return KIRIMA_PROCESS_INVALID;
    *out = {c, fromC(c).scheduler().addProgram().index(), 0};
    return KIRIMA_PROCESS_OK;
}
std::uint32_t addInitial(kirima_context* c, kirima_program p, kirima_resume_fn fn, void* frame,
                         kirima_process* out) {
    if (!c || !out || !fn || p.owner != c || p.reserved) return KIRIMA_PROCESS_INVALID;
    auto& sched = fromC(c).scheduler();
    const ProgramId program{p.index};
    if (!sched.hasProgram(program)) return KIRIMA_PROCESS_INVALID;
    return snapshot(c, sched.addProgramInitial(fn, frame, program), out);
}
std::uint32_t addChild(kirima_context* c, kirima_process p, kirima_resume_fn fn, void* frame,
                       kirima_process* out) {
    if (!out || !fn || !valid(c, p)) return KIRIMA_PROCESS_INVALID;
    auto& sched = fromC(c).scheduler();
    if (sched.finished(ProcessId{p.index})) return KIRIMA_PROCESS_INVALID;
    return snapshot(c, sched.addChildProcess(fn, frame, ProcessId{p.index}), out);
}
std::uint32_t action(kirima_context* c, kirima_process h, std::uint32_t op) {
    if (!valid(c, h)) return KIRIMA_PROCESS_INVALID;
    auto& s = fromC(c).scheduler();
    const ProcessId p{h.index};
    switch (op) {
        case KIRIMA_START_PROCESS:
            if (s.finished(p)) return KIRIMA_PROCESS_INVALID;
            s.start(p);
            break;
        case KIRIMA_CANCEL_WAIT: s.cancelWait(p); break;
        case KIRIMA_KILL_PROCESS: s.kill(p); break;
        case KIRIMA_END_PROCESS: s.ended(p); break;
        case KIRIMA_RELEASE_PROCESS:
            if (!s.releasable(p)) return KIRIMA_PROCESS_BUSY;
            s.releaseProcess(p);
            break;
        case KIRIMA_EXIT_PROGRAM: s.exitProgram(p); break;
        case KIRIMA_STROBE: s.strobe(p); break;
        default: return KIRIMA_PROCESS_INVALID;
    }
    return KIRIMA_PROCESS_OK;
}
std::uint32_t flags(kirima_context* c, kirima_process h, std::uint32_t* out) {
    if (!out || !valid(c, h)) return KIRIMA_PROCESS_INVALID;
    const auto& s = fromC(c).scheduler();
    const ProcessId p{h.index};
    *out = (s.finished(p) ? KIRIMA_PROCESS_FINISHED : 0U) |
           (s.hasPending(p) ? KIRIMA_PENDING_REPORT : 0U) |
           (s.hasCallbackOrigins(p) ? KIRIMA_CALLBACK_ORIGIN : 0U) |
           (s.programStopped(p) ? KIRIMA_PROGRAM_STOPPED : 0U) |
           (s.processGroup(p) == Group::Reactive ? KIRIMA_REACTIVE_GROUP : 0U);
    return KIRIMA_PROCESS_OK;
}
std::uint32_t deferReport(kirima_context* c, kirima_process h, kirima_bytes line) {
    if (!valid(c, h) || !valid(line)) return KIRIMA_PROCESS_INVALID;
    fromC(c).scheduler().deferReport(ProcessId{h.index}, copy(line));
    return KIRIMA_PROCESS_OK;
}
std::uint32_t deferCall(kirima_context* c, kirima_process h, std::uint32_t final,
                        kirima_deferred_fn fn, void* frame, std::uint32_t token,
                        std::uint32_t assertion) {
    if (!valid(c, h) || !fn || final > 1) return KIRIMA_PROCESS_INVALID;
    fromC(c).scheduler().deferCall(ProcessId{h.index}, final != 0, fn, frame, token, assertion);
    return KIRIMA_PROCESS_OK;
}
std::uint32_t flush(kirima_context* c, kirima_process h) {
    if (!valid(c, h)) return KIRIMA_PROCESS_INVALID;
    return fromC(c).scheduler().flushDeferredAssertions(ProcessId{h.index})
               ? KIRIMA_PROCESS_OK
               : KIRIMA_PROCESS_STOPPED;
}
std::uint32_t cancel(kirima_context* c, std::uint32_t assertion) {
    if (!c || assertion == kNone) return KIRIMA_PROCESS_INVALID;
    return fromC(c).scheduler().cancelDeferredAssertions(assertion) ? KIRIMA_PROCESS_OK
                                                                    : KIRIMA_PROCESS_STOPPED;
}
std::uint32_t takeEnded(kirima_context* c, std::uint32_t kind, std::uint32_t* present,
                        kirima_process* out) {
    if (!c || !present || !out || kind > KIRIMA_ENDED_FILE_MONITOR) return KIRIMA_PROCESS_INVALID;
    auto& s = fromC(c).scheduler();
    ProcessId id{kNone};
    *present = kind == KIRIMA_ENDED_PROGRAM_PROCESS ? s.takeEndedProgramProcess(id)
                                                    : s.takeEndedFileMonitor(id);
    if (!*present) return KIRIMA_PROCESS_OK;
    return snapshot(c, id, out);
}
std::uint32_t state(kirima_context* c, kirima_scheduler_state* out) {
    if (!c || !out) return KIRIMA_PROCESS_INVALID;
    const auto& s = fromC(c).scheduler();
    const std::uint32_t bits = (s.ending() ? KIRIMA_ENDING : 0U) |
                               (s.inPostponed() ? KIRIMA_POSTPONED : 0U) |
                               (s.permitsEventCollection() ? KIRIMA_COLLECT_EVENTS : 0U) |
                               (s.permitsTransientCollection() ? KIRIMA_COLLECT_TRANSIENTS : 0U) |
                               (s.hasEndedProcessNotifications() ? KIRIMA_END_NOTIFICATIONS : 0U) |
                               (s.memoryRanOut() ? KIRIMA_MEMORY_RAN_OUT : 0U);
    *out = {s.time(),  s.callbackReleaseEpoch(),
            bits,      static_cast<std::uint32_t>(s.reason()),
            s.level(), 0};
    return KIRIMA_PROCESS_OK;
}
std::uint32_t checkpoint(kirima_context* c, std::uint32_t kind, std::uint32_t* taken) {
    if (!c || !taken || kind > KIRIMA_TAKE_OVERFLOW) return KIRIMA_PROCESS_INVALID;
    auto& s = fromC(c).scheduler();
    *taken = kind == KIRIMA_TAKE_INTERRUPT ? s.takeInterrupt() : s.takeOverflow();
    return KIRIMA_PROCESS_OK;
}
std::uint32_t terminate(kirima_context* c, std::uint32_t kind) {
    if (!c) return KIRIMA_PROCESS_INVALID;
    auto& s = fromC(c).scheduler();
    switch (kind) {
        case KIRIMA_DESIGN_ERROR: s.runtimeError(); break;
        case KIRIMA_RESOURCE_LIMIT: s.resourceLimit(); break;
        case KIRIMA_OUT_OF_MEMORY: s.outOfMemory(); break;
        default: return KIRIMA_PROCESS_INVALID;
    }
    return KIRIMA_PROCESS_OK;
}
std::uint32_t setNote(kirima_context* c, kirima_bytes note) {
    if (!c || !valid(note)) return KIRIMA_PROCESS_INVALID;
    fromC(c).scheduler().setEndNote(copy(note));
    return KIRIMA_PROCESS_OK;
}
std::uint32_t getText(kirima_context* c, std::uint32_t kind, kirima_byte_sink sink, void* user) {
    if (!c || !sink || kind > KIRIMA_END_NOTE) return KIRIMA_PROCESS_INVALID;
    const auto& s = fromC(c).scheduler();
    const std::string_view text = kind == KIRIMA_ERROR_TEXT ? s.error() : s.endNote();
    sink(user, {text.data(), text.size()});
    return KIRIMA_PROCESS_OK;
}
constexpr kirima_process_api api{
    Boundary<add>::call,         Boundary<addProgram>::call, Boundary<addInitial>::call,
    Boundary<addChild>::call,    Boundary<action>::call,     Boundary<flags>::call,
    Boundary<deferReport>::call, Boundary<deferCall>::call,  Boundary<flush>::call,
    Boundary<cancel>::call,      Boundary<takeEnded>::call,  Boundary<state>::call,
    Boundary<checkpoint>::call,  Boundary<terminate>::call,  Boundary<setNote>::call,
    Boundary<getText>::call};
}  // namespace
const void* queryProcessService(std::uint32_t id, std::uint32_t version) noexcept {
    return id == KIRIMA_SERVICE_PROCESS && version == KIRIMA_PROCESS_VERSION ? &api : nullptr;
}
}  // namespace kirimasim::kernel
