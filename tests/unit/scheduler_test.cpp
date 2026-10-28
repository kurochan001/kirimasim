// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Tests for the kernel scheduler (SCHEDULER_SPEC.md).
//
// Processes here are small scripts: a list of steps such as "write 3 to
// slot 1" or "wait for a posedge of slot 0". The same scripts also run on
// RefSim, a deliberately naive model of the reference algorithm (4.5 of
// the standard, SCHEDULER_SPEC.md 3) that keeps every event in plain lists
// and scans them. Random scripts must give the same log on both.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <memory_resource>
#include <new>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "fuzz.h"
#include "kirimasim/kernel/abi.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/edge.h"
#include "kirimasim/valsem/logic.h"

namespace {

namespace k = kirimasim::kernel;
using k::Edge;
using k::EndReason;
using k::Group;
using k::Status;

// --- Scripts -----------------------------------------------------------------

enum class Op {
    Log,        // log "L<a>"
    Delay,      // #b
    Write,      // slot a = b
    Nba,        // slot a <= #c b
    WaitEdge,   // @(<edge c> slot a)
    WaitNamed,  // @ev a
    Trigger,    // -> ev a
    TriggerNb,  // ->> #c ev a
    WaitTrue,   // wait (slot a != 0)
    Finish,     // $finish
    Kill,       // kill process a (not the running one)
};

struct Step {
    Op op = Op::Log;
    std::uint32_t a = 0;
    std::uint64_t b = 0;
    std::uint64_t c = 0;
};

struct Script {
    std::vector<Step> steps;
    Group group = Group::Active;
};

/// What a script needs from a scheduler. Slots are two-state, 8 bits.
class Backend {
public:
    virtual ~Backend() = default;
    virtual std::uint64_t now() = 0;
    virtual std::uint64_t read(std::uint32_t slot) = 0;
    virtual void write(std::uint32_t slot, std::uint64_t v) = 0;
    virtual void nba(std::uint32_t p, std::uint32_t slot, std::uint64_t v, std::uint64_t d) = 0;
    virtual void waitDelay(std::uint32_t p, std::uint64_t d) = 0;
    virtual void waitEdge(std::uint32_t p, std::uint32_t slot, Edge e) = 0;
    virtual void waitNamed(std::uint32_t p, std::uint32_t ev) = 0;
    virtual void trigger(std::uint32_t ev) = 0;
    virtual void triggerNb(std::uint32_t p, std::uint32_t ev, std::uint64_t d) = 0;
    virtual void waitTrue(std::uint32_t p, std::uint32_t slot) = 0;
    virtual void finish() = 0;
    virtual void kill(std::uint32_t p) = 0;
};

/// Runs a script from its program counter until it waits or ends, writing
/// what it does to the log.
struct Runner {
    std::vector<Script> scripts;
    std::vector<std::size_t> pc;
    std::string log;

    Status step(Backend& b, std::uint32_t p) {
        const auto& steps = scripts[p].steps;
        log += "t" + std::to_string(b.now()) + " p" + std::to_string(p) + " run\n";
        while (pc[p] < steps.size()) {
            const Step s = steps[pc[p]++];
            const std::string who = "t" + std::to_string(b.now()) + " p" + std::to_string(p);
            switch (s.op) {
                case Op::Log:
                    log += who + " L" + std::to_string(s.a) + " s0=" + std::to_string(b.read(0)) +
                           "\n";
                    break;
                case Op::Delay: b.waitDelay(p, s.b); return Status::Waiting;
                case Op::Write:
                    b.write(s.a, s.b);
                    log += who + " w" + std::to_string(s.a) + "=" + std::to_string(s.b) + "\n";
                    break;
                case Op::Nba: b.nba(p, s.a, s.b, s.c); break;
                case Op::WaitEdge:
                    b.waitEdge(p, s.a, static_cast<Edge>(s.c));
                    return Status::Waiting;
                case Op::WaitNamed: b.waitNamed(p, s.a); return Status::Waiting;
                case Op::Trigger: b.trigger(s.a); break;
                case Op::TriggerNb: b.triggerNb(p, s.a, s.c); break;
                case Op::WaitTrue:
                    if (b.read(s.a) != 0) break;
                    b.waitTrue(p, s.a);
                    return Status::Waiting;
                case Op::Finish: b.finish(); return Status::Stop;
                case Op::Kill:
                    if (s.a != p && s.a < scripts.size()) b.kill(s.a);
                    log += who + " k" + std::to_string(s.a) + "\n";
                    break;
            }
        }
        return Status::Done;
    }
};

// --- The kernel as a backend ---------------------------------------------------

class KernelSim : public Backend {
public:
    explicit KernelSim(std::vector<Script> scripts, std::uint32_t slots = 4,
                       std::uint32_t events = 2) {
        runner_.scripts = std::move(scripts);
        runner_.pc.assign(runner_.scripts.size(), 0);
        for (std::uint32_t i = 0; i < slots; ++i)
            ctx_.store().add(k::Domain::Two, 8, 1, k::Init::Zero);
        for (std::uint32_t i = 0; i < events; ++i)
            ctx_.scheduler().addEvent();
        frames_.resize(runner_.scripts.size());
        monitors_.resize(runner_.scripts.size());
        for (std::uint32_t p = 0; p < runner_.scripts.size(); ++p) {
            frames_[p] = {this, p};
            ctx_.scheduler().addProcess(&resume, &frames_[p], runner_.scripts[p].group);
        }
    }

    EndReason run() {
        for (std::uint32_t p = 0; p < runner_.scripts.size(); ++p)
            ctx_.scheduler().start(k::ProcessId{p});
        return ctx_.scheduler().run();
    }
    const std::string& log() const { return runner_.log; }
    k::Context& context() { return ctx_; }

    std::uint64_t now() override { return ctx_.scheduler().time(); }
    std::uint64_t read(std::uint32_t slot) override {
        return ctx_.store().element(k::SlotId{slot}, 0)[0];
    }
    void write(std::uint32_t slot, std::uint64_t v) override {
        ctx_.scheduler().write({.slot = k::SlotId{slot}}, &v);
    }
    void nba(std::uint32_t p, std::uint32_t slot, std::uint64_t v, std::uint64_t d) override {
        ctx_.scheduler().scheduleNba(k::ProcessId{p}, {.slot = k::SlotId{slot}}, &v, d);
    }
    void waitDelay(std::uint32_t p, std::uint64_t d) override {
        ctx_.scheduler().waitDelay(k::ProcessId{p}, d);
    }
    void waitEdge(std::uint32_t p, std::uint32_t slot, Edge e) override {
        const k::WaitItem item{{k::SlotId{slot}, 0, 0, 0, 0}, e, nullptr, nullptr};
        ctx_.scheduler().waitEvent(k::ProcessId{p}, {&item, 1});
    }
    void waitNamed(std::uint32_t p, std::uint32_t ev) override {
        ctx_.scheduler().waitNamed(k::ProcessId{p}, k::EventId{ev});
    }
    void trigger(std::uint32_t ev) override { ctx_.scheduler().trigger(k::EventId{ev}); }
    void triggerNb(std::uint32_t p, std::uint32_t ev, std::uint64_t d) override {
        ctx_.scheduler().triggerNb(k::ProcessId{p}, k::EventId{ev}, d);
    }
    void waitTrue(std::uint32_t p, std::uint32_t slot) override {
        monitors_[p] = {this, slot};
        k::MonitorSpec m;
        m.slots = {{k::SlotId{slot}, 0, 0, 0, 0}};
        m.eval = &evalSlot;
        m.frame = &monitors_[p];
        m.domain = k::Domain::Two;
        m.width = 8;
        m.untilTrue = true;
        ctx_.scheduler().waitMonitor(k::ProcessId{p}, m);
    }
    void finish() override { ctx_.scheduler().requestEnd(EndReason::Finish, 1); }
    void kill(std::uint32_t p) override { ctx_.scheduler().kill(k::ProcessId{p}); }

private:
    struct Frame {
        KernelSim* sim;
        std::uint32_t process;
    };
    struct MonitorFrame {
        KernelSim* sim = nullptr;
        std::uint32_t slot = 0;
    };
    k::Context ctx_;
    Runner runner_;
    std::vector<Frame> frames_;
    std::vector<MonitorFrame> monitors_;

    static kirima_status resume(kirima_context* /*ctx*/, void* frame) {
        auto* f = static_cast<Frame*>(frame);
        return static_cast<kirima_status>(f->sim->runner_.step(*f->sim, f->process));
    }
    static void evalSlot(kirima_context* ctx, void* frame, std::uint64_t* out) {
        const auto* m = static_cast<MonitorFrame*>(frame);
        out[0] = k::fromC(ctx).store().element(k::SlotId{m->slot}, 0)[0];
    }
};

// --- The reference model -------------------------------------------------------

/// A naive model of the reference algorithm. Events are kept in plain
/// vectors per region and one unsorted list of future events, and every
/// wait is one entry of a single list scanned in the order it was made.
/// Wake-ups caused by one write go in that order, subscriptions on the
/// slot first and then `wait` conditions, as kirimasim does (SCHEDULER_SPEC.md
/// 4.3: events leave a region in the order they were scheduled).
class RefSim : public Backend {
public:
    explicit RefSim(std::vector<Script> scripts, std::uint32_t slots = 4,
                    std::uint32_t events = 2) {
        runner_.scripts = std::move(scripts);
        runner_.pc.assign(runner_.scripts.size(), 0);
        slots_.assign(slots, 0);
        triggeredAt_.assign(events, UINT64_MAX);
        gen_.assign(runner_.scripts.size(), 0);
        killed_.assign(runner_.scripts.size(), false);
    }

    EndReason run() {
        for (std::uint32_t p = 0; p < runner_.scripts.size(); ++p)
            wake(p);
        while (!ending_) {
            if (empty(0, 16)) {
                if (future_.empty()) break;
                std::uint64_t t = UINT64_MAX;
                for (const Fut& f : future_)
                    t = std::min(t, f.time);
                time_ = t;
                std::vector<Fut> due;
                std::vector<Fut> rest;
                for (const Fut& f : future_)
                    (f.time == t ? due : rest).push_back(f);
                future_ = rest;
                std::sort(due.begin(), due.end(), [](const Fut& x, const Fut& y) {
                    return x.region != y.region ? x.region < y.region : x.seq < y.seq;
                });
                for (const Fut& f : due)
                    regions_[f.region].push_back(f.ev);
            }
            // One time slot (SCHEDULER_SPEC.md 3).
            runRegion(0);
            runRegion(1);
            while (!ending_ && !empty(2, 15)) {
                while (!ending_ && !empty(2, 9)) {
                    runRegion(2);
                    moveFirst(3, 9, 2);
                }
                while (!ending_ && !empty(10, 14)) {
                    runRegion(10);
                    moveFirst(11, 14, 10);
                }
                if (!ending_ && empty(2, 14)) runRegion(15);
            }
            if (!ending_) runRegion(16);
        }
        return ending_ ? reason_ : EndReason::Quiescent;
    }
    const std::string& log() const { return runner_.log; }
    std::uint64_t slot(std::uint32_t i) const { return slots_[i]; }

    std::uint64_t now() override { return time_; }
    std::uint64_t read(std::uint32_t slot) override { return slots_[slot]; }
    void write(std::uint32_t slot, std::uint64_t v) override {
        v &= 0xff;
        const std::uint64_t old = slots_[slot];
        slots_[slot] = v;
        if (old == v) return;
        const auto copy = waits_;
        for (const Wait& w : copy) {
            if (w.kind != 0 || w.obj != slot || gen_[w.p] != w.gen) continue;
            const unsigned from = old & 1U;
            const unsigned to = v & 1U;
            const bool hit = w.edge == Edge::Any || (w.edge == Edge::Pos && from == 0 && to == 1) ||
                             (w.edge == Edge::Neg && from == 1 && to == 0) ||
                             (w.edge == Edge::Both && from != to);
            if (hit) wake(w.p);
        }
        for (const Wait& w : copy)
            if (w.kind == 2 && w.obj == slot && gen_[w.p] == w.gen && v != 0) wake(w.p);
    }
    void nba(std::uint32_t p, std::uint32_t slot, std::uint64_t v, std::uint64_t d) override {
        put(runner_.scripts[p].group == Group::Active ? 5 : 13, {1, slot, v}, d);
    }
    void waitDelay(std::uint32_t p, std::uint64_t d) override {
        ++gen_[p];
        const bool active = runner_.scripts[p].group == Group::Active;
        if (d == 0)
            put(active ? 3 : 11, {0, p, gen_[p]}, 0);
        else
            put(active ? 2 : 10, {0, p, gen_[p]}, d);
    }
    void waitEdge(std::uint32_t p, std::uint32_t slot, Edge e) override {
        waits_.push_back({p, ++gen_[p], 0, slot, e});
    }
    void waitNamed(std::uint32_t p, std::uint32_t ev) override {
        waits_.push_back({p, ++gen_[p], 1, ev, Edge::Any});
    }
    void trigger(std::uint32_t ev) override {
        triggeredAt_[ev] = time_;
        const auto copy = waits_;
        for (const Wait& w : copy)
            if (w.kind == 1 && w.obj == ev && gen_[w.p] == w.gen) wake(w.p);
    }
    void triggerNb(std::uint32_t p, std::uint32_t ev, std::uint64_t d) override {
        put(runner_.scripts[p].group == Group::Active ? 5 : 13, {2, ev, 0}, d);
    }
    void waitTrue(std::uint32_t p, std::uint32_t slot) override {
        waits_.push_back({p, ++gen_[p], 2, slot, Edge::Any});
    }
    void finish() override {
        if (!ending_) ending_ = true, reason_ = EndReason::Finish;
    }
    void kill(std::uint32_t p) override {
        // Never resumed again: every pending wake-up is stale.
        killed_[p] = true;
        ++gen_[p];
    }

private:
    struct Ev {
        int kind;  // 0 resume (a = process, b = gen), 1 NBA (a = slot, b = value), 2 trigger
        std::uint64_t a;
        std::uint64_t b;
    };
    struct Fut {
        std::uint64_t time;
        int region;
        std::uint64_t seq;
        Ev ev;
    };
    struct Wait {
        std::uint32_t p;
        std::uint32_t gen;
        int kind;  // 0 edge, 1 named event, 2 wait (slot != 0)
        std::uint32_t obj;
        Edge edge;
    };
    Runner runner_;
    std::vector<std::uint64_t> slots_;
    std::vector<std::uint64_t> triggeredAt_;
    std::vector<std::uint32_t> gen_;
    std::vector<bool> killed_;
    std::vector<Ev> regions_[17];
    std::vector<Fut> future_;
    std::vector<Wait> waits_;
    std::uint64_t time_ = 0;
    std::uint64_t seq_ = 0;
    bool ending_ = false;
    EndReason reason_ = EndReason::Quiescent;

