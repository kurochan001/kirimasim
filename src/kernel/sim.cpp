// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/sim.h"

#include <algorithm>
#include <cstdio>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/edge.h"

namespace kirimasim::kernel {

namespace {

/// Adds x to a wait list. A wait that one of its items wakes leaves its
/// other items behind (SCHEDULER_SPEC.md 9.5); they are swept out when the
/// list has doubled since its last sweep, so that a list holds at most
/// about twice its live entries, at a cost of O(1) a push on average. Only
/// waits push, and no wait runs while a list is walked.
template <typename T, typename Stale>
void pushSwept(std::vector<T>& list, std::uint32_t& limit, const T& x, Stale stale) {
    if (list.size() >= limit) {
        std::erase_if(list, stale);
        limit = static_cast<std::uint32_t>(std::max<std::size_t>(16, 2 * list.size()));
    }
    list.push_back(x);
}

bool overlaps(std::uint32_t a, std::uint32_t na, std::uint32_t b, std::uint32_t nb) {
    if (na == 0 || nb == 0) return true;  // 0 means "all"
    return a < b + nb && b < a + na;
}

bool edgeMatches(Edge e, unsigned from, unsigned to) {
    switch (e) {
        case Edge::Any: return from != to;
        case Edge::Pos: return isPosedge(from, to);
        case Edge::Neg: return isNegedge(from, to);
        case Edge::Both: return isPosedge(from, to) || isNegedge(from, to);
    }
    return false;
}

/// Bit i of an element of the given domain, as a logic value.
unsigned logicAt(Domain d, const std::uint64_t* p, std::uint64_t i) {
    if (d == Domain::Four) return logicOf(bitAt(p, 2, i), bitAt(p + 1, 2, i));
    if (d == Domain::Two) return bitAt(p, 1, i) ? 1U : 0U;
    return (p[0] != 0) ? 1U : 0U;  // real or handle: only "changed" matters
}

/// Whether bits [lsb, lsb + width) are equal in two elements.
bool sameBits(Domain d, const std::uint64_t* x, const std::uint64_t* y, std::uint64_t lsb,
              std::uint64_t width) {
    switch (d) {
        case Domain::Four:
            return equalBits(x, 2, lsb, y, 2, lsb, width) &&
                   equalBits(x + 1, 2, lsb, y + 1, 2, lsb, width);
        case Domain::Two: return equalBits(x, 1, lsb, y, 1, lsb, width);
        case Domain::Real:
        case Domain::Handle: return x[0] == y[0];
    }
    return true;
}

/// Whether a value is true: some bit is a known 1 (VALUE_SEMANTICS.md 3.5),
/// or a real is not 0.0.
bool isTrue(Domain d, std::uint32_t width, const std::uint64_t* p) {
    if (d == Domain::Real) return (p[0] << 1) != 0;  // +0.0 and -0.0 are false
    const std::size_t n = wordsFor(width);
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t a = d == Domain::Four ? p[2 * i] : p[i];
        const std::uint64_t b = d == Domain::Four ? p[2 * i + 1] : 0;
        if ((a & ~b) != 0) return true;
    }
    return false;
}

std::uint32_t elementWords(Domain d, std::uint32_t width) {
    if (d == Domain::Four) return 2 * wordsFor(width);
    if (d == Domain::Two) return wordsFor(width);
    return 1;
}

/// Order of future events: time, then region, then the order they were
/// scheduled in (SCHEDULER_SPEC.md 8).
template <typename F> bool before(const F& x, const F& y) {
    if (x.time != y.time) return x.time < y.time;
    if (x.region != y.region) return x.region < y.region;
    return x.seq < y.seq;
}

struct HeapReads {
    std::pmr::vector<HeapRoot> roots;
    HeapResult<bool> (*filter)(const void*, HeapRoot) noexcept;
    const void* filterFrame;
    HeapError error = HeapError::None;
    static void record(void* frame, HeapRoot root) noexcept {
        auto& r = *static_cast<HeapReads*>(frame);
        if (r.error != HeapError::None) return;
        try {
            if (r.filter != nullptr) {
                const auto keep = r.filter(r.filterFrame, root);
                if (!keep) {
                    r.error = keep.status.error;
                    return;
                }
                if (!keep.value) return;
            }
            if (std::find(r.roots.begin(), r.roots.end(), root) == r.roots.end())
                r.roots.push_back(root);
        } catch (const std::bad_alloc&) {
            r.error = HeapError::ResourceLimit;
        } catch (const std::length_error&) {
            r.error = HeapError::ResourceLimit;
        }
    }
};

struct ActivationReads {
    std::pmr::vector<ActivationRegion> regions;
    std::span<const ActivationLease> leases;
    bool limited = false;
    static void record(void* frame, const ActivationAccess& access) noexcept {
        auto& reads = *static_cast<ActivationReads*>(frame);
        if (reads.limited) return;
        // Calls with private automatic locals do not expand the waiter's
        // lexical environment. Its explicit extents own all captured reads.
        if (std::none_of(reads.leases.begin(), reads.leases.end(),
                         [&](const auto& lease) { return lease.token() == access.token; }))
            return;
        const ActivationRegion region{access.token, access.field, access.element, 1,
                                      access.lsb,   access.width};
        try {
            if (std::find(reads.regions.begin(), reads.regions.end(), region) ==
                reads.regions.end())
                reads.regions.push_back(region);
        } catch (const std::bad_alloc&) {
            reads.limited = true;
        } catch (const std::length_error&) {
            reads.limited = true;
        }
    }
};

bool dependsOn(const ActivationRegion& region, const ActivationAccess& access) {
    return region.token == access.token && region.field == access.field &&
           overlaps(region.firstElem, region.numElems, access.element, 1) &&
           overlaps(region.lsb, region.width, access.lsb, access.width);
}

}  // namespace

const char* endReasonName(EndReason r) {
    switch (r) {
        case EndReason::Finish: return "finish";
        case EndReason::Quiescent: return "quiescent";
        case EndReason::Stop: return "stop";
        case EndReason::Fatal: return "fatal";
        case EndReason::TimeLimit: return "time_limit";
        case EndReason::WallTimeLimit: return "wall_time_limit";
        case EndReason::ZeroTimeLimit: return "zero_time_limit";
        case EndReason::ResourceLimit: return "resource_limit";
        case EndReason::Interrupted: return "interrupted";
        case EndReason::Error: return "error";
        case EndReason::Internal: return "internal";
    }
    return "internal";
}

const char* regionName(Region r) {
    switch (r) {
        case Region::Preponed: return "preponed";
        case Region::PreActive: return "pre_active";
        case Region::Active: return "active";
        case Region::Inactive: return "inactive";
        case Region::PreNba: return "pre_nba";
        case Region::Nba: return "nba";
        case Region::PostNba: return "post_nba";
        case Region::PreObserved: return "pre_observed";
        case Region::Observed: return "observed";
        case Region::PostObserved: return "post_observed";
        case Region::Reactive: return "reactive";
        case Region::ReInactive: return "re_inactive";
        case Region::PreReNba: return "pre_re_nba";
        case Region::ReNba: return "re_nba";
        case Region::PostReNba: return "post_re_nba";
        case Region::PrePostponed: return "pre_postponed";
        case Region::Postponed: return "postponed";
    }
    return "?";
}

// --- Setup -------------------------------------------------------------------

Scheduler::Scheduler(Context& ctx, Store& store, Counters& counters)
    : ctx_(ctx), store_(store), counters_(counters), lateChanges_(ctx.heap().memoryResource()),
      monitor_(ctx.heap().memoryResource()) {}

void Scheduler::begin() {
    if (begun_) return;
    begun_ = true;
    // The order of PERFORMANCE.md 3.2.
    cTimeSlots_ = counters_.add("sched.time_slots");
    cTimeAdvances_ = counters_.add("sched.time_advances");
    cActiveLoops_ = counters_.add("sched.active_loops");
    cReactiveLoops_ = counters_.add("sched.reactive_loops");
    for (int r = 0; r < kNumRegions; ++r)
        cEvents_[r] =
            counters_.add(std::string("sched.events.") + regionName(static_cast<Region>(r)));
    cWakeups_ = counters_.add("proc.wakeups");
    cCreated_ = counters_.add("proc.created");
    cFinished_ = counters_.add("proc.finished");
    cKeptPeak_ = counters_.add("proc.kept_peak");
    cWrites_ = counters_.add("slot.writes");
    cChanges_ = counters_.add("slot.changes");
    cNbaScheduled_ = counters_.add("nba.scheduled");
    cSubscriptions_ = counters_.add("wait.subscriptions");
    cMonitorEvals_ = counters_.add("wait.monitor_evals");
    cEndTime_ = counters_.add("sim.end_time");
    cEndReason_ = counters_.addText("sim.end_reason");
}

ProcessId Scheduler::addProcess(ResumeFn fn, void* frame, Group group) {
    begin();
    ProcessId id{0};
    Process proc;
    if (freeProcesses_.empty()) {
        id = static_cast<ProcessId>(processes_.size());
        processes_.emplace_back();
    } else {
        // The wait numbers go on from the released process's, so that its
        // stale wake-ups never match (SCHEDULER_SPEC.md 10.8).
        id = freeProcesses_.back();
        freeProcesses_.pop_back();
        proc.gen = processes_[id.index()].gen + 1;
    }
    proc.fn = fn;
    proc.frame = frame;
    proc.group = group;
    proc.serial = nextSerial_++;
    processes_[id.index()] = std::move(proc);
    counters_.increment(cCreated_);
    if (++kept_ > counters_.value(cKeptPeak_)) counters_.set(cKeptPeak_, kept_);
    return id;
}

