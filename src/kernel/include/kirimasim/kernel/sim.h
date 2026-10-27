// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The scheduler: time slots with the 17 regions of IEEE 1800-2023 4.4, the
// reference algorithm of 4.5, waits and wake-ups, NBAs, named events, and
// the reasons a simulation ends (SCHEDULER_SPEC.md 1 to 9, 13 and 16).
//
// Every execution tier registers its processes in the same form: a resume
// function and a frame (RUNTIME_ABI.md 4.3). The kernel cannot tell an
// interpreted process from a compiled one.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "kirimasim/kernel/abi.h"
#include "kirimasim/kernel/activation.h"
#include "kirimasim/kernel/counters.h"
#include "kirimasim/kernel/heap.h"
#include "kirimasim/kernel/id.h"
#include "kirimasim/kernel/managed_event.h"
#include "kirimasim/kernel/nba.h"
#include "kirimasim/kernel/store.h"
#include "kirimasim/kernel/strength_drivers.h"
#include "kirimasim/kernel/strength_store.h"

namespace kirimasim::kernel {

class Context;

/// Why a simulation ended (SCHEDULER_SPEC.md 16.2, CLI.md 5).
enum class EndReason : std::uint8_t {
    Finish,
    Quiescent,
    Stop,
    Fatal,
    TimeLimit,
    ZeroTimeLimit,
    Interrupted,
    Error,
    Internal,
    ResourceLimit,
    WallTimeLimit,  // Append reasons so that existing request_end numbers stay.
};

/// The name used in messages and in --stats=json: finish, quiescent, ...
const char* endReasonName(EndReason r);

/// CLI.md 2.2: $stop ends normally, continues, or ends with a failing code.
enum class StopAction : std::uint8_t { Finish, Ignore, Error };

/// The 17 regions of a time slot, in order (SCHEDULER_SPEC.md 2.1).
enum class Region : std::uint8_t {
    Preponed,
    PreActive,
    Active,
    Inactive,
    PreNba,
    Nba,
    PostNba,
    PreObserved,
    Observed,
    PostObserved,
    Reactive,
    ReInactive,
    PreReNba,
    ReNba,
    PostReNba,
    PrePostponed,
    Postponed,
};
inline constexpr int kNumRegions = 17;

/// The region's name in counters: active, re_inactive, pre_postponed, ...
const char* regionName(Region r);

/// The set of regions a thread runs in (SCHEDULER_SPEC.md 9.5).
enum class Group : std::uint8_t { Active, Reactive };

/// The tier's root publication/collection callback. It runs only after
/// an outer region has drained, outside process and guard callbacks.
struct SafePointHook {
    void (*fn)(void*) noexcept = nullptr;
    void* frame = nullptr;
};
using PendingValueVisitor = bool (*)(void*, SlotId, std::span<const std::uint64_t>) noexcept;

/// Edge of an event-control item (VALUE_SEMANTICS.md 4.3).
enum class Edge : std::uint8_t { Any, Pos, Neg, Both };

/// What a resume function reports when it returns (RUNTIME_ABI.md 4.3).
enum class Status : std::uint8_t {
    Waiting,  ///< It registered a wait and will be resumed later.
    Done,     ///< It ran to its end.
    Killed,   ///< It ended by kill or disable.
    Error,    ///< It failed; the message was given with Scheduler::fail.
    Stop,     ///< It saw the request to end and stopped.
};

struct ProcessTag;
using ProcessId = Id<ProcessTag>;
struct ProgramTag;
using ProgramId = Id<ProgramTag>;

/// The handle an element of an event slot holds for event e: its number
/// plus 1, 0 being null (SIR_SPEC.md 3.2.1); and the event of a handle that
/// is not 0. Every reader and writer of event slots goes through these.
constexpr std::uint64_t eventHandle(EventId e) { return std::uint64_t{e.index()} + 1; }
constexpr EventId eventOfHandle(std::uint64_t h) {
    return EventId{static_cast<std::uint32_t>(h - 1)};
}

/// Runs a process from where it waited until its next wait or its end. All
/// three callback types are the C forms of abi.h, so every tier registers
/// the same way (RUNTIME_ABI.md 1, 4.3); toC() and fromC() in context.h
/// convert the context.
using ResumeFn = kirima_resume_fn;
/// The iff condition of an event-control item, read when the item's event
/// occurs (9.4.2.3). Nonzero is true.
using GuardFn = kirima_guard_fn;
/// Evaluates a monitored expression into `out` (laid out like a slot
/// element of the monitor's domain and width).
using EvalFn = kirima_eval_fn;
/// The call of a deferred assertion, run or dropped (SCHEDULER_SPEC.md
/// 15.2).
using DeferredFn = kirima_deferred_fn;

/// A part of a slot: elements [firstElem, firstElem + numElems) (numElems
/// 0: all) and bits [lsb, lsb + width) of each (width 0: all).
struct SlotPart {
    SlotId slot{0};
    std::uint32_t firstElem = 0;
    std::uint32_t numElems = 0;
    std::uint32_t lsb = 0;
    std::uint32_t width = 0;
};

/// The files a descriptor named when it was read. Each file is kept with
/// the serial of its opening, so that one closed since, or opened again
/// under the same number, is left out (SYSTEM_TASKS.md 6.2).
struct FileTargets {
    bool out = false;  ///< Standard output: bit 0 of an mcd, or STDOUT.
    bool err = false;  ///< Standard error: STDERR.
    std::vector<std::pair<std::uint32_t, std::uint64_t>> files;  ///< (row, serial)
    /// Some of the descriptor named no open file: x or z bits, an fd that
    /// is not open, or a bit of an mcd whose file is not open.
    bool notOpen = false;
};

/// One item of an event control on a fixed slot part: a subscription
/// (SCHEDULER_SPEC.md 9.5). The edge is judged on the part's lowest bit.
struct WaitItem {
    SlotPart part;
    Edge edge = Edge::Any;
    GuardFn guard = nullptr;
    void* guardFrame = nullptr;
};

/// A named-event item in the same event control as slots or watches.
struct NamedWaitItem {
    EventId event{0};
    GuardFn guard = nullptr;
    void* guardFrame = nullptr;
    bool flushPoint = true;
};
struct NamedValueWaitItem {
    EventValue value;
    GuardFn guard = nullptr;
    void* guardFrame = nullptr;
    bool flushPoint = true;
};

/// Automatic storage owned and observed by one expression monitor.
struct ActivationReadSpec {
    /// Capture executed reads only from these owned lexical extents.
    /// Supplying a binding pins its entire scope, not just that field.
    std::vector<ActivationBinding> bindings;
    /// Static implicit sensitivity; never replaced by executed reads.
    std::vector<ActivationPart> parts;
    bool capture = false;
    [[nodiscard]] bool enabled() const { return capture || !bindings.empty() || !parts.empty(); }
};

/// An event control on an expression, or a `wait (expr)`: a monitor
/// record (SCHEDULER_SPEC.md 9.5). Its dependencies schedule reevaluation.
struct MonitorSpec {
    std::vector<SlotPart> slots;
    std::vector<EventId> events;
    std::vector<EventValue> eventValues;
    bool captureEvents = false;
    EvalFn eval = nullptr;
    void* frame = nullptr;
    Domain domain = Domain::Four;  ///< Two, Four or Real.
    std::uint32_t width = 1;
    Edge edge = Edge::Any;   ///< For an event control.
    bool untilTrue = false;  ///< `wait (expr)`: wake when the value is true.
    /// An event control item's iff, read when the item's event occurs
    /// (9.4.2.3); null for none.
    GuardFn guard = nullptr;
    void* guardFrame = nullptr;
    bool heap = false;  ///< Capture the expression's typed heap reads on each evaluation.
    HeapResult<bool> (*heapReadFilter)(const void*, HeapRoot) noexcept = nullptr;
    const void* heapReadFrame = nullptr;
    ActivationReadSpec activation;
};

/// Ordered event items sharing one independent countdown. These descriptors
/// create no language process and never suspend their issuing process.
using NbaEventItem = std::variant<WaitItem, NamedValueWaitItem, MonitorSpec>;

/// Typed heap reads of a display monitor are captured only while its body
/// actually runs in Postponed, without an extra expression evaluation.
struct DisplayReadSpec {
    bool heap = false;
    HeapResult<bool> (*filter)(const void*, HeapRoot) noexcept = nullptr;
    const void* frame = nullptr;
};

/// The levels of a procedural continuous assignment; force is above
/// assign (SCHEDULER_SPEC.md 14).
enum class Hold : std::uint8_t { Assign, Force };

/// Limits of a run (CLI.md 2.2, 2.6, 2.7). 0 means no limit for the counts.
struct Limits {
    std::uint64_t maxTime = UINT64_MAX;
    std::uint64_t maxZeroDelay = 0;    ///< Moves into Active or Reactive per time slot.
    std::uint64_t maxSlotEvents = 0;   ///< Events run per time slot, in any region.
    std::uint64_t maxProcs = 1000000;  ///< Unfinished children made at run time (CLI.md 2.7).
};

/// The scheduler of one Context.
/// One owned-by-copy bit of a delayed driver's complete output state.
/// The target is a scalar two-state bit, separate from the primary value.
struct DelayedSideUpdate {
    SlotBits at;
    bool value = false;
};

class Scheduler {
public:
    Scheduler(Context& ctx, Store& store, Counters& counters);
    ~Scheduler();

