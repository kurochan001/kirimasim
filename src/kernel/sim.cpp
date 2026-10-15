#include "haremasim/kernel/sim.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

#include "haremasim/kernel/bits.h"
#include "haremasim/kernel/context.h"
#include "haremasim/kernel/edge.h"

namespace haremasim::kernel {

namespace {

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

}  // namespace

const char* endReasonName(EndReason r) {
    switch (r) {
        case EndReason::Finish: return "finish";
        case EndReason::Quiescent: return "quiescent";
        case EndReason::Stop: return "stop";
        case EndReason::Fatal: return "fatal";
        case EndReason::TimeLimit: return "time_limit";
        case EndReason::ZeroTimeLimit: return "zero_time_limit";
        case EndReason::Interrupted: return "interrupted";
        case EndReason::Error: return "error";
        case EndReason::Internal: return "internal";
    }
    return "internal";
}

int exitCode(EndReason r) {
    switch (r) {
        case EndReason::Finish:
        case EndReason::Quiescent:
        case EndReason::Stop: return 0;
        case EndReason::Fatal: return 1;
        case EndReason::Error: return 2;
        case EndReason::Internal: return 3;
        case EndReason::TimeLimit: return 4;
        case EndReason::ZeroTimeLimit: return 5;
        case EndReason::Interrupted: return 130;
    }
    return 3;
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
    Process proc;
    proc.fn = fn;
    proc.frame = frame;
    proc.group = group;
    processes_.push_back(proc);
    counters_.increment(cCreated_);
    return static_cast<ProcessId>(processes_.size() - 1);
}

void Scheduler::kill(ProcessId p) {
    Process& proc = processes_[p];
    if (proc.done) return;
    // A new wait number makes every pending wake-up of the process stale:
    // its delay events, subscriptions and named-event waits are dropped
    // when they come up (SCHEDULER_SPEC.md 10.7). NBAs it scheduled stay.
    freeMonitor(p);
    proc.done = true;
    ++proc.gen;
    counters_.increment(cFinished_);
}

void Scheduler::start(ProcessId p) {
    begin();
    wake(p);
}

EventId Scheduler::addEvent() {
    events_.emplace_back();
    return static_cast<EventId>(events_.size() - 1);
}

void Scheduler::ensureSlotTables() {
    if (subscriptions_.size() < store_.size()) {
        subscriptions_.resize(store_.size());
        slotMonitors_.resize(store_.size());
    }
}

// --- Scheduling --------------------------------------------------------------

void Scheduler::freeMonitor(ProcessId p) {
    Process& proc = processes_[p];
    if (proc.monitor == UINT32_MAX) return;
    Monitor& m = monitors_[proc.monitor];
    m.serial = ++monitorSerial_;  // references from slots and events go stale
    m.spec = {};
    freeMonitors_.push_back(proc.monitor);
    proc.monitor = UINT32_MAX;
}

void Scheduler::wake(ProcessId p) {
    freeMonitor(p);
    Process& proc = processes_[p];
    ++proc.gen;  // ends every other item of the wait
    const Region r = proc.group == Group::Active ? Region::Active : Region::Reactive;
    schedule(r, {Kind::Resume, r, p, proc.gen}, 0);
}

void Scheduler::schedule(Region r, Event e, std::uint64_t delay) {
    e.origin = r;
    if (delay == 0) {
        regions_[static_cast<int>(r)].push_back(e);
        return;
    }
    if (delay > UINT64_MAX - time_) return;  // beyond the end of time: never happens
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

void Scheduler::runFinal(std::span<const ProcessId> finals) {
    if (reason() == EndReason::Internal) return;
    const bool wasEnding = ending_;
    const EndReason endReason = reason_;
    const int endLevel = level_;
    for (const ProcessId p : finals) {
        if (processes_[p].done) continue;
        ending_ = false;
        counters_.increment(cWakeups_);
        const auto st = static_cast<Status>(processes_[p].fn(toC(ctx_), processes_[p].frame));
        processes_[p].done = true;
        ++processes_[p].gen;
        counters_.increment(cFinished_);
        if (st == Status::Error && reason_ != EndReason::Internal) {
            if (error_.empty()) error_ = "a final procedure failed";
            reason_ = EndReason::Internal;
        }
        if (ending_) break;  // $finish, or a failure
    }
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
        if (limits_.maxZeroDelay != 0 && movesThisSlot_ > limits_.maxZeroDelay)
            requestEnd(EndReason::ZeroTimeLimit, 1);
        return true;
    }
    return false;
}

void Scheduler::runSlot() {
    counters_.increment(cTimeSlots_);
    movesThisSlot_ = 0;
    runRegion(Region::Preponed);
    runRegion(Region::PreActive);
    while (!ending_ && anyEvents(Region::Active, Region::PrePostponed)) {
        while (!ending_ && anyEvents(Region::Active, Region::PostObserved)) {
            counters_.increment(cActiveLoops_);
            runRegion(Region::Active);
            if (!ending_) moveFirst(Region::Inactive, Region::PostObserved, Region::Active);
        }
        while (!ending_ && anyEvents(Region::Reactive, Region::PostReNba)) {
            counters_.increment(cReactiveLoops_);
            runRegion(Region::Reactive);
            if (!ending_) moveFirst(Region::ReInactive, Region::PostReNba, Region::Reactive);
        }
        if (!ending_ && !anyEvents(Region::Active, Region::PostReNba))
            runRegion(Region::PrePostponed);
    }
    if (!ending_) runRegion(Region::Postponed);
    // The arena of NBA values without a delay is freed with the slot.
    slotNbas_.clear();
    arena_.clear();
}

void Scheduler::runRegion(Region r) {
    auto& q = regions_[static_cast<int>(r)];
    while (!ending_ && !q.empty()) {
        const Event e = q.front();
        q.pop_front();
        execute(e);
    }
}

void Scheduler::execute(const Event& e) {
    counters_.increment(cEvents_[static_cast<int>(e.origin)]);
    switch (e.kind) {
        case Kind::Resume: {
            Process& proc = processes_[e.id];
            if (proc.done || proc.gen != e.gen) return;  // killed, or woken again
            counters_.increment(cWakeups_);
            const auto st = static_cast<Status>(proc.fn(toC(ctx_), proc.frame));
            Process& after = processes_[e.id];  // the table may have grown
            switch (st) {
                case Status::Waiting: break;
                case Status::Done:
                case Status::Killed:
                    freeMonitor(e.id);
                    if (!after.done) counters_.increment(cFinished_);
                    after.done = true;
                    ++after.gen;
                    break;
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
            scratch_.assign(arena_.begin() + static_cast<std::ptrdiff_t>(n.arenaOffset),
                            arena_.begin() + static_cast<std::ptrdiff_t>(n.arenaOffset + n.words));
            const std::vector<std::uint64_t> value = scratch_;
            write(n.slot, n.elem, n.lsb, n.width, value.data());
            return;
        }
        case Kind::NbaDelayed: {
            NbaRecord& n = delayedNbas_[e.id];
            const std::vector<std::uint64_t> value = std::move(n.value);
            const NbaRecord where = n;
            write(where.slot, where.elem, where.lsb, where.width, value.data());
            delayedNbas_[e.id].value.clear();
            freeDelayedNbas_.push_back(e.id);
            return;
        }
        case Kind::TriggerNb: trigger(e.id); return;
    }
}

// --- Waits -------------------------------------------------------------------

void Scheduler::waitDelay(ProcessId p, std::uint64_t ticks) {
    freeMonitor(p);
    Process& proc = processes_[p];
    ++proc.gen;
    const bool active = proc.group == Group::Active;
    if (ticks == 0) {
        const Region r = active ? Region::Inactive : Region::ReInactive;
        schedule(r, {Kind::Resume, r, p, proc.gen}, 0);
    } else {
        const Region r = active ? Region::Active : Region::Reactive;
        schedule(r, {Kind::Resume, r, p, proc.gen}, ticks);
    }
}

void Scheduler::waitEvent(ProcessId p, std::span<const WaitItem> items) {
    ensureSlotTables();
    freeMonitor(p);
    Process& proc = processes_[p];
    ++proc.gen;
    for (const WaitItem& item : items) {
        subscriptions_[item.part.slot].push_back({p, proc.gen, item});
        counters_.increment(cSubscriptions_);
    }
}

void Scheduler::waitMonitor(ProcessId p, const MonitorSpec& m) {
    ensureSlotTables();
    freeMonitor(p);
    ++processes_[p].gen;
    std::uint32_t id = 0;
    if (!freeMonitors_.empty()) {
        id = freeMonitors_.back();
        freeMonitors_.pop_back();
    } else {
        id = static_cast<std::uint32_t>(monitors_.size());
        monitors_.emplace_back();
    }
    processes_[p].monitor = id;
    Monitor& mon = monitors_[id];
    mon.process = p;
    mon.serial = ++monitorSerial_;
    mon.spec = m;
    mon.last.assign(elementWords(m.domain, m.width), 0);
    m.eval(toC(ctx_), m.frame, mon.last.data());
    counters_.increment(cMonitorEvals_);
    const MonitorRef ref{id, monitors_[id].serial};
    for (const SlotPart& part : m.slots) {
        auto& list = slotMonitors_[part.slot];
        if (list.empty() || list.back().id != id || list.back().serial != ref.serial)
            list.push_back(ref);
    }
    for (const EventId e : m.events)
        events_[e].monitors.push_back(ref);
}

void Scheduler::waitNamed(ProcessId p, EventId e) {
    freeMonitor(p);
    Process& proc = processes_[p];
    ++proc.gen;
    events_[e].waiters.emplace_back(p, proc.gen);
}

// --- Effects -----------------------------------------------------------------

bool Scheduler::write(SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
                      const std::uint64_t* value) {
    begin();
    ensureSlotTables();
    const SlotLayout& l = store_.layout(s);
    if (width == 0) {
        lsb = 0;
        width = l.width;
    }
    std::uint64_t* p = store_.element(s, elem);
    const bool watched = !subscriptions_[s].empty() || !slotMonitors_[s].empty();
    if (watched) old_.assign(p, p + l.elemWords);
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
    if (watched) notify(s, elem, lsb, width);
    return true;
}

void Scheduler::notify(SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width) {
    const SlotLayout& l = store_.layout(s);
    const std::uint64_t* now = store_.element(s, elem);
    // Subscriptions. Waking a process makes its other items stale; they
    // are dropped here or on the next write of their slot.
    auto& subs = subscriptions_[s];
    std::size_t keep = 0;
    for (std::size_t i = 0; i < subs.size(); ++i) {
        const Subscription sub = subs[i];
        if (processes_[sub.process].gen != sub.gen) continue;  // stale
        subs[keep++] = sub;
        const SlotPart& part = sub.item.part;
        const std::uint32_t plsb = part.width == 0 ? 0 : part.lsb;
        const std::uint32_t pwidth = part.width == 0 ? l.width : part.width;
        if (!overlaps(part.firstElem, part.numElems, elem, 1) ||
            !overlaps(plsb, pwidth, lsb, width))
            continue;
        if (sameBits(l.domain, old_.data(), now, plsb, pwidth)) continue;
        if (sub.item.edge != Edge::Any &&
            !edgeMatches(sub.item.edge, logicAt(l.domain, old_.data(), plsb),
                         logicAt(l.domain, now, plsb)))
            continue;
        if (sub.item.guard != nullptr && sub.item.guard(toC(ctx_), sub.item.guardFrame) == 0)
            continue;
        wake(sub.process);
        --keep;  // this item is used up
    }
    subscriptions_[s].resize(keep);

    // Monitor records that depend on the changed part.
    auto& mons = slotMonitors_[s];
    std::size_t kept = 0;
    for (std::size_t i = 0; i < mons.size(); ++i) {
        const MonitorRef ref = slotMonitors_[s][i];
        if (!live(ref)) continue;  // stale
        slotMonitors_[s][kept++] = ref;
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
    slotMonitors_[s].resize(kept);
}

void Scheduler::evaluateMonitor(std::uint32_t id) {
    Monitor& m = monitors_[id];
    if (processes_[m.process].monitor != id) return;  // already woken
    const MonitorSpec& spec = m.spec;
    scratch_.assign(m.last.size(), 0);
    spec.eval(toC(ctx_), spec.frame, scratch_.data());
    counters_.increment(cMonitorEvals_);
    bool fire = false;
    if (spec.untilTrue) {
        fire = isTrue(spec.domain, spec.width, scratch_.data());
    } else if (!sameBits(spec.domain, m.last.data(), scratch_.data(), 0, spec.width)) {
        fire =
            spec.edge == Edge::Any || edgeMatches(spec.edge, logicAt(spec.domain, m.last.data(), 0),
                                                  logicAt(spec.domain, scratch_.data(), 0));
    }
    Monitor& again = monitors_[id];
    again.last = scratch_;
    if (fire) wake(again.process);  // frees the record
}

void Scheduler::scheduleNba(ProcessId p, SlotId s, std::uint32_t elem, std::uint32_t lsb,
                            std::uint32_t width, const std::uint64_t* value, std::uint64_t delay) {
    begin();
    const SlotLayout& l = store_.layout(s);
    const std::uint32_t bits = width == 0 ? l.width : width;
    const std::uint32_t words = elementWords(l.domain, bits);
    counters_.increment(cNbaScheduled_);
    const Region r = processes_[p].group == Group::Active ? Region::Nba : Region::ReNba;
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

void Scheduler::trigger(EventId e) {
    NamedEvent& ev = events_[e];
    ev.triggeredAt = time_;
    const auto waiters = std::move(ev.waiters);
    events_[e].waiters.clear();
    for (const auto& [p, gen] : waiters)
        if (processes_[p].gen == gen) wake(p);
    const auto mons = events_[e].monitors;
    std::vector<MonitorRef> keep;
    for (const MonitorRef ref : mons) {
        if (!live(ref)) continue;
        keep.push_back(ref);
        evaluateMonitor(ref.id);
    }
    for (const MonitorRef ref : events_[e].monitors)  // added while evaluating
        if (live(ref) && std::find_if(keep.begin(), keep.end(), [&](const MonitorRef& k) {
                             return k.id == ref.id && k.serial == ref.serial;
                         }) == keep.end())
            keep.push_back(ref);
    events_[e].monitors = std::move(keep);
}

void Scheduler::triggerNb(ProcessId p, EventId e, std::uint64_t delay) {
    const Region r = processes_[p].group == Group::Active ? Region::Nba : Region::ReNba;
    schedule(r, {Kind::TriggerNb, r, e, 0}, delay);
}

bool Scheduler::triggered(EventId e) const { return events_[e].triggeredAt == time_; }

// --- Ending ------------------------------------------------------------------

void Scheduler::requestEnd(EndReason r, int level) {
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
    if (text.empty()) return;
    std::fwrite(text.data(), 1, text.size(), stdout);
    atLineStart_ = text.back() == '\n';
}

void Output::flush() { std::fflush(stdout); }

}  // namespace haremasim::kernel