    void put(int region, Ev ev, std::uint64_t d) {
        if (d == 0)
            regions_[region].push_back(ev);
        else
            future_.push_back({time_ + d, region, seq_++, ev});
    }
    void wake(std::uint32_t p) {
        ++gen_[p];
        put(runner_.scripts[p].group == Group::Active ? 2 : 10, {0, p, gen_[p]}, 0);
    }
    bool empty(int first, int last) const {
        for (int r = first; r <= last; ++r)
            if (!regions_[r].empty()) return false;
        return true;
    }
    void moveFirst(int first, int last, int to) {
        if (ending_) return;
        for (int r = first; r <= last; ++r) {
            if (regions_[r].empty()) continue;
            for (const Ev& e : regions_[r])
                regions_[to].push_back(e);
            regions_[r].clear();
            return;
        }
    }
    void runRegion(int r) {
        while (!ending_ && !regions_[r].empty()) {
            const Ev e = regions_[r].front();
            regions_[r].erase(regions_[r].begin());
            if (e.kind == 0) {
                const auto p = static_cast<std::uint32_t>(e.a);
                if (gen_[p] != e.b || killed_[p]) continue;
                if (runner_.step(*this, p) != Status::Waiting) ++gen_[p];
            } else if (e.kind == 1) {
                write(static_cast<std::uint32_t>(e.a), e.b);
            } else {
                trigger(static_cast<std::uint32_t>(e.a));
            }
        }
    }
};

// --- Fixed scenarios -------------------------------------------------------------

Step log(std::uint32_t n) { return {Op::Log, n, 0, 0}; }
Step delay(std::uint64_t d) { return {Op::Delay, 0, d, 0}; }
Step write(std::uint32_t s, std::uint64_t v) { return {Op::Write, s, v, 0}; }
Step nba(std::uint32_t s, std::uint64_t v, std::uint64_t d = 0) { return {Op::Nba, s, v, d}; }
Step waitEdge(std::uint32_t s, Edge e) {
    return {Op::WaitEdge, s, 0, static_cast<std::uint64_t>(e)};
}
Step waitNamed(std::uint32_t ev) { return {Op::WaitNamed, ev, 0, 0}; }
Step trig(std::uint32_t ev) { return {Op::Trigger, ev, 0, 0}; }
Step trigNb(std::uint32_t ev, std::uint64_t d = 0) { return {Op::TriggerNb, ev, 0, d}; }
Step waitTrue(std::uint32_t s) { return {Op::WaitTrue, s, 0, 0}; }
Step finish() { return {Op::Finish, 0, 0, 0}; }

TEST(Scheduler, NbaUpdatesInStatementOrder) {
    // q <= 1; q <= 2; leaves q == 2 (4.6), and the old value is seen until
    // the NBA region runs.
    KernelSim sim({{{nba(0, 1), nba(0, 2), log(1), delay(1), log(2)}}});
    EXPECT_EQ(sim.run(), EndReason::Quiescent);
    EXPECT_EQ(sim.log(), "t0 p0 run\nt0 p0 L1 s0=0\nt1 p0 run\nt1 p0 L2 s0=2\n");
}

TEST(Scheduler, ZeroDelayResumesInInactive) {
    // p0: #0 then log; p1: log. p1 runs first, in the same time slot.
    KernelSim sim({{{delay(0), log(0)}}, {{log(1)}}});
    sim.run();
    EXPECT_EQ(sim.log(), "t0 p0 run\nt0 p1 run\nt0 p1 L1 s0=0\nt0 p0 run\nt0 p0 L0 s0=0\n");
    const auto& c = sim.context().counters();
    const std::string json = c.toJson();
    EXPECT_NE(json.find("\"sched.events.inactive\": 1"), std::string::npos) << json;
    EXPECT_NE(json.find("\"sched.events.active\": 2"), std::string::npos) << json;
}

TEST(Scheduler, EdgeWaitsAndValueChanges) {
    // p0 waits for a posedge of s1; p1 writes 2 (bit 0 stays 0), then 3.
    KernelSim sim({{{waitEdge(1, Edge::Pos), log(0)}}, {{write(1, 2), delay(1), write(1, 3)}}});
    sim.run();
    EXPECT_EQ(sim.log(), "t0 p0 run\nt0 p1 run\nt0 p1 w1=2\nt1 p1 run\nt1 p1 w1=3\n"
                         "t1 p0 run\nt1 p0 L0 s0=0\n");
}

TEST(Scheduler, NamedEventNeedsTheWaiterFirst) {
    // p0 waits on ev 0 before p1 triggers it; p2 waits after, and stays.
    KernelSim sim({{{waitNamed(0), log(0)}}, {{trig(0)}}, {{waitNamed(0), log(2)}}});
    EXPECT_EQ(sim.run(), EndReason::Quiescent);
    EXPECT_EQ(sim.log(), "t0 p0 run\nt0 p1 run\nt0 p2 run\nt0 p0 run\nt0 p0 L0 s0=0\n");
}

TEST(Scheduler, NonblockingTriggerInTheNbaRegion) {
    // ->> triggers in the NBA region, so p1, which waits after p0's ->>,
    // still wakes.
    KernelSim sim({{{trigNb(0)}}, {{waitNamed(0), log(1)}}});
    sim.run();
    EXPECT_EQ(sim.log(), "t0 p0 run\nt0 p1 run\nt0 p1 run\nt0 p1 L1 s0=0\n");
    EXPECT_TRUE(sim.context().scheduler().triggered(k::EventId{0}));
}

TEST(Scheduler, WaitConditionWakesWhenTrue) {
    KernelSim sim({{{waitTrue(2), log(0)}}, {{delay(5), write(2, 4)}}});
    sim.run();
    EXPECT_EQ(sim.log(), "t0 p0 run\nt0 p1 run\nt5 p1 run\nt5 p1 w2=4\nt5 p0 run\nt5 p0 L0 s0=0\n");
    EXPECT_EQ(sim.context().scheduler().time(), 5U);
}

TEST(Scheduler, ReactiveRunsAfterTheActiveSide) {
    // p1 is in the reactive group. p0's write at time 1 wakes it, but it
    // runs only after p0's NBA has gone through the active side.
    Script reactive{{waitEdge(1, Edge::Any), log(1)}, Group::Reactive};
    KernelSim sim({{{delay(1), write(1, 1), nba(0, 7), log(0)}}, reactive});
    sim.run();
    EXPECT_EQ(sim.log(), "t0 p0 run\nt0 p1 run\nt1 p0 run\nt1 p0 w1=1\nt1 p0 L0 s0=0\n"
                         "t1 p1 run\nt1 p1 L1 s0=7\n");
}

TEST(Scheduler, FinishStopsEverything) {
    KernelSim sim({{{delay(3), finish(), log(0)}}, {{delay(10), log(1)}}});
    EXPECT_EQ(sim.run(), EndReason::Finish);
    EXPECT_EQ(sim.context().scheduler().time(), 3U);
    EXPECT_EQ(sim.log().find('L'), std::string::npos);
}

TEST(Scheduler, TimeLimit) {
    KernelSim sim({{{delay(3), log(0), delay(10), log(1)}}});
    sim.context().scheduler().setLimits({5, 0});
    EXPECT_EQ(sim.run(), EndReason::TimeLimit);
    EXPECT_EQ(sim.context().scheduler().time(), 5U);
    EXPECT_NE(sim.log().find("L0"), std::string::npos);
    EXPECT_EQ(sim.log().find("L1"), std::string::npos);
}

TEST(Scheduler, TimeLimitRunsEventsAtTheLimit) {
    // CLI.md 2.5: events at the limit itself still run.
    KernelSim sim({{{delay(5), log(0), delay(1), log(1)}}});
    sim.context().scheduler().setLimits({5, 0});
    EXPECT_EQ(sim.run(), EndReason::TimeLimit);
    EXPECT_EQ(sim.context().scheduler().time(), 5U);
    EXPECT_NE(sim.log().find("L0"), std::string::npos);
    EXPECT_EQ(sim.log().find("L1"), std::string::npos);
}

TEST(Scheduler, KilledProcessNeverResumes) {
    // SCHEDULER_SPEC.md 10.7: the delay, the subscription and the named
    // event wait of a killed process are all dropped; its NBA still runs.
    KernelSim sim({{{nba(1, 5, 2), delay(3), log(0)}},
                   {{waitEdge(0, Edge::Any), log(1)}},
                   {{waitNamed(0), log(2)}},
                   {{delay(1),
                     {Op::Kill, 0, 0, 0},
                     {Op::Kill, 1, 0, 0},
                     {Op::Kill, 2, 0, 0},
                     write(0, 1),
                     trig(0),
                     delay(5),
                     log(3)}}});
    EXPECT_EQ(sim.run(), EndReason::Quiescent);
    EXPECT_EQ(sim.log().find(" L0"), std::string::npos);
    EXPECT_EQ(sim.log().find(" L1"), std::string::npos);
    EXPECT_EQ(sim.log().find(" L2"), std::string::npos);
    EXPECT_NE(sim.log().find("t6 p3 L3"), std::string::npos);
    EXPECT_EQ(sim.context().store().element(k::SlotId{1}, 0)[0], 5U);
    const std::string json = sim.context().counters().toJson();
    EXPECT_NE(json.find("\"proc.created\": 4, \"proc.finished\": 4"), std::string::npos) << json;
}

TEST(Scheduler, ZeroDelayLimit) {
    const std::vector<Step> spin(50, delay(0));
    KernelSim sim({{spin}});
    sim.context().scheduler().setLimits({UINT64_MAX, 10});
    EXPECT_EQ(sim.run(), EndReason::ZeroTimeLimit);
}

TEST(Scheduler, SlotEventLimit) {
    // A chain of processes that wake each other through named events stays
    // in the Active region: 41 starts and 40 wakeups, and no moves, so only
    // the limit on the events of a time slot stops it (CLI.md 2.6). The
    // event past the limit does not run.
    const auto chain = [] {
        std::vector<Script> s;
        s.reserve(41);
        for (std::uint32_t i = 0; i < 40; ++i)
            s.push_back({{waitNamed(i), trig(i + 1)}});
        s.push_back({{trig(0)}});
        return s;
    };
    const auto runs = [](const std::string& log) {
        std::size_t n = 0;
        for (std::size_t at = log.find(" run\n"); at != std::string::npos;
             at = log.find(" run\n", at + 1))
            ++n;
        return n;
    };
    {
        KernelSim sim(chain(), 4, 41);
        sim.context().scheduler().setLimits({UINT64_MAX, 1, 81});
        EXPECT_EQ(sim.run(), EndReason::Quiescent);
        EXPECT_EQ(sim.context().scheduler().endNote(), "");
        EXPECT_EQ(runs(sim.log()), 81U) << sim.log();
    }
    {
        KernelSim sim(chain(), 4, 41);
        sim.context().scheduler().setLimits({UINT64_MAX, 0, 80});
        EXPECT_EQ(sim.run(), EndReason::ZeroTimeLimit);
        EXPECT_EQ(sim.context().scheduler().endNote(),
                  "80 events ran in one time slot (--max-slot-events=80)");
        EXPECT_EQ(runs(sim.log()), 80U) << sim.log();
    }
    {
        // The count starts again in each time slot: one event in each of
        // ten slots stays under a limit of 2.
        KernelSim sim({{std::vector<Step>(10, delay(1))}});
        sim.context().scheduler().setLimits({UINT64_MAX, 0, 2});
        EXPECT_EQ(sim.run(), EndReason::Quiescent);
        EXPECT_EQ(sim.context().scheduler().time(), 10U);
    }
}

TEST(Scheduler, FarFutureEventsUseTheHeap) {
    // Delays beyond the timing wheel (256 ticks) keep their order.
    KernelSim sim({{{delay(1000), log(0)}},
                   {{delay(300), log(1)}},
                   {{delay(1000), log(2)}},
                   {{delay(255), log(3)}}});
    sim.run();
    EXPECT_EQ(sim.log(), "t0 p0 run\nt0 p1 run\nt0 p2 run\nt0 p3 run\nt255 p3 run\n"
                         "t255 p3 L3 s0=0\nt300 p1 run\nt300 p1 L1 s0=0\nt1000 p0 run\n"
                         "t1000 p0 L0 s0=0\nt1000 p2 run\nt1000 p2 L2 s0=0\n");
}

TEST(Scheduler, EndReasonInStats) {
    KernelSim sim({{{finish()}}});
    sim.run();
    const std::string json = sim.context().counters().toJson();
    EXPECT_NE(json.find("\"sim.end_reason\": \"finish\""), std::string::npos) << json;
}

// --- Four-state slots, guards and monitors ----------------------------------------

struct GuardState {
    int calls = 0;
    bool value = false;
};

int guardFn(kirima_context* /*ctx*/, void* frame) {
    auto* g = static_cast<GuardState*>(frame);
    ++g->calls;
    return g->value ? 1 : 0;
}

struct Waiter {
    std::vector<std::string>* log;
    std::string name;
};

kirima_status logOnce(kirima_context* ctx, void* frame) {
    auto* w = static_cast<Waiter*>(frame);
    w->log->push_back(w->name + "@" + std::to_string(k::fromC(ctx).scheduler().time()));
    return KIRIMA_DONE;
}

TEST(Scheduler, FourStateEdgesMatchTheReference) {
    namespace v = kirimasim::valsem;
    const v::Logic values[4] = {v::Logic::Zero, v::Logic::One, v::Logic::Z, v::Logic::X};
    for (unsigned from = 0; from < 4; ++from) {
        for (unsigned to = 0; to < 4; ++to) {
            EXPECT_EQ(k::isPosedge(from, to), v::isPosedge(values[from], values[to]))
                << from << " -> " << to;
            EXPECT_EQ(k::isNegedge(from, to), v::isNegedge(values[from], values[to]))
                << from << " -> " << to;
        }
    }
}

TEST(Scheduler, PosedgeOnFourStateSlots) {
    // A 4-bit four-state slot starting at x: x -> 1 on bit 0 is a posedge;
    // a change of bit 3 alone is not.
    k::Context ctx;
    auto& s = ctx.scheduler();
    const k::SlotId slot = ctx.store().add(k::Domain::Four, 4, 1, k::Init::X);
    std::vector<std::string> log;
    Waiter w{&log, "w"};
    const k::ProcessId p = s.addProcess(&logOnce, &w, Group::Active);
    const k::WaitItem item{{slot, 0, 0, 0, 0}, Edge::Pos, nullptr, nullptr};
    s.waitEvent(p, {&item, 1});
    const std::uint64_t one[2] = {1, 0};                 // a = 1, b = 0: a known 1
    s.write({.slot = slot, .lsb = 3, .width = 1}, one);  // bit 3: x -> 1; bit 0 stays x
    s.run();
    EXPECT_TRUE(log.empty());
    s.write({.slot = slot, .width = 1}, one);  // bit 0: x -> 1, a posedge
    s.run();
    EXPECT_EQ(log, std::vector<std::string>{"w@0"});
}

TEST(Scheduler, IffGuardIsReadOnlyWhenTheEventOccurs) {
    k::Context ctx;
    auto& s = ctx.scheduler();
    const k::SlotId clk = ctx.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
    const k::SlotId en = ctx.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
    std::vector<std::string> log;
    Waiter w{&log, "w"};
    GuardState g;
    const k::ProcessId p = s.addProcess(&logOnce, &w, Group::Active);
    const k::WaitItem item{{clk, 0, 0, 0, 0}, Edge::Pos, &guardFn, &g};
    s.waitEvent(p, {&item, 1});
    const std::uint64_t one = 1;
    const std::uint64_t zero = 0;
    s.write({.slot = en}, &one);  // the guard alone changing does nothing
    EXPECT_EQ(g.calls, 0);
    s.write({.slot = clk}, &one);  // posedge, guard false: keep waiting
    EXPECT_EQ(g.calls, 1);
    s.write({.slot = clk}, &zero);  // negedge: guard not read
    EXPECT_EQ(g.calls, 1);
    g.value = true;
    s.write({.slot = clk}, &one);
    EXPECT_EQ(g.calls, 2);
    s.run();
    EXPECT_EQ(log, std::vector<std::string>{"w@0"});
}

struct BitFrame {
    k::SlotId slot;
};

/// The monitored expression `slot & 1`.
void evalBit0(kirima_context* ctx, void* frame, std::uint64_t* out) {
    const auto* f = static_cast<BitFrame*>(frame);
    out[0] = k::fromC(ctx).store().element(f->slot, 0)[0] & 1U;
}

/// Slots of an expression whose function writes a variable or triggers an
/// event while the kernel walks its lists (RUNTIME_ABI.md 5).
struct WritingFrame {
    k::SlotId read;
    k::SlotId written;
    k::EventId event;
};

/// `read & 1`, which also writes `(read & 1) + 1` to another slot after
/// computing its value.
void evalAndWrite(kirima_context* ctx, void* frame, std::uint64_t* out) {
    const auto* f = static_cast<WritingFrame*>(frame);
    out[0] = k::fromC(ctx).store().element(f->read, 0)[0] & 1U;
    const std::uint64_t v = out[0] + 1;
    k::fromC(ctx).scheduler().write({.slot = f->written}, &v);
}

/// An iff that also triggers a named event.
int guardAndTrigger(kirima_context* ctx, void* frame) {
    k::fromC(ctx).scheduler().trigger(static_cast<WritingFrame*>(frame)->event);
    return 1;
}

/// `read & 1`, which also triggers a named event after computing its value.
void evalAndTrigger(kirima_context* ctx, void* frame, std::uint64_t* out) {
    const auto* f = static_cast<WritingFrame*>(frame);
    out[0] = k::fromC(ctx).store().element(f->read, 0)[0] & 1U;
    k::fromC(ctx).scheduler().trigger(f->event);
}

/// An expression on an event whose value stays 0.
void evalZero(kirima_context*, void*, std::uint64_t* out) { out[0] = 0; }

TEST(Scheduler, ChangesMadeWhileWalkingAreAnnouncedAfterTheWalk) {
    // An eval or guard function may write the variables of the functions it
    // calls and trigger events (RUNTIME_ABI.md 5). Such a change is
    // announced after the walk of waiters or monitor records that called the
    // function, so no list changes under the walk and no evaluation
    // overwrites another's value.
    {
        // @((a & 1)) whose function writes b, and @((b & 1)): the second
        // monitor's evaluation must not take the place of the first's.
        k::Context ctx;
        auto& s = ctx.scheduler();
        const k::SlotId a = ctx.store().add(k::Domain::Two, 8, 1, k::Init::Zero);
        const k::SlotId b = ctx.store().add(k::Domain::Two, 8, 1, k::Init::Zero);
        std::vector<std::string> log;
        Waiter wp{&log, "p"};
        Waiter wq{&log, "q"};
        WritingFrame f{a, b, k::EventId{0}};
        BitFrame fb{b};
        const k::ProcessId p = s.addProcess(&logOnce, &wp, Group::Active);
        const k::ProcessId q = s.addProcess(&logOnce, &wq, Group::Active);
        k::MonitorSpec mb;
        mb.slots = {{b, 0, 0, 0, 0}};
        mb.eval = &evalBit0;
        mb.frame = &fb;
        mb.domain = k::Domain::Two;
        mb.width = 1;
        s.waitMonitor(q, mb);  // b & 1 is 0
        k::MonitorSpec ma = mb;
        ma.slots = {{a, 0, 0, 0, 0}};
        ma.eval = &evalAndWrite;
        ma.frame = &f;
        s.waitMonitor(p, ma);  // a & 1 is 0; writes b = 1: b & 1 becomes 1
        s.run();
        EXPECT_EQ(log, std::vector<std::string>{"q@0"});
        log.clear();
        Waiter wq2{&log, "q2"};
        const k::ProcessId q2 = s.addProcess(&logOnce, &wq2, Group::Active);
        s.waitMonitor(q2, mb);  // b & 1 is 1
        const std::uint64_t one = 1;
        s.write({.slot = a}, &one);  // a & 1 becomes 1; writes b = 2: b & 1 becomes 0
        s.run();
        std::sort(log.begin(), log.end());
        EXPECT_EQ(log, (std::vector<std::string>{"p@0", "q2@0"}));
    }
    {
        // @(a iff (-> e, 1)) and @(e): the trigger made by the iff wakes the
        // waiter of e after the walk of a's subscriptions.
        k::Context ctx;
        auto& s = ctx.scheduler();
        const k::SlotId a = ctx.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
        const k::EventId e = s.addEvent();
        std::vector<std::string> log;
        Waiter wp{&log, "p"};
        Waiter wr{&log, "r"};
        WritingFrame f{a, a, e};
        const k::ProcessId p = s.addProcess(&logOnce, &wp, Group::Active);
        const k::ProcessId r = s.addProcess(&logOnce, &wr, Group::Active);
        s.waitNamed(r, e);
        const k::WaitItem item{{a, 0, 0, 0, 0}, Edge::Any, &guardAndTrigger, &f};
        s.waitAny(p, {&item, 1}, {});
        const std::uint64_t one = 1;
        s.write({.slot = a}, &one);
        s.run();
        std::sort(log.begin(), log.end());
        EXPECT_EQ(log, (std::vector<std::string>{"p@0", "r@0"}));
    }
    {
        // @((a & 1)) whose function triggers e, and a wait on an expression
        // of e: the second evaluation must not take the place of the first's.
        k::Context ctx;
        auto& s = ctx.scheduler();
        const k::SlotId a = ctx.store().add(k::Domain::Two, 8, 1, k::Init::Zero);
        const k::EventId e = s.addEvent();
        std::vector<std::string> log;
        Waiter wp{&log, "p"};
        Waiter wq{&log, "q"};
        WritingFrame f{a, a, e};
        const k::ProcessId p = s.addProcess(&logOnce, &wp, Group::Active);
        const k::ProcessId q = s.addProcess(&logOnce, &wq, Group::Active);
        k::MonitorSpec me;
        me.events = {e};
        me.eval = &evalZero;
        me.domain = k::Domain::Two;
        me.width = 1;
        s.waitMonitor(q, me);
        k::MonitorSpec ma;
        ma.slots = {{a, 0, 0, 0, 0}};
        ma.eval = &evalAndTrigger;
        ma.frame = &f;
        ma.domain = k::Domain::Two;
        ma.width = 1;
        s.waitMonitor(p, ma);  // a & 1 is 0
        const std::uint64_t one = 1;
        s.write({.slot = a}, &one);  // a & 1 becomes 1, and e is triggered
        s.run();
        EXPECT_EQ(log, std::vector<std::string>{"p@0"});
    }
}

TEST(Scheduler, MonitorWakesOnlyWhenTheValueChanges) {
    // @(a & 1): a change of a that keeps a & 1 does not wake the process,
    // even though the operand changed (9.4.2).
    k::Context ctx;
    auto& s = ctx.scheduler();
    const k::SlotId a = ctx.store().add(k::Domain::Two, 8, 1, k::Init::Zero);
    const std::uint64_t one = 1;
    const std::uint64_t three = 3;
    const std::uint64_t two = 2;
    s.write({.slot = a}, &one);
    std::vector<std::string> log;
    Waiter w{&log, "w"};
    BitFrame f{a};
    const k::ProcessId p = s.addProcess(&logOnce, &w, Group::Active);
    k::MonitorSpec m;
    m.slots = {{a, 0, 0, 0, 0}};
    m.eval = &evalBit0;
    m.frame = &f;
    m.domain = k::Domain::Two;
    m.width = 1;
    s.waitMonitor(p, m);
    s.write({.slot = a}, &three);  // a & 1 stays 1
    s.run();
    EXPECT_TRUE(log.empty());
    s.write({.slot = a}, &two);  // a & 1 becomes 0
    s.run();
    EXPECT_EQ(log, std::vector<std::string>{"w@0"});
}

TEST(Scheduler, MixedWaitEndsAtItsFirstItem) {
    // @(posedge clk or (a & 1) or (b & 1)): a subscription and two monitor
    // records; the first item that fires wakes the process once and ends
    // the others (SCHEDULER_SPEC.md 9.5).
    k::Context ctx;
    auto& s = ctx.scheduler();
    const k::SlotId clk = ctx.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
    const k::SlotId a = ctx.store().add(k::Domain::Two, 8, 1, k::Init::Zero);
    const k::SlotId b = ctx.store().add(k::Domain::Two, 8, 1, k::Init::Zero);
    std::vector<std::string> log;
    Waiter w{&log, "w"};
    BitFrame fa{a};
    BitFrame fb{b};
    const k::ProcessId p = s.addProcess(&logOnce, &w, Group::Active);
    const k::WaitItem item{{clk, 0, 0, 0, 0}, Edge::Pos, nullptr, nullptr};
    std::vector<k::MonitorSpec> mons(2);
    for (std::size_t i = 0; i < 2; ++i) {
        mons[i].slots = {{i == 0 ? a : b, 0, 0, 0, 0}};
        mons[i].eval = &evalBit0;
        mons[i].frame = i == 0 ? &fa : &fb;
        mons[i].domain = k::Domain::Two;
        mons[i].width = 1;
    }
    s.waitAny(p, {&item, 1}, mons);
    const std::uint64_t two = 2;
    const std::uint64_t one = 1;
    s.write({.slot = a}, &two);  // a & 1 stays 0
    s.run();
    EXPECT_TRUE(log.empty());
    s.write({.slot = b}, &one);  // b & 1 becomes 1: wakes
    s.write({.slot = a}, &one);  // the other items are gone
    s.write({.slot = clk}, &one);
    s.run();
    EXPECT_EQ(log, std::vector<std::string>{"w@0"});
}

/// A process that waits again on the same items every time it wakes, as
/// always_ff @(posedge clk or negedge rst_n) does: slot items, and
/// optionally a monitor record on a slot or on a named event.
struct Rewaiter {
    std::vector<k::WaitItem> items;
    std::vector<k::MonitorSpec> monitors;
    k::ProcessId self{0};
    int wakes = 0;
};

kirima_status rewait(kirima_context* ctx, void* frame) {
    auto* r = static_cast<Rewaiter*>(frame);
    ++r->wakes;
    k::fromC(ctx).scheduler().waitAny(r->self, r->items, r->monitors);
    return KIRIMA_WAITING;
}

TEST(Scheduler, WaitListsDoNotGrowWithTheItemsLeftBehind) {
    // SCHEDULER_SPEC.md 9.5: a wait that one item wakes ends its other
    // items. Processes woken by clk over and over, whose other items (a
    // slot, a monitor on a slot, a monitor on a named event) never fire,
    // leave those items behind on every clock; the lists keep about as many
    // entries after 2N clocks as after N. And processes killed while they
    // wait on a named event leave their waiters behind just as well.
    k::Context ctx;
    auto& s = ctx.scheduler();
    const k::SlotId clk = ctx.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
    const k::SlotId rst = ctx.store().add(k::Domain::Two, 1, 1, k::Init::One);
    const k::SlotId a = ctx.store().add(k::Domain::Two, 8, 1, k::Init::Zero);
    const k::EventId ev = s.addEvent();
    const k::WaitItem posClk{{clk, 0, 0, 0, 0}, Edge::Pos, nullptr, nullptr};
    const k::WaitItem negRst{{rst, 0, 0, 0, 0}, Edge::Neg, nullptr, nullptr};
    BitFrame fa{a};
    k::MonitorSpec onSlot;
    onSlot.slots = {{a, 0, 0, 0, 0}};
    onSlot.eval = &evalBit0;
    onSlot.frame = &fa;
    onSlot.domain = k::Domain::Two;
    k::MonitorSpec onEvent = onSlot;
    onEvent.slots = {};
    onEvent.events = {ev};
    std::vector<Rewaiter> rs(12);
    for (std::size_t i = 0; i < rs.size(); ++i) {
        rs[i].items = {posClk};
        if (i % 3 == 0) rs[i].items.push_back(negRst);  // the slot kind
        if (i % 3 == 1) rs[i].monitors = {onSlot};      // a monitor on a slot
        if (i % 3 == 2) rs[i].monitors = {onEvent};     // a monitor on an event
        rs[i].self = s.addProcess(&rewait, &rs[i], Group::Active);
        s.waitAny(rs[i].self, rs[i].items, rs[i].monitors);
    }
    std::vector<std::string> log;
    Waiter w{&log, "w"};
    const std::uint64_t one = 1;
    const std::uint64_t zero = 0;
    const auto clocks = [&](int n) {
        for (int c = 0; c < n; ++c) {
            s.write({.slot = clk}, &one);
            s.run();
            s.write({.slot = clk}, &zero);
            s.run();
            // A process killed while it waits on the event.
            const k::ProcessId q = s.addProcess(&logOnce, &w, Group::Active);
            s.waitNamed(q, ev, true);
            s.kill(q);
        }
    };
    // Live: 12 clk items, and 4 each of rst items, slot monitor and event
    // monitor references. A list is swept when it reaches twice its live
    // entries or 16, so the 12 of clk keep at most 24 and the four other
    // lists, the event's waiters among them, 16 each.
    const std::size_t bound = 2 * 12 + 4 * 16;
    clocks(500);
    EXPECT_LE(s.waitListEntries(), bound);
    clocks(500);
    EXPECT_LE(s.waitListEntries(), bound);
    EXPECT_EQ(rs[0].wakes, 1000);
    EXPECT_EQ(rs[2].wakes, 1000);
    EXPECT_TRUE(log.empty());
}

TEST(Scheduler, MonitorIffIsReadOnlyWhenItsEventOccurs) {
    // @(posedge (a & 1) iff g): the guard is read on the expression's
    // posedge alone (9.4.2.3).
    k::Context ctx;
    auto& s = ctx.scheduler();
    const k::SlotId a = ctx.store().add(k::Domain::Two, 8, 1, k::Init::Zero);
    std::vector<std::string> log;
    Waiter w{&log, "w"};
    BitFrame f{a};
    GuardState g;
    const k::ProcessId p = s.addProcess(&logOnce, &w, Group::Active);
    k::MonitorSpec m;
    m.slots = {{a, 0, 0, 0, 0}};
    m.eval = &evalBit0;
    m.frame = &f;
    m.domain = k::Domain::Two;
    m.width = 1;
    m.edge = Edge::Pos;
    m.guard = &guardFn;
    m.guardFrame = &g;
    s.waitAny(p, {}, {&m, 1});
    const std::uint64_t two = 2;
    const std::uint64_t one = 1;
    const std::uint64_t zero = 0;
    s.write({.slot = a}, &two);  // the value stays 0: guard not read
    EXPECT_EQ(g.calls, 0);
    s.write({.slot = a}, &one);  // posedge, guard false: keep waiting
    EXPECT_EQ(g.calls, 1);
    s.write({.slot = a}, &zero);  // negedge: guard not read
    EXPECT_EQ(g.calls, 1);
    g.value = true;
    s.write({.slot = a}, &one);
    EXPECT_EQ(g.calls, 2);
    s.run();
    EXPECT_EQ(log, std::vector<std::string>{"w@0"});
}

struct NbaObserver {
    k::SlotId slot;
    k::SlotId side;
    k::ProcessId process;
    std::vector<std::vector<std::uint64_t>> values;
    std::vector<std::uint64_t> times;
};

int observeNbaAndSchedule(kirima_context* ctx, void* frame) {
    auto& c = k::fromC(ctx);
    auto& f = *static_cast<NbaObserver*>(frame);
    const auto* p = c.store().element(f.slot, 1);
    f.values.emplace_back(p, p + c.store().layout(f.slot).elemWords);
    f.times.push_back(c.scheduler().time());
    // Grow the NBA arena and record table while an update is being
    // announced. Neither may invalidate the update that is running.
    const std::vector<std::uint64_t> side(64, f.values.size());
    c.scheduler().scheduleNba(f.process, {.slot = f.side}, side.data(), 0);
    return 0;  // Keep observing the next update too.
}

TEST(Scheduler, NbaSnapshotsAcrossDomainsAndRanges) {
    // SCHEDULER_SPEC.md 5, 13 (4.9.4, 10.4.2): capture the RHS and target
    // when scheduled, preserve statement order, and leave other bits and
    // array elements alone. Cover values of one and multiple chunks,
    // with zero and nonzero delays in NBA and Re-NBA.
    for (const auto domain :
         {k::Domain::Two, k::Domain::Four, k::Domain::Real, k::Domain::Handle}) {
        for (const unsigned width : {1U, 63U, 64U, 65U, 127U, 128U, 129U, 260U}) {
            for (const int offset : {-1, 0, 7, 63}) {
                const bool integral = domain == k::Domain::Two || domain == k::Domain::Four;
                if (!integral && (width != 64 || offset != -1)) continue;
                for (const auto group : {Group::Active, Group::Reactive}) {
                    for (const std::uint64_t delay : {0U, 3U}) {
                        SCOPED_TRACE(::testing::Message()
                                     << "domain=" << static_cast<int>(domain) << " width=" << width
                                     << " offset=" << offset << " group=" << static_cast<int>(group)
                                     << " delay=" << delay);
                        k::Context ctx;
                        auto& s = ctx.scheduler();
                        const unsigned lsb = offset < 0 ? 0 : static_cast<unsigned>(offset);
                        const unsigned targetWidth = offset < 0 ? width : lsb + width + 5;
                        const auto slot = ctx.store().add(domain, targetWidth, 2, k::Init::Zero);
                        const auto side = ctx.store().add(k::Domain::Two, 4096, 1, k::Init::Zero);
                        // A slot that has had an override retains its base
                        // value even after the override has been released.
                        if (offset == 0 || offset == 63) s.markHeld(slot, false, false);
                        std::vector<std::string> log;
                        Waiter waiter{&log, "unexpected resume"};
                        const auto process = s.addProcess(&logOnce, &waiter, group);
                        NbaObserver observer{slot, side, process};
                        const k::WaitItem item{
                            {slot, 1, 1, 0, 0}, Edge::Any, &observeNbaAndSchedule, &observer};
                        s.waitEvent(process, {&item, 1});
                        const unsigned stride = domain == k::Domain::Four ? 2 : 1;
                        const unsigned words = stride * ((width + 63) / 64);
                        const auto targetWords = ctx.store().layout(slot).elemWords;
                        std::vector<std::vector<std::uint64_t>> expected;
                        for (unsigned phase = 0; phase < 2; ++phase) {
                            std::vector<std::uint64_t> value(words, 0);
                            std::vector<std::uint64_t> want(targetWords, 0);
                            for (unsigned bit = 0; bit < width; ++bit) {
                                for (unsigned plane = 0; plane < stride; ++plane) {
                                    const bool set =
                                        plane == 0 ? (bit % 3 == 0) != (phase != 0) : bit % 4 >= 2;
                                    if (!set) continue;
                                    value[stride * (bit / 64) + plane] |= std::uint64_t{1}
                                                                          << (bit % 64);
                                    want[stride * ((lsb + bit) / 64) + plane] |=
                                        std::uint64_t{1} << ((lsb + bit) % 64);
                                }
                            }
                            expected.push_back(want);
                            s.scheduleNba(process,
                                          {.slot = slot,
                                           .elem = 1,
                                           .lsb = lsb,
                                           .width = offset < 0 ? 0 : width},
                                          value.data(), delay);
                            std::fill(value.begin(), value.end(), 0);  // RHS storage is reused.
                        }
                        const std::vector<std::uint64_t> zero(targetWords, 0);
                        EXPECT_EQ(std::vector(ctx.store().element(slot, 1),
                                              ctx.store().element(slot, 1) + targetWords),
                                  zero);
                        EXPECT_EQ(s.run(), EndReason::Quiescent);
                        EXPECT_EQ(observer.values, expected);
                        EXPECT_EQ(observer.times, (std::vector<std::uint64_t>{delay, delay}));
                        EXPECT_EQ(std::vector(ctx.store().element(slot, 0),
                                              ctx.store().element(slot, 0) + targetWords),
                                  zero);
                        EXPECT_EQ(std::vector(ctx.store().element(slot, 1),
                                              ctx.store().element(slot, 1) + targetWords),
                                  expected.back());
                        EXPECT_EQ(ctx.store().element(side, 0)[63], 2U);
                        EXPECT_TRUE(log.empty());
                    }
                }
            }
        }
    }
}

// --- The C function table -------------------------------------------------------

struct AbiProcess {
    const kirima_kernel_api* api;
    std::uint32_t id = 0;
    int step = 0;
    std::vector<std::uint64_t> seen;
};

kirima_status abiResume(kirima_context* ctx, void* frame) {
    auto* p = static_cast<AbiProcess*>(frame);
    const kirima_kernel_api* api = p->api;
    p->seen.push_back(api->time(ctx));
    if (p->step++ == 0) {
        const std::uint64_t v = 5;
        api->schedule_nba(ctx, p->id, 0, 0, 0, 0, &v, 2);
        api->wait_delay(ctx, p->id, 3);
        return KIRIMA_WAITING;
    }
    p->seen.push_back(api->slot_words(ctx, 0, 0)[0]);
    api->request_end(ctx, static_cast<std::uint32_t>(EndReason::Finish), 0);
    return KIRIMA_STOP;
}

TEST(KernelAbi, TableReachesTheScheduler) {
    const kirima_kernel_api* api = kirima_get_kernel_api();
    ASSERT_NE(api, nullptr);
    EXPECT_EQ(api->abi_major, static_cast<std::uint32_t>(KIRIMA_ABI_MAJOR));
    EXPECT_EQ(api->abi_minor, static_cast<std::uint32_t>(KIRIMA_ABI_MINOR));
    k::Context ctx;
    ctx.store().add(k::Domain::Two, 8, 1, k::Init::Zero);
    AbiProcess p{api};
    p.id = api->add_process(k::toC(ctx), &abiResume, &p, KIRIMA_GROUP_ACTIVE);
    api->start(k::toC(ctx), p.id);
    EXPECT_EQ(ctx.scheduler().run(), EndReason::Finish);
    // Run at 0, then at 3 after the NBA of 5 at time 2.
    EXPECT_EQ(p.seen, (std::vector<std::uint64_t>{0, 3, 5}));
    EXPECT_EQ(ctx.scheduler().level(), 0);
}

TEST(KernelAbi, WaitAnyTakesMonitorItemsWithIff) {
    const kirima_kernel_api* api = kirima_get_kernel_api();
    k::Context ctx;
    auto& s = ctx.scheduler();
    const k::SlotId a = ctx.store().add(k::Domain::Two, 8, 1, k::Init::Zero);
    std::vector<std::string> log;
    Waiter w{&log, "w"};
    BitFrame f{a};
    GuardState g;
    const std::uint32_t p = api->add_process(k::toC(ctx), &logOnce, &w, KIRIMA_GROUP_ACTIVE);
    const kirima_slot_part part{a.index(), 0, 0, 0, 0};
    kirima_monitor_item item{};
    item.monitor.slots = &part;
    item.monitor.num_slots = 1;
    item.monitor.eval = &evalBit0;
    item.monitor.frame = &f;
    item.monitor.domain = KIRIMA_DOMAIN_TWO;
    item.monitor.width = 1;
    item.monitor.edge = KIRIMA_EDGE_POS;
    item.guard = &guardFn;
    item.guard_frame = &g;
    api->wait_any(k::toC(ctx), p, nullptr, 0, &item, 1);
    const std::uint64_t one = 1;
    const std::uint64_t zero = 0;
    s.write({.slot = a}, &one);  // guard false
    s.write({.slot = a}, &zero);
    g.value = true;
    s.write({.slot = a}, &one);
    s.run();
    EXPECT_EQ(g.calls, 2);
    EXPECT_EQ(log, std::vector<std::string>{"w@0"});
}

TEST(Scheduler, NamedIffRetainsFalseItemsAndCancelsOtherItems) {
    for (const Group group : {Group::Active, Group::Reactive}) {
        k::Context ctx;
        auto& s = ctx.scheduler();
        const k::EventId first = s.addEvent();
        const k::EventId second = s.addEvent();
        const k::SlotId clk = ctx.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
        const k::SlotId unrelated = ctx.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
        std::vector<std::string> log;
        Waiter w{&log, "named"};
        GuardState a;
        GuardState b;
        b.value = true;
        const k::ProcessId p = s.addProcess(&logOnce, &w, group);
        const k::WaitItem signal{{clk, 0, 0, 0, 0}, Edge::Pos, nullptr, nullptr};
        const k::NamedWaitItem named[] = {{first, &guardFn, &a}, {second, &guardFn, &b}};
        s.waitAny(p, {&signal, 1}, {}, named);
        const std::uint64_t one = 1;
        s.write({.slot = unrelated}, &one);
        EXPECT_EQ(a.calls, 0);
        EXPECT_EQ(b.calls, 0);
        for (unsigned i = 0; i < 100; ++i)
            s.trigger(first);
        EXPECT_EQ(a.calls, 100);
        EXPECT_EQ(s.waitListEntries(), 3U);
        a.value = true;
        s.trigger(first);
        s.trigger(second);
        s.write({.slot = clk}, &one);
        s.run();
        EXPECT_EQ(a.calls, 101);
        EXPECT_EQ(b.calls, 0);
        EXPECT_EQ(log, (std::vector<std::string>{"named@0"}));
    }
}

TEST(Scheduler, MixedNamedSlotAndWatchWaitEndsAtItsFirstItem) {
    for (unsigned winner = 0; winner < 3; ++winner) {
        k::Context ctx;
        auto& s = ctx.scheduler();
        const k::EventId event = s.addEvent();
        const k::SlotId clk = ctx.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
        const k::SlotId value = ctx.store().add(k::Domain::Two, 8, 1, k::Init::Zero);
        std::vector<std::string> log;
        Waiter w{&log, "mixed"};
        GuardState guard;
        guard.value = true;
        BitFrame frame{value};
        const k::ProcessId p = s.addProcess(&logOnce, &w, Group::Active);
        const k::WaitItem signal{{clk, 0, 0, 0, 0}, Edge::Pos, nullptr, nullptr};
        k::MonitorSpec monitor;
        monitor.slots = {{value, 0, 0, 0, 0}};
        monitor.eval = &evalBit0;
        monitor.frame = &frame;
        monitor.domain = k::Domain::Two;
        monitor.width = 1;
        const k::NamedWaitItem named{event, &guardFn, &guard};
        s.waitAny(p, {&signal, 1}, {&monitor, 1}, {&named, 1});
        const std::uint64_t one = 1;
        if (winner == 0) s.trigger(event);
        if (winner == 1) s.write({.slot = clk}, &one);
        if (winner == 2) s.write({.slot = value}, &one);
        s.trigger(event);
        s.write({.slot = clk}, &one);
        s.write({.slot = value}, &one);
        s.run();
        EXPECT_EQ(guard.calls, winner == 0 ? 1 : 0);
        EXPECT_EQ(log, (std::vector<std::string>{"mixed@0"}));
    }
}

TEST(Scheduler, NamedIffMayTriggerAnotherEventWhileListsAreWalked) {
    k::Context ctx;
    auto& s = ctx.scheduler();
    const k::EventId first = s.addEvent();
    const k::EventId second = s.addEvent();
    std::vector<std::string> log;
    Waiter a{&log, "first"};
    Waiter b{&log, "second"};
    const k::ProcessId p = s.addProcess(&logOnce, &a, Group::Active);
    const k::ProcessId q = s.addProcess(&logOnce, &b, Group::Active);
    WritingFrame frame{k::SlotId{0}, k::SlotId{0}, second};
    const k::NamedWaitItem item{first, &guardAndTrigger, &frame};
    s.waitAny(p, {}, {}, {&item, 1});
    s.waitNamed(q, second);
    s.trigger(first);
    s.run();
    EXPECT_EQ(log, (std::vector<std::string>{"first@0", "second@0"}));
}

TEST(Scheduler, CanceledNamedWaitDoesNotReadItsOldGuard) {
    k::Context ctx;
    auto& s = ctx.scheduler();
    const k::EventId event = s.addEvent();
    std::vector<std::string> log;
    Waiter old{&log, "old"};
    Waiter current{&log, "current"};
    GuardState stale;
    GuardState live;
    live.value = true;
    const k::ProcessId p = s.addProcess(&logOnce, &old, Group::Active);
    const k::NamedWaitItem first{event, &guardFn, &stale};
    s.waitAny(p, {}, {}, {&first, 1});
    s.kill(p);
    const k::ProcessId q = s.addProcess(&logOnce, &current, Group::Active);
    const k::NamedWaitItem second{event, &guardFn, &live};
    s.waitAny(q, {}, {}, {&second, 1});
    s.trigger(event);
    s.run();
    EXPECT_EQ(stale.calls, 0);
    EXPECT_EQ(live.calls, 1);
    EXPECT_EQ(log, (std::vector<std::string>{"current@0"}));
}

TEST(Scheduler, NamedItemsAreRegisteredBeforeWatchInitialization) {
    k::Context ctx;
    auto& s = ctx.scheduler();
    const k::EventId event = s.addEvent();
    const k::SlotId value = ctx.store().add(k::Domain::Two, 8, 1, k::Init::Zero);
    std::vector<std::string> log;
    Waiter waiter{&log, "initial"};
    const k::ProcessId p = s.addProcess(&logOnce, &waiter, Group::Active);
    WritingFrame frame{value, value, event};
    k::MonitorSpec monitor;
    monitor.slots = {{value, 0, 0, 0, 0}};
    monitor.eval = &evalAndTrigger;
    monitor.frame = &frame;
    monitor.domain = k::Domain::Two;
    monitor.width = 1;
    GuardState guard;
    guard.value = true;
    const k::NamedWaitItem named{event, &guardFn, &guard};
    s.waitAny(p, {}, {&monitor, 1}, {&named, 1});
    EXPECT_EQ(guard.calls, 1);
    EXPECT_TRUE(log.empty());
    s.run();
    EXPECT_EQ(log, (std::vector<std::string>{"initial@0"}));
}

TEST(KernelAbi, MixedWaitTakesGuardedNamedEvents) {
    const kirima_kernel_api* api = kirima_get_kernel_api();
    ASSERT_NE(api->wait_mixed, nullptr);
    EXPECT_GE(api->abi_minor, 3U);
    EXPECT_GT(offsetof(kirima_kernel_api, wait_mixed), offsetof(kirima_kernel_api, wait_any));
    k::Context ctx;
    auto& s = ctx.scheduler();
    const k::EventId event = s.addEvent();
    std::vector<std::string> log;
    Waiter waiter{&log, "abi"};
    GuardState guard;
    const std::uint32_t p = api->add_process(k::toC(ctx), &logOnce, &waiter, KIRIMA_GROUP_REACTIVE);
    const kirima_named_wait_item item{event.index(), &guardFn, &guard};
    api->wait_mixed(k::toC(ctx), p, nullptr, 0, nullptr, 0, &item, 1);
    s.trigger(event);
    EXPECT_EQ(guard.calls, 1);
    guard.value = true;
    s.trigger(event);
    s.trigger(event);
    s.run();
    EXPECT_EQ(guard.calls, 2);
    EXPECT_EQ(log, (std::vector<std::string>{"abi@0"}));
}

// --- Random scripts against the reference model -----------------------------------

std::vector<Script> randomScripts(std::mt19937_64& rng) {
    const auto pick = [&](std::uint64_t n) { return rng() % n; };
    std::vector<Script> scripts(2 + pick(4));
    for (Script& s : scripts) {
        s.group = pick(5) == 0 ? Group::Reactive : Group::Active;
        const std::size_t n = 3 + pick(10);
        for (std::size_t i = 0; i < n; ++i) {
            const auto slot = static_cast<std::uint32_t>(pick(4));
            const auto ev = static_cast<std::uint32_t>(pick(2));
            switch (pick(20)) {
                case 0:
                case 1:
                case 2: s.steps.push_back(log(static_cast<std::uint32_t>(i))); break;
                case 3:
                case 4:
                case 5: s.steps.push_back(delay(pick(4))); break;
                case 6: s.steps.push_back(delay(250 + pick(20))); break;
                case 7:
                case 8:
                case 9: s.steps.push_back(write(slot, pick(4))); break;
                case 10:
                case 11: s.steps.push_back(nba(slot, pick(4), pick(2) == 0 ? 0 : pick(3))); break;
                case 12:
                case 13: s.steps.push_back(waitEdge(slot, static_cast<Edge>(pick(4)))); break;
                case 14: s.steps.push_back(waitNamed(ev)); break;
                case 15: s.steps.push_back(trig(ev)); break;
                case 16: s.steps.push_back(trigNb(ev, pick(2))); break;
                case 17:
                case 18: s.steps.push_back(waitTrue(slot)); break;
                default:
                    if (pick(4) == 0)
                        s.steps.push_back(finish());
                    else if (pick(3) == 0)
                        s.steps.push_back({Op::Kill, static_cast<std::uint32_t>(pick(6)), 0, 0});
                    break;
            }
        }
    }
    return scripts;
}

TEST(Scheduler, MatchesTheReferenceModel) {
    const std::uint64_t seed = kirimasim::test::fuzzSeed(0x5c4ed);
    std::mt19937_64 rng(seed);
    const std::uint64_t rounds = 3000 * kirimasim::test::fuzzScale();
    SCOPED_TRACE(kirimasim::test::fuzzSetting(seed));
    for (std::uint64_t round = 0; round < rounds; ++round) {
        const std::vector<Script> scripts = randomScripts(rng);
        KernelSim fast(scripts);
        RefSim ref(scripts);
        const EndReason rf = fast.run();
        const EndReason rr = ref.run();
        ASSERT_EQ(fast.log(), ref.log()) << "round " << round;
        ASSERT_EQ(rf, rr) << "round " << round;
        ASSERT_EQ(fast.now(), ref.now()) << "round " << round;
        for (std::uint32_t i = 0; i < 4; ++i)
            ASSERT_EQ(fast.read(i), ref.slot(i)) << "round " << round << " slot " << i;
    }
}

// --- Released processes (SCHEDULER_SPEC.md 10.8) -----------------------------------

/// A process for the tests below: what it does when it runs, by role.
struct Actor {
    enum class Role : std::uint8_t {
        Wait,    // waits `delay` once, then ends
        End,     // ends at once
        Report,  // holds a deferred call that logs `name`, then ends
        Swap,    // releases `other` and starts `next` in its place
    };
    Role role = Role::End;
    k::Scheduler* sched = nullptr;
    k::ProcessId id{0};
    std::uint64_t delay = 0;
    char name = '?';
    Actor* other = nullptr;
    Actor* next = nullptr;
    std::string* log = nullptr;
    int runs = 0;
    std::vector<std::uint64_t> times;
};

void logCall(kirima_context* /*ctx*/, void* frame, std::uint32_t /*token*/, int run) {
    auto* a = static_cast<Actor*>(frame);
    if (run != 0) *a->log += a->name;
}

kirima_status act(kirima_context* /*ctx*/, void* frame) {
    auto* a = static_cast<Actor*>(frame);
    a->times.push_back(a->sched->time());
    switch (a->role) {
        case Actor::Role::Wait:
            if (a->runs++ == 0) {
                a->sched->waitDelay(a->id, a->delay);
                return KIRIMA_WAITING;
            }
            return KIRIMA_DONE;
        case Actor::Role::End: return KIRIMA_DONE;
        case Actor::Role::Report:
            a->sched->deferCall(a->id, false, &logCall, a, 0);
            return KIRIMA_DONE;
        case Actor::Role::Swap:
            if (a->other->runs == 1) a->sched->kill(a->other->id);  // still waiting
            a->sched->releaseProcess(a->other->id);
            a->next->id = a->sched->addProcess(&act, a->next, Group::Active);
            a->sched->start(a->next->id);
            return KIRIMA_DONE;
    }
    return KIRIMA_DONE;
}

std::uint64_t counter(k::Context& ctx, std::string_view name) {
    const k::Counters& c = ctx.counters();
    for (std::uint32_t i = 0; i < c.size(); ++i)
        if (c.name(k::CounterId{i}) == name) return c.value(k::CounterId{i});
    return UINT64_MAX;
}

TEST(Scheduler, AReusedPlaceGetsNoStaleWakeUp) {
    // a waits #10 and is killed at time 0; b takes its place and waits #20.
    // a's wake-up at 10 must not reach b: b runs at 0 and 20 only.
    k::Context ctx;
    k::Scheduler& s = ctx.scheduler();
    Actor a{Actor::Role::Wait, &s, k::ProcessId{0}, 10};
    Actor b{Actor::Role::Wait, &s, k::ProcessId{0}, 20};
    Actor swap{Actor::Role::Swap, &s};
    swap.other = &a;
    swap.next = &b;
    a.id = s.addProcess(&act, &a, Group::Active);
    swap.id = s.addProcess(&act, &swap, Group::Active);
    s.start(a.id);
    s.start(swap.id);
    EXPECT_EQ(s.run(), EndReason::Quiescent);
    EXPECT_EQ(b.id, a.id);  // the place was reused
    EXPECT_EQ(b.times, (std::vector<std::uint64_t>{0, 20}));
    EXPECT_EQ(counter(ctx, "proc.created"), 3U);
    EXPECT_EQ(counter(ctx, "proc.kept_peak"), 2U);
}

TEST(Scheduler, HeldReportsGoInCreationOrder) {
    // r1 is made before r2, which takes the place of the released e, a
    // lower number: r1's report is still confirmed first (15.4, 10.8).
    k::Context ctx;
    k::Scheduler& s = ctx.scheduler();
    std::string log;
    Actor e{Actor::Role::End, &s};
    Actor r1{Actor::Role::Report, &s};
    Actor r2{Actor::Role::Report, &s};
    Actor swap{Actor::Role::Swap, &s};
    r1.name = '1';
    r2.name = '2';
    r1.log = &log;
    r2.log = &log;
    swap.other = &e;
    swap.next = &r2;
    e.id = s.addProcess(&act, &e, Group::Active);
    r1.id = s.addProcess(&act, &r1, Group::Active);
    swap.id = s.addProcess(&act, &swap, Group::Active);
    s.start(e.id);
    s.start(r1.id);
    s.start(swap.id);
    s.run();
    EXPECT_LT(r2.id, r1.id);
    EXPECT_EQ(log, "12");
}

struct DeferredLog {
    std::vector<std::pair<std::uint32_t, int>> calls;
    static void call(kirima_context*, void* frame, std::uint32_t token, int run) {
        static_cast<DeferredLog*>(frame)->calls.emplace_back(token, run);
    }
};

TEST(Scheduler, OutermostDisablePreservesUniqueAndPriorityReports) {
    const std::unique_ptr<std::FILE, decltype(&std::fclose)> messages(std::tmpfile(), &std::fclose);
    ASSERT_NE(messages, nullptr);
    k::Context ctx(k::OutputDestinations{stdout, messages.get()});
    k::Scheduler& s = ctx.scheduler();
    Actor owner{Actor::Role::End, &s};
    owner.id = s.addProcess(&act, &owner, Group::Active);
    DeferredLog log;
    s.deferReport(owner.id, "unique retained");
    s.deferCall(owner.id, false, &DeferredLog::call, &log, 0);
    s.deferReport(owner.id, "priority retained");
    s.deferCall(owner.id, true, &DeferredLog::call, &log, 1);
    ASSERT_TRUE(s.flushDeferredAssertions(owner.id));
    EXPECT_TRUE(s.hasPending(owner.id));
    EXPECT_EQ(log.calls, (std::vector<std::pair<std::uint32_t, int>>{{0, 0}, {1, 0}}));
    s.start(owner.id);
    EXPECT_EQ(s.run(), EndReason::Quiescent);
    EXPECT_FALSE(s.hasPending(owner.id));
    ASSERT_EQ(std::fflush(messages.get()), 0);
    ASSERT_EQ(std::fseek(messages.get(), 0, SEEK_SET), 0);
    std::string text;
    for (int ch; (ch = std::fgetc(messages.get())) != EOF;)
        text += static_cast<char>(ch);
    EXPECT_EQ(text, "unique retained\npriority retained\n");
    EXPECT_EQ(log.calls.size(), 2U);
}

TEST(Scheduler, OutermostDisableCanEmptyThePendingQueue) {
    k::Context ctx;
    k::Scheduler& s = ctx.scheduler();
    Actor owner{Actor::Role::End, &s};
    owner.id = s.addProcess(&act, &owner, Group::Active);
    DeferredLog log;
    s.deferCall(owner.id, false, &DeferredLog::call, &log, 0);
    s.deferCall(owner.id, true, &DeferredLog::call, &log, 1);
    ASSERT_TRUE(s.flushDeferredAssertions(owner.id));
    EXPECT_FALSE(s.hasPending(owner.id));
    ASSERT_TRUE(s.flushDeferredAssertions(owner.id));
    s.start(owner.id);
    EXPECT_EQ(s.run(), EndReason::Quiescent);
    EXPECT_EQ(log.calls, (std::vector<std::pair<std::uint32_t, int>>{{0, 0}, {1, 0}}));
}

struct DeferredFlusher {
    k::Scheduler* scheduler;
    k::ProcessId owner;
    bool flushed = false;
    static kirima_status flush(kirima_context*, void* frame) {
        auto& f = *static_cast<DeferredFlusher*>(frame);
        f.flushed = f.scheduler->flushDeferredAssertions(f.owner);
        return f.flushed ? KIRIMA_DONE : KIRIMA_STOP;
    }
};

TEST(Scheduler, OutermostDisableKeepsConfirmedCalls) {
    k::Context ctx;
    k::Scheduler& s = ctx.scheduler();
    Actor owner{Actor::Role::End, &s};
    owner.id = s.addProcess(&act, &owner, Group::Active);
    DeferredLog log;
    s.deferCall(owner.id, false, &DeferredLog::call, &log, 0);
    s.deferCall(owner.id, true, &DeferredLog::call, &log, 1);
    DeferredFlusher flusher{&s, owner.id};
    const auto reactive = s.addProcess(&DeferredFlusher::flush, &flusher, Group::Reactive);
    // The flusher is first in Reactive. Observed has already confirmed the
    // observed call, while the final call is still on the pending queue.
    s.start(reactive);
    s.start(owner.id);
    EXPECT_EQ(s.run(), EndReason::Quiescent);
    EXPECT_TRUE(flusher.flushed);
    EXPECT_EQ(log.calls, (std::vector<std::pair<std::uint32_t, int>>{{1, 0}, {0, 1}}));
}

struct FailingDeferredDrop {
    enum class Failure { Allocation, Length, Unknown, Stop };
    k::Scheduler* scheduler;
    Failure failure;
    static void call(kirima_context*, void* frame, std::uint32_t, int) {
        auto& f = *static_cast<FailingDeferredDrop*>(frame);
        switch (f.failure) {
            case Failure::Allocation: throw std::bad_alloc{};
            case Failure::Length: throw std::length_error("deferred drop");
            case Failure::Unknown: throw 1;
            case Failure::Stop: f.scheduler->resourceLimit(); break;
        }
    }
};

TEST(Scheduler, OutermostDisableContainsCallbackFailures) {
    for (const auto failure :
         {FailingDeferredDrop::Failure::Allocation, FailingDeferredDrop::Failure::Length,
          FailingDeferredDrop::Failure::Unknown, FailingDeferredDrop::Failure::Stop}) {
        k::Context ctx;
        k::Scheduler& s = ctx.scheduler();
        Actor owner{Actor::Role::End, &s};
        owner.id = s.addProcess(&act, &owner, Group::Active);
        FailingDeferredDrop drop{&s, failure};
        s.deferCall(owner.id, false, &FailingDeferredDrop::call, &drop, 0);
        bool flushed = true;
        EXPECT_NO_THROW(flushed = s.flushDeferredAssertions(owner.id));
        EXPECT_FALSE(flushed);
        EXPECT_TRUE(s.ending());
        EXPECT_FALSE(s.hasPending(owner.id));
        EXPECT_EQ(s.reason(), failure == FailingDeferredDrop::Failure::Unknown
                                  ? EndReason::Internal
                                  : EndReason::ResourceLimit);
        if (failure == FailingDeferredDrop::Failure::Unknown)
            EXPECT_EQ(s.error(), "deferred assertion drop callback threw");
    }
}

TEST(Scheduler, StopPoliciesAreContextLocal) {
    k::Context one, two;
    two.scheduler().setStopAction(k::StopAction::Ignore);
    one.scheduler().requestEnd(EndReason::Stop, 2);
    two.scheduler().requestEnd(EndReason::Stop, 2);
    EXPECT_TRUE(one.scheduler().ending());
    EXPECT_EQ(one.scheduler().reason(), EndReason::Stop);
    EXPECT_FALSE(one.scheduler().ignoredStopStatistics());
    EXPECT_FALSE(two.scheduler().ending());
    EXPECT_TRUE(two.scheduler().ignoredStopStatistics());
    two.scheduler().requestEnd(EndReason::Finish, 0);
    EXPECT_EQ(two.scheduler().reason(), EndReason::Finish);
}

TEST(Scheduler, WallTimeLimitIsLocalAndTakenOnce) {
    k::Context one, two;
    const auto started = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    one.scheduler().setWallTimeLimit(1, started);
    EXPECT_TRUE(one.scheduler().takeInterrupt());
    EXPECT_EQ(one.scheduler().reason(), EndReason::WallTimeLimit);
    EXPECT_EQ(one.scheduler().endNote(), "wall-clock limit reached: 1 seconds (--max-wall-time=1)");
    EXPECT_FALSE(one.scheduler().takeInterrupt());
    EXPECT_FALSE(two.scheduler().takeInterrupt());
    EXPECT_FALSE(two.scheduler().ending());
}

TEST(Scheduler, WallTimeZeroAndLargeLimitsDoNotExpire) {
    k::Context c;
    const auto started = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    c.scheduler().setWallTimeLimit(0, started);
    EXPECT_FALSE(c.scheduler().takeInterrupt());
    c.scheduler().setWallTimeLimit(UINT64_MAX, started);
    EXPECT_FALSE(c.scheduler().takeInterrupt());
    EXPECT_FALSE(c.scheduler().ending());
}

TEST(Scheduler, WallTimeDoesNotInterruptFinalProcedures) {
    k::Context c;
    c.scheduler().setWallTimeLimit(1, std::chrono::steady_clock::now() - std::chrono::seconds(2));
    bool completed = false;
    const auto finish = [](kirima_context* ctx, void* frame) -> kirima_status {
        *static_cast<bool*>(frame) = !k::fromC(ctx).scheduler().takeInterrupt();
        return KIRIMA_DONE;
    };
    const k::ProcessId finals[] = {c.scheduler().addProcess(finish, &completed, Group::Active)};
    c.scheduler().requestEnd(EndReason::Finish, 0);
    c.scheduler().runFinal(finals);
    EXPECT_TRUE(completed);
    EXPECT_EQ(c.scheduler().reason(), EndReason::Finish);
    EXPECT_EQ(c.scheduler().level(), 0);
}

TEST(Scheduler, StopErrorKeepsStopReasonAndOtherRequests) {
    k::Context c;
    c.scheduler().setStopAction(k::StopAction::Error);
    c.scheduler().requestEnd(EndReason::Stop, 1);
    EXPECT_TRUE(c.scheduler().ending());
    EXPECT_EQ(c.scheduler().reason(), EndReason::Stop);
    EXPECT_EQ(c.scheduler().level(), 1);
    c.scheduler().requestEnd(EndReason::Fatal, 1);
    EXPECT_EQ(c.scheduler().reason(), EndReason::Stop);
}

// 24.3/24.7: origin and completion are kernel state, independent of tiers.
struct ProgramFrame {
    k::ProcessId id{0};
    std::uint64_t delay = 0;
    bool waiting = false;
    bool exit = false;
    std::string tag;
    std::vector<std::string>* log = nullptr;
};

kirima_status runProgramFrame(kirima_context* ctx, void* frame) {
    auto& scheduler = k::fromC(ctx).scheduler();
    auto& f = *static_cast<ProgramFrame*>(frame);
    if (!f.waiting && f.delay != 0) {
        f.waiting = true;
        scheduler.waitDelay(f.id, f.delay);
        return KIRIMA_WAITING;
    }
    if (f.log != nullptr) f.log->push_back(f.tag + std::to_string(scheduler.time()));
    if (f.exit) scheduler.exitProgram(f.id);
    return scheduler.finished(f.id) ? KIRIMA_KILLED : KIRIMA_DONE;
}

TEST(Scheduler, EmptyProgramsDoNotCauseFinish) {
    k::Context c;
    auto& scheduler = c.scheduler();
    scheduler.addProgram();
    std::vector<std::string> log;
    ProgramFrame module{k::ProcessId{0}, 2, false, false, "module", &log};
    module.id = scheduler.addProcess(&runProgramFrame, &module, Group::Active);
    scheduler.start(module.id);
    EXPECT_EQ(scheduler.run(), EndReason::Quiescent);
    EXPECT_EQ(log, std::vector<std::string>{"module2"});
}

TEST(Scheduler, ProgramCompletionCountsInitialsAndEndsDescendants) {
    k::Context c;
    auto& scheduler = c.scheduler();
    std::vector<std::string> log;
    const k::ProgramId first = scheduler.addProgram();
    const k::ProgramId second = scheduler.addProgram();
    ProgramFrame a{k::ProcessId{0}, 1, false, false, "a", &log};
    ProgramFrame b{k::ProcessId{0}, 3, false, false, "b", &log};
    ProgramFrame child{k::ProcessId{0}, 2, false, false, "child", &log};
    ProgramFrame late{k::ProcessId{0}, 20, false, false, "late", &log};
    ProgramFrame keeper{k::ProcessId{0}, 4, false, false, "keeper", &log};
    a.id = scheduler.addProgramInitial(&runProgramFrame, &a, first);
    b.id = scheduler.addProgramInitial(&runProgramFrame, &b, first);
    child.id = scheduler.addChildProcess(&runProgramFrame, &child, a.id);
    late.id = scheduler.addChildProcess(&runProgramFrame, &late, child.id);
    keeper.id = scheduler.addProgramInitial(&runProgramFrame, &keeper, second);
    for (const k::ProcessId p : {a.id, b.id, child.id, late.id, keeper.id})
        scheduler.start(p);
    EXPECT_EQ(scheduler.processGroup(child.id), Group::Reactive);
    EXPECT_EQ(scheduler.processGroup(late.id), Group::Reactive);
    EXPECT_EQ(scheduler.run(), EndReason::Finish);
    EXPECT_EQ(scheduler.time(), 4U);
    EXPECT_EQ(log, (std::vector<std::string>{"a1", "child2", "b3", "keeper4"}));
    EXPECT_TRUE(scheduler.finished(late.id));
    EXPECT_TRUE(scheduler.programStopped(late.id));
    EXPECT_FALSE(scheduler.programStopped(keeper.id));
    k::ProcessId ended = k::kNone;
    ASSERT_TRUE(scheduler.takeEndedProgramProcess(ended));
    EXPECT_EQ(ended, late.id);
    EXPECT_FALSE(scheduler.takeEndedProgramProcess(ended));
}

TEST(Scheduler, ProgramExitUsesInheritedOriginAndKeepsOtherPrograms) {
    k::Context c;
    auto& scheduler = c.scheduler();
    std::vector<std::string> log;
    const k::ProgramId first = scheduler.addProgram();
    const k::ProgramId second = scheduler.addProgram();
    ProgramFrame a{k::ProcessId{0}, 5, false, false, "unexpected", &log};
    ProgramFrame b{k::ProcessId{0}, 6, false, false, "unexpected", &log};
    ProgramFrame child{k::ProcessId{0}, 1, false, true, "exit", &log};
    ProgramFrame keeper{k::ProcessId{0}, 3, false, false, "keeper", &log};
    a.id = scheduler.addProgramInitial(&runProgramFrame, &a, first);
    b.id = scheduler.addProgramInitial(&runProgramFrame, &b, first);
    child.id = scheduler.addChildProcess(&runProgramFrame, &child, a.id);
    keeper.id = scheduler.addProgramInitial(&runProgramFrame, &keeper, second);
    for (const k::ProcessId p : {a.id, b.id, child.id, keeper.id})
        scheduler.start(p);
    EXPECT_EQ(scheduler.run(), EndReason::Finish);
    EXPECT_EQ(log, (std::vector<std::string>{"exit1", "keeper3"}));
    EXPECT_TRUE(scheduler.finished(a.id));
    EXPECT_TRUE(scheduler.finished(b.id));
    EXPECT_TRUE(scheduler.finished(child.id));
    std::vector<k::ProcessId> ended;
    k::ProcessId p = k::kNone;
    while (scheduler.takeEndedProgramProcess(p))
        ended.push_back(p);
    std::sort(ended.begin(), ended.end());
    EXPECT_EQ(ended, (std::vector<k::ProcessId>{a.id, b.id, child.id}));
}

TEST(Scheduler, ProgramExitOutsideInitialOriginIsIgnored) {
    k::Context c;
    auto& scheduler = c.scheduler();
    scheduler.addProgram();
    ProgramFrame module;
    module.id = scheduler.addProcess(&runProgramFrame, &module, Group::Active);
    ProgramFrame child;
    child.id = scheduler.addChildProcess(&runProgramFrame, &child, module.id);
    scheduler.exitProgram(child.id);
    EXPECT_FALSE(scheduler.finished(child.id));
    EXPECT_FALSE(scheduler.ending());
    EXPECT_EQ(scheduler.processGroup(child.id), Group::Active);
}

struct ProgramNbaFrame {
    k::ProcessId id{0};
    k::SlotId slot{0};
    unsigned stage = 0;
    std::vector<std::uint64_t> observed;
};

kirima_status programNba(kirima_context* ctx, void* frame) {
    auto& c = k::fromC(ctx);
    auto& f = *static_cast<ProgramNbaFrame*>(frame);
    f.observed.push_back(*c.store().element(f.slot, 0));
    switch (f.stage++) {
        case 0: {
            const std::uint64_t value = 7;
            c.scheduler().scheduleNba(f.id, {.slot = f.slot}, &value, 0);
            c.scheduler().waitDelay(f.id, 0);
            return KIRIMA_WAITING;
        }
        case 1: c.scheduler().waitDelay(f.id, 1); return KIRIMA_WAITING;
        default: return KIRIMA_DONE;
    }
}

TEST(Scheduler, ProgramZeroDelayPrecedesReNbaAndPositiveDelayResumesReactive) {
    k::Context c;
    ProgramNbaFrame f;
    f.slot = c.store().add(k::Domain::Two, 8, 1, k::Init::Zero);
    *c.store().element(f.slot, 0) = 3;
    const k::ProgramId program = c.scheduler().addProgram();
    f.id = c.scheduler().addProgramInitial(&programNba, &f, program);
    c.scheduler().start(f.id);
    EXPECT_EQ(c.scheduler().run(), EndReason::Finish);
    EXPECT_EQ(f.observed, (std::vector<std::uint64_t>{3, 3, 7}));
    EXPECT_EQ(c.scheduler().processGroup(f.id), Group::Reactive);
}

TEST(Scheduler, ProgramsAreIsolatedBetweenContexts) {
    k::Context first, second;
    ProgramFrame a, b;
    a.id =
        first.scheduler().addProgramInitial(&runProgramFrame, &a, first.scheduler().addProgram());
    b.id =
        second.scheduler().addProgramInitial(&runProgramFrame, &b, second.scheduler().addProgram());
    first.scheduler().start(a.id);
    second.scheduler().start(b.id);
    EXPECT_EQ(first.scheduler().run(), EndReason::Finish);
    EXPECT_FALSE(second.scheduler().ending());
    EXPECT_EQ(second.scheduler().run(), EndReason::Finish);
}

TEST(Scheduler, ReusedProgramRecordsDropOriginAndCancellation) {
    k::Context c;
    auto& scheduler = c.scheduler();
    ProgramFrame first, child, keeper, module;
    const k::ProgramId program = scheduler.addProgram();
    first.id = scheduler.addProgramInitial(&runProgramFrame, &first, program);
    child.id = scheduler.addChildProcess(&runProgramFrame, &child, first.id);
    keeper.id = scheduler.addProgramInitial(&runProgramFrame, &keeper, scheduler.addProgram());
    scheduler.ended(first.id);
    EXPECT_TRUE(scheduler.programStopped(child.id));
    k::ProcessId ended = k::kNone;
    ASSERT_TRUE(scheduler.takeEndedProgramProcess(ended));
    EXPECT_EQ(ended, child.id);
    scheduler.releaseProcess(child.id);
    module.id = scheduler.addProcess(&runProgramFrame, &module, Group::Active);
    EXPECT_EQ(module.id, child.id);
    EXPECT_FALSE(scheduler.programStopped(module.id));
    scheduler.exitProgram(module.id);
    EXPECT_FALSE(scheduler.ending());
    EXPECT_FALSE(scheduler.finished(keeper.id));
}

struct SafeBoundary {
    k::ProcessId process{0};
    unsigned step = 0;
    unsigned visits = 0;
    bool callback = false;
    bool unsafe = false;
    static void collect(void* frame) noexcept {
        auto& s = *static_cast<SafeBoundary*>(frame);
        s.unsafe = s.unsafe || s.callback;
        ++s.visits;
    }
    static kirima_status resume(kirima_context* context, void* frame) {
        auto& s = *static_cast<SafeBoundary*>(frame);
        s.callback = true;
        const bool done = s.step == 3;
        if (!done) k::fromC(context).scheduler().waitDelay(s.process, s.step == 1 ? 1 : 0);
        ++s.step;
        s.callback = false;
        return static_cast<kirima_status>(done ? Status::Done : Status::Waiting);
    }
};

TEST(Scheduler, SafePointsFollowDrainedRegionsAndPreserveHookOwnership) {
    k::Context context;
    auto& scheduler = context.scheduler();
    SafeBoundary state;
    const auto previous = scheduler.setSafePointHook({&SafeBoundary::collect, &state});
    EXPECT_EQ(previous.fn, nullptr);
    state.process = scheduler.addProcess(&SafeBoundary::resume, &state, Group::Active);
    scheduler.start(state.process);
    EXPECT_EQ(scheduler.run(), EndReason::Quiescent);
    EXPECT_FALSE(state.unsafe);
    EXPECT_EQ(state.step, 4U);
    EXPECT_GE(state.visits, 4U);
    const auto hook = scheduler.setSafePointHook(previous);
    EXPECT_EQ(hook.fn, &SafeBoundary::collect);
    EXPECT_EQ(hook.frame, &state);
}

struct PendingValues {
    std::vector<std::pair<std::uint32_t, std::uint64_t>> values;
    bool stop = false;
    unsigned calls = 0;
    static bool visit(void* frame, k::SlotId slot, std::span<const std::uint64_t> values) noexcept {
        auto& out = *static_cast<PendingValues*>(frame);
        ++out.calls;
        if (out.stop) return false;
        try {
            for (const auto v : values)
                out.values.push_back({slot.index(), v});
            return true;
        } catch (const std::bad_alloc&) {
            return false;
        } catch (const std::length_error&) {
            return false;
        }
    }
};

TEST(Scheduler, PendingValueVisitorIncludesOwnedPayloadsAndSkipsCompletedDrivers) {
    k::Context context;
    auto& scheduler = context.scheduler();
    const auto nba = context.store().add(k::Domain::Handle, 64, 1, k::Init::Zero);
    const auto driver = context.store().add(k::Domain::Handle, 64, 1, k::Init::Zero);
    const auto held = context.store().add(k::Domain::Handle, 64, 1, k::Init::Zero);
    SafeBoundary state;
    const auto process = scheduler.addProcess(&SafeBoundary::resume, &state, Group::Active);
    const std::uint64_t immediate = 9, delayed = 11, driven = 17, forced = 23;
    scheduler.scheduleNba(process, {.slot = nba}, &immediate, 0);
    scheduler.scheduleNba(process, {.slot = nba}, &delayed, 3);
    const auto id = scheduler.addDelayedDriver();
    scheduler.driveDelayed(id, {.slot = driver}, &driven, 2, 2, 2);
    scheduler.markHeld(held, false, false);
    scheduler.hold(k::Hold::Force, process, {.slot = held});
    scheduler.writeHeld(k::Hold::Force, process, {.slot = held}, &forced);
    PendingValues pending;
    ASSERT_TRUE(scheduler.visitPendingValues(&PendingValues::visit, &pending));
    std::sort(pending.values.begin(), pending.values.end());
    EXPECT_EQ(pending.values,
              (std::vector<std::pair<std::uint32_t, std::uint64_t>>{{nba.index(), 9},
                                                                    {nba.index(), 11},
                                                                    {driver.index(), 17},
                                                                    {held.index(), 0},
                                                                    {held.index(), 23}}));
    PendingValues stop;
    stop.stop = true;
    EXPECT_FALSE(scheduler.visitPendingValues(&PendingValues::visit, &stop));
    EXPECT_EQ(stop.calls, 1U);
    EXPECT_EQ(scheduler.run(), EndReason::Quiescent);
    PendingValues after;
    ASSERT_TRUE(scheduler.visitPendingValues(&PendingValues::visit, &after));
    std::sort(after.values.begin(), after.values.end());
    EXPECT_EQ(after.values, (std::vector<std::pair<std::uint32_t, std::uint64_t>>{
                                {held.index(), 0}, {held.index(), 23}}));
}

struct HeapWatch {
    k::HeapTypeId type;
    k::HeapLayoutId layout;
    k::SlotId binding{0}, index{0};
    k::CellToken detached;
    bool size = false, allow = false, valid = true;
    unsigned evals = 0, guards = 0, resumes = 0;
    static void eval(kirima_context* handle, void* frame, std::uint64_t* out) {
        auto& s = *static_cast<HeapWatch*>(frame);
        auto& ctx = k::fromC(handle);
        ++s.evals;
        if (s.size) {
            const auto value = ctx.heap().size(s.type, {*ctx.store().element(s.binding, 0)});
            s.valid = s.valid && static_cast<bool>(value);
            out[0] = value.value;
        } else {
            k::W4 value{};
            const auto status =
                s.detached.bits != 0
                    ? ctx.heap().readCell(s.layout, s.detached, {&value, 1})
                    : ctx.heap().read(s.type, {*ctx.store().element(s.binding, 0)},
                                      static_cast<std::int64_t>(*ctx.store().element(s.index, 0)),
                                      {&value, 1});
            s.valid = s.valid && static_cast<bool>(status);
            out[0] = value.a;
            out[1] = value.b;
        }
    }
    static int iff(kirima_context*, void* frame) {
        auto& s = *static_cast<HeapWatch*>(frame);
        ++s.guards;
        return s.allow ? 1 : 0;
    }
    static kirima_status resume(kirima_context*, void* frame) {
        ++static_cast<HeapWatch*>(frame)->resumes;
        return static_cast<kirima_status>(Status::Done);
    }
    k::MonitorSpec spec() {
        k::MonitorSpec m;
        m.heap = true;
        if (detached.bits == 0) m.slots = {{binding}, {index}};
        m.eval = &eval;
        m.frame = this;
        m.domain = size ? k::Domain::Two : k::Domain::Four;
        m.width = 64;
        m.guard = &iff;
        m.guardFrame = this;
        return m;
    }
};

TEST(Scheduler, HeapMonitorsTrackSelectedCellsAndReattachAfterStructureAndBindingChanges) {
    for (const auto group : {Group::Active, Group::Reactive}) {
        k::Context ctx;
        auto& heap = ctx.heap();
        auto& scheduler = ctx.scheduler();
        const auto layout = heap.addLayout(std::vector<k::HeapWord>{{0xff, true}});
        ASSERT_TRUE(layout);
        const auto type = heap.addType({k::ContainerKind::Queue, layout.value});
        ASSERT_TRUE(type);
        auto value = heap.fromValues(type.value, std::vector<k::W4>{{1, 0}, {2, 0}});
        ASSERT_TRUE(value);
        const auto original = value.value;
        const auto selected = heap.cell(type.value, value.value, 1);
        ASSERT_TRUE(selected);
        HeapWatch watch{type.value, layout.value};
        watch.binding = ctx.store().add(k::Domain::Handle, 64, 1, k::Init::Zero);
        watch.index = ctx.store().add(k::Domain::Two, 32, 1, k::Init::Zero);
        *ctx.store().element(watch.binding, 0) = value.value.bits;
        *ctx.store().element(watch.index, 0) = 1;
        const auto p = scheduler.addProcess(&HeapWatch::resume, &watch, group);
        scheduler.waitMonitor(p, watch.spec());
        EXPECT_EQ(watch.evals, 1U);
        ASSERT_TRUE(heap.write(type.value, value.value, 0, std::vector<k::W4>{{9, 0}}));
        scheduler.flushHeapChanges();
        EXPECT_EQ(watch.evals, 1U);  // A different cell was not read.
        ASSERT_TRUE(heap.write(type.value, value.value, 1, std::vector<k::W4>{{2, 0}}));
        scheduler.flushHeapChanges();
        EXPECT_EQ(watch.evals, 1U);  // Equal normalized value.
        ASSERT_TRUE(heap.insert(type.value, value.value, 0, std::vector<k::W4>{{7, 0}}));
        scheduler.flushHeapChanges();
        EXPECT_EQ(watch.evals, 2U);
        ASSERT_TRUE(heap.writeCell(layout.value, selected.value, std::vector<k::W4>{{13, 0}}));
        scheduler.flushHeapChanges();
        EXPECT_EQ(watch.evals, 2U);  // Insertion moved the old selected cell away.
        const std::uint64_t index = 2;
        scheduler.write({.slot = watch.index}, &index);
        EXPECT_EQ(watch.evals, 3U);
        ASSERT_TRUE(heap.writeCell(layout.value, selected.value, std::vector<k::W4>{{14, 0}}));
        scheduler.flushHeapChanges();
        EXPECT_EQ(watch.evals, 4U);
        auto replacement =
            heap.fromValues(type.value, std::vector<k::W4>{{31, 0}, {32, 0}, {33, 0}});
        ASSERT_TRUE(replacement);
        scheduler.write({.slot = watch.binding}, &replacement.value.bits);
        EXPECT_EQ(watch.evals, 5U);
        ASSERT_TRUE(heap.writeCell(layout.value, selected.value, std::vector<k::W4>{{15, 0}}));
        scheduler.flushHeapChanges();
        EXPECT_EQ(watch.evals, 5U);  // Replacement removed old object dependencies.
        watch.allow = true;
        ASSERT_TRUE(heap.write(type.value, replacement.value, 2, std::vector<k::W4>{{33, 0x20}}));
        scheduler.flushHeapChanges();
        EXPECT_EQ(watch.evals, 6U);  // B bits alone are observable.
        EXPECT_EQ(watch.guards, 5U);
        ASSERT_EQ(scheduler.run(), EndReason::Quiescent);
        EXPECT_EQ(watch.resumes, 1U);
        EXPECT_TRUE(watch.valid);
        ASSERT_TRUE(
            heap.collect(std::vector<k::HeapRoot>{k::HeapRoot::container(replacement.value)}));
        EXPECT_EQ(heap.actualType(original).status.error, k::HeapError::InvalidHandle);
    }
}

TEST(Scheduler, HeapSizeMonitorsIgnoreValueWritesAndDetachedCellMonitorsIgnoreMovement) {
    k::Context ctx;
    auto& heap = ctx.heap();
    auto& scheduler = ctx.scheduler();
    const auto layout = heap.addLayout(std::vector<k::HeapWord>{{0xff, true}});
    ASSERT_TRUE(layout);
    const auto type = heap.addType({k::ContainerKind::Queue, layout.value});
    ASSERT_TRUE(type);
    auto value = heap.fromValues(type.value, std::vector<k::W4>{{1, 0}});
    ASSERT_TRUE(value);
    const auto cell = heap.cell(type.value, value.value, 0);
    ASSERT_TRUE(cell);
    HeapWatch size{type.value, layout.value}, detached{type.value, layout.value};
    size.size = true;
    size.binding = ctx.store().add(k::Domain::Handle, 64, 1, k::Init::Zero);
    size.index = ctx.store().add(k::Domain::Two, 32, 1, k::Init::Zero);
    *ctx.store().element(size.binding, 0) = value.value.bits;
    detached.detached = cell.value;
    const auto a = scheduler.addProcess(&HeapWatch::resume, &size, Group::Active);
    const auto b = scheduler.addProcess(&HeapWatch::resume, &detached, Group::Reactive);
    scheduler.waitMonitor(a, size.spec());
    scheduler.waitMonitor(b, detached.spec());
    ASSERT_TRUE(heap.writeCell(layout.value, cell.value, std::vector<k::W4>{{9, 0}}));
    scheduler.flushHeapChanges();
    EXPECT_EQ(size.evals, 1U);
    EXPECT_EQ(detached.evals, 2U);
    ASSERT_TRUE(heap.erase(type.value, value.value, 0));
    scheduler.flushHeapChanges();
    EXPECT_EQ(size.evals, 2U);
    EXPECT_EQ(detached.evals, 2U);
    ASSERT_TRUE(heap.collect(std::vector<k::HeapRoot>{k::HeapRoot::cell(cell.value),
                                                      k::HeapRoot::container(value.value)}));
    detached.allow = true;
    ASSERT_TRUE(heap.writeCell(layout.value, cell.value, std::vector<k::W4>{{11, 0}}));
    scheduler.flushHeapChanges();
    EXPECT_EQ(size.evals, 2U);
    EXPECT_EQ(detached.evals, 3U);
    ASSERT_EQ(scheduler.run(), EndReason::Quiescent);
    EXPECT_EQ(detached.resumes, 1U);
    EXPECT_EQ(size.resumes, 0U);
    EXPECT_TRUE(size.valid && detached.valid);
    scheduler.kill(a);
}

struct HeapNoticeCount {
    unsigned calls = 0;
    static void record(void* frame, k::HeapRoot) noexcept {
        ++static_cast<HeapNoticeCount*>(frame)->calls;
    }
};

TEST(Scheduler, HeapMonitorWaitReuseKeepsListsBoundedAndRestoresThePreviousHook) {
    k::Context ctx;
    auto& heap = ctx.heap();
    auto& scheduler = ctx.scheduler();
    const auto l = heap.addLayout(std::vector<k::HeapWord>{{0xff, true}});
    ASSERT_TRUE(l);
    const auto t = heap.addType({k::ContainerKind::Queue, l.value});
    ASSERT_TRUE(t);
    auto value = heap.fromValues(t.value, std::vector<k::W4>{{1, 0}});
    ASSERT_TRUE(value);
    HeapWatch watch{t.value, l.value};
    watch.binding = ctx.store().add(k::Domain::Handle, 64, 1, k::Init::Zero);
    watch.index = ctx.store().add(k::Domain::Two, 32, 1, k::Init::Zero);
    *ctx.store().element(watch.binding, 0) = value.value.bits;
    HeapNoticeCount previous;
    heap.setChangeHook({&HeapNoticeCount::record, &previous});
    const auto p = scheduler.addProcess(&HeapWatch::resume, &watch, Group::Active);
    for (unsigned i = 0; i < 1000; ++i) {
        scheduler.waitMonitor(p, watch.spec());
        // Existing slot-list sweeping keeps at most 16 entries per list.
        ASSERT_LE(scheduler.waitListEntries(), 33U);
    }
    EXPECT_EQ(watch.evals, 1000U);
    const auto spec = watch.spec();
    scheduler.waitAny(p, {}, std::vector<k::MonitorSpec>{spec, spec});
    watch.allow = true;
    ASSERT_TRUE(heap.write(t.value, value.value, 0, std::vector<k::W4>{{9, 0}}));
    scheduler.flushHeapChanges();
    EXPECT_EQ(watch.evals, 1003U);  // First notice cancels both items; the second is stale.
    EXPECT_EQ(previous.calls, 0U);
    ASSERT_EQ(scheduler.run(), EndReason::Quiescent);
    EXPECT_EQ(watch.resumes, 1U);
    ASSERT_TRUE(heap.write(t.value, value.value, 0, std::vector<k::W4>{{11, 0}}));
    EXPECT_EQ(previous.calls, 1U);
    const auto restored = heap.setChangeHook({});
    EXPECT_EQ(restored.fn, &HeapNoticeCount::record);
    EXPECT_EQ(restored.frame, &previous);
}

class HeapMonitorMemory final : public std::pmr::memory_resource {
public:
    bool fail = false;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (fail) throw std::bad_alloc{};
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

TEST(Scheduler, OutermostDisableAllocationFailureKeepsPendingReports) {
    for (const bool observed : {false, true}) {
        HeapMonitorMemory memory;
        k::Context ctx(k::OutputDestinations{}, nullptr, &memory);
        k::Scheduler& s = ctx.scheduler();
        Actor owner{Actor::Role::End, &s};
        owner.id = s.addProcess(&act, &owner, Group::Active);
        DeferredLog log;
        s.deferCall(owner.id, !observed, &DeferredLog::call, &log, 17);
        s.deferCall(owner.id, true, &DeferredLog::call, &log, 23);
        memory.fail = true;
        bool flushed = true;
        EXPECT_NO_THROW(flushed = s.flushDeferredAssertions(owner.id));
        EXPECT_FALSE(flushed);
        EXPECT_EQ(s.reason(), EndReason::ResourceLimit);
        EXPECT_TRUE(s.hasPending(owner.id));
        EXPECT_TRUE(log.calls.empty());
        // Ending remains requested, but a cleanup retry consumes the same
        // two reports and original tokens, once each and in queue order.
        memory.fail = false;
        EXPECT_FALSE(s.flushDeferredAssertions(owner.id));
        EXPECT_FALSE(s.hasPending(owner.id));
        EXPECT_EQ(log.calls, (std::vector<std::pair<std::uint32_t, int>>{{17, 0}, {23, 0}}));
    }
}

TEST(Scheduler, HeapMonitorCaptureAndNoticeFailuresReturnResourceLimitAndRestoreHooks) {
    for (bool notice : {false, true}) {
        HeapMonitorMemory memory;
        k::Context ctx(k::OutputDestinations{}, nullptr, &memory);
        auto& heap = ctx.heap();
        auto& scheduler = ctx.scheduler();
        const auto l = heap.addLayout(std::vector<k::HeapWord>{{0xff, true}});
        ASSERT_TRUE(l);
        const auto t = heap.addType({k::ContainerKind::Queue, l.value});
        ASSERT_TRUE(t);
        auto value = heap.fromValues(t.value, std::vector<k::W4>{{1, 0}});
        ASSERT_TRUE(value);
        const auto cell = heap.cell(t.value, value.value, 0);
        ASSERT_TRUE(cell);
        HeapWatch watch{t.value, l.value};
        watch.binding = ctx.store().add(k::Domain::Handle, 64, 1, k::Init::Zero);
        watch.index = ctx.store().add(k::Domain::Two, 32, 1, k::Init::Zero);
        *ctx.store().element(watch.binding, 0) = value.value.bits;
        HeapNoticeCount reads, changes;
        heap.setReadHook({&HeapNoticeCount::record, &reads});
        heap.setChangeHook({&HeapNoticeCount::record, &changes});
        const auto p = scheduler.addProcess(&HeapWatch::resume, &watch, Group::Active);
        memory.fail = !notice;
        scheduler.waitMonitor(p, watch.spec());
        if (notice) {
            ASSERT_FALSE(scheduler.ending());
            memory.fail = true;
            // Erasure commits without allocating; allocation of its queued
            // notice fails afterwards. The committed empty queue remains valid.
            ASSERT_TRUE(heap.erase(t.value, value.value, 0));
        }
        EXPECT_EQ(scheduler.reason(), EndReason::ResourceLimit);
        EXPECT_TRUE(scheduler.heapMonitorLimited());
        EXPECT_EQ(watch.evals, 1U);
        EXPECT_EQ(watch.resumes, 0U);
        memory.fail = false;
        EXPECT_EQ(heap.size(t.value, value.value).value, notice ? 0U : 1U);
        const auto oldRead = heap.setReadHook({});
        EXPECT_EQ(oldRead.fn, &HeapNoticeCount::record);
        EXPECT_EQ(oldRead.frame, &reads);
        scheduler.kill(p);
        const auto oldChange = heap.setChangeHook({});
        EXPECT_EQ(oldChange.fn, &HeapNoticeCount::record);
        EXPECT_EQ(oldChange.frame, &changes);
        std::vector<k::W4> out(1);
        ASSERT_TRUE(heap.readCell(l.value, cell.value, out));
        EXPECT_EQ(out[0].a, 1U);
    }
}

TEST(Scheduler, HeapMonitorNoticesCannotWakeAReusedRecord) {
    k::Context ctx;
    auto& heap = ctx.heap();
    auto& scheduler = ctx.scheduler();
    const auto l = heap.addLayout(std::vector<k::HeapWord>{{0xff, true}});
    ASSERT_TRUE(l);
    const auto t = heap.addType({k::ContainerKind::Queue, l.value});
    ASSERT_TRUE(t);
    auto value = heap.fromValues(t.value, std::vector<k::W4>{{1, 0}});
    ASSERT_TRUE(value);
    const auto cell = heap.cell(t.value, value.value, 0);
    ASSERT_TRUE(cell);
    HeapWatch first{t.value, l.value}, next{t.value, l.value};
    first.detached = next.detached = cell.value;
    const auto p = scheduler.addProcess(&HeapWatch::resume, &first, Group::Active);
    const auto q = scheduler.addProcess(&HeapWatch::resume, &next, Group::Reactive);
    struct Replace {
        k::ProcessId old, fresh;
        HeapWatch& next;
        bool done = false;
        static int guard(kirima_context* context, void* frame) {
            auto& s = *static_cast<Replace*>(frame);
            if (!s.done) {
                s.done = true;
                auto& scheduler = k::fromC(context).scheduler();
                scheduler.kill(s.old);
                scheduler.waitMonitor(s.fresh, s.next.spec());
            }
            return 1;
        }
    } replace{p, q, next};
    auto m = first.spec();
    m.guard = &Replace::guard;
    m.guardFrame = &replace;
    scheduler.waitAny(p, {}, std::vector<k::MonitorSpec>{m, m});
    ASSERT_TRUE(heap.writeCell(l.value, cell.value, std::vector<k::W4>{{9, 0}}));
    scheduler.flushHeapChanges();
    EXPECT_TRUE(replace.done);
    EXPECT_EQ(first.evals, 3U);
    EXPECT_EQ(next.evals, 1U);  // Not evaluated for the old wait's second notice.
    EXPECT_EQ(next.resumes, 0U);
    next.allow = true;
    ASSERT_TRUE(heap.writeCell(l.value, cell.value, std::vector<k::W4>{{11, 0}}));
    scheduler.flushHeapChanges();
    ASSERT_EQ(scheduler.run(), EndReason::Quiescent);
    EXPECT_EQ(first.resumes, 0U);
    EXPECT_EQ(next.resumes, 1U);
    EXPECT_TRUE(first.valid && next.valid);
}

struct HeapDisplay {
    k::HeapTypeId type;
    k::ContainerToken value;
    std::int64_t index = 0;
    std::vector<std::uint64_t> observed;
    bool valid = true;
    static kirima_status resume(kirima_context* handle, void* frame) {
        auto& self = *static_cast<HeapDisplay*>(frame);
        auto& ctx = k::fromC(handle);
        k::W4 word;
        self.valid =
            self.valid && static_cast<bool>(ctx.heap().read(self.type, self.value, self.index,
                                                            std::span<k::W4>{&word, 1}));
        self.observed.push_back(word.a);
        return KIRIMA_DONE;
    }
    static kirima_status tick(kirima_context*, void*) { return KIRIMA_DONE; }
};

TEST(Scheduler, HeapFileAndDisplayMonitorsReleaseOnlyTheirOwnReadsAndRestoreHooks) {
    for (const bool file : {false, true}) {
        SCOPED_TRACE(file);
        k::Context ctx;
        auto& heap = ctx.heap();
        auto& scheduler = ctx.scheduler();
        const auto layout = heap.addLayout(std::vector<k::HeapWord>{{0xff, true}});
        ASSERT_TRUE(layout);
        const auto type = heap.addType({k::ContainerKind::Queue, layout.value});
        ASSERT_TRUE(type);
        const auto value = heap.fromValues(type.value, std::vector<k::W4>{{1, 0}, {2, 0}});
        ASSERT_TRUE(value);
        const auto firstCell = heap.cell(type.value, value.value, 0);
        const auto secondCell = heap.cell(type.value, value.value, 1);
        ASSERT_TRUE(firstCell);
        ASSERT_TRUE(secondCell);
        HeapNoticeCount previous;
        heap.setChangeHook({&HeapNoticeCount::record, &previous});
        HeapDisplay first{type.value, value.value}, second{type.value, value.value, 1};
        const auto a = scheduler.addProcess(&HeapDisplay::resume, &first, Group::Active);
        const auto b = scheduler.addProcess(&HeapDisplay::resume, &second, Group::Active);
        const auto clock = scheduler.addProcess(&HeapDisplay::tick, nullptr, Group::Active);
        const auto to = ctx.files().targets({1, 0});
        if (file)
            scheduler.addFileMonitor(a, {}, to, {.heap = true});
        else
            scheduler.setMonitor(a, {}, {.heap = true});
        scheduler.addFileMonitor(b, {}, to, {.heap = true});
        scheduler.start(clock);
        ASSERT_EQ(scheduler.run(), EndReason::Quiescent);
        EXPECT_EQ(first.observed, (std::vector<std::uint64_t>{1}));
        EXPECT_EQ(second.observed, (std::vector<std::uint64_t>{2}));
        // Kill releases a callback's reads before its frame can be destroyed.
        scheduler.kill(a);
        EXPECT_TRUE(scheduler.finished(a));
        EXPECT_FALSE(scheduler.finished(b));
        ASSERT_TRUE(heap.writeCell(layout.value, firstCell.value, std::array<k::W4, 1>{{{9, 0}}}));
        ASSERT_TRUE(heap.writeCell(layout.value, secondCell.value, std::array<k::W4, 1>{{{8, 0}}}));
        scheduler.start(clock);
        ASSERT_EQ(scheduler.run(), EndReason::Quiescent);
        EXPECT_EQ(first.observed, (std::vector<std::uint64_t>{1}));
        EXPECT_EQ(second.observed, (std::vector<std::uint64_t>{2, 8}));
        scheduler.kill(b);
        EXPECT_TRUE(scheduler.finished(b));
        const auto restored = heap.setChangeHook({});
        EXPECT_EQ(restored.fn, &HeapNoticeCount::record);
        EXPECT_EQ(restored.frame, &previous);
        EXPECT_TRUE(first.valid && second.valid);
        ASSERT_TRUE(heap.collect({}));
        EXPECT_EQ(heap.stats().containers, 0U);
        EXPECT_EQ(heap.stats().cells, 0U);
    }
}

TEST(Scheduler, HeapFileAndDisplayCaptureFailuresKeepTheCallbackOriginAndRestoreReadHooks) {
    for (const bool file : {false, true}) {
        SCOPED_TRACE(file);
        HeapMonitorMemory memory;
        k::Context ctx(k::OutputDestinations{}, nullptr, &memory);
        auto& heap = ctx.heap();
        auto& scheduler = ctx.scheduler();
        const auto layout = heap.addLayout(std::vector<k::HeapWord>{{0xff, true}});
        ASSERT_TRUE(layout);
        const auto type = heap.addType({k::ContainerKind::Queue, layout.value});
        ASSERT_TRUE(type);
        const auto value = heap.fromValues(type.value, std::vector<k::W4>{{7, 0}});
        ASSERT_TRUE(value);
        HeapDisplay display{type.value, value.value};
        const auto callback = scheduler.addProcess(&HeapDisplay::resume, &display, Group::Active);
        const auto clock = scheduler.addProcess(&HeapDisplay::tick, nullptr, Group::Active);
        HeapNoticeCount reads, changes;
        heap.setReadHook({&HeapNoticeCount::record, &reads});
        heap.setChangeHook({&HeapNoticeCount::record, &changes});
        if (file)
            scheduler.addFileMonitor(callback, {}, ctx.files().targets({1, 0}), {.heap = true});
        else
            scheduler.setMonitor(callback, {}, {.heap = true});
        scheduler.start(clock);
        memory.fail = true;
        EXPECT_EQ(scheduler.run(), EndReason::ResourceLimit);
        memory.fail = false;
        EXPECT_TRUE(scheduler.heapMonitorLimited());
        EXPECT_EQ(scheduler.failedHeapMonitorProcess(), callback);
        EXPECT_EQ(display.observed, (std::vector<std::uint64_t>{7}));
        EXPECT_TRUE(display.valid);
        const auto restoredRead = heap.setReadHook({});
        EXPECT_EQ(restoredRead.fn, &HeapNoticeCount::record);
        EXPECT_EQ(restoredRead.frame, &reads);
        scheduler.kill(callback);
        const auto restoredChange = heap.setChangeHook({});
        EXPECT_EQ(restoredChange.fn, &HeapNoticeCount::record);
        EXPECT_EQ(restoredChange.frame, &changes);
    }
}

}  // namespace
