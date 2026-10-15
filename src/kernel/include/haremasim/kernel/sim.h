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

#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haremasim/kernel/abi.h"
#include "haremasim/kernel/counters.h"
#include "haremasim/kernel/store.h"

namespace haremasim::kernel {

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
};

/// The name used on stderr and in --stats=json: finish, quiescent, ...
const char* endReasonName(EndReason r);

/// The default exit code for a reason.
int exitCode(EndReason r);

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

using ProcessId = std::uint32_t;
using EventId = std::uint32_t;

/// Runs a process from where it waited until its next wait or its end. All
/// three callback types are the C forms of abi.h, so every tier registers
/// the same way (RUNTIME_ABI.md 1, 4.3); toC() and fromC() in context.h
/// convert the context.
using ResumeFn = hs_resume_fn;
/// The iff condition of an event-control item, read when the item's event
/// occurs (9.4.2.3). Nonzero is true.
using GuardFn = hs_guard_fn;
/// Evaluates a monitored expression into `out` (laid out like a slot
/// element of the monitor's domain and width).
using EvalFn = hs_eval_fn;

/// A part of a slot: elements [firstElem, firstElem + numElems) (numElems
/// 0: all) and bits [lsb, lsb + width) of each (width 0: all).
struct SlotPart {
    SlotId slot = 0;
    std::uint32_t firstElem = 0;
    std::uint32_t numElems = 0;
    std::uint32_t lsb = 0;
    std::uint32_t width = 0;
};

/// One item of an event control on a fixed slot part: a subscription
/// (SCHEDULER_SPEC.md 9.5). The edge is judged on the part's lowest bit.
struct WaitItem {
    SlotPart part;
    Edge edge = Edge::Any;
    GuardFn guard = nullptr;
    void* guardFrame = nullptr;
};

/// An event control on an expression, or a `wait (expr)`: a monitor
/// record (SCHEDULER_SPEC.md 9.5). The expression is evaluated again when
/// one of its slots changes or one of its events is triggered.
struct MonitorSpec {
    std::vector<SlotPart> slots;
    std::vector<EventId> events;
    EvalFn eval = nullptr;
    void* frame = nullptr;
    Domain domain = Domain::Four;  ///< Two, Four or Real.
    std::uint32_t width = 1;
    Edge edge = Edge::Any;   ///< For an event control.
    bool untilTrue = false;  ///< `wait (expr)`: wake when the value is true.
};

/// Limits of a run (CLI.md 2.2). 0 means no limit for maxZeroDelay.
struct Limits {
    std::uint64_t maxTime = UINT64_MAX;
    std::uint64_t maxZeroDelay = 0;  ///< Moves into Active or Reactive per time slot.
};

/// The scheduler of one Context.
class Scheduler {
public:
    Scheduler(Context& ctx, Store& store, Counters& counters)
        : ctx_(ctx), store_(store), counters_(counters) {}

    void setLimits(const Limits& l) { limits_ = l; }

    /// Registers a process; it does not run until start() is called.
    ProcessId addProcess(ResumeFn fn, void* frame, Group group);
    /// Schedules a process to run in the current time slot, in Active or
    /// Reactive by its group.
    void start(ProcessId p);
    /// Creates a named event object.
    EventId addEvent();

    // Waits. A running process calls one of these and then returns
    // Status::Waiting. A later wait replaces nothing: a process has one
    // wait at a time.
    void waitDelay(ProcessId p, std::uint64_t ticks);
    void waitEvent(ProcessId p, std::span<const WaitItem> items);
    void waitMonitor(ProcessId p, const MonitorSpec& m);
    void waitNamed(ProcessId p, EventId e);

    /// Writes bits [lsb, lsb + width) of element `elem` (width 0: all) from
    /// `value`, laid out like a slot element from bit 0, and wakes the
    /// processes whose subscriptions and monitors see a change. Returns
    /// whether the value changed.
    bool write(SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
               const std::uint64_t* value);
    /// Schedules a nonblocking update, `delay` ticks from now, in the NBA
    /// region of the scheduling process's group (SCHEDULER_SPEC.md 13).
    void scheduleNba(ProcessId p, SlotId s, std::uint32_t elem, std::uint32_t lsb,
                     std::uint32_t width, const std::uint64_t* value, std::uint64_t delay);
    /// `-> e`: wakes the processes waiting for e now.
    void trigger(EventId e);
    /// `->> e`: triggers e in the NBA region, `delay` ticks from now.
    void triggerNb(ProcessId p, EventId e, std::uint64_t delay);
    /// Whether e was triggered in the current time slot (15.5.3).
    [[nodiscard]] bool triggered(EventId e) const;

    /// Runs the simulation until it ends, and returns the reason.
    EndReason run();

    /// Asks the simulation to end. `level` is the argument of $finish or
    /// $stop (0, 1 or 2); it decides what the termination line prints.
    void requestEnd(EndReason r, int level);
    /// Ends the simulation with an internal error.
    void fail(std::string message);