ProgramId Scheduler::addProgram() {
    const ProgramId id{static_cast<std::uint32_t>(programs_.size())};
    programs_.emplace_back();
    return id;
}

ProcessId Scheduler::addProgramInitial(ResumeFn fn, void* frame, ProgramId program) {
    const ProcessId p = addProcess(fn, frame, Group::Reactive);
    processes_[p.index()].program = program;
    processes_[p.index()].programInitial = true;
    if (programs_[program.index()].initials++ == 0) ++activePrograms_;
    return p;
}

ProcessId Scheduler::addChildProcess(ResumeFn fn, void* frame, ProcessId parent) {
    const Group group = processes_[parent.index()].group;
    const ProgramId program = processes_[parent.index()].program;
    const ProcessId child = addProcess(fn, frame, group);
    processes_[child.index()].program = program;
    return child;
}

void Scheduler::initialEnded(ProcessId p) {
    Process& process = processes_[p.index()];
    if (!process.programInitial) return;
    process.programInitial = false;
    const ProgramId program = process.program;
    if (--programs_[program.index()].initials == 0) completeProgram(program);
}

void Scheduler::completeProgram(ProgramId program) {
    Program& state = programs_[program.index()];
    if (state.ended) return;
    state.ended = true;
    --activePrograms_;
    // Only initial threads and their descendants carry this origin.
    // Continuous assignments and final procedures are not terminated here.
    for (std::uint32_t i = 0; i < processes_.size(); ++i) {
        if (processes_[i].program != program || processes_[i].done) continue;
        const ProcessId p{i};
        processes_[i].programStopped = true;
        endedProgramProcesses_.push_back(p);
        kill(p);
    }
    if (activePrograms_ == 0) requestEnd(EndReason::Finish, 0);
}

void Scheduler::exitProgram(ProcessId caller) {
    const ProgramId program = processes_[caller.index()].program;
    if (program != kNone) completeProgram(program);
}

bool Scheduler::takeEndedProgramProcess(ProcessId& process) {
    if (endedProgramProcesses_.empty()) return false;
    process = endedProgramProcesses_.back();
    endedProgramProcesses_.pop_back();
    return true;
}

void Scheduler::releaseProcess(ProcessId p) {
    detachDisplayMonitor(p);
    Process& proc = processes_[p.index()];
    proc.fn = nullptr;
    proc.frame = nullptr;
    proc.pending = {};
    --kept_;
    if (proc.gen < kReuseGenLimit) freeProcesses_.push_back(p);
}

void Scheduler::kill(ProcessId p) {
    Process& proc = processes_[p.index()];
    if (proc.done) return;
    // A new wait number makes every pending wake-up of the process stale:
    // its delay events, subscriptions and named-event waits are dropped
    // when they come up (SCHEDULER_SPEC.md 10.7). NBAs it scheduled stay.
    // A display callback owns its registration independently of its issuer.
    // Killing that callback must detach it before its frame can be released.
    detachDisplayMonitor(p);
    freeMonitor(p);
    proc.done = true;
    ++proc.gen;
    counters_.increment(cFinished_);
    initialEnded(p);
}

void Scheduler::ended(ProcessId p) {
    freeMonitor(p);
    Process& proc = processes_[p.index()];
    const bool wasDone = proc.done;
    if (!wasDone) counters_.increment(cFinished_);
    proc.done = true;
    ++proc.gen;
    if (!wasDone) initialEnded(p);
}

void Scheduler::start(ProcessId p) {
    // A start, or going on after a disable, is no flush point (15.4).
    begin();
    wake(p, false);
}

EventId Scheduler::addEvent() {
    events_.emplace_back();
    return EventId{static_cast<std::uint32_t>(events_.size() - 1)};
}

ManagedEventBackend Scheduler::managedEventBackend() noexcept {
    return {this,
            [](void* frame) noexcept -> ManagedEventResult<EventId> {
                auto& scheduler = *static_cast<Scheduler*>(frame);
                if (scheduler.events_.size() >= kNone)
                    return {kNone, ManagedEventError::ResourceLimit};
                try {
                    return {scheduler.addEvent()};
                } catch (const std::bad_alloc&) {
                    return {kNone, ManagedEventError::ResourceLimit};
                } catch (const std::length_error&) {
                    return {kNone, ManagedEventError::ResourceLimit};
                }
            },
            [](void* frame, EventId id) noexcept {
                auto& event = static_cast<Scheduler*>(frame)->events_[id.index()];
                event.waiters.clear();
                event.monitors.clear();
                event.waiterLimit = event.monitorLimit = 0;
                event.triggeredAt = UINT64_MAX;
                event.wasTriggered = false;
            }};
}

void Scheduler::ensureSlotTables() {
    if (subscriptions_.size() < store_.size()) {
        subscriptions_.resize(store_.size());
        subscriptionLimit_.resize(store_.size());
        slotMonitors_.resize(store_.size());
        slotMonitorLimit_.resize(store_.size());
        monitorSlot_.resize(store_.size());
    }
}

std::size_t Scheduler::waitListEntries() const {
    std::size_t n = heapMonitors_.size();
    for (const auto& l : subscriptions_)
        n += l.size();
    for (const auto& l : slotMonitors_)
        n += l.size();
    for (const NamedEvent& e : events_)
        n += e.waiters.size() + e.monitors.size();
    return n;
}

// --- $strobe and $monitor ----------------------------------------------------

void Scheduler::strobe(ProcessId p) {
    begin();
    schedule(Region::Postponed, {Kind::Postponed, Region::Postponed, p.index(), 0}, 0);
}

void Scheduler::strobe(ProcessId p, FileTargets to) {
    begin();
    strobeTargets_.push_back(std::move(to));
    schedule(Region::Postponed,
             {Kind::Postponed, Region::Postponed, p.index(), strobeTargets_.size()}, 0);
}

void Scheduler::countMonitorParts(const DisplayMonitor& m, bool add) {
    for (const SlotPart& part : m.parts)
        monitorSlot_[part.slot.index()] =
            add ? monitorSlot_[part.slot.index()] + 1 : monitorSlot_[part.slot.index()] - 1;
}

void Scheduler::setMonitor(ProcessId p, std::span<const SlotPart> parts) {
    setMonitor(p, parts, {});
}
void Scheduler::setMonitor(ProcessId p, std::span<const SlotPart> parts, DisplayReadSpec reads) {
    begin();
    ensureSlotTables();
    countMonitorParts(monitor_, false);
    clearDisplayReads(monitor_);
    monitor_.process = p;
    monitor_.parts.assign(parts.begin(), parts.end());
    monitor_.reads = reads;
    monitor_.serial = ++monitorSerial_;
    if (reads.heap) retainHeapHook();
    countMonitorParts(monitor_, true);
    monitor_.due = true;
}
void Scheduler::addFileMonitor(ProcessId p, std::span<const SlotPart> parts, FileTargets to) {
    addFileMonitor(p, parts, std::move(to), {});
}
void Scheduler::addFileMonitor(ProcessId p, std::span<const SlotPart> parts, FileTargets to,
                               DisplayReadSpec reads) {
    begin();
    ensureSlotTables();
    fileMonitors_.emplace_back(ctx_.heap().memoryResource());
    auto& monitor = fileMonitors_.back();
    monitor.process = p;
    monitor.parts.assign(parts.begin(), parts.end());
    monitor.to = std::move(to);
    monitor.due = true;
    monitor.reads = reads;
    monitor.serial = ++monitorSerial_;
    if (reads.heap) retainHeapHook();
    countMonitorParts(monitor, true);
}

bool Scheduler::takeEndedFileMonitor(ProcessId& ended) {
    if (endedFileMonitors_.empty()) return false;
    ended = endedFileMonitors_.back();
    endedFileMonitors_.pop_back();
    return true;
}

void Scheduler::setMonitoring(bool on) {
    monitorOn_ = on;
    if (on) monitor_.due = true;
}

void Scheduler::runPostponed(ProcessId p, const FileTargets* to) {
    counters_.increment(cWakeups_);
    printingTo_ = to;
    const auto st =
        static_cast<Status>(processes_[p.index()].fn(toC(ctx_), processes_[p.index()].frame));
    printingTo_ = nullptr;
    if (st == Status::Error) {
        if (error_.empty()) error_ = "a process failed";
        requestEnd(EndReason::Internal, 1);
    }
}

void Scheduler::retainHeapHook() {
    if (heapMonitorCount_++ == 0) {
        previousHeapHook_ = ctx_.heap().setChangeHook({&changedHeap, this});
        previousActivationHook_ = ctx_.activations().setChangeHook({&changedActivation, this});
    }
}

void Scheduler::releaseHeapHook() noexcept {
    if (--heapMonitorCount_ == 0) {
        ctx_.heap().setChangeHook(previousHeapHook_);
        ctx_.activations().setChangeHook(previousActivationHook_);
        heapMonitors_.clear();
        heapMonitorLimit_ = 0;
    }
}

void Scheduler::clearDisplayReads(DisplayMonitor& monitor) noexcept {
    for (const auto root : monitor.heapReads)
        ctx_.heap().watch(root, false);
    monitor.heapReads.clear();
    if (monitor.reads.heap) releaseHeapHook();
    monitor.reads = {};
}