    void setLimits(const Limits& l) { limits_ = l; }
    SafePointHook setSafePointHook(SafePointHook hook) noexcept;
    /// Typed interpretation belongs to the tier. The visitor sees only
    /// owned values: NBA snapshots, pending drivers and held storage.
    bool visitPendingValues(PendingValueVisitor visitor, void* frame) const noexcept;
    /// Tiers call after publishing a heap mutation, before the next SIR
    /// instruction. Native callbacks also drain at their return/wait boundary.
    /// During a monitor/guard walk, notices wait in the shared change queue.
    void flushHeapChanges() noexcept;
    /// Observable automatic writes flush notices at the same instruction
    /// boundary as heap writes. No callback runs inside the store's hook.
    ActivationError writeActivation(const ActivationBinding& binding, std::uint32_t element,
                                    std::span<const W4> value, std::uint32_t lsb = 0,
                                    std::uint32_t width = 0) noexcept;
    [[nodiscard]] bool heapMonitorLimited() const noexcept { return heapMonitorLimited_; }
    [[nodiscard]] ProcessId failedHeapMonitorProcess() const noexcept {
        return heapMonitorProcess_;
    }
    /// Starts the optional wall-clock budget, excluding front-end work.
    void setWallTimeLimit(std::uint64_t seconds, std::chrono::steady_clock::time_point start) {
        wallTimeLimit_ = seconds;
        wallStarted_ = start;
    }
    void setStopAction(StopAction action) { stopAction_ = action; }
    /// A $stop(2) that continued still requests statistics at the eventual end.
    [[nodiscard]] bool ignoredStopStatistics() const { return ignoredStopStatistics_; }
    /// The flag that the driver's SIGINT handler sets (CLAUDE.md,
    /// RUNTIME_ABI.md 4.4); nothing when no handler is installed.
    void setInterruptFlag(const volatile std::sig_atomic_t* flag) { interruptFlag_ = flag; }
    /// Takes SIGINT or an expired wall-clock budget at a safe point
    /// (SCHEDULER_SPEC.md 16.2). Taken once so final procedures can complete.
    bool takeInterrupt() {
        if (interruptTaken_) return false;
        if (interruptFlag_ != nullptr && *interruptFlag_ != 0) {
            interruptTaken_ = true;
            requestEnd(EndReason::Interrupted, 1);
            return true;
        }
        if (wallTimeLimit_ != 0 && !runningFinal_) return takeWallTimeLimit();
        return false;
    }

    /// Registers a process; it does not run until start() is called. It
    /// may get the place of a released process (SCHEDULER_SPEC.md 10.8).
    ProcessId addProcess(ResumeFn fn, void* frame, Group group);
    /// Program execution and lifetime (24.3, 24.3.1, 24.7). Register every
    /// initial before run(); programs with no initials do not cause finish.
    ProgramId addProgram();
    ProcessId addProgramInitial(ResumeFn fn, void* frame, ProgramId program);
    /// A fork child inherits its caller's group and program-initial origin.
    /// At the live-child limit, returns kNone and requests ResourceLimit
    /// before registering the rejected child (CLI.md 2.7).
    ProcessId addChildProcess(ResumeFn fn, void* frame, ProcessId parent);
    /// $exit ends the originating program; without such an origin, no-op.
    void exitProgram(ProcessId caller);
    /// Whether program completion canceled this execution. Natural process
    /// completion alone does not cancel its already confirmed report calls.
    [[nodiscard]] bool programStopped(ProcessId p) const {
        return processes_[p.index()].programStopped;
    }
    /// Processes ended by program completion, for tier frame bookkeeping.
    bool takeEndedProgramProcess(ProcessId& process);
    [[nodiscard]] Group processGroup(ProcessId p) const { return processes_[p.index()].group; }
    /// Gives back the record of a process that has ended and holds no
    /// report (SCHEDULER_SPEC.md 10.8). Not while the process is running:
    /// the kernel marks it ended after it returns. A caller must preserve its
    /// frame while hasCallbackOrigins is true; release then has no effect.
    void releaseProcess(ProcessId p);
    /// Whether a process holds reports not yet confirmed (SCHEDULER_SPEC.md 15).
    [[nodiscard]] bool hasPending(ProcessId p) const {
        return !processes_[p.index()].pending.empty();
    }
    [[nodiscard]] bool hasCallbackOrigins(ProcessId p) const {
        return processes_[p.index()].nbaOrigins != 0;
    }
    [[nodiscard]] std::uint64_t callbackReleaseEpoch() const { return callbackReleaseEpoch_; }
    /// Schedules a process to run in the current time slot, in Active or
    /// Reactive by its group.
    void start(ProcessId p);
    /// Creates a named event object.
    EventId addEvent();
    /// Object allocator/resetter for the Context's managed-event registry.
    /// Its runtime users must pin objects until subscriptions/updates detach.
    ManagedEventBackend managedEventBackend() noexcept;
    /// Ends a process that is not running now (kill, disable, disable
    /// fork): it is never resumed again. Ending a finished process does
    /// nothing.
    void kill(ProcessId p);
    /// Whether a process has finished or been killed.
    [[nodiscard]] bool finished(ProcessId p) const { return processes_[p.index()].done; }
    /// A process that its tier ran outside the scheduler, as static-init
    /// before time 0 starts (SCHEDULER_SPEC.md 6.1), has run to its end: as
    /// when a resumption returns Status::Done. What it put on its pending
    /// queue is settled as for any process (15.3).
    void ended(ProcessId p);
    /// The entries the wait lists hold, live ones and those that waits left
    /// when another of their items woke them: slot subscriptions, monitor
    /// references from slots and named events, and named-event waiters
    /// (SCHEDULER_SPEC.md 9.5). For tests.
    [[nodiscard]] std::size_t waitListEntries() const;

