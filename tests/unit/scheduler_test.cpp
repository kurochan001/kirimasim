// Tests for the kernel scheduler (SCHEDULER_SPEC.md).
//
// Processes here are small scripts: a list of steps such as "write 3 to
// slot 1" or "wait for a posedge of slot 0". The same scripts also run on
// RefSim, a deliberately naive model of the reference algorithm (4.5 of
// the standard, SCHEDULER_SPEC.md 3) that keeps every event in plain lists
// and scans them. Random scripts must give the same log on both.

#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
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
            ctx_.scheduler().start(p);
        return ctx_.scheduler().run();
    }
    const std::string& log() const { return runner_.log; }
    k::Context& context() { return ctx_; }

    std::uint64_t now() override { return ctx_.scheduler().time(); }
    std::uint64_t read(std::uint32_t slot) override { return ctx_.store().element(slot, 0)[0]; }
    void write(std::uint32_t slot, std::uint64_t v) override {
        ctx_.scheduler().write(slot, 0, 0, 0, &v);
    }
    void nba(std::uint32_t p, std::uint32_t slot, std::uint64_t v, std::uint64_t d) override {
        ctx_.scheduler().scheduleNba(p, slot, 0, 0, 0, &v, d);
    }
    void waitDelay(std::uint32_t p, std::uint64_t d) override { ctx_.scheduler().waitDelay(p, d); }
    void waitEdge(std::uint32_t p, std::uint32_t slot, Edge e) override {
        const k::WaitItem item{{slot, 0, 0, 0, 0}, e, nullptr, nullptr};
        ctx_.scheduler().waitEvent(p, {&item, 1});
    }
    void waitNamed(std::uint32_t p, std::uint32_t ev) override {
        ctx_.scheduler().waitNamed(p, ev);
    }
    void trigger(std::uint32_t ev) override { ctx_.scheduler().trigger(ev); }
    void triggerNb(std::uint32_t p, std::uint32_t ev, std::uint64_t d) override {
        ctx_.scheduler().triggerNb(p, ev, d);
    }
    void waitTrue(std::uint32_t p, std::uint32_t slot) override {
        monitors_[p] = {this, slot};
        k::MonitorSpec m;
        m.slots = {{slot, 0, 0, 0, 0}};
        m.eval = &evalSlot;
        m.frame = &monitors_[p];
        m.domain = k::Domain::Two;
        m.width = 8;
        m.untilTrue = true;
        ctx_.scheduler().waitMonitor(p, m);
    }
    void finish() override { ctx_.scheduler().requestEnd(EndReason::Finish, 1); }
    void kill(std::uint32_t p) override { ctx_.scheduler().kill(p); }

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
        out[0] = k::fromC(ctx).store().element(m->slot, 0)[0];
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
    EXPECT_TRUE(sim.context().scheduler().triggered(0));
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
    EXPECT_EQ(sim.context().store().element(1, 0)[0], 5U);
    const std::string json = sim.context().counters().toJson();
    EXPECT_NE(json.find("\"proc.created\": 4, \"proc.finished\": 4"), std::string::npos) << json;
}

TEST(Scheduler, ZeroDelayLimit) {
    const std::vector<Step> spin(50, delay(0));
    KernelSim sim({{spin}});
    sim.context().scheduler().setLimits({UINT64_MAX, 10});
    EXPECT_EQ(sim.run(), EndReason::ZeroTimeLimit);
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
    const std::uint64_t one[2] = {1, 0};  // a = 1, b = 0: a known 1
    s.write(slot, 0, 3, 1, one);          // bit 3: x -> 1; bit 0 stays x
    s.run();
    EXPECT_TRUE(log.empty());
    s.write(slot, 0, 0, 1, one);  // bit 0: x -> 1, a posedge
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
    s.write(en, 0, 0, 0, &one);  // the guard alone changing does nothing
    EXPECT_EQ(g.calls, 0);
    s.write(clk, 0, 0, 0, &one);  // posedge, guard false: keep waiting
    EXPECT_EQ(g.calls, 1);
    s.write(clk, 0, 0, 0, &zero);  // negedge: guard not read
    EXPECT_EQ(g.calls, 1);
    g.value = true;
    s.write(clk, 0, 0, 0, &one);
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
    k::fromC(ctx).scheduler().write(f->written, 0, 0, 0, &v);
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
        WritingFrame f{a, b, 0};
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
        s.write(a, 0, 0, 0, &one);  // a & 1 becomes 1; writes b = 2: b & 1 becomes 0
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
        s.write(a, 0, 0, 0, &one);
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
        s.write(a, 0, 0, 0, &one);  // a & 1 becomes 1, and e is triggered
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
    s.write(a, 0, 0, 0, &one);
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
    s.write(a, 0, 0, 0, &three);  // a & 1 stays 1
    s.run();
    EXPECT_TRUE(log.empty());
    s.write(a, 0, 0, 0, &two);  // a & 1 becomes 0
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
    s.write(a, 0, 0, 0, &two);  // a & 1 stays 0
    s.run();
    EXPECT_TRUE(log.empty());
    s.write(b, 0, 0, 0, &one);  // b & 1 becomes 1: wakes
    s.write(a, 0, 0, 0, &one);  // the other items are gone
    s.write(clk, 0, 0, 0, &one);
    s.run();
    EXPECT_EQ(log, std::vector<std::string>{"w@0"});
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
    s.write(a, 0, 0, 0, &two);  // the value stays 0: guard not read
    EXPECT_EQ(g.calls, 0);
    s.write(a, 0, 0, 0, &one);  // posedge, guard false: keep waiting
    EXPECT_EQ(g.calls, 1);
    s.write(a, 0, 0, 0, &zero);  // negedge: guard not read
    EXPECT_EQ(g.calls, 1);
    g.value = true;
    s.write(a, 0, 0, 0, &one);
    EXPECT_EQ(g.calls, 2);
    s.run();
    EXPECT_EQ(log, std::vector<std::string>{"w@0"});
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
    const kirima_slot_part part{a, 0, 0, 0, 0};
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
    s.write(a, 0, 0, 0, &one);  // guard false
    s.write(a, 0, 0, 0, &zero);
    g.value = true;
    s.write(a, 0, 0, 0, &one);
    s.run();
    EXPECT_EQ(g.calls, 2);
    EXPECT_EQ(log, std::vector<std::string>{"w@0"});
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
    k::ProcessId id = 0;
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
    for (k::CounterId i = 0; i < c.size(); ++i)
        if (c.name(i) == name) return c.value(i);
    return UINT64_MAX;
}

TEST(Scheduler, AReusedPlaceGetsNoStaleWakeUp) {
    // a waits #10 and is killed at time 0; b takes its place and waits #20.
    // a's wake-up at 10 must not reach b: b runs at 0 and 20 only.
    k::Context ctx;
    k::Scheduler& s = ctx.scheduler();
    Actor a{Actor::Role::Wait, &s, 0, 10};
    Actor b{Actor::Role::Wait, &s, 0, 20};
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

}  // namespace