void Scheduler::detachDisplayMonitor(ProcessId p) noexcept {
    if (monitor_.process == p) {
        countMonitorParts(monitor_, false);
        clearDisplayReads(monitor_);
        monitor_.process = kNoHolder;
        monitor_.parts.clear();
        monitor_.due = false;
        monitor_.serial = ++monitorSerial_;
    }
    for (std::size_t i = 0; i < fileMonitors_.size();) {
        if (fileMonitors_[i].process != p) {
            ++i;
            continue;
        }
        countMonitorParts(fileMonitors_[i], false);
        clearDisplayReads(fileMonitors_[i]);
        fileMonitors_.erase(fileMonitors_.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

void Scheduler::runDisplayMonitor(std::uint64_t serial, ProcessId p, const FileTargets* to,
                                  DisplayReadSpec spec) {
    if (!spec.heap) {
        runPostponed(p, to);
        return;
    }
    // Retain the hook while a callback replaces/removes its own registration.
    retainHeapHook();
    struct Release {
        Scheduler& scheduler;
        ~Release() { scheduler.releaseHeapHook(); }
    } release{*this};
    HeapReads reads{std::pmr::vector<HeapRoot>(ctx_.heap().memoryResource()), spec.filter,
                    spec.frame};
    {
        struct Restore {
            Heap& heap;
            HeapHook previous;
            ~Restore() { heap.setReadHook(previous); }
        } restore{ctx_.heap(), ctx_.heap().setReadHook({&HeapReads::record, &reads})};
        runPostponed(p, to);
    }
    if (reads.error != HeapError::None) {
        if (reads.error == HeapError::ResourceLimit)
            heapLimit(p);
        else
            requestEnd(EndReason::Internal, 1);
        return;
    }
    DisplayMonitor* target = monitor_.serial == serial ? &monitor_ : nullptr;
    if (target == nullptr)
        for (auto& monitor : fileMonitors_)
            if (monitor.serial == serial) {
                target = &monitor;
                break;
            }
    if (target == nullptr || ending_) return;
    for (const auto root : target->heapReads)
        ctx_.heap().watch(root, false);
    for (const auto root : reads.roots)
        ctx_.heap().watch(root, true);
    target->heapReads.swap(reads.roots);
}

// --- Scheduling --------------------------------------------------------------

void Scheduler::freeMonitor(ProcessId p) {
    Process& proc = processes_[p.index()];
    for (const std::uint32_t id : proc.monitors) {
        Monitor& m = monitors_[id];
        for (const auto root : m.heapReads)
            ctx_.heap().watch(root, false);
        if (m.spec.heap || m.spec.activation.enabled()) releaseHeapHook();
        m.heapReads.clear();
        m.activationReads.clear();
        m.activationParts.clear();
        m.activationLeases.clear();
        m.serial = ++monitorSerial_;  // references from slots and events go stale
        m.spec = {};
        freeMonitors_.push_back(id);
    }
    proc.monitors.clear();
}

void Scheduler::wake(ProcessId p, bool flushPoint) {
    freeMonitor(p);
    Process& proc = processes_[p.index()];
    proc.flushOnResume = flushPoint;
    ++proc.gen;  // ends every other item of the wait
    const Region r = proc.group == Group::Active ? Region::Active : Region::Reactive;
    schedule(r, {Kind::Resume, r, p.index(), proc.gen}, 0);
}

void Scheduler::schedule(Region r, Event e, std::uint64_t delay) {
    e.origin = r;
    if (delay == 0) {
        regions_[static_cast<int>(r)].push_back(e);
        return;
    }
    if (delay > UINT64_MAX - time_) {  // past the largest time: an error of the design (8)
        overflow_ = true;
        return;
    }
    pushFuture({time_ + delay, seq_++, r, e});
}

void Scheduler::pushFuture(const Future& f) {
    if (f.time - time_ < kWheel) {
        wheel_[f.time % kWheel].push_back(f);
        ++wheelCount_;
        return;
    }
    // 4-ary heap: the children of i are 4i+1 .. 4i+4.
    heap_.push_back(f);
    std::size_t i = heap_.size() - 1;
    while (i > 0) {
        const std::size_t parent = (i - 1) / 4;
        if (!before(heap_[i], heap_[parent])) break;
        std::swap(heap_[i], heap_[parent]);
        i = parent;
    }
}

bool Scheduler::advance() {
    // The earliest future time: the nearest wheel bucket or the heap's top.
    std::uint64_t next = UINT64_MAX;
    bool found = false;
    if (wheelCount_ != 0) {
        for (std::uint64_t i = 1; i < kWheel; ++i) {
            if (!wheel_[(time_ + i) % kWheel].empty()) {
                next = time_ + i;
                found = true;
                break;
            }
        }
    }
    if (!heap_.empty() && (!found || heap_[0].time < next)) {
        next = heap_[0].time;
        found = true;
    }
    if (!found) return false;
    if (next > limits_.maxTime) {
        time_ = limits_.maxTime;
        requestEnd(EndReason::TimeLimit, 1);
        return false;
    }
    time_ = next;
    counters_.increment(cTimeAdvances_);

    std::vector<Future> due;
    auto& bucket = wheel_[next % kWheel];
    for (const Future& f : bucket)
        if (f.time == next) due.push_back(f);
    wheelCount_ -= bucket.size();
    bucket.clear();
    while (!heap_.empty() && heap_[0].time == next) {
        due.push_back(heap_[0]);
        heap_[0] = heap_.back();
        heap_.pop_back();
        std::size_t i = 0;
        for (;;) {
            std::size_t best = i;
            for (std::size_t c = 4 * i + 1; c <= 4 * i + 4 && c < heap_.size(); ++c)
                if (before(heap_[c], heap_[best])) best = c;
            if (best == i) break;
            std::swap(heap_[i], heap_[best]);
            i = best;
        }
    }
    std::sort(due.begin(), due.end(),
              [](const Future& x, const Future& y) { return before(x, y); });
    for (const Future& f : due)
        regions_[static_cast<int>(f.region)].push_back(f.event);
    return true;
}

// --- The reference algorithm (SCHEDULER_SPEC.md 3) ---------------------------

EndReason Scheduler::run() {
    begin();
    for (;;) {
        if (ending_) break;
        if (!anyEvents(Region::Preponed, Region::Postponed) && !advance()) break;
        runSlot();
    }
    counters_.set(cEndTime_, time_);
    counters_.setText(cEndReason_, endReasonName(reason()));
    return reason();
}

SafePointHook Scheduler::setSafePointHook(SafePointHook hook) noexcept {
    return std::exchange(safePoint_, hook);
}
void Scheduler::safePoint() noexcept {
    if (!ending_ && walking_ == 0 && !draining_ && safePoint_.fn != nullptr)
        safePoint_.fn(safePoint_.frame);
}
bool Scheduler::visitPendingValues(PendingValueVisitor visitor, void* frame) const noexcept {
    for (const NbaRecord& n : slotNbas_)
        if (!visitor(frame, n.slot, {arena_.data() + n.arenaOffset, n.words})) return false;
    for (const NbaRecord& n : delayedNbas_)
        if (!n.value.empty() && !visitor(frame, n.slot, n.value)) return false;
    for (const DelayedDriver& d : drivers_)
        if (d.pending && !visitor(frame, d.slot, d.value)) return false;
    for (std::size_t s = 0; s < heldOf_.size(); ++s) {
        if (heldOf_[s] == kNotHeld) continue;
        const Held& h = held_[heldOf_[s]];
        const SlotId slot{static_cast<std::uint32_t>(s)};
        if (!h.base.empty() && !visitor(frame, slot, h.base)) return false;
        const SlotLayout& layout = store_.layout(slot);
        const auto stride = elementWords(layout.domain, layout.width);
        for (unsigned level = 0; level < 2; ++level) {
            if (h.value[level].empty()) continue;
            for (std::uint32_t e = 0; e < layout.elements; ++e) {
                const auto first = h.owner[level].begin() + std::size_t{e} * layout.width;
                const bool live = std::any_of(first, first + layout.width,
                                              [](ProcessId owner) { return owner != kNoHolder; });
                if (live && !visitor(frame, slot,
                                     {h.value[level].data() + std::size_t{e} * stride, stride}))
                    return false;
            }
        }
    }
    return true;
}

void Scheduler::runFinal(std::span<const ProcessId> finals) {
    if (reason() == EndReason::Internal) return;
    const bool wasEnding = ending_;
    const EndReason endReason = reason_;
    const int endLevel = level_;
    runningFinal_ = true;
    for (const ProcessId p : finals) {
        if (processes_[p.index()].done) continue;
        ending_ = false;
        counters_.increment(cWakeups_);
        const auto st =
            static_cast<Status>(processes_[p.index()].fn(toC(ctx_), processes_[p.index()].frame));
        processes_[p.index()].done = true;
        ++processes_[p.index()].gen;
        counters_.increment(cFinished_);
        // A run-time error of the design, or a limit on resources, that
        // the final procedure reached stays one (16.2); any other failure
        // is internal.
        const bool designError =
            ending_ && (reason_ == EndReason::Error || reason_ == EndReason::ResourceLimit);
        if (st == Status::Error && reason_ != EndReason::Internal && !designError) {
            if (error_.empty()) error_ = "a final procedure failed";
            reason_ = EndReason::Internal;
        }
        if (ending_) break;  // $finish, or a failure
    }
    runningFinal_ = false;
    if (ending_ && reason_ == EndReason::Internal) {
        counters_.setText(cEndReason_, endReasonName(EndReason::Internal));
        return;
    }
    ending_ = wasEnding;
    reason_ = endReason;
    level_ = endLevel;
}

bool Scheduler::anyEvents(Region first, Region last) const {
    for (int r = static_cast<int>(first); r <= static_cast<int>(last); ++r)
        if (!regions_[r].empty()) return true;
    return false;
}

bool Scheduler::moveFirst(Region first, Region last, Region to) {
    for (int r = static_cast<int>(first); r <= static_cast<int>(last); ++r) {
        auto& q = regions_[r];
        if (q.empty()) continue;
        auto& dst = regions_[static_cast<int>(to)];
        dst.insert(dst.end(), q.begin(), q.end());
        q.clear();
        ++movesThisSlot_;
        if (limits_.maxZeroDelay != 0 && movesThisSlot_ > limits_.maxZeroDelay) {
            const std::string n = std::to_string(limits_.maxZeroDelay);
            std::string note = n;
            note += " moves into the ";
            note += to == Region::Active ? "Active" : "Reactive";
            note += " region in one time slot (--max-zero-delay=";
            note += n;
            note += ")";
            setEndNote(std::move(note));
            requestEnd(EndReason::ZeroTimeLimit, 1);
        }
        return true;
    }
    return false;
}

void Scheduler::runSlot() {
    counters_.increment(cTimeSlots_);
    movesThisSlot_ = 0;
    eventsThisSlot_ = 0;
    runRegion(Region::Preponed);
    runRegion(Region::PreActive);
    // A pending report counts as an event of the Observed region (15.3).
    while (!ending_ &&
           (anyEvents(Region::Active, Region::PrePostponed) || pendingProcesses_ != 0)) {
        while (!ending_ &&
               (anyEvents(Region::Active, Region::PostObserved) || pendingProcesses_ != 0)) {
            counters_.increment(cActiveLoops_);
            runRegion(Region::Active);
            if (ending_) break;
            if (pendingProcesses_ != 0 && !anyEvents(Region::Inactive, Region::PreObserved)) {
                confirmReports();
                continue;
            }
            moveFirst(Region::Inactive, Region::PostObserved, Region::Active);
        }
        while (!ending_ && anyEvents(Region::Reactive, Region::PostReNba)) {
            counters_.increment(cReactiveLoops_);
            runRegion(Region::Reactive);
            if (!ending_) moveFirst(Region::ReInactive, Region::PostReNba, Region::Reactive);
        }
        if (!ending_ && !anyEvents(Region::Active, Region::PostReNba))
            runRegion(Region::PrePostponed);
    }
    if (!ending_) {
        // The final assertions, then the file monitors in the order made
        // and the monitor, then the strobes (SCHEDULER_SPEC.md 15.2,
        // SYSTEM_TASKS.md 4). A file monitor none of whose files is open
        // any more ends (SYSTEM_TASKS.md 6.2).
        postponed_ = true;
        confirmFinals();
        for (std::size_t i = 0; i < fileMonitors_.size();) {
            if (!ctx_.files().live(fileMonitors_[i].to)) {
                countMonitorParts(fileMonitors_[i], false);
                clearDisplayReads(fileMonitors_[i]);
                const ProcessId p = fileMonitors_[i].process;
                fileMonitors_.erase(fileMonitors_.begin() + static_cast<std::ptrdiff_t>(i));
                // The monitor ends with its last file; its process ends
                // too, and the tier releases its frame (SYSTEM_TASKS.md 6.2).
                kill(p);
                endedFileMonitors_.push_back(p);
                continue;
            }
            if (fileMonitors_[i].due) {
                fileMonitors_[i].due = false;
                const FileTargets to = fileMonitors_[i].to;  // the run may add monitors
                const auto serial = fileMonitors_[i].serial;
                runDisplayMonitor(serial, fileMonitors_[i].process, &to, fileMonitors_[i].reads);
                if (i >= fileMonitors_.size() || fileMonitors_[i].serial != serial) continue;
            }
            ++i;
        }
        const bool due = monitor_.due && monitorOn_ && monitor_.process != kNoHolder;
        monitor_.due = false;
        if (due) runDisplayMonitor(monitor_.serial, monitor_.process, nullptr, monitor_.reads);
        runRegion(Region::Postponed);
        strobeTargets_.clear();
        postponed_ = false;
        // The waveform takes the values at the end of the time slot
        // (WAVEFORM.md 4).
        if (ctx_.dump().busy()) ctx_.dump().endSlot(*this);
    }
    // The arena of NBA values without a delay is freed with the slot.
    slotNbas_.clear();
    arena_.clear();
    safePoint();
}

void Scheduler::runRegion(Region r) {
    auto& q = regions_[static_cast<int>(r)];
    while (!ending_ && !q.empty()) {
        // SIGINT is looked at before each event, so at every time-slot
        // boundary too (SCHEDULER_SPEC.md 16.2).
        if (takeInterrupt()) return;
        // The events of one time slot, in any region (CLI.md 2.6): this
        // stops processes that wake each other inside one region, which
        // move nothing between regions. The event past the limit does not
        // run.
        if (limits_.maxSlotEvents != 0 && eventsThisSlot_ == limits_.maxSlotEvents) {
            const std::string n = std::to_string(limits_.maxSlotEvents);
            std::string note = n;
            note += " events ran in one time slot (--max-slot-events=";
            note += n;
            note += ")";
            setEndNote(std::move(note));
            requestEnd(EndReason::ZeroTimeLimit, 1);
            return;
        }
        ++eventsThisSlot_;
        const Event e = q.front();
        q.pop_front();
        execute(e);
    }
    safePoint();
}

void Scheduler::execute(const Event& e) {
    counters_.increment(cEvents_[static_cast<int>(e.origin)]);
    switch (e.kind) {
        case Kind::Resume: {
            Process& proc = processes_[e.id];
            if (proc.done || proc.gen != e.gen) return;  // killed, or woken again
            if (proc.flushOnResume) {
                proc.flushOnResume = false;
                flushReports(ProcessId{e.id});
            }
            counters_.increment(cWakeups_);
            // proc may be stale after the call: the table may have grown.
            const auto st = static_cast<Status>(proc.fn(toC(ctx_), proc.frame));
            flushHeapChanges();
            switch (st) {
                case Status::Waiting: break;
                case Status::Done:
                case Status::Killed: ended(ProcessId{e.id}); break;
                case Status::Error:
                    if (error_.empty()) error_ = "a process failed";
                    requestEnd(EndReason::Internal, 1);
                    break;
                case Status::Stop: break;
            }
            return;
        }
        case Kind::Nba: {
            const NbaRecord n = slotNbas_[e.id];
            // write consumes every input bit before calling guards or
            // monitors, which may grow the arena and the record table.
            // The value already lives until the end of this time slot
            // (SCHEDULER_SPEC.md 5); no second snapshot is needed.
            write({.slot = n.slot, .elem = n.elem, .lsb = n.lsb, .width = n.width},
                  arena_.data() + n.arenaOffset);
            return;
        }
        case Kind::NbaDelayed: {
            NbaRecord& n = delayedNbas_[e.id];
            const std::vector<std::uint64_t> value = std::move(n.value);
            const NbaRecord where = n;
            write({.slot = where.slot, .elem = where.elem, .lsb = where.lsb, .width = where.width},
                  value.data());
            delayedNbas_[e.id].value.clear();
            freeDelayedNbas_.push_back(e.id);
            return;
        }
        case Kind::TriggerNb: trigger(EventId{e.id}); return;
        case Kind::Postponed: {
            if (e.gen == 0) {
                runPostponed(ProcessId{e.id});
            } else {
                const FileTargets to = strobeTargets_[e.gen - 1];  // the run may add strobes
                runPostponed(ProcessId{e.id}, &to);
            }
            return;
        }
        case Kind::Deferred: {
            const PendingReport r = std::move(deferredCalls_[e.id]);
            freeDeferred_.push_back(e.id);
            r.fn(toC(ctx_), r.frame, r.token, 1);
            return;
        }
        case Kind::Show:
            showHeld(SlotId{e.id}, static_cast<std::uint32_t>(e.gen), 0,
                     store_.layout(SlotId{e.id}).width);
            return;
        case Kind::Drive: {
            DelayedDriver& d = drivers_[e.id];
            if (!d.pending || d.gen != e.gen) return;  // dropped
            d.pending = false;
            // Copy all targets before callbacks can grow the driver table.
            const SlotBits target{d.slot, d.elem, d.lsb, d.width};
            const std::vector<std::uint64_t> value = d.value;
            const std::vector<DelayedSideUpdate> sides = d.sides;
            if (sides.empty()) {
                write(target, value.data());
                return;
            }
            // Defer notifications until the complete state has committed.
            beginWalk();
            write(target, value.data());
            for (const auto& side : sides) {
                const std::uint64_t bit = side.value ? 1 : 0;
                write(side.at, &bit);
            }
            endWalk();
            return;
        }
    }
}

// --- Waits -------------------------------------------------------------------

void Scheduler::waitDelay(ProcessId p, std::uint64_t ticks) {
    flushHeapChanges();
    freeMonitor(p);
    Process& proc = processes_[p.index()];
    ++proc.gen;
    const bool active = proc.group == Group::Active;
    if (ticks == 0) {
        const Region r = active ? Region::Inactive : Region::ReInactive;
        schedule(r, {Kind::Resume, r, p.index(), proc.gen}, 0);
    } else {
        const Region r = active ? Region::Active : Region::Reactive;
        schedule(r, {Kind::Resume, r, p.index(), proc.gen}, ticks);
    }
}

void Scheduler::waitEvent(ProcessId p, std::span<const WaitItem> items) { waitAny(p, items, {}); }

void Scheduler::waitMonitor(ProcessId p, const MonitorSpec& m) { waitAny(p, {}, {&m, 1}); }

void Scheduler::waitAny(ProcessId p, std::span<const WaitItem> items,
                        std::span<const MonitorSpec> monitors,
                        std::span<const NamedWaitItem> named) {
    flushHeapChanges();
    ensureSlotTables();
    freeMonitor(p);
    const std::uint64_t gen = ++processes_[p.index()].gen;
    for (const WaitItem& item : items) {
        const SlotId s = item.part.slot;
        pushSwept(
            subscriptions_[s.index()], subscriptionLimit_[s.index()], Subscription{p, gen, item},
            [&](const Subscription& x) { return processes_[x.process.index()].gen != x.gen; });
        counters_.increment(cSubscriptions_);
    }
    for (const NamedWaitItem& item : named)
        pushSwept(events_[item.event.index()].waiters, events_[item.event.index()].waiterLimit,
                  Waiter{p, gen, true, item.guard, item.guardFrame},
                  [&](const Waiter& w) { return processes_[w.process.index()].gen != w.gen; });
    // The monitors' first evaluation may write or trigger: announced once
    // the whole wait is registered.
    beginWalk();
    for (const MonitorSpec& m : monitors)
        addMonitor(p, m);
    endWalk();
}

void Scheduler::addMonitor(ProcessId p, const MonitorSpec& m) {
    std::uint32_t id = 0;
    if (!freeMonitors_.empty()) {
        id = freeMonitors_.back();
        freeMonitors_.pop_back();
    } else {
        id = static_cast<std::uint32_t>(monitors_.size());
        if (freeMonitors_.capacity() <= monitors_.size())
            freeMonitors_.reserve(std::max(monitors_.size() + 1, freeMonitors_.capacity() * 2));
        monitors_.emplace_back(ctx_.heap().memoryResource());
    }
    processes_[p.index()].monitors.push_back(id);
    Monitor& mon = monitors_[id];
    mon.process = p;
    mon.gen = processes_[p.index()].gen;
    mon.serial = ++monitorSerial_;
    mon.spec = m;
    mon.heapReads.clear();
    if (m.heap || m.activation.enabled()) {
        retainHeapHook();
        pushSwept(heapMonitors_, heapMonitorLimit_, MonitorRef{id, mon.serial},
                  [&](const MonitorRef& r) { return !live(r); });
    }
    if (!prepareActivationReads(id)) return;
    mon.last.assign(elementWords(m.domain, m.width), 0);
    if (!evalMonitorValue(id, mon.last)) return;
    counters_.increment(cMonitorEvals_);
    const MonitorRef ref{id, monitors_[id].serial};
    for (const SlotPart& part : m.slots) {
        auto& list = slotMonitors_[part.slot.index()];
        if (list.empty() || list.back().id != id || list.back().serial != ref.serial)
            pushSwept(list, slotMonitorLimit_[part.slot.index()], ref,
                      [&](const MonitorRef& r) { return !live(r); });
    }
    for (const EventId e : m.events)
        pushSwept(events_[e.index()].monitors, events_[e.index()].monitorLimit, ref,
                  [&](const MonitorRef& r) { return !live(r); });
}

void Scheduler::waitNamed(ProcessId p, EventId e, bool flushPoint) {
    freeMonitor(p);
    Process& proc = processes_[p.index()];
    ++proc.gen;
    pushSwept(events_[e.index()].waiters, events_[e.index()].waiterLimit,
              Waiter{p, proc.gen, flushPoint},
              [&](const Waiter& w) { return processes_[w.process.index()].gen != w.gen; });
}

// --- Effects -----------------------------------------------------------------

bool Scheduler::write(SlotBits at, const std::uint64_t* value) {
    auto [s, elem, lsb, width] = at;
    if (heldOf_.empty() || heldOf_[s.index()] == kNotHeld)
        return writeRaw(s, elem, lsb, width, value);
    // A held slot: the write goes to the value of the ordinary writes,
    // which shows where nothing holds the bits (SCHEDULER_SPEC.md 14).
    const SlotLayout& l = store_.layout(s);
    if (width == 0) {
        lsb = 0;
        width = l.width;
    }
    const unsigned stride = l.domain == Domain::Four ? 2 : 1;
    std::uint64_t* base = held_[heldOf_[s.index()]].base.data() + std::uint64_t{elem} * l.elemWords;
    for (unsigned k = 0; k < stride; ++k)
        copyBits(base + k, stride, lsb, value + k, stride, 0, width);
    return showHeld(s, elem, lsb, width);
}

void Scheduler::markHeld(SlotId s, bool net, bool driven) {
    if (heldOf_.size() < store_.size()) heldOf_.resize(store_.size(), kNotHeld);
    if (heldOf_[s.index()] != kNotHeld) return;
    const SlotLayout& l = store_.layout(s);
    Held h;
    h.net = net;
    h.driven = driven;
    const std::uint64_t* first = store_.element(s, 0);
    h.base.assign(first, first + std::uint64_t{l.elements} * l.elemWords);
    for (int level = 0; level < 2; ++level) {
        h.value[level].assign(h.base.size(), 0);
        h.owner[level].assign(std::uint64_t{l.elements} * l.width, kNoHolder);
    }
    heldOf_[s.index()] = static_cast<std::uint32_t>(held_.size());
    held_.push_back(std::move(h));
}

void Scheduler::hold(Hold level, ProcessId owner, const SlotPart& part) {
    Held& h = held_[heldOf_[part.slot.index()]];
    const SlotLayout& l = store_.layout(part.slot);
    const std::uint32_t n = part.numElems == 0 ? l.elements : part.numElems;
    const std::uint32_t lsb = part.width == 0 ? 0 : part.lsb;
    const std::uint32_t width = part.width == 0 ? l.width : part.width;
    std::vector<ProcessId>& who = h.owner[static_cast<int>(level)];
    for (std::uint32_t e = part.firstElem; e < part.firstElem + n; ++e)
        for (std::uint32_t i = lsb; i < lsb + width; ++i)
            who[std::uint64_t{e} * l.width + i] = owner;
}

void Scheduler::writeHeld(Hold level, ProcessId owner, SlotBits at, const std::uint64_t* value) {
    auto [s, elem, lsb, width] = at;
    Held& h = held_[heldOf_[s.index()]];
    const SlotLayout& l = store_.layout(s);
    if (width == 0) {
        lsb = 0;
        width = l.width;
    }
    const unsigned stride = l.domain == Domain::Four ? 2 : 1;
    const int k = static_cast<int>(level);
    std::uint64_t* v = h.value[k].data() + std::uint64_t{elem} * l.elemWords;
    const ProcessId* who = h.owner[k].data() + std::uint64_t{elem} * l.width;
    for (std::uint32_t i = 0; i < width; ++i) {
        if (who[lsb + i] != owner) continue;
        for (unsigned plane = 0; plane < stride; ++plane)
            copyBits(v + plane, stride, lsb + i, value + plane, stride, i, 1);
    }
    showHeld(s, elem, lsb, width);
}

void Scheduler::release(Hold level, const SlotPart& part) {
    Held& h = held_[heldOf_[part.slot.index()]];
    const SlotLayout& l = store_.layout(part.slot);
    const std::uint32_t n = part.numElems == 0 ? l.elements : part.numElems;
    const std::uint32_t lsb = part.width == 0 ? 0 : part.lsb;
    const std::uint32_t width = part.width == 0 ? l.width : part.width;
    const unsigned stride = l.domain == Domain::Four ? 2 : 1;
    const int k = static_cast<int>(level);
    for (std::uint32_t e = part.firstElem; e < part.firstElem + n; ++e) {
        const std::uint64_t first = std::uint64_t{e} * l.width;
        std::uint64_t* base = h.base.data() + std::uint64_t{e} * l.elemWords;
        const std::uint64_t* now = store_.element(part.slot, e);
        bool later = false;
        for (std::uint32_t i = lsb; i < lsb + width; ++i) {
            h.owner[k][first + i] = kNoHolder;
            if (h.net) continue;
            const bool other = h.owner[1 - k][first + i] != kNoHolder;
            if (level == Hold::Force && (other || h.driven)) {
                // The assign or the continuous assignment is evaluated
                // again in its region (10.6.2).
                later = true;
            } else if (!other) {
                // Keeps its value until the next write (10.6.1, 10.6.2).
                for (unsigned plane = 0; plane < stride; ++plane)
                    copyBits(base + plane, stride, i, now + plane, stride, i, 1);
            }
        }
        // A net takes its drivers' value now (10.6.2). A variable's other
        // bits show what they show already.
        if (h.net)
            showHeld(part.slot, e, lsb, width);
        else if (later)
            schedule(Region::Active, {Kind::Show, Region::Active, part.slot.index(), e}, 0);
    }
}

bool Scheduler::holdsAny(Hold level, ProcessId owner) const {
    for (const Held& h : held_)
        for (const ProcessId p : h.owner[static_cast<int>(level)])
            if (p == owner) return true;
    return false;
}

void Scheduler::cancelWait(ProcessId p) {
    freeMonitor(p);
    ++processes_[p.index()].gen;
}

bool Scheduler::showHeld(SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width) {
    const Held& h = held_[heldOf_[s.index()]];
    const SlotLayout& l = store_.layout(s);
    const unsigned stride = l.domain == Domain::Four ? 2 : 1;
    const std::uint64_t at = std::uint64_t{elem} * l.elemWords;
    const std::uint64_t first = std::uint64_t{elem} * l.width;
    shown_.assign(std::uint64_t{stride} * wordsFor(width), 0);
    for (std::uint32_t i = 0; i < width; ++i) {
        const std::uint64_t bit = first + lsb + i;
        const std::uint64_t* from = h.owner[1][bit] != kNoHolder   ? h.value[1].data() + at
                                    : h.owner[0][bit] != kNoHolder ? h.value[0].data() + at
                                                                   : h.base.data() + at;
        for (unsigned plane = 0; plane < stride; ++plane)
            copyBits(shown_.data() + plane, stride, i, from + plane, stride, lsb + i, 1);
    }
    return writeRaw(s, elem, lsb, width, shown_.data());
}

bool Scheduler::writeRaw(SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
                         const std::uint64_t* value) {
    begin();
    ensureSlotTables();
    const SlotLayout& l = store_.layout(s);
    if (width == 0) {
        lsb = 0;
        width = l.width;
    }
    std::uint64_t* p = store_.element(s, elem);
    const bool watched = !subscriptions_[s.index()].empty() || !slotMonitors_[s.index()].empty() ||
                         monitorSlot_[s.index()] != 0;
    // During a walk, the change waits for its end with its own copies.
    const bool late = watched && walking_ > 0;
    LateChange change;
    if (late)
        change.old.assign(p, p + l.elemWords);
    else if (watched)
        old_.assign(p, p + l.elemWords);
    bool changed = false;
    switch (l.domain) {
        case Domain::Four:
            changed = !equalBits(p, 2, lsb, value, 2, 0, width) ||
                      !equalBits(p + 1, 2, lsb, value + 1, 2, 0, width);
            copyBits(p, 2, lsb, value, 2, 0, width);
            copyBits(p + 1, 2, lsb, value + 1, 2, 0, width);
            break;
        case Domain::Two:
            changed = !equalBits(p, 1, lsb, value, 1, 0, width);
            copyBits(p, 1, lsb, value, 1, 0, width);
            break;
        case Domain::Real:
        case Domain::Handle:
            changed = p[0] != value[0];
            p[0] = value[0];
            break;
    }
    counters_.increment(cWrites_);
    if (!changed) return false;
    counters_.increment(cChanges_);
    if (ctx_.dump().glitches()) ctx_.dump().changed(s, time_);  // --wave-glitches
    if (late) {
        change.slot = s;
        change.elem = elem;
        change.lsb = lsb;
        change.width = width;
        change.now.assign(p, p + l.elemWords);
        lateChanges_.push_back(std::move(change));
    } else if (watched) {
        now_.assign(p, p + l.elemWords);
        notify(s, elem, lsb, width, old_.data(), now_.data());
    }
    return true;
}

void Scheduler::endWalk() {
    if (--walking_ != 0 || draining_) return;
    draining_ = true;
    for (std::size_t i = 0; i < lateChanges_.size() && !ending_; ++i) {  // grows while announcing
        LateChange c = std::move(lateChanges_[i]);
        if (c.heap) {
            if (live(c.monitor)) {
                beginWalk();
                evaluateMonitor(c.monitor.id);
                endWalk();
            }
        } else if (c.trigger)
            trigger(c.event);
        else
            notify(c.slot, c.elem, c.lsb, c.width, c.old.data(), c.now.data());
    }
    lateChanges_.clear();
    draining_ = false;
}

void Scheduler::markDue(DisplayMonitor& m, SlotId s, std::uint32_t elem, std::uint32_t lsb,
                        std::uint32_t width, const std::uint64_t* old, const std::uint64_t* now) {
    if (m.due) return;
    const SlotLayout& l = store_.layout(s);
    for (const SlotPart& part : m.parts) {
        const std::uint32_t plsb = part.width == 0 ? 0 : part.lsb;
        const std::uint32_t pwidth = part.width == 0 ? l.width : part.width;
        if (part.slot == s && overlaps(part.firstElem, part.numElems, elem, 1) &&
            overlaps(plsb, pwidth, lsb, width) && !sameBits(l.domain, old, now, plsb, pwidth)) {
            m.due = true;
            return;
        }
    }
}

void Scheduler::notify(SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
                       const std::uint64_t* old, const std::uint64_t* now) {
    beginWalk();
    const SlotLayout& l = store_.layout(s);
    // Subscriptions. Waking a process makes its other items stale; they
    // are dropped here or on the next write of their slot.
    auto& subs = subscriptions_[s.index()];
    std::size_t keep = 0;
    for (std::size_t i = 0; i < subs.size(); ++i) {
        const Subscription sub = subs[i];
        if (processes_[sub.process.index()].gen != sub.gen) continue;  // stale
        subs[keep++] = sub;
        const SlotPart& part = sub.item.part;
        const std::uint32_t plsb = part.width == 0 ? 0 : part.lsb;
        const std::uint32_t pwidth = part.width == 0 ? l.width : part.width;
        if (!overlaps(part.firstElem, part.numElems, elem, 1) ||
            !overlaps(plsb, pwidth, lsb, width))
            continue;
        if (sameBits(l.domain, old, now, plsb, pwidth)) continue;
        if (sub.item.edge != Edge::Any &&
            !edgeMatches(sub.item.edge, logicAt(l.domain, old, plsb), logicAt(l.domain, now, plsb)))
            continue;
        if (sub.item.guard != nullptr && sub.item.guard(toC(ctx_), sub.item.guardFrame) == 0)
            continue;
        wake(sub.process, true);  // an event control: a flush point (15.4)
        --keep;                   // this item is used up
    }
    subscriptions_[s.index()].resize(keep);

    // A part that the monitor or a file monitor shows (SYSTEM_TASKS.md 4,
    // 6.2).
    if (monitorSlot_[s.index()] != 0) {
        markDue(monitor_, s, elem, lsb, width, old, now);
        for (DisplayMonitor& m : fileMonitors_)
            markDue(m, s, elem, lsb, width, old, now);
    }

    // Monitor records that depend on the changed part.
    auto& mons = slotMonitors_[s.index()];
    std::size_t kept = 0;
    for (std::size_t i = 0; i < mons.size(); ++i) {
        const MonitorRef ref = slotMonitors_[s.index()][i];
        if (!live(ref)) continue;  // stale
        slotMonitors_[s.index()][kept++] = ref;
        const std::uint32_t id = ref.id;
        const Monitor& m = monitors_[id];
        bool depends = false;
        for (const SlotPart& part : m.spec.slots) {
            const std::uint32_t plsb = part.width == 0 ? 0 : part.lsb;
            const std::uint32_t pwidth = part.width == 0 ? l.width : part.width;
            depends =
                depends || (part.slot == s && overlaps(part.firstElem, part.numElems, elem, 1) &&
                            overlaps(plsb, pwidth, lsb, width));
        }
        if (depends) evaluateMonitor(id);
    }
    slotMonitors_[s.index()].resize(kept);
    endWalk();
}

void Scheduler::heapLimit(ProcessId p) noexcept {
    if (!heapMonitorLimited_) heapMonitorProcess_ = p;
    heapMonitorLimited_ = true;
    resourceLimit();
}

void Scheduler::changedHeap(void* frame, HeapRoot root) noexcept {
    auto& s = *static_cast<Scheduler*>(frame);
    if (s.ending_ || !s.ctx_.heap().watched(root)) return;
    try {
        const auto mark = [&](DisplayMonitor& monitor) {
            if (std::find(monitor.heapReads.begin(), monitor.heapReads.end(), root) !=
                monitor.heapReads.end())
                monitor.due = true;
        };
        mark(s.monitor_);
        for (auto& monitor : s.fileMonitors_)
            mark(monitor);
        for (const MonitorRef ref : s.heapMonitors_) {
            if (!s.live(ref)) continue;
            const auto& reads = s.monitors_[ref.id].heapReads;
            if (std::find(reads.begin(), reads.end(), root) == reads.end()) continue;
            LateChange notice;
            notice.heap = true;
            notice.monitor = ref;
            s.lateChanges_.push_back(std::move(notice));
        }
    } catch (const std::bad_alloc&) {
        s.heapLimit();
    } catch (const std::length_error&) {
        s.heapLimit();
    }
}

bool Scheduler::prepareActivationReads(std::uint32_t id) {
    auto& monitor = monitors_[id];
    std::pmr::vector<ActivationLease> leases(ctx_.heap().memoryResource());
    std::pmr::vector<ActivationRegion> parts(ctx_.heap().memoryResource());
    const auto retain = [&](const ActivationBinding& binding) {
        auto held = ctx_.activations().retain(binding);
        if (!held) {
            if (held.error == ActivationError::ResourceLimit)
                heapLimit(monitor.process);
            else
                fail("invalid automatic monitor binding");
            return false;
        }
        if (std::none_of(leases.begin(), leases.end(),
                         [&](const auto& lease) { return lease.token() == held.value.token(); }))
            leases.push_back(std::move(held.value));
        return true;
    };
    try {
        for (const auto& binding : monitor.spec.activation.bindings)
            if (!retain(binding)) return false;
        for (const auto& part : monitor.spec.activation.parts) {
            const auto region = ctx_.activations().region(part);
            if (!region) {
                fail("invalid automatic monitor range");
                return false;
            }
            if (!retain(part.binding)) return false;
            parts.push_back(region.value);
        }
        monitor.activationLeases.swap(leases);
        monitor.activationParts.swap(parts);
        return true;
    } catch (const std::bad_alloc&) {
        heapLimit(monitor.process);
    } catch (const std::length_error&) {
        heapLimit(monitor.process);
    }
    return false;
}

void Scheduler::changedActivation(void* frame, const ActivationAccess& access) noexcept {
    auto& scheduler = *static_cast<Scheduler*>(frame);
    if (scheduler.ending_) return;
    try {
        for (const auto ref : scheduler.heapMonitors_) {
            if (!scheduler.live(ref)) continue;
            const auto& monitor = scheduler.monitors_[ref.id];
            const auto matches = [&](const auto& region) { return dependsOn(region, access); };
            const auto inProgress = [&] {
                for (auto* capture = scheduler.activationCapture_; capture != nullptr;
                     capture = capture->previous)
                    if (capture->monitor.id == ref.id && capture->monitor.serial == ref.serial &&
                        std::any_of(capture->reads->begin(), capture->reads->end(), matches))
                        return true;
                return false;
            };
            if (!std::any_of(monitor.activationParts.begin(), monitor.activationParts.end(),
                             matches) &&
                !std::any_of(monitor.activationReads.begin(), monitor.activationReads.end(),
                             matches) &&
                !inProgress())
                continue;
            LateChange notice;
            notice.heap = true;
            notice.monitor = ref;
            scheduler.lateChanges_.push_back(std::move(notice));
        }
    } catch (const std::bad_alloc&) {
        scheduler.heapLimit();
    } catch (const std::length_error&) {
        scheduler.heapLimit();
    }
}

ActivationError Scheduler::writeActivation(const ActivationBinding& binding, std::uint32_t element,
                                           std::span<const W4> value, std::uint32_t lsb,
                                           std::uint32_t width) noexcept {
    const auto result = ctx_.activations().write(binding, element, value, lsb, width);
    flushHeapChanges();
    return result;
}

void Scheduler::flushHeapChanges() noexcept {
    if (walking_ != 0 || draining_ || lateChanges_.empty() || ending_) return;
    try {
        beginWalk();
        endWalk();
    } catch (const std::bad_alloc&) {
        walking_ = 0;
        draining_ = false;
        heapLimit();
    } catch (const std::length_error&) {
        walking_ = 0;
        draining_ = false;
        heapLimit();
    }
}

bool Scheduler::evalMonitorValue(std::uint32_t id, std::span<std::uint64_t> out) {
    const auto serial = monitors_[id].serial;
    const EvalFn eval = monitors_[id].spec.eval;
    void* const frame = monitors_[id].spec.frame;
    if (!monitors_[id].spec.heap && !monitors_[id].spec.activation.enabled()) {
        eval(toC(ctx_), frame, out.data());
        return monitors_[id].serial == serial;
    }
    HeapReads reads{std::pmr::vector<HeapRoot>(ctx_.heap().memoryResource()),
                    monitors_[id].spec.heapReadFilter, monitors_[id].spec.heapReadFrame};
    ActivationReads activation{std::pmr::vector<ActivationRegion>(ctx_.heap().memoryResource()),
                               {}};
    // Evaluating source code may cancel this very wait or grow the monitor
    // table. Keep the environment alive until the callback has returned.
    std::pmr::vector<ActivationLease> leases(ctx_.heap().memoryResource());
    try {
        for (const auto& lease : monitors_[id].activationLeases) {
            auto held = ctx_.activations().retain(lease);
            if (!held) {
                fail("invalid automatic monitor extent");
                return false;
            }
            leases.push_back(std::move(held.value));
        }
    } catch (const std::bad_alloc&) {
        heapLimit();
        return false;
    } catch (const std::length_error&) {
        heapLimit();
        return false;
    }
    activation.leases = leases;
    ActivationCapture capture{{id, serial}, &activation.regions, activationCapture_};
    struct Restore {
        Heap& heap;
        HeapHook previous;
        ActivationStore& store;
        ActivationHook activation;
        ActivationCapture*& current;
        ActivationCapture* previousCapture;
        ~Restore() {
            current = previousCapture;
            heap.setReadHook(previous);
            store.setReadHook(activation);
        }
    } restore{ctx_.heap(), {}, ctx_.activations(), {}, activationCapture_, activationCapture_};
    if (monitors_[id].spec.activation.capture) activationCapture_ = &capture;
    restore.previous = ctx_.heap().setReadHook(
        monitors_[id].spec.heap ? HeapHook{&HeapReads::record, &reads} : HeapHook{});
    restore.activation =
        ctx_.activations().setReadHook(monitors_[id].spec.activation.capture
                                           ? ActivationHook{&ActivationReads::record, &activation}
                                           : ActivationHook{});
    eval(toC(ctx_), frame, out.data());
    if (activation.limited) {
        heapLimit();
        return false;
    }
    if (reads.error != HeapError::None) {
        if (reads.error == HeapError::ResourceLimit)
            heapLimit();
        else
            fail("invalid heap monitor dependency");
        return false;
    }
    if (ending_ || monitors_[id].serial != serial) return false;
    // The callback can grow the monitor table. Reacquire the entry.
    for (const auto root : monitors_[id].heapReads)
        ctx_.heap().watch(root, false);
    for (const auto root : reads.roots)
        ctx_.heap().watch(root, true);
    monitors_[id].heapReads.swap(reads.roots);
    monitors_[id].activationReads.swap(activation.regions);
    return true;
}

void Scheduler::evaluateMonitor(std::uint32_t id) {
    Monitor& m = monitors_[id];
    if (processes_[m.process.index()].gen != m.gen) return;  // already woken
    const auto serial = m.serial;
    scratch_.assign(m.last.size(), 0);
    if (!evalMonitorValue(id, scratch_)) return;
    counters_.increment(cMonitorEvals_);
    const MonitorSpec& spec = monitors_[id].spec;
    bool fire = false;
    if (spec.untilTrue) {
        fire = isTrue(spec.domain, spec.width, scratch_.data());
    } else if (!sameBits(spec.domain, monitors_[id].last.data(), scratch_.data(), 0, spec.width)) {
        fire = spec.edge == Edge::Any ||
               edgeMatches(spec.edge, logicAt(spec.domain, monitors_[id].last.data(), 0),
                           logicAt(spec.domain, scratch_.data(), 0));
        // iff: read when the item's event occurs, and only then (9.4.2.3).
        if (fire && spec.guard != nullptr && spec.guard(toC(ctx_), spec.guardFrame) == 0)
            fire = false;
    }
    if (monitors_[id].serial != serial) return;
    Monitor& again = monitors_[id];
    again.last = scratch_;
    if (fire) wake(again.process, true);  // frees the record; a flush point (15.4)
}

void Scheduler::scheduleNba(ProcessId p, SlotBits at, const std::uint64_t* value,
                            std::uint64_t delay) {
    auto [s, elem, lsb, width] = at;
    begin();
    const SlotLayout& l = store_.layout(s);
    const std::uint32_t bits = width == 0 ? l.width : width;
    const std::uint32_t words = elementWords(l.domain, bits);
    counters_.increment(cNbaScheduled_);
    const Region r = processes_[p.index()].group == Group::Active ? Region::Nba : Region::ReNba;
    NbaRecord n;
    n.slot = s;
    n.elem = elem;
    n.lsb = lsb;
    n.width = width;
    n.words = words;
    if (delay == 0) {
        n.arenaOffset = arena_.size();
        arena_.insert(arena_.end(), value, value + words);
        slotNbas_.push_back(n);
        schedule(r, {Kind::Nba, r, static_cast<std::uint32_t>(slotNbas_.size() - 1), 0}, 0);
        return;
    }
    n.value.assign(value, value + words);
    std::uint32_t id = 0;
    if (!freeDelayedNbas_.empty()) {
        id = freeDelayedNbas_.back();
        freeDelayedNbas_.pop_back();
        delayedNbas_[id] = std::move(n);
    } else {
        id = static_cast<std::uint32_t>(delayedNbas_.size());
        delayedNbas_.push_back(std::move(n));
    }
    schedule(r, {Kind::NbaDelayed, r, id, 0}, delay);
}

std::uint32_t Scheduler::addDelayedDriver() {
    drivers_.emplace_back();
    return static_cast<std::uint32_t>(drivers_.size() - 1);
}

void Scheduler::driveDelayed(std::uint32_t driver, SlotBits at, const std::uint64_t* value,
                             std::uint64_t rise, std::uint64_t fall, std::uint64_t off,
                             std::span<const DelayedSideUpdate> sides) {
    auto [s, elem, lsb, width] = at;
    begin();
    const SlotLayout& l = store_.layout(s);
    if (width == 0) {
        lsb = 0;
        width = l.width;
    }
    const bool four = l.domain == Domain::Four;
    const unsigned stride = four ? 2 : 1;
    const std::uint32_t words = stride * wordsFor(width);
    DelayedDriver& d = drivers_[driver];
    const auto sameTarget = [](SlotBits a, SlotBits b) {
        return a.slot == b.slot && a.elem == b.elem && a.lsb == b.lsb && a.width == b.width;
    };
    const bool sameSides =
        sides.size() == d.sides.size() &&
        std::all_of(sides.begin(), sides.end(), [&](const DelayedSideUpdate& side) {
            return std::any_of(d.sides.begin(), d.sides.end(), [&](const DelayedSideUpdate& old) {
                return sameTarget(side.at, old.at) && side.value == old.value;
            });
        });
    // A side-only change is a changed complete output (SCHEDULER_SPEC 12).
    const bool samePending = sameTarget({s, elem, lsb, width}, {d.slot, d.elem, d.lsb, d.width}) &&
                             sameSides &&
                             std::equal(value, value + words, d.value.begin(), d.value.end());
    if (d.pending && !samePending) {
        d.pending = false;
        ++d.gen;
    }
    // The value there now, laid out like `value`.
    std::vector<std::uint64_t> now(words, 0);
    const std::uint64_t* p = store_.element(s, elem);
    for (unsigned k = 0; k < stride; ++k)
        copyBits(now.data() + k, stride, 0, p + k, stride, lsb, width);
    const bool sidesCommitted = std::all_of(sides.begin(), sides.end(), [&](const auto& side) {
        return bitAt(store_.element(side.at.slot, side.at.elem), 1, side.at.lsb) == side.value;
    });
    if ((std::equal(value, value + words, now.begin()) && sidesCommitted) || d.pending) return;
    // The delay for the change (10.3.3, 28.16, table 28-9).
    const auto all = [&](const std::uint64_t* v, bool a, bool b) {
        for (std::uint32_t i = 0; i < width; ++i) {
            if (bitAt(v, stride, i) != a) return false;
            if (four && bitAt(v + 1, stride, i) != b) return false;
            if (!four && b) return false;
        }
        return true;
    };
    std::uint64_t delay = rise;
    if (width == 1) {
        const bool a = bitAt(value, stride, 0);
        const bool b = four && bitAt(value + 1, stride, 0);
        delay = !b ? (a ? rise : fall) : (a ? std::min({rise, fall, off}) : off);
    } else if (!all(now.data(), false, false) && all(value, false, false)) {
        delay = fall;
    } else if (all(value, false, true)) {
        delay = off;
    }
    d.slot = s;
    d.elem = elem;
    d.lsb = lsb;
    d.width = width;
    d.value.assign(value, value + words);
    d.sides.assign(sides.begin(), sides.end());
    d.pending = true;
    ++d.gen;
    schedule(delay == 0 ? Region::Inactive : Region::Active,
             {Kind::Drive, Region::Active, driver, d.gen}, delay);
}

void Scheduler::trigger(EventId e) {
    if (walking_ > 0) {  // made during a walk: after it
        LateChange c;
        c.trigger = true;
        c.event = e;
        lateChanges_.push_back(std::move(c));
        return;
    }
    beginWalk();
    NamedEvent& ev = events_[e.index()];
    ev.triggeredAt = time_;
    ev.wasTriggered = true;
    if (ctx_.dump().glitches()) ctx_.dump().triggered(e.index(), time_);  // --wave-glitches
    const auto waiters = std::move(ev.waiters);
    events_[e.index()].waiters.clear();
    for (const Waiter& w : waiters) {
        if (processes_[w.process.index()].gen != w.gen) continue;
        if (w.guard == nullptr || w.guard(toC(ctx_), w.guardFrame)) {
            if (processes_[w.process.index()].gen == w.gen) wake(w.process, w.flushPoint);
        } else if (processes_[w.process.index()].gen == w.gen) {
            // A false iff leaves this subscription live for the next trigger.
            pushSwept(events_[e.index()].waiters, events_[e.index()].waiterLimit, w,
                      [&](const Waiter& x) { return processes_[x.process.index()].gen != x.gen; });
        }
    }
    const auto mons = events_[e.index()].monitors;
    std::vector<MonitorRef> keep;
    for (const MonitorRef ref : mons) {
        if (!live(ref)) continue;
        keep.push_back(ref);
        evaluateMonitor(ref.id);
    }
    for (const MonitorRef ref : events_[e.index()].monitors)  // added while evaluating
        if (live(ref) && std::find_if(keep.begin(), keep.end(), [&](const MonitorRef& k) {
                             return k.id == ref.id && k.serial == ref.serial;
                         }) == keep.end())
            keep.push_back(ref);
    events_[e.index()].monitors = std::move(keep);
    endWalk();
}

void Scheduler::triggerNb(ProcessId p, EventId e, std::uint64_t delay) {
    const Region r = processes_[p.index()].group == Group::Active ? Region::Nba : Region::ReNba;
    schedule(r, {Kind::TriggerNb, r, e.index(), 0}, delay);
}

bool Scheduler::triggered(EventId e) const {
    const auto& event = events_[e.index()];
    return event.wasTriggered && event.triggeredAt == time_;
}

// --- Pending reports (SCHEDULER_SPEC.md 15) -------------------------------------

void Scheduler::addPending(ProcessId p, PendingReport r) {
    Process& proc = processes_[p.index()];
    if (std::find(pendingOrder_.begin(), pendingOrder_.end(), p) == pendingOrder_.end())
        pendingOrder_.push_back(p);
    if (!r.final && proc.observed++ == 0) ++pendingProcesses_;
    proc.pending.push_back(std::move(r));
}

void Scheduler::deferReport(ProcessId p, std::string line) {
    PendingReport r;
    r.line = std::move(line);
    addPending(p, std::move(r));
}

void Scheduler::deferCall(ProcessId p, bool final, DeferredFn fn, void* frame,
                          std::uint32_t token) {
    PendingReport r;
    r.fn = fn;
    r.frame = frame;
    r.token = token;
    r.final = final;
    addPending(p, std::move(r));
}

void Scheduler::flushReports(ProcessId p) {
    Process& proc = processes_[p.index()];
    if (proc.pending.empty()) return;
    std::vector<PendingReport> dropped = std::move(proc.pending);
    proc.pending.clear();
    if (proc.observed != 0) --pendingProcesses_;
    proc.observed = 0;
    for (const PendingReport& r : dropped)
        if (r.fn != nullptr) r.fn(toC(ctx_), r.frame, r.token, 0);
}

void Scheduler::confirmReports() {
    // In the order of the processes' numbers, not of their first reports
    // (SCHEDULER_SPEC.md 15.4; the numbers are the creation order, 10.8);
    // within a process, in the order held. A confirmed call runs in the
    // Reactive region; final ones wait for the Postponed region (15.2).
    sortPending();
    std::vector<ProcessId> left;
    for (const ProcessId p : pendingOrder_) {
        std::vector<PendingReport> finals;
        for (PendingReport& r : processes_[p.index()].pending) {
            if (r.final) {
                finals.push_back(std::move(r));
            } else if (r.fn == nullptr) {
                ctx_.output().message(r.line);
            } else {
                std::uint32_t id = 0;
                if (!freeDeferred_.empty()) {
                    id = freeDeferred_.back();
                    freeDeferred_.pop_back();
                    deferredCalls_[id] = std::move(r);
                } else {
                    id = static_cast<std::uint32_t>(deferredCalls_.size());
                    deferredCalls_.push_back(std::move(r));
                }
                schedule(Region::Reactive, {Kind::Deferred, Region::Reactive, id, 0}, 0);
            }
        }
        processes_[p.index()].pending = std::move(finals);
        processes_[p.index()].observed = 0;
        if (!processes_[p.index()].pending.empty()) left.push_back(p);
    }
    pendingOrder_ = std::move(left);
    pendingProcesses_ = 0;
}

void Scheduler::sortPending() {
    std::sort(pendingOrder_.begin(), pendingOrder_.end(), [&](ProcessId a, ProcessId b) {
        return processes_[a.index()].serial < processes_[b.index()].serial;
    });
}

void Scheduler::confirmFinals() {
    sortPending();
    const std::vector<ProcessId> order = std::move(pendingOrder_);
    pendingOrder_.clear();
    for (const ProcessId p : order) {
        const std::vector<PendingReport> calls = std::move(processes_[p.index()].pending);
        processes_[p.index()].pending.clear();
        for (const PendingReport& r : calls)
            if (!ending_ && r.fn != nullptr) r.fn(toC(ctx_), r.frame, r.token, 1);
    }
}

// --- Ending ------------------------------------------------------------------

bool Scheduler::takeWallTimeLimit() {
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::steady_clock::now() - wallStarted_)
                             .count();
    if (elapsed < 0 || static_cast<std::uint64_t>(elapsed) < wallTimeLimit_) return false;
    interruptTaken_ = true;
    const std::string n = std::to_string(wallTimeLimit_);
    setEndNote("wall-clock limit reached: " + n + " seconds (--max-wall-time=" + n + ")");
    requestEnd(EndReason::WallTimeLimit, 1);
    return true;
}

void Scheduler::requestEnd(EndReason r, int level) {
    if (r == EndReason::Stop && stopAction_ == StopAction::Ignore) {
        ignoredStopStatistics_ = ignoredStopStatistics_ || level == 2;
        return;
    }
    if (ending_) return;
    ending_ = true;
    reason_ = r;
    level_ = level;
}

void Scheduler::fail(std::string message) {
    if (error_.empty()) error_ = std::move(message);
    requestEnd(EndReason::Internal, 1);
}

void Output::write(std::string_view text) {
    if (!text.empty()) {
        std::fwrite(text.data(), 1, text.size(), to_.out);
        atLineStart_ = text.back() == '\n';
    }
    if (autoFlush_) flush();
}

void Output::flush() { std::fflush(to_.out); }

void Output::writeError(std::string_view text) {
    std::fflush(to_.out);
    std::fwrite(text.data(), 1, text.size(), to_.err);
}

void Output::message(std::string_view line) {
    std::fflush(to_.out);
    std::fwrite(line.data(), 1, line.size(), to_.err);
    std::fputc('\n', to_.err);
}

}  // namespace kirimasim::kernel