    // Waits. A running process calls one of these and then returns
    // Status::Waiting. A later wait replaces nothing: a process has one
    // wait at a time.
    void waitDelay(ProcessId p, std::uint64_t ticks);
    void waitEvent(ProcessId p, std::span<const WaitItem> items);
    /// One wait combines ordinary value events with internal range changes.
    /// Any match cancels all alternatives through the same process generation.
    StrengthError waitEventStrength(ProcessId p, std::span<const WaitItem> items,
                                    std::span<const SlotPart> strengths) noexcept;
    void waitMonitor(ProcessId p, const MonitorSpec& m);
    /// An event control whose items are subscriptions and monitor records:
    /// the first item whose event occurs wakes the process and ends the
    /// others (SCHEDULER_SPEC.md 9.5).
    void waitAny(ProcessId p, std::span<const WaitItem> items,
                 std::span<const MonitorSpec> monitors, std::span<const NamedWaitItem> named = {});
    /// `flushPoint`: whether the trigger that ends the wait is a flush point
    /// of the process's pending reports (SCHEDULER_SPEC.md 15.3, 15.4): true
    /// for an event control on a named event, false for a wait for children
    /// to end (join, join_any, wait fork).
    void waitNamed(ProcessId p, EventId e, bool flushPoint = true);
    /// Typed named-event values are resolved once before registration. No
    /// wait changes on null/invalid input or staging allocation failure.
    ManagedEventError waitAnyValues(ProcessId p, std::span<const WaitItem> items,
                                    std::span<const MonitorSpec> monitors,
                                    std::span<const NamedValueWaitItem> named) noexcept;
    ManagedEventError waitNamedValue(ProcessId p, EventValue value,
                                     bool flushPoint = true) noexcept;

    /// Writes the bits `at` names from `value`, laid out like a slot element
    /// from bit 0, and wakes the processes whose subscriptions and monitors
    /// see a change. Returns whether the value changed.
    bool write(SlotBits at, const std::uint64_t* value);
    /// Registers complete initial intervals without writing or resolving the
    /// slot. Their projection must match every current four-state bit.
    /// Held slots are not connected to this publisher yet.
    StrengthError enableStrengths(SlotId slot, std::span<const StrengthRange> initial) noexcept;
    /// One committed component snapshot: copy/validate all inputs, publish
    /// all intervals and values, then announce changes. No overlapping parts.
    /// Range-only changes have internal notices but no ordinary value notices.
    /// A resource failure reports whether the whole snapshot had committed;
    /// never retry it after commitment. No partial snapshot is published.
    StrengthPublication publishStrengths(std::span<const StrengthUpdate> updates) noexcept;

    /// Copies a static node/origin/passive-edge bank once. No writes,
    /// resolution, notifications or processes on registration. Implicit roots
    /// must be explicit origins: node kinds are wire/tri, wand/triand, wor/trior
    /// or uwire, not tri0/tri1/supply0/supply1. Force remains separate.
    StrengthError configureStrengthDrivers(std::span<const StrengthNodeSpec> nodes,
                                           std::span<const StrengthOriginSpec> origins,
                                           std::span<const StrengthEdgeSpec> edges = {}) noexcept;
    /// Queue initial resolution in Active before starting device evaluations.
    StrengthError startStrengthDrivers() noexcept;
    /// Stable origin ID is its ordinal in the copied origin table. No borrowed
    /// values, process ownership or synthetic fork children. All changes queue
    /// the owning component once in Active, even when issued in Reactive.
    StrengthError driveStrength(std::uint32_t origin, StrengthRange value) noexcept;
    /// Scalar full-range inertial state. Same pending range retains its due
    /// time; a different result cancels it; committed reversion schedules none.
    /// A new result uses its projected destination delay, including same-value
    /// strength changes. Explicit zero commits in Inactive, then queues Active.
    StrengthError driveStrengthDelayed(std::uint32_t origin, StrengthRange value,
                                       std::uint64_t rise, std::uint64_t fall,
                                       std::uint64_t off) noexcept;

    /// Stable edge ID is its copied table ordinal. Conduction changes queue
    /// its component in Active. Transport through a committed edge is immediate.
    StrengthError setStrengthEdge(std::uint32_t edge, StrengthConduction value) noexcept;
    /// Delay on/off conduction, not data; uncertainty uses min(on, off).
    /// Own the complete three-way state with the same inertial rules as roots.
    StrengthError setStrengthEdgeDelayed(std::uint32_t edge, StrengthConduction value,
                                         std::uint64_t on, std::uint64_t off) noexcept;

    // Procedural continuous assignments (SCHEDULER_SPEC.md 14, SIR_SPEC.md
    // 5.6). A marked slot keeps, bit by bit, who holds it at each level and
    // the value that level gives, and the value its ordinary writes give.
    // Its value is that of force where a force holds it, else that of
    // assign where an assign holds it, else that of the ordinary writes.

    /// Marks slot s (integral) as one that assign or force may hold
    /// (SCHEDULER_SPEC.md 14.4): a net, or a variable, which a continuous
    /// assignment may drive (`driven`). Writes to unmarked slots never look
    /// at holds.
    void markHeld(SlotId s, bool net, bool driven);
    /// Process `owner` holds the bits of `part` at `level` from now on,
    /// taking them from any other holder at that level. The value changes
    /// when the owner writes it.
    void hold(Hold level, ProcessId owner, const SlotPart& part);
    /// Writes the bits of the part (laid out as in write()) that `owner`
    /// holds at `level`.
    void writeHeld(Hold level, ProcessId owner, SlotBits at, const std::uint64_t* value);
    /// Ends the holds at `level` on `part` (10.6.1, 10.6.2). A net takes its
    /// drivers' value now. A variable that an assign still holds, or that
    /// a continuous assignment drives, takes that value when it is
    /// evaluated again: in the Active region. Any other variable keeps its
    /// value until the next write.
    void release(Hold level, const SlotPart& part);
    /// Whether `owner` holds any bit at `level`.
    [[nodiscard]] bool holdsAny(Hold level, ProcessId owner) const;
    /// Drops the wait of a process that is not running, without ending it:
    /// it runs again only when started, or when it waits again and is
    /// woken.
    void cancelWait(ProcessId p);