    [[nodiscard]] bool ending() const { return ending_; }
    [[nodiscard]] EndReason reason() const { return ending_ ? reason_ : EndReason::Quiescent; }
    [[nodiscard]] int level() const { return level_; }
    [[nodiscard]] std::uint64_t time() const { return time_; }
    [[nodiscard]] const std::string& error() const { return error_; }

private:
    enum class Kind : std::uint8_t { Resume, Nba, NbaDelayed, TriggerNb };
    struct Event {
        Kind kind = Kind::Resume;
        Region origin = Region::Active;  ///< The region it was scheduled in.
        std::uint32_t id = 0;            ///< Process, NBA record or event object.
        std::uint32_t gen = 0;           ///< Resume: the wait it ends.
    };
    struct Future {
        std::uint64_t time = 0;
        std::uint64_t seq = 0;
        Region region = Region::Active;
        Event event;
    };
    struct Process {
        ResumeFn fn = nullptr;
        void* frame = nullptr;
        Group group = Group::Active;
        std::uint32_t gen = 0;               ///< Number of the current wait.
        std::uint32_t monitor = UINT32_MAX;  ///< Monitor record of the current wait.
        bool done = false;
    };
    struct Subscription {
        ProcessId process = 0;
        std::uint32_t gen = 0;
        WaitItem item;
    };
    struct Monitor {
        ProcessId process = 0;
        std::uint32_t serial = 0;  ///< Changes when the record is reused.
        MonitorSpec spec;
        std::vector<std::uint64_t> last;
    };
    /// A reference to a monitor record from a slot or an event; stale when
    /// the record's serial has changed.
    struct MonitorRef {
        std::uint32_t id = 0;
        std::uint32_t serial = 0;
    };
    struct NamedEvent {
        std::vector<std::pair<ProcessId, std::uint32_t>> waiters;
        std::vector<MonitorRef> monitors;
        std::uint64_t triggeredAt = UINT64_MAX;
    };
    /// An NBA update. Without a delay, the value lives in the time slot's
    /// arena, freed all at once when the slot ends; with a delay, in the
    /// record itself, freed when the update runs (SCHEDULER_SPEC.md 5).
    struct NbaRecord {
        SlotId slot = 0;
        std::uint32_t elem = 0;
        std::uint32_t lsb = 0;
        std::uint32_t width = 0;
        std::uint64_t arenaOffset = 0;
        std::uint32_t words = 0;
        std::vector<std::uint64_t> value;  ///< Delayed updates only.
    };

    static constexpr std::uint64_t kWheel = 256;

    Context& ctx_;
    Store& store_;
    Counters& counters_;
    Limits limits_;

    std::vector<Process> processes_;
    std::deque<Event> regions_[kNumRegions];
    std::vector<std::vector<Future>> wheel_ = std::vector<std::vector<Future>>(kWheel);
    std::uint64_t wheelCount_ = 0;
    std::vector<Future> heap_;  ///< 4-ary min-heap by (time, region, seq).
    std::uint64_t seq_ = 0;

    std::vector<std::vector<Subscription>> subscriptions_;  ///< By slot.
    std::vector<Monitor> monitors_;
    std::vector<std::uint32_t> freeMonitors_;
    std::vector<std::vector<MonitorRef>> slotMonitors_;  ///< By slot.
    std::uint32_t monitorSerial_ = 0;
    std::vector<NamedEvent> events_;
    std::vector<NbaRecord> slotNbas_;     ///< Updates without a delay, this slot.
    std::vector<std::uint64_t> arena_;    ///< Their values.
    std::vector<NbaRecord> delayedNbas_;  ///< Updates with a delay.
    std::vector<std::uint32_t> freeDelayedNbas_;
    std::vector<std::uint64_t> old_;      ///< Scratch: an element before a write.
    std::vector<std::uint64_t> scratch_;  ///< Scratch: a monitor's new value.

    bool ending_ = false;
    EndReason reason_ = EndReason::Quiescent;
    int level_ = 1;
    std::uint64_t time_ = 0;
    std::uint64_t movesThisSlot_ = 0;
    std::string error_;

    bool begun_ = false;
    CounterId cTimeSlots_ = 0, cTimeAdvances_ = 0, cActiveLoops_ = 0, cReactiveLoops_ = 0;
    CounterId cEvents_[kNumRegions] = {};
    CounterId cWakeups_ = 0, cWrites_ = 0, cChanges_ = 0, cNbaScheduled_ = 0;
    CounterId cSubscriptions_ = 0, cMonitorEvals_ = 0, cEndTime_ = 0, cEndReason_ = 0;

    void begin();
    void wake(ProcessId p);
    void schedule(Region r, Event e, std::uint64_t delay);
    void pushFuture(const Future& f);
    [[nodiscard]] bool futureEmpty() const { return wheelCount_ == 0 && heap_.empty(); }
    bool advance();
    void runSlot();
    void runRegion(Region r);
    bool anyEvents(Region first, Region last) const;
    bool moveFirst(Region first, Region last, Region to);
    void execute(const Event& e);
    void notify(SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width);
    void evaluateMonitor(std::uint32_t id);
    [[nodiscard]] bool live(MonitorRef r) const { return monitors_[r.id].serial == r.serial; }
    void freeMonitor(ProcessId p);
    void ensureSlotTables();
};

/// Writes the design's output to stdout.
void writeOut(std::string_view text);

/// Flushes stdout, so that it comes before what follows on stderr.
void flushOut();

}  // namespace haremasim::kernel