    /// A driver whose writes are delayed (SCHEDULER_SPEC.md 12): a
    /// continuous assignment, or the output of a gate or a UDP. It has at
    /// most one update pending.
    std::uint32_t addDelayedDriver();
    /// drive.delay (SIR_SPEC.md 5.4): the bits `at` names, laid out as in
    /// write(), take `value` after the
    /// delay for the change from their value now (10.3.3, 28.16): to 1
    /// `rise`, to 0 `fall`, to z `off`, to x the least of the three, for a
    /// target of one bit; for a wider one, `fall` from a value that is not 0
    /// to 0, `off` to all z, else `rise`. A pending update of another value
    /// is dropped first; none is scheduled when the value is the one there
    /// now. Updates after 0 ticks go to the Inactive region.
    /// Optional side fields are copied into the same pending state. Equality
    /// includes every field, preserves an identical pending deadline, and
    /// commits all fields before any callbacks (approved 2026-10-25).
    void driveDelayed(std::uint32_t driver, SlotBits at, const std::uint64_t* value,
                      std::uint64_t rise, std::uint64_t fall, std::uint64_t off,
                      std::span<const DelayedSideUpdate> sides = {});

    /// Schedules a nonblocking update, `delay` ticks from now, in the NBA
    /// region of the scheduling process's group (SCHEDULER_SPEC.md 13).
    void scheduleNba(ProcessId p, SlotBits at, const std::uint64_t* value, std::uint64_t delay);
    /// Stage an ordered payload without suspending or retaining a process.
    /// Each value is copied before publication; empty payloads are allowed.
    /// The owner of event callbacks separately retains their origin/frame.
    /// Invalid selections are represented by omitted leaves, not invalid ranges.
    NbaCapture captureNba(ProcessId origin, std::span<const NbaWrite> writes) noexcept;
    /// Publish an older captured statement into the pending NBA/Re-NBA suffix.
    /// Group and issue order come from capture, independent of the caller.
    NbaError readyNba(NbaTicket ticket) noexcept;
    /// Discard an unready capture. Ready/executing payloads are committed.
    NbaError discardNba(NbaTicket ticket) noexcept;
    [[nodiscard]] NbaStorageStats nbaStorageStats() const noexcept;
    /// Bind a captured payload to an independent persistent concrete control.
    /// All descriptors/counts/pins are staged before publication. onRelease is
    /// transferred only on acceptance, and runs after the last callback returns.
    /// Accepted initialization may already cancel the control or end execution;
    /// runtime callback failures use reason(), and still return an accepted ticket.
    /// Cancellation cleanup rejects new registrations until cancellation ends.
    NbaEventRegistration registerNbaEvent(ProcessId origin, NbaTicket payload, NbaRepeat repeat,
                                          std::span<const NbaEventItem> items,
                                          SafePointHook onRelease = {}) noexcept;
    NbaError discardNbaEvent(NbaEventTicket ticket) noexcept;
    void cancelNbaEvents() noexcept;
    [[nodiscard]] NbaEventStats nbaEventStats() const noexcept;
    /// `-> e`: wakes the processes waiting for e now.
    void trigger(EventId e);
    /// `->> e`: triggers e in the NBA region, `delay` ticks from now.
    void triggerNb(ProcessId p, EventId e, std::uint64_t delay);
    /// Whether e was triggered in the current time slot (15.5.3).
    [[nodiscard]] bool triggered(EventId e) const;
    [[nodiscard]] ManagedEventResult<EventId> resolveEventValue(EventValue value) const noexcept;
    ManagedEventError triggerValue(EventValue value) noexcept;
    ManagedEventError triggerNbValue(ProcessId p, EventValue value, std::uint64_t delay) noexcept;
    /// An unrepresentable normalized delay overflows only for a nonnull target.
    ManagedEventError triggerNbValue(ProcessId p, EventValue value,
                                     std::optional<std::uint64_t> delay) noexcept;
    ManagedEventResult<bool> triggeredValue(EventValue value) noexcept;
    /// Cancel managed pending operations/subscriptions. A callback currently
    /// executing retains its local pin until it returns.
    void cancelManagedEventOperations() noexcept;

    /// Puts a report (a whole line of the messages) on the pending queue of the
    /// running process p (SCHEDULER_SPEC.md 15.3). The queue is dropped
    /// when the process resumes from an event control or wait (a flush
    /// point), and confirmed when the time slot reaches the Observed
    /// region.
    void deferReport(ProcessId p, std::string line);
    /// report.call (SIR_SPEC.md 5.7): puts the call of a deferred
    /// assertion on the same queue. An Observed one (`final` false) is
    /// confirmed with the reports and runs in the Reactive region; a final
    /// one is confirmed in the Postponed region and runs there. `fn` gets
    /// `frame` and `token`, and run 0 when the call is dropped.
    void deferCall(ProcessId p, bool final, DeferredFn fn, void* frame, std::uint32_t token);

    // $strobe and $monitor (SYSTEM_TASKS.md 4, SIR_SPEC.md 7.1): processes
    // that run only in the Postponed region, where they display. The
    // monitor runs first, then the strobes in the order they were asked
    // for. Each run starts the process afresh and must not wait.

    /// strobe @P: p runs once in the Postponed region of this time slot
    /// (21.2.2).
    void strobe(ProcessId p);
    /// strobe @P, d: the same, printing to `to` (SYSTEM_TASKS.md 6.2).
    void strobe(ProcessId p, FileTargets to);
    /// monitor @P: p becomes the one monitor (21.2.3). It runs in the
    /// Postponed region of this time slot, and of every later time slot in
    /// which a bit of `parts` changes, while monitoring is on.
    void setMonitor(ProcessId p, std::span<const SlotPart> parts);
    void setMonitor(ProcessId p, std::span<const SlotPart> parts, DisplayReadSpec reads);
    /// monitor @P, d: adds a file monitor printing to `to`, which runs as
    /// the monitor does; any number run at once, $monitoroff stops none,
    /// and one ends when none of `to` is open any more (SYSTEM_TASKS.md 6.2).
    void addFileMonitor(ProcessId p, std::span<const SlotPart> parts, FileTargets to);
    void addFileMonitor(ProcessId p, std::span<const SlotPart> parts, FileTargets to,
                        DisplayReadSpec reads);
    /// Detach only registrations belonging to this callback process.
    void detachDisplayMonitor(ProcessId p) noexcept;
    /// A file monitor that ended because none of its files was open any
    /// more (SYSTEM_TASKS.md 6.2), for the tier to release its frame. False
    /// when no such monitor is waiting. The process is already ended.
    bool takeEndedFileMonitor(ProcessId& ended);
    /// $monitoron and $monitoroff: monitoring on (the default) or off.
    /// Turning it on runs the monitor in this time slot's Postponed region.
    void setMonitoring(bool on);
    /// While a strobe or a monitor with a descriptor runs: the files it
    /// prints to; otherwise null, for standard output.
    [[nodiscard]] const FileTargets* printingTo() const { return printingTo_; }

    /// Runs the simulation until it ends, and returns the reason.
    EndReason run();
    /// After run(): runs the final procedures once each, in the order
    /// given, as calls from one process with no time passing; nothing they
    /// schedule runs, and $finish in one of them skips the rest
    /// (SCHEDULER_SPEC.md 6.2, 16.2). The end reason stays the one run()
    /// gave, unless a final procedure fails. Not run after an internal
    /// error.
    void runFinal(std::span<const ProcessId> finals);

    /// Asks the simulation to end. `level` is the argument of $finish or
    /// $stop (0, 1 or 2); it decides what the termination line prints.
    void requestEnd(EndReason r, int level);
    /// Ends the simulation with an internal error.
    void fail(std::string message);
    /// Ends the simulation with a run-time error of the design: the reason
    /// error, code 2, and final still runs (SCHEDULER_SPEC.md 16.2, CLI.md
    /// 5). The tier that found it has written its line.
    void runtimeError() { requestEnd(EndReason::Error, 1); }
    /// Ends the simulation at a limit on resources (CLI.md 2.7): the
    /// reason resource_limit, code 6, and final still runs (SCHEDULER_SPEC.md
    /// 16.2). The tier that reached it has written its line.
    void resourceLimit() { requestEnd(EndReason::ResourceLimit, 1); }
    /// Whether the Postponed region of the time slot is running, where
    /// nothing may be written or scheduled (SCHEDULER_SPEC.md 2.2).
    [[nodiscard]] bool inPostponed() const { return postponed_; }
    /// Whether something scheduled since the last call had a delay that took
    /// the time past 64 bits, and was dropped (SCHEDULER_SPEC.md 8): the
    /// tier reports it as a run-time error at its instruction.
    [[nodiscard]] bool takeOverflow() {
        const bool o = overflow_;
        overflow_ = false;
        return o;
    }
    /// Which zero-time limit stopped the run, for the line the driver
    /// prints before the end-reason line (CLI.md 2.6). Empty otherwise.
    void setEndNote(std::string note) {
        if (endNote_.empty()) endNote_ = std::move(note);
    }
    [[nodiscard]] const std::string& endNote() const { return endNote_; }

    [[nodiscard]] bool ending() const { return ending_; }
    /// RUNTIME_ABI 6's event-only exception excludes termination/final.
    /// The caller must still publish complete typed roots and operation pins.
    [[nodiscard]] bool permitsEventCollection() const noexcept {
        return !ending_ && !runningFinal_;
    }
    [[nodiscard]] EndReason reason() const { return ending_ ? reason_ : EndReason::Quiescent; }
    [[nodiscard]] int level() const { return level_; }
    [[nodiscard]] std::uint64_t time() const { return time_; }
    [[nodiscard]] const std::string& error() const { return error_; }

private:
    friend class StrengthStorage;
    void strengthChanged(std::uint32_t process, std::uint64_t generation) noexcept;
    bool takeWallTimeLimit();
    void deliverStrengthDrive(std::uint32_t origin, std::uint64_t generation) noexcept;
    void deliverStrengthEdge(std::uint32_t edge, std::uint64_t generation) noexcept;
    bool takeStrengthInterrupt() noexcept;
    bool solvePassiveStrengthComponent(std::uint32_t component) noexcept;
    bool solvePassiveStrengthIsland(std::uint32_t island) noexcept;
    void resolveStrengthComponent(std::uint32_t component) noexcept;
    StrengthError queueStrengthComponent(std::uint32_t component) noexcept;
    /// Show: a released held variable shows its value (id: the slot, gen:
    /// the element). Drive: a delayed driver's update (id: the driver).
    /// Postponed: a strobe's run (id: the process). Deferred: a deferred
    /// assertion's call (id: its row in deferredCalls_).
    enum class Kind : std::uint8_t {
        Resume,
        Nba,
        NbaDelayed,
        NbaCaptured,
        TriggerNb,
        TriggerValue,
        Show,
        Drive,
        StrengthDrive,
        StrengthResolve,
        StrengthEdge,
        Postponed,
        Deferred
    };
    template <typename State, typename Value>
    StrengthError setStrengthState(State& state, Value value, std::uint32_t component) noexcept;
    template <typename State, typename Value>
    StrengthError delayStrengthState(State& state, Value value, std::uint64_t delay, Kind kind,
                                     std::uint32_t id) noexcept;
    template <typename State>
    void commitStrengthState(State& state, std::uint32_t component,
                             std::uint64_t generation) noexcept;
    /// An entry of a process's pending queue: a unique or priority
    /// violation's line, or a deferred assertion's call.
    struct PendingReport {
        std::string line;
        DeferredFn fn = nullptr;
        void* frame = nullptr;
        std::uint32_t token = 0;
        bool final = false;
    };
    struct Event {
        Kind kind = Kind::Resume;
        Region origin = Region::Active;  ///< The region it was scheduled in.
        std::uint32_t id = 0;            ///< Process, NBA record or event object.
        /// Resume wait number, ordinary NBA issue or captured-payload generation.
        std::uint64_t gen = 0;
    };
    struct Future {
        std::uint64_t time = 0;
        std::uint64_t seq = 0;
        Region region = Region::Active;
        Event event;
    };
    struct EventBinding {
        EventValue value;
        EventId event = kNone;
        ManagedEventPin pin;
    };
    struct EventCapture {
        explicit EventCapture(std::pmr::memory_resource* memory) : reads(memory) {}
        std::pmr::vector<EventBinding> reads;
        ManagedEventError error = ManagedEventError::None;
    };
    struct Process {
        explicit Process(std::pmr::memory_resource* memory) noexcept
            : eventPins(memory), strengthWaits(memory) {}
        ResumeFn fn = nullptr;
        void* frame = nullptr;
        Group group = Group::Active;
        ProgramId program = kNone;  ///< Dynamic initial origin, not lexical scope.
        bool programInitial = false;
        bool programStopped = false;
        std::uint64_t gen = 0;                ///< Number of the current wait.
        std::vector<std::uint32_t> monitors;  ///< Monitor records of the current wait.
        std::pmr::vector<ManagedEventPin> eventPins;
        std::pmr::vector<StrengthWatchToken> strengthWaits;
        bool done = false;
        bool runtimeCreated = false;  ///< Counts until done, independent of record retention.
        bool flushOnResume = false;   ///< Woken by an event: a flush point.
        std::vector<PendingReport> pending;  ///< Pending reports (SCHEDULER_SPEC.md 15.3).
        std::uint32_t observed = 0;          ///< Those confirmed in the Observed region.
        std::uint32_t nbaOrigins = 0;  ///< Pending controls/current callbacks, not live children.
        std::uint64_t serial = 0;      ///< Creation order, for 15.4 (10.8).
    };
    struct Program {
        std::uint32_t initials = 0;
        bool ended = false;
    };
    std::vector<Program> programs_;
    std::uint32_t activePrograms_ = 0;
    std::vector<ProcessId> endedProgramProcesses_;
    void markProcessDone(ProcessId p);
    void initialEnded(ProcessId p);
    void completeProgram(ProgramId program);

    struct Subscription {
        ProcessId process{0};
        std::uint64_t gen = 0;
        WaitItem item;
    };
    struct Monitor {
        explicit Monitor(std::pmr::memory_resource* memory)
            : eventBindings(memory), heapReads(memory), activationLeases(memory),
              activationParts(memory), activationReads(memory) {}
        ProcessId process{0};
        std::uint32_t control = kNone;  ///< Independent NBA owner, or an ordinary process wait.
        std::uint64_t gen = 0;          ///< Process wait number, or independent control serial.
        std::uint64_t serial = 0;       ///< Changes when the record is reused.
        MonitorSpec spec;
        std::pmr::vector<EventBinding> eventBindings;
        std::vector<std::uint64_t> last;
        std::pmr::vector<HeapRoot> heapReads;
        std::pmr::vector<ActivationLease> activationLeases;
        std::pmr::vector<ActivationRegion> activationParts, activationReads;
        bool hooked = false;
    };
    /// A reference to a monitor record from a slot or an event; stale when
    /// the record's serial has changed.
    struct MonitorRef {
        std::uint32_t id = 0;
        std::uint64_t serial = 0;
    };
    struct ActivationCapture {
        MonitorRef monitor;
        const std::pmr::vector<ActivationRegion>* reads = nullptr;
        ActivationCapture* previous = nullptr;
    };
    struct HeapCapture {
        MonitorRef monitor;
        const std::pmr::vector<HeapRoot>* reads = nullptr;
        HeapCapture* previous = nullptr;
    };
    struct Waiter {
        ProcessId process{0};
        std::uint64_t gen = 0;
        bool flushPoint = true;
        GuardFn guard = nullptr;
        void* guardFrame = nullptr;
    };
    struct NamedEvent {
        std::vector<Waiter> waiters;
        std::vector<MonitorRef> monitors;
        std::uint32_t waiterLimit = 0;  ///< Sizes that start a sweep (pushSwept).
        std::uint32_t monitorLimit = 0;
        std::uint64_t triggeredAt = UINT64_MAX;
        bool wasTriggered = false;
        bool managed = false;
    };
    /// An NBA update. Without a delay, the value lives in the time slot's
    /// arena, freed all at once when the slot ends; with a delay, in the
    /// record itself, freed when the update runs (SCHEDULER_SPEC.md 5).
    struct NbaRecord {
        SlotId slot{0};
        std::uint32_t elem = 0;
        std::uint32_t lsb = 0;
        std::uint32_t width = 0;
        std::uint64_t arenaOffset = 0;
        std::uint32_t words = 0;
        std::vector<std::uint64_t> value;  ///< Delayed updates only.
    };

    /// A slot that assign or force may hold. `base` and `value` are laid
    /// out like the slot's storage; `owner` has one entry per bit, element
    /// by element: the holding process, or kNoHolder.
    struct Held {
        bool net = false;
        bool driven = false;
        std::vector<std::uint64_t> base;
        std::vector<std::uint64_t> value[2];
        std::vector<ProcessId> owner[2];
    };
    /// A delayed driver: where it writes and its pending update, if any;
    /// gen numbers the update, so a dropped one is stale when it comes. It
    /// is 64 bits, as Event::gen is: a 32-bit one would come round to a
    /// dropped update's number in a long run (#36).
    struct DelayedDriver {
        SlotId slot{0};
        std::uint32_t elem = 0;
        std::uint32_t lsb = 0;
        std::uint32_t width = 0;
        bool pending = false;
        std::uint64_t gen = 0;
        std::vector<std::uint64_t> value;
        std::vector<DelayedSideUpdate> sides;
    };
    static_assert(sizeof(DelayedDriver::gen) == sizeof(Event::gen));
    std::vector<DelayedDriver> drivers_;
    static constexpr std::uint32_t kNotHeld = UINT32_MAX;
    static constexpr ProcessId kNoHolder = kNone;

    static constexpr std::uint64_t kWheel = 256;

    Context& ctx_;
    Store& store_;
    Counters& counters_;
    Limits limits_;
    SafePointHook safePoint_;
    void safePoint() noexcept;

    std::vector<Process> processes_;
    /// Released places to reuse, and the number of records kept (10.8).
    std::vector<ProcessId> freeProcesses_;
    std::uint64_t kept_ = 0;
    std::uint64_t liveRuntime_ = 0;  ///< Unfinished runtime children, not retained records.
    std::uint64_t nextSerial_ = 0;
    /// A released place whose wait number reached this is not reused, so
    /// that the next process has room to count its waits (10.8).
    static constexpr std::uint64_t kReuseGenLimit = std::uint64_t{1} << 63;
    std::deque<Event> regions_[kNumRegions];
    std::vector<std::vector<Future>> wheel_ = std::vector<std::vector<Future>>(kWheel);
    std::uint64_t wheelCount_ = 0;
    std::vector<Future> heap_;  ///< 4-ary min-heap by (time, region, seq).
    std::uint64_t seq_ = 0;

    std::vector<std::vector<Subscription>> subscriptions_;  ///< By slot.
    /// By slot: the sizes of subscriptions_ and slotMonitors_ that start a
    /// sweep of their stale entries (pushSwept).
    std::vector<std::uint32_t> subscriptionLimit_;
    std::vector<std::uint32_t> slotMonitorLimit_;
    std::vector<Monitor> monitors_;
    std::uint32_t freeMonitors_ = kNone;
    std::vector<std::vector<MonitorRef>> slotMonitors_;  ///< By slot.
    std::uint64_t monitorSerial_ = 0;
    std::vector<NamedEvent> events_;
    struct PendingEventValue {
        ManagedEventPin pin;
        EventId event = kNone;
        std::uint64_t serial = 0;
        std::uint32_t next = kNone;
        bool pending = false;
    };
    std::pmr::vector<PendingEventValue> pendingEventValues_;
    std::uint32_t freeEventValue_ = kNone;
    EventCapture* eventCapture_ = nullptr;
    bool usedEventValues_ = false;
    ManagedEventResult<EventBinding> bindEventValue(EventValue value) noexcept;
    bool observesEvent(MonitorRef ref, EventId event) const;
    void attachEventMonitor(EventId event, MonitorRef ref);
    bool publishEventReads(std::uint32_t id, EventCapture& capture);
    void releaseEventValue(std::uint32_t id) noexcept;
    void deliverEventValue(const Event& event);
    std::vector<NbaRecord> slotNbas_;     ///< Updates without a delay, this slot.
    std::vector<std::uint64_t> arena_;    ///< Their values.
    std::vector<NbaRecord> delayedNbas_;  ///< Updates with a delay.
    std::vector<std::uint32_t> freeDelayedNbas_;
    enum class NbaState : std::uint8_t { Free, Waiting, Ready, Executing };
    struct NbaField {
        SlotBits target;
        std::size_t offset = 0;
        std::uint32_t words = 0;
    };
    struct CapturedNba {
        explicit CapturedNba(std::pmr::memory_resource* memory) : fields(memory), value(memory) {}
        NbaState state = NbaState::Free;
        Group group = Group::Active;
        std::uint64_t issue = 0;
        std::uint64_t serial = 0;
        std::uint32_t next = kNone;
        std::uint32_t control = kNone;
        std::pmr::vector<NbaField> fields;
        std::pmr::vector<std::uint64_t> value;
    };
    std::pmr::vector<CapturedNba> capturedNbas_;
    std::uint32_t freeCapturedNba_ = kNone;
    std::uint64_t nextNbaIssue_ = 1;
    [[nodiscard]] bool liveNba(NbaTicket ticket, NbaState state) const noexcept;
    void releaseCapturedNba(std::uint32_t index) noexcept;
    void cancelCapturedNbas() noexcept;
    void deliverCapturedNba(NbaTicket ticket);
    void enqueueNba(Region region, Event event, std::uint64_t issue);
    enum class NbaEventState : std::uint8_t { Free, Active, Retiring, Releasing };
    struct NbaBoundItem {
        bool named = false;
        bool monitored = false;
        WaitItem slot;
        EventBinding binding;
        MonitorRef monitor;
        GuardFn guard = nullptr;
        void* frame = nullptr;
    };
    struct NbaEventRecord {
        explicit NbaEventRecord(std::pmr::memory_resource* memory)
            : remaining(memory), items(memory), slots(memory), events(memory), monitors(memory) {}
        NbaEventState state = NbaEventState::Free;
        std::uint64_t serial = 0, issue = 0;
        std::uint32_t next = kNone, callbacks = 0;
        ProcessId origin = kNone;
        NbaTicket payload;
        SafePointHook release;
        bool managed = false, ready = false;
        std::pmr::vector<std::uint64_t> remaining;
        std::pmr::vector<NbaBoundItem> items;
        std::pmr::vector<std::uint32_t> slots, events;
        std::pmr::vector<MonitorRef> monitors;
    };
    std::pmr::vector<NbaEventRecord> nbaEvents_;
    std::pmr::vector<std::pmr::vector<NbaEventTicket>> nbaEventSlots_, nbaEventNames_;
    std::uint32_t freeNbaEvent_ = kNone;
    std::uint64_t liveNbaEvents_ = 0, callbackReleaseEpoch_ = 0;
    bool cancelingNbaEvents_ = false;
    [[nodiscard]] bool liveNbaEvent(NbaEventTicket ticket) const noexcept;
    void finishNbaEvent(NbaEventTicket ticket, bool ready) noexcept;
    void releaseNbaEvent(NbaEventTicket ticket) noexcept;
    void endNbaEventCallback(NbaEventTicket ticket) noexcept;
    void notifyNbaSlot(SlotId slot, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
                       const std::uint64_t* old, const std::uint64_t* now) noexcept;
    void notifyNbaNamed(EventId event) noexcept;
    enum class NbaSource : std::uint8_t { Slot, Named, Monitors };
    struct NbaMonitorNotice {
        explicit NbaMonitorNotice(std::pmr::memory_resource* memory) : monitors(memory) {}
        std::pmr::vector<MonitorRef> monitors;
        std::uint64_t serial = 0;
        std::uint32_t next = kNone;
        bool pending = false;
    };
    std::pmr::vector<NbaMonitorNotice> nbaNotices_;
    std::pmr::vector<MonitorRef> nbaHeapPending_;
    std::uint32_t freeNbaNotice_ = kNone;
    void queueNbaMonitorNotice(std::pmr::vector<MonitorRef>& monitors);
    void deliverNbaMonitorNotice(MonitorRef notice);
    void releaseNbaMonitorNotice(std::uint32_t index) noexcept;
    void cancelNbaMonitorNotices() noexcept;
    void matchNbaEvents(std::pmr::vector<NbaEventTicket>& candidates, NbaSource kind,
                        std::uint32_t source, std::uint32_t elem, std::uint32_t lsb,
                        std::uint32_t width, const std::uint64_t* old, const std::uint64_t* now,
                        std::span<const MonitorRef> monitors = {});
    void initializeNbaMonitors(NbaEventTicket ticket);
    void notifyNbaMonitors(std::span<const MonitorRef> monitors);
    [[nodiscard]] bool nbaMonitorDepends(MonitorRef monitor, NbaSource kind, std::uint32_t source,
                                         std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
                                         std::span<const MonitorRef> monitors) const;
    void cancelManagedNbaEvents() noexcept;
    static bool slotEventMatches(const WaitItem& item, SlotId slot, const SlotLayout& layout,
                                 std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
                                 const std::uint64_t* old, const std::uint64_t* now);
    std::vector<std::uint64_t> old_;  ///< Scratch: an element before a write.
    std::vector<std::uint64_t> now_;  ///< Scratch: the element after it.
    /// A change of a watched slot, or a trigger, made while the kernel walks
    /// its lists of waiters and monitor records: by an eval or guard
    /// function, which may write the variables of the functions it calls
    /// and trigger events. It is announced when the outermost walk ends, so
    /// that no list changes under a walk (RUNTIME_ABI.md 5).
    struct LateChange {
        bool canceled = false;
        bool trigger = false;
        bool heap = false;
        bool nba = false;
        MonitorRef monitor;
        EventId event{0};
        ManagedEventPin eventPin;
        SlotId slot{0};
        std::uint32_t elem = 0;
        std::uint32_t lsb = 0;
        std::uint32_t width = 0;
        std::vector<std::uint64_t> old;
        std::vector<std::uint64_t> now;
    };
    std::pmr::vector<LateChange> lateChanges_;
    std::uint32_t walking_ = 0;          ///< Walks in progress.
    bool draining_ = false;              ///< lateChanges_ is being announced.
    std::vector<std::uint32_t> heldOf_;  ///< By slot: its row in held_; empty if none is marked.
    std::vector<Held> held_;
    std::vector<std::uint64_t> shown_;    ///< Scratch: a held part's value.
    std::vector<std::uint64_t> scratch_;  ///< Scratch: a monitor's new value.
    // A process that shows parts when they change: the $monitor, or a
    // file monitor, with the files it prints to.
    struct DisplayMonitor {
        explicit DisplayMonitor(std::pmr::memory_resource* memory) : heapReads(memory) {}
        DisplayMonitor(const DisplayMonitor&) = delete;
        DisplayMonitor& operator=(const DisplayMonitor&) = delete;
        DisplayMonitor(DisplayMonitor&&) noexcept = default;
        DisplayMonitor& operator=(DisplayMonitor&& other) noexcept {
            process = other.process;
            parts = std::move(other.parts);
            due = other.due;
            to = std::move(other.to);
            reads = other.reads;
            // Every record uses this Scheduler's one heap resource.
            heapReads.swap(other.heapReads);
            serial = other.serial;
            return *this;
        }
        ProcessId process = kNoHolder;
        std::vector<SlotPart> parts;
        bool due = false;  ///< To run at the end of this time slot.
        FileTargets to;
        DisplayReadSpec reads;
        std::pmr::vector<HeapRoot> heapReads;
        std::uint64_t serial = 0;
    };
    // The $monitor and the file monitors in the order made; by slot, how
    // many of their parts are in it; whether monitoring is on.
    DisplayMonitor monitor_;
    std::vector<DisplayMonitor> fileMonitors_;
    /// The file monitors that ended in the Postponed region, whose frames
    /// the tier has not released yet (SYSTEM_TASKS.md 6.2).
    std::vector<ProcessId> endedFileMonitors_;
    std::vector<std::uint32_t> monitorSlot_;
    bool monitorOn_ = true;
    // The files of the strobes with a descriptor of this time slot (a
    // Postponed event's gen is 1 + its row; 0 for standard output), and
    // those of the run in progress.
    std::vector<FileTargets> strobeTargets_;
    const FileTargets* printingTo_ = nullptr;

    bool ending_ = false;
    EndReason reason_ = EndReason::Quiescent;
    int level_ = 1;
    std::uint64_t time_ = 0;
    std::uint64_t movesThisSlot_ = 0;
    std::uint64_t eventsThisSlot_ = 0;
    const volatile std::sig_atomic_t* interruptFlag_ = nullptr;
    bool interruptTaken_ = false;
    std::uint64_t wallTimeLimit_ = 0;
    std::chrono::steady_clock::time_point wallStarted_{};
    bool runningFinal_ = false;
    StopAction stopAction_ = StopAction::Finish;
    bool ignoredStopStatistics_ = false;
    std::string error_;
    bool postponed_ = false;  ///< The Postponed region is running (2.2).
    bool overflow_ = false;   ///< A delay went past the largest time (8).
    std::string endNote_;
    std::vector<ProcessId> pendingOrder_;  ///< Processes that got a pending report.
    /// Processes with a report to confirm in the Observed region.
    std::uint32_t pendingProcesses_ = 0;
    std::vector<PendingReport> deferredCalls_;  ///< Confirmed calls, until they run.
    std::vector<std::uint32_t> freeDeferred_;

    bool begun_ = false;
    CounterId cTimeSlots_{0}, cTimeAdvances_{0}, cActiveLoops_{0}, cReactiveLoops_{0};
    CounterId cEvents_[kNumRegions] = {};
    CounterId cWakeups_{0}, cCreated_{0}, cFinished_{0}, cKeptPeak_{0}, cWrites_{0}, cChanges_{0},
        cNbaScheduled_{0};
    CounterId cSubscriptions_{0}, cMonitorEvals_{0}, cEndTime_{0}, cEndReason_{0};

    void begin();
    /// Resumes a process. `flushPoint`: whether resuming empties its queue of
    /// pending reports (SCHEDULER_SPEC.md 15.4).
    void wake(ProcessId p, bool flushPoint);
    /// Puts pendingOrder_ in creation order (15.4, 10.8).
    void sortPending();
    void schedule(Region r, Event e, std::uint64_t delay);
    void pushFuture(const Future& f);
    [[nodiscard]] bool futureEmpty() const { return wheelCount_ == 0 && heap_.empty(); }
    bool advance();
    void runSlot();
    void runRegion(Region r);
    bool anyEvents(Region first, Region last) const;
    bool moveFirst(Region first, Region last, Region to);
    void execute(const Event& e);
    void runPostponed(ProcessId p, const FileTargets* to = nullptr);
    void runDisplayMonitor(std::uint64_t serial, ProcessId p, const FileTargets* to,
                           DisplayReadSpec reads);
    void clearDisplayReads(DisplayMonitor& monitor) noexcept;
    void retainHeapHook();
    void releaseHeapHook() noexcept;
    void countMonitorParts(const DisplayMonitor& m, bool add);
    void markDue(DisplayMonitor& m, SlotId s, std::uint32_t elem, std::uint32_t lsb,
                 std::uint32_t width, const std::uint64_t* old, const std::uint64_t* now);
    /// Announces a change of bits [lsb, lsb + width) of element elem of s
    /// from `old` to `now` (whole elements) to its waiters and monitors.
    void notify(SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
                const std::uint64_t* old, const std::uint64_t* now);
    /// A walk of the lists of waiters or monitor records begins or ends;
    /// the end of the outermost one announces the late changes.
    void beginWalk() { ++walking_; }
    void endWalk();
    /// write() without looking at holds.
    bool writeRaw(SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
                  const std::uint64_t* value);
    /// Gives bits [lsb, lsb + width) of element elem of a held slot the
    /// value of their highest hold, or of the ordinary writes. Returns
    /// whether the value changed.
    bool showHeld(SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width);
    void evaluateMonitor(std::uint32_t id);
    bool evalMonitorValue(std::uint32_t id, std::span<std::uint64_t> out);
    bool evalMonitorBody(std::uint32_t id, std::span<std::uint64_t> out);
    static void changedHeap(void* frame, HeapRoot root) noexcept;
    static void changedActivation(void* frame, const ActivationAccess& access) noexcept;
    bool prepareActivationReads(std::uint32_t id);
    void heapLimit(ProcessId p = kNoHolder) noexcept;
    std::vector<MonitorRef> heapMonitors_;
    std::uint32_t heapMonitorLimit_ = 0;
    std::size_t heapMonitorCount_ = 0;
    HeapHook previousHeapHook_;
    ActivationHook previousActivationHook_;
    ActivationCapture* activationCapture_ = nullptr;
    HeapCapture* heapCapture_ = nullptr;
    bool heapMonitorLimited_ = false;
    ProcessId heapMonitorProcess_ = kNoHolder;
    [[nodiscard]] bool live(MonitorRef r) const {
        return r.id < monitors_.size() && monitors_[r.id].serial == r.serial &&
               (monitors_[r.id].control == kNone ||
                liveNbaEvent({monitors_[r.id].control, monitors_[r.id].gen}));
    }
    std::uint32_t allocateMonitor();
    void activateMonitor(std::uint32_t id);
    void releaseMonitor(std::uint32_t id, bool eager) noexcept;
    bool initializeMonitor(std::uint32_t id);
    bool updateMonitor(std::uint32_t id);
    [[nodiscard]] bool monitorDependsOnSlot(MonitorRef monitor, SlotId slot, std::uint32_t elem,
                                            std::uint32_t lsb, std::uint32_t width) const;
    /// Adds a monitor record to p's current wait.
    MonitorRef addMonitor(ProcessId p, const MonitorSpec& m);
    void registerWait(ProcessId p, std::span<const WaitItem> items,
                      std::span<const MonitorSpec> monitors, std::span<const NamedWaitItem> named,
                      std::uint64_t* registered);
    /// Frees the monitor records of p's current wait.
    void freeMonitor(ProcessId p);
    void ensureSlotTables();
    void flushReports(ProcessId p);
    /// The Observed region: confirms every pending report but the final
    /// assertions' calls (15.3).
    void confirmReports();
    /// The Postponed region: confirms and runs the final assertions' calls.
    void confirmFinals();
    void addPending(ProcessId p, PendingReport r);
};

/// Where a Context writes (RUNTIME_ABI.md 9.3.10): the design's output,
/// and the messages (CLI.md 3).
struct OutputDestinations {
    std::FILE* out;
    std::FILE* err;
};

/// The design's output, on the destinations the Context was made with
/// (CLI.md 3). It remembers whether the output so far ends a line, so that
/// --stats=json can start its own line.
class Output {
public:
    explicit Output(OutputDestinations to) : to_(to) {}

    /// --autoflush: publish each output write immediately (SYSTEM_TASKS.md 2).
    void setAutoFlush(bool enabled) { autoFlush_ = enabled; }
    void write(std::string_view text);
    /// The design's text on the messages' destination (STDERR, 21.3.1),
    /// after flushing the output's.
    void writeError(std::string_view text);
    /// One line on the messages' destination, after flushing the output's
    /// (CLI.md 3).
    void message(std::string_view line);
    /// Flushes the output's destination, so that it comes before what
    /// follows on the messages'.
    void flush();
    /// Whether nothing was written, or the last thing written ended a line.
    [[nodiscard]] bool atLineStart() const { return atLineStart_; }

private:
    OutputDestinations to_;
    bool atLineStart_ = true;
    bool autoFlush_ = false;
};

}  // namespace kirimasim::kernel
