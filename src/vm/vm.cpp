#include "haremasim/vm/vm.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haremasim/kernel/bits.h"
#include "haremasim/kernel/format.h"
#include "haremasim/kernel/w4ops.h"
#include "haremasim/sir/types.h"

namespace haremasim::vm {

namespace {

using kernel::W4;
using sir::Op;
using sir::Operand;

/// System tasks and functions the VM knows. Names are resolved to these
/// when the design is loaded, so nothing compares names while the design
/// runs (SIR_SPEC.md 1).
enum class SysTask : std::uint8_t { None, Display, Write, Finish, Stop, Time, Unknown };

SysTask resolveSysTask(std::string_view name) {
    if (name == "$display") return SysTask::Display;
    if (name == "$write") return SysTask::Write;
    if (name == "$finish") return SysTask::Finish;
    if (name == "$stop") return SysTask::Stop;
    if (name == "$time") return SysTask::Time;
    return SysTask::Unknown;
}

/// What the VM works out once per instruction when it loads the design.
struct InstrInfo {
    SysTask task = SysTask::None;
    /// For $display and $write: the parsed format of each text operand
    /// (empty for the others).
    std::vector<std::vector<kernel::FormatPiece>> formats;
};

kernel::Domain kernelDomain(sir::Domain d) {
    switch (d) {
        case sir::Domain::Two: return kernel::Domain::Two;
        case sir::Domain::Four: return kernel::Domain::Four;
        case sir::Domain::Real: return kernel::Domain::Real;
        case sir::Domain::Handle: return kernel::Domain::Handle;
    }
    return kernel::Domain::Four;
}

kernel::Edge kernelEdge(sir::Edge e) {
    switch (e) {
        case sir::Edge::Any: return kernel::Edge::Any;
        case sir::Edge::Pos: return kernel::Edge::Pos;
        case sir::Edge::Neg: return kernel::Edge::Neg;
        case sir::Edge::Both: return kernel::Edge::Both;
    }
    return kernel::Edge::Any;
}

/// Whether a 1-bit condition is true: a known 1 (x and z are false).
bool isTrue(W4 c) { return (c.a & ~c.b & 1U) != 0; }

/// The value of an index: nothing if it has x or z bits or does not fit.
std::optional<std::int64_t> indexValue(W4 v, sir::Type t) {
    if (v.b != 0) return std::nullopt;
    if (sir::isSigned(t)) return kernel::asSigned(v.a, t.width);
    if (v.a > static_cast<std::uint64_t>(INT64_MAX)) return std::nullopt;
    return static_cast<std::int64_t>(v.a);
}

class Interpreter;

/// The frame of a process run by the VM (RUNTIME_ABI.md 4.3): where it
/// goes on when the kernel resumes it, and its frame variables.
struct Frame {
    Interpreter* vm = nullptr;
    std::uint32_t process = 0;       ///< Index in the SIR process table.
    kernel::ProcessId kernelId = 0;  ///< Id in the kernel.
    sir::BlockId block = 0;          ///< Block to resume at.
    std::vector<W4> vars;            ///< Frame variables (SIR_SPEC.md 7.4).

    // Process control (SCHEDULER_SPEC.md 10).
    enum class Join : std::uint8_t { None, All, Any, WaitFork };
    Frame* parent = nullptr;       ///< The process that forked this one.
    std::vector<Frame*> children;  ///< Every child, finished or not.
    std::uint32_t liveChildren = 0;
    bool finished = false;
    std::uint32_t group = 0;      ///< The parent's fork group it belongs to (0: none).
    std::uint32_t forkGroup = 0;  ///< This frame's latest join or join_any group.
    std::uint32_t groupSize = 0;
    std::uint32_t groupDone = 0;
    Join pending = Join::None;               ///< What the frame waits for before going on.
    kernel::EventId joinEvent = UINT32_MAX;  ///< Triggered when a child ends.
};

class Interpreter {
public:
    Interpreter(const sir::Design& d, kernel::Context& ctx, const RunOptions& options)
        : d_(d), sched_(ctx.scheduler()), store_(ctx.store()), output_(ctx.output()),
          counters_(ctx.counters()), options_(options) {
        timeFormat_.unit = d.timePrecision;
        consts_.reserve(d.constants.size());
        for (const sir::Constant& c : d.constants)
            consts_.push_back(
                {c.aWords.empty() ? 0 : c.aWords[0], c.bWords.empty() ? 0 : c.bWords[0]});
        std::uint32_t maxTemps = 0;
        info_.resize(d.processes.size());
        tempTypes_.resize(d.processes.size());
        for (std::size_t p = 0; p < d.processes.size(); ++p) {
            const sir::Process& proc = d.processes[p];
            maxTemps = std::max(maxTemps, proc.numTemps);
            tempTypes_[p].resize(proc.numTemps);
            info_[p].resize(proc.blocks.size());
            for (std::size_t b = 0; b < proc.blocks.size(); ++b) {
                for (const sir::BlockParam& bp : proc.blocks[b].params)
                    if (bp.temp < proc.numTemps) tempTypes_[p][bp.temp] = bp.type;
                for (const sir::Instr& in : proc.blocks[b].instrs) {
                    if (in.result != sir::kNone && in.result < proc.numTemps)
                        tempTypes_[p][in.result] = in.type;
                    info_[p][b].push_back(decode(in));
                }
            }
        }
        temps_.resize(maxTemps);
    }

    RunResult run() {
        if (!loadError_.empty()) return {kernel::EndReason::Internal, loadError_};
        if (store_.size() != 0) return {kernel::EndReason::Internal, "the store is not empty"};
        for (const sir::Slot& s : d_.slots) {
            const sir::Domain dom = sir::domainOf(s.type);
            const kernel::Init init = dom != sir::Domain::Four       ? kernel::Init::Zero
                                      : s.kind == sir::SlotKind::Var ? kernel::Init::X
                                                                     : kernel::Init::Z;
            store_.add(kernelDomain(dom), s.type.width,
                       static_cast<std::uint32_t>(sir::elementCount(s)), init);
        }
        std::vector<kernel::ProcessId> combs;
        std::vector<kernel::ProcessId> finals;
        for (std::uint32_t p = 0; p < d_.processes.size(); ++p) {
            const sir::Process& proc = d_.processes[p];
            if (proc.parent.kind != sir::Owner::Kind::None) continue;  // made by fork
            Frame& f = newFrame(p);
            switch (proc.kind) {
                case sir::ProcessKind::Initial:
                case sir::ProcessKind::General:
                    f.kernelId = sched_.addProcess(&resume, &f, kernel::Group::Active);
                    sched_.start(f.kernelId);
                    break;
                case sir::ProcessKind::Ff:
                case sir::ProcessKind::FfPlain:
                    f.kernelId = sched_.addProcess(&resume, &f, kernel::Group::Active);
                    waitTriggers(f, proc);
                    break;
                case sir::ProcessKind::Comb:
                case sir::ProcessKind::Latch:
                    // Started once at time 0 after every initial and always
                    // (SCHEDULER_SPEC.md 6.1).
                    f.kernelId = sched_.addProcess(&resume, &f, kernel::Group::Active);
                    combs.push_back(f.kernelId);
                    break;
                case sir::ProcessKind::Final:
                    // Run once each when the simulation ends.
                    f.kernelId = sched_.addProcess(&resume, &f, kernel::Group::Active);
                    finals.push_back(f.kernelId);
                    break;
                default:
                    return {kernel::EndReason::Internal, "process \"" + proc.name + "\": kind " +
                                                             sir::kindName(proc.kind) +
                                                             " is not supported by the VM yet"};
            }
        }
        // After the kernel's own counters, which the first process registers.
        cInstrs_ = counters_.add("vm.instrs");
        for (const kernel::ProcessId k : combs)
            sched_.start(k);
        sched_.run();
        sched_.runFinal(finals);
        const kernel::EndReason reason = sched_.reason();
        return {reason, reason == kernel::EndReason::Internal ? sched_.error() : std::string()};
    }

    static hs_status resume(hs_context* /*ctx*/, void* frame) {
        auto* f = static_cast<Frame*>(frame);
        Interpreter& vm = *f->vm;
        std::uint64_t steps = 0;
        const kernel::Status st = vm.runProcess(*f, steps);
        vm.counters_.increment(vm.cInstrs_, steps);
        if (st == kernel::Status::Done) vm.ended(*f);
        return static_cast<hs_status>(st);
    }

private:
    const sir::Design& d_;
    kernel::Scheduler& sched_;
    kernel::Store& store_;
    kernel::Output& output_;
    kernel::Counters& counters_;
    kernel::CounterId cInstrs_ = 0;
    RunOptions options_;
    kernel::TimeFormat timeFormat_;
    std::string loadError_;
    std::vector<W4> consts_;
    std::vector<std::vector<sir::Type>> tempTypes_;          ///< [process][temp]
    std::vector<std::vector<std::vector<InstrInfo>>> info_;  ///< [process][block][instr]
    std::vector<W4> temps_;     ///< Temps never live across a wait, so one set serves all.
    std::deque<Frame> frames_;  ///< A deque, so that frames never move.
    std::vector<kernel::WaitItem> items_;  ///< Scratch for wait.event.

    InstrInfo decode(const sir::Instr& in) {
        InstrInfo info;
        if (in.op != Op::Syscall || in.operands.empty()) return info;
        info.task = resolveSysTask(in.operands[0].text);
        if (info.task != SysTask::Display && info.task != SysTask::Write) return info;
        info.formats.resize(in.operands.size());
        for (std::size_t i = 1; i < in.operands.size(); ++i) {
            if (in.operands[i].kind != Operand::Kind::Text) continue;
            std::string error;
            if (!kernel::parseFormat(in.operands[i].text, info.formats[i], error) &&
                loadError_.empty())
                loadError_ = error;
        }
        return info;
    }

    Frame& newFrame(std::uint32_t process) {
        Frame& f = frames_.emplace_back();
        f.vm = this;
        f.process = process;
        f.vars.assign(d_.processes[process].frame.size(), W4{0, 0});
        return f;
    }

    // --- Process control (SCHEDULER_SPEC.md 10) ---

    /// Bookkeeping when a process ends, normally or killed: its parent may
    /// be waiting for it at a join or wait fork.
    void ended(Frame& c) {
        if (c.finished) return;
        c.finished = true;
        Frame* parent = c.parent;
        if (parent == nullptr) return;
        --parent->liveChildren;
        if (c.group != 0 && c.group == parent->forkGroup) ++parent->groupDone;
        if (parent->pending != Frame::Join::None && !parent->finished &&
            parent->joinEvent != UINT32_MAX)
            sched_.trigger(parent->joinEvent);
    }

    static bool joined(const Frame& f) {
        switch (f.pending) {
            case Frame::Join::None: return true;
            case Frame::Join::All: return f.groupDone >= f.groupSize;
            case Frame::Join::Any: return f.groupDone >= 1 || f.groupSize == 0;
            case Frame::Join::WaitFork: return f.liveChildren == 0;
        }
        return true;
    }

    /// Waits until the frame's pending join is satisfied. Returns false if
    /// it already is, so the process goes on at once.
    bool waitJoin(Frame& f) {
        if (joined(f)) {
            f.pending = Frame::Join::None;
            return false;
        }
        if (f.joinEvent == UINT32_MAX) f.joinEvent = sched_.addEvent();
        sched_.waitNamed(f.kernelId, f.joinEvent);
        return true;
    }

    /// fork: makes a frame and a kernel process for each child, in order.
    /// They run after the parent waits or ends (9.3.2): start() queues
    /// them behind the running parent.
    void fork(Frame& f, const std::vector<Operand>& ops) {
        const std::string& join = ops[ops.size() - 2].text;
        const bool none = join == "join_none";
        if (!none) {
            ++f.forkGroup;
            f.groupDone = 0;
            f.groupSize = static_cast<std::uint32_t>(ops.size() - 2);
            f.pending = join == "join" ? Frame::Join::All : Frame::Join::Any;
        }
        for (std::size_t i = 0; i + 2 < ops.size(); ++i) {
            Frame& c = newFrame(ops[i].id);
            c.parent = &f;
            c.group = none ? 0 : f.forkGroup;
            c.kernelId = sched_.addProcess(&resume, &c, kernel::Group::Active);
            f.children.push_back(&c);
            ++f.liveChildren;
            sched_.start(c.kernelId);
        }
    }

    /// disable fork: ends every descendant, also those of children that
    /// have already finished (9.6.3).
    void disableFork(Frame& f) {
        for (Frame* c : f.children) {
            if (!c->finished) {
                sched_.kill(c->kernelId);
                ended(*c);
            }
            disableFork(*c);
        }
    }

    kernel::Status fail(const sir::Process& p, const std::string& what) {
        sched_.fail("process \"" + p.name + "\": " + what);
        return kernel::Status::Error;
    }

    // --- Values ---

    sir::Type typeOf(std::uint32_t p, const Operand& o) const {
        switch (o.kind) {
            case Operand::Kind::Temp: return tempTypes_[p][o.id];
            case Operand::Kind::Const: return d_.constants[o.id].type;
            case Operand::Kind::Frame: return d_.processes[p].frame[o.id].type;
            case Operand::Kind::Slot: return d_.slots[o.id].type;
            default: return sir::voidType();
        }
    }

    W4 value(const Frame& f, const Operand& o) const {
        switch (o.kind) {
            case Operand::Kind::Temp: return temps_[o.id];
            case Operand::Kind::Const: return consts_[o.id];
            case Operand::Kind::Frame: return f.vars[o.id];
            default: return {0, 0};
        }
    }

    W4 readSlot(sir::SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width) const {
        const kernel::SlotLayout& l = store_.layout(s);
        if (width == 0) width = l.width;
        const std::uint64_t* p = store_.element(s, elem);
        if (l.domain == kernel::Domain::Four)
            return {kernel::readField(p, 2, lsb, width), kernel::readField(p + 1, 2, lsb, width)};
        return {kernel::readField(p, 1, lsb, width), 0};
    }

    void writeSlot(sir::SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
                   W4 v) {
        const std::uint64_t buf[2] = {v.a, v.b};
        sched_.write(s, elem, lsb, width, buf);
    }

    void nbaSlot(const Frame& f, sir::SlotId s, std::uint32_t elem, std::uint32_t lsb,
                 std::uint32_t width, W4 v) {
        const std::uint64_t buf[2] = {v.a, v.b};
        sched_.scheduleNba(f.kernelId, s, elem, lsb, width, buf, 0);
    }

    /// The element number of an unpacked-array access from the index
    /// operands ops[first .. first + dims), or nothing for an invalid index
    /// (VALUE_SEMANTICS.md 3.11).
    std::optional<std::uint32_t> elementOf(const Frame& f, const sir::Slot& s,
                                           const std::vector<Operand>& ops,
                                           std::size_t first) const {
        std::uint64_t elem = 0;
        for (std::size_t d = 0; d < s.dims.size(); ++d) {
            const Operand& o = ops[first + d];
            const std::optional<std::int64_t> idx = indexValue(value(f, o), typeOf(f.process, o));
            if (!idx) return std::nullopt;
            const std::int64_t l = s.dims[d].left;
            const std::int64_t r = s.dims[d].right;
            const std::int64_t lo = l < r ? l : r;
            const std::int64_t hi = l < r ? r : l;
            if (*idx < lo || *idx > hi) return std::nullopt;
            const std::int64_t off = l <= r ? *idx - l : l - *idx;
            elem = elem * static_cast<std::uint64_t>(hi - lo + 1) + static_cast<std::uint64_t>(off);
        }
        return static_cast<std::uint32_t>(elem);
    }

    /// Writes `width` bits of v at a run-time position `pos` of an
    /// element; only the bits inside the element (VALUE_SEMANTICS.md 3.11).
    void storeBits(const Frame& f, sir::SlotId s, std::uint32_t elem, W4 posv, sir::Type post, W4 v,
                   std::uint32_t width, bool nba) {
        const std::optional<std::int64_t> pos = indexValue(posv, post);
        if (!pos) return;
        const std::int64_t ew = d_.slots[s].type.width;
        std::int64_t lo = *pos;
        std::int64_t hi = *pos + width;  // exclusive
        if (lo >= ew || hi <= 0) return;
        std::uint32_t skip = 0;
        if (lo < 0) {
            skip = static_cast<std::uint32_t>(-lo);
            lo = 0;
        }
        if (hi > ew) hi = ew;
        const auto n = static_cast<std::uint32_t>(hi - lo);
        const W4 part{(v.a >> skip) & kernel::mask(n), (v.b >> skip) & kernel::mask(n)};
        if (nba)
            nbaSlot(f, s, elem, static_cast<std::uint32_t>(lo), n, part);
        else
            writeSlot(s, elem, static_cast<std::uint32_t>(lo), n, part);
    }

    void waitTriggers(const Frame& f, const sir::Process& proc) {
        items_.clear();
        for (const sir::Trigger& t : proc.triggers) {
            kernel::WaitItem item;
            item.part = {t.range.slot, t.range.firstElem, t.range.numElems, t.range.lsb,
                         t.range.width};
            item.edge = kernelEdge(t.edge);
            items_.push_back(item);
        }
        sched_.waitEvent(f.kernelId, items_);
    }

    // --- Running ---

    /// Runs a process from where it stopped until it waits, returns or the
    /// simulation ends.
    /// Counts the instructions it runs in `steps` (vm.instrs).
    kernel::Status runProcess(Frame& f, std::uint64_t& steps) {
        const std::uint32_t pid = f.process;
        const sir::Process& p = d_.processes[pid];
        // Woken at a join or wait fork: wait again until it is satisfied.
        if (f.pending != Frame::Join::None && waitJoin(f)) return kernel::Status::Waiting;
        sir::BlockId b = f.block;
        for (;;) {
            const auto& instrs = p.blocks[b].instrs;
            bool jumped = false;
            for (std::size_t k = 0; k < instrs.size() && !jumped; ++k) {
                const sir::Instr& in = instrs[k];
                if (++steps > options_.maxSteps && options_.maxSteps != 0) {
                    sched_.requestEnd(kernel::EndReason::ZeroTimeLimit, 1);
                    return kernel::Status::Stop;
                }
                const auto& ops = in.operands;
                switch (in.op) {
                    case Op::Br:
                        b = takeBranch(f, ops, 0);
                        jumped = true;
                        break;
                    case Op::BrIf: {
                        const std::size_t second = 2 + ops[1].count;
                        b = takeBranch(f, ops, isTrue(value(f, ops[0])) ? 1 : second);
                        jumped = true;
                        break;
                    }
                    case Op::Ret:
                        switch (p.kind) {
                            case sir::ProcessKind::Comb:
                            case sir::ProcessKind::Latch:
                            case sir::ProcessKind::Ff:
                            case sir::ProcessKind::FfPlain:
                                f.block = 0;
                                waitTriggers(f, p);
                                return kernel::Status::Waiting;
                            default: return kernel::Status::Done;
                        }
                    case Op::Fork:
                    case Op::WaitFork: {
                        if (in.op == Op::Fork)
                            fork(f, ops);
                        else
                            f.pending = Frame::Join::WaitFork;
                        f.block = ops.back().id;
                        if (f.pending != Frame::Join::None && waitJoin(f))
                            return kernel::Status::Waiting;
                        b = f.block;
                        jumped = true;
                        break;
                    }
                    case Op::DisableFork: disableFork(f); break;
                    case Op::WaitDelay: {
                        const W4 t = value(f, ops[0]);
                        sched_.waitDelay(f.kernelId, t.b != 0 ? 0 : t.a);  // x or z: 0 (9.4.1)
                        f.block = ops[1].id;
                        return kernel::Status::Waiting;
                    }
                    case Op::WaitEvent: {
                        items_.clear();
                        for (std::size_t i = 0; i + 1 < ops.size(); ++i) {
                            const Operand& o = ops[i];
                            if (o.kind != Operand::Kind::Slot)
                                return fail(p, "wait.event with an iff guard is not supported "
                                               "by the VM yet");
                            kernel::WaitItem item;
                            item.part = {o.id, o.firstElem, o.numElems, o.lsb, o.width};
                            item.edge = kernelEdge(o.edge);
                            items_.push_back(item);
                        }
                        sched_.waitEvent(f.kernelId, items_);
                        f.block = ops.back().id;
                        return kernel::Status::Waiting;
                    }
                    case Op::Syscall: {
                        const InstrInfo& info = info_[pid][b][k];
                        if (!syscall(f, p, in, info)) return kernel::Status::Error;
                        if (sched_.ending()) return kernel::Status::Stop;
                        break;
                    }
                    default:
                        if (!execute(f, p, in)) return kernel::Status::Error;
                        if (sched_.ending()) return kernel::Status::Stop;
                        break;
                }
            }
            if (!jumped) return fail(p, "block bb" + std::to_string(b) + " has no terminator");
        }
    }

    /// Passes the arguments of the block operand at ops[i] to its
    /// parameters and returns the block.
    sir::BlockId takeBranch(const Frame& f, const std::vector<Operand>& ops, std::size_t i) {
        const Operand& target = ops[i];
        const auto& params = d_.processes[f.process].blocks[target.id].params;
        if (target.count != 0) {
            // Read every argument before writing any parameter.
            W4 vals[16];
            std::vector<W4> many;
            W4* v = target.count <= 16 ? vals : (many.resize(target.count), many.data());
            for (std::uint32_t a = 0; a < target.count; ++a)
                v[a] = value(f, ops[i + 1 + a]);
            for (std::uint32_t a = 0; a < target.count; ++a)
                temps_[params[a].temp] = v[a];
        }
        return target.id;
    }

    /// Runs one instruction that is not a terminator or a system call.
    bool execute(Frame& f, const sir::Process& p, const sir::Instr& in) {
        const auto& ops = in.operands;
        const sir::Type rt = in.type;
        const unsigned w = rt.width;
        const auto v = [&](std::size_t i) { return value(f, ops[i]); };
        const auto t = [&](std::size_t i) { return typeOf(f.process, ops[i]); };
        const auto set = [&](W4 r) {
            // A two-state result has no x or z (VALUE_SEMANTICS.md 1.2).
            if (sir::isIntegral(rt) && !sir::isFourState(rt)) r = kernel::to2(r);
            temps_[in.result] = r;
        };
        switch (in.op) {
            case Op::Load: {
                const Operand& o = ops[0];
                set(readSlot(o.id, o.firstElem, o.lsb, o.width));
                return true;
            }
            case Op::Store:
            case Op::Nba: {
                const Operand& o = ops[0];
                if (in.op == Op::Store)
                    writeSlot(o.id, o.firstElem, o.lsb, o.width, v(1));
                else
                    nbaSlot(f, o.id, o.firstElem, o.lsb, o.width, v(1));
                return true;
            }
            case Op::LoadElem: {
                const sir::Slot& s = d_.slots[ops[0].id];
                const std::optional<std::uint32_t> e = elementOf(f, s, ops, 1);
                // An invalid index reads the element type's default
                // (VALUE_SEMANTICS.md 3.11).
                set(e ? readSlot(ops[0].id, *e, 0, 0)
                      : (sir::isFourState(rt) ? kernel::allX(w) : W4{0, 0}));
                return true;
            }
            case Op::StoreElem:
            case Op::NbaElem: {
                const sir::Slot& s = d_.slots[ops[0].id];
                const std::optional<std::uint32_t> e = elementOf(f, s, ops, 1);
                if (!e) return true;  // an invalid index writes nothing
                if (in.op == Op::StoreElem)
                    writeSlot(ops[0].id, *e, 0, 0, v(ops.size() - 1));
                else
                    nbaSlot(f, ops[0].id, *e, 0, 0, v(ops.size() - 1));
                return true;
            }
            case Op::StoreBits:
            case Op::NbaBits: {
                const sir::Slot& s = d_.slots[ops[0].id];
                std::uint32_t e = 0;
                if (!s.dims.empty()) {
                    const std::optional<std::uint32_t> el = elementOf(f, s, ops, 1);
                    if (!el) return true;
                    e = *el;
                }
                const std::size_t n = s.dims.size();
                storeBits(f, ops[0].id, e, v(n + 1), t(n + 1), v(n + 2), t(n + 2).width,
                          in.op == Op::NbaBits);
                return true;
            }
            case Op::Fload: set(f.vars[ops[0].id]); return true;
            case Op::Fstore: f.vars[ops[0].id] = v(1); return true;
            case Op::Add: set(kernel::add4(v(0), v(1), w)); return true;
            case Op::Sub: set(kernel::sub4(v(0), v(1), w)); return true;
            case Op::Mul: set(kernel::mul4(v(0), v(1), w)); return true;
            case Op::Div: set(kernel::div4(v(0), v(1), w, sir::isSigned(rt))); return true;
            case Op::Mod: set(kernel::mod4(v(0), v(1), w, sir::isSigned(rt))); return true;
            case Op::Pow:
                set(kernel::pow4(v(0), v(1), w, sir::isSigned(rt), t(1).width,
                                 sir::isSigned(t(1))));
                return true;
            case Op::Neg: set(kernel::neg4(v(0), w)); return true;
            case Op::Lt: set(kernel::lt4(v(0), v(1), t(0).width, sir::isSigned(t(0)))); return true;
            case Op::Le: set(kernel::le4(v(0), v(1), t(0).width, sir::isSigned(t(0)))); return true;
            case Op::Gt: set(kernel::gt4(v(0), v(1), t(0).width, sir::isSigned(t(0)))); return true;
            case Op::Ge: set(kernel::ge4(v(0), v(1), t(0).width, sir::isSigned(t(0)))); return true;
            case Op::Eq: set(kernel::eq4(v(0), v(1))); return true;
            case Op::Ne: set(kernel::ne4(v(0), v(1))); return true;
            case Op::Ceq: set(kernel::ceq4(v(0), v(1))); return true;
            case Op::Cne: set(kernel::cne4(v(0), v(1))); return true;
            case Op::Weq: set(kernel::weq4(v(0), v(1))); return true;
            case Op::Wne: set(kernel::wne4(v(0), v(1))); return true;
            case Op::MatchMask:
                set(kernel::caseMatch4(v(0), v(1), ops[2].text == "casex"));
                return true;
            case Op::Lnot: set(kernel::lnot4(v(0))); return true;
            case Op::Truth: set(kernel::truth4(v(0))); return true;
            case Op::Land: set(kernel::land4(v(0), v(1))); return true;
            case Op::Lor: set(kernel::lor4(v(0), v(1))); return true;
            case Op::Limp: set(kernel::lor4(kernel::lnot4(v(0)), v(1))); return true;
            case Op::Leqv: {
                const W4 x = kernel::truth4(v(0));
                const W4 y = kernel::truth4(v(1));
                set((x.b | y.b) != 0 ? kernel::kW4X : W4{x.a == y.a ? 1U : 0U, 0});
                return true;
            }
            case Op::And: set(kernel::and4(v(0), v(1))); return true;
            case Op::Or: set(kernel::or4(v(0), v(1))); return true;
            case Op::Xor: set(kernel::xor4(v(0), v(1))); return true;
            case Op::Xnor: set(kernel::xnor4(v(0), v(1), w)); return true;
            case Op::Not: set(kernel::not4(v(0), w)); return true;
            case Op::Rand: set(kernel::redAnd4(v(0), t(0).width)); return true;
            case Op::Rnand: set(kernel::redNand4(v(0), t(0).width)); return true;
            case Op::Ror: set(kernel::redOr4(v(0))); return true;
            case Op::Rnor: set(kernel::redNor4(v(0))); return true;
            case Op::Rxor: set(kernel::redXor4(v(0))); return true;
            case Op::Rxnor: set(kernel::redXnor4(v(0))); return true;
            case Op::Shl: set(kernel::shl4(v(0), v(1), w)); return true;
            case Op::Shr: set(kernel::shr4(v(0), v(1), w)); return true;
            case Op::Ashr: set(kernel::ashr4(v(0), v(1), w, sir::isSigned(rt))); return true;
            case Op::Select: set(kernel::select4(v(0), v(1), v(2), w)); return true;
            case Op::Bitsel: {
                const std::optional<std::int64_t> pos = indexValue(v(1), t(1));
                const W4 x = v(0);
                if (!pos || *pos < 0 || *pos >= t(0).width) {
                    set(kernel::kW4X);  // to2 makes it 0 for two-state
                    return true;
                }
                const auto s = static_cast<unsigned>(*pos);
                set({(x.a >> s) & 1U, (x.b >> s) & 1U});
                return true;
            }
            case Op::Partsel: {
                const W4 x = v(0);
                const auto s = static_cast<unsigned>(consts_[ops[1].id].a);
                set({(x.a >> s) & kernel::mask(w), (x.b >> s) & kernel::mask(w)});
                return true;
            }
            case Op::IdxPartsel: {
                const std::optional<std::int64_t> pos = indexValue(v(1), t(1));
                if (!pos) {
                    set(kernel::allX(w));
                    return true;
                }
                const W4 x = v(0);
                const std::int64_t xw = t(0).width;
                W4 r{0, 0};
                for (unsigned i = 0; i < w; ++i) {
                    const std::int64_t src = *pos + i;
                    if (src < 0 || src >= xw) {
                        r.a |= std::uint64_t{1} << i;  // outside: x
                        r.b |= std::uint64_t{1} << i;
                        continue;
                    }
                    const auto s = static_cast<unsigned>(src);
                    r.a |= ((x.a >> s) & 1U) << i;
                    r.b |= ((x.b >> s) & 1U) << i;
                }
                set(r);
                return true;
            }
            case Op::Concat: {
                W4 r{0, 0};
                for (std::size_t i = 0; i < ops.size(); ++i) {
                    const unsigned ow = t(i).width;
                    const W4 x = v(i);
                    r.a = (ow >= 64 ? 0 : r.a << ow) | x.a;
                    r.b = (ow >= 64 ? 0 : r.b << ow) | x.b;
                }
                set(r);
                return true;
            }
            case Op::Repl: {
                const std::uint64_t n = v(0).a;
                const unsigned ow = t(1).width;
                const W4 x = v(1);
                W4 r{0, 0};
                for (std::uint64_t i = 0; i < n; ++i) {
                    r.a = (ow >= 64 ? 0 : r.a << ow) | x.a;
                    r.b = (ow >= 64 ? 0 : r.b << ow) | x.b;
                }
                set(r);
                return true;
            }
            case Op::Zext:
            case Op::To2:
            case Op::To4: set(v(0)); return true;
            case Op::Sext: set(kernel::sext4(v(0), t(0).width, w)); return true;
            case Op::Trunc: set(kernel::trunc4(v(0), w)); return true;
            default: break;
        }
        fail(p,
             std::string("instruction ") + sir::opName(in.op) + " is not supported by the VM yet");
        return false;
    }

    std::string formatValue(const Frame& f, const Operand& o, kernel::FormatSpec spec) const {
        const sir::Type t = typeOf(f.process, o);
        const W4 x = value(f, o);
        const std::uint64_t words[2] = {x.a, x.b};
        const kernel::IntValue iv{words, t.width, sir::isFourState(t), sir::isSigned(t)};
        if (spec.conv == 't')
            return kernel::formatTime(iv, d_.timePrecision, timeFormat_, spec.width);
        return kernel::formatInt(iv, spec);
    }

    bool syscall(Frame& f, const sir::Process& p, const sir::Instr& in, const InstrInfo& info) {
        const auto& ops = in.operands;
        switch (info.task) {
            case SysTask::Display:
            case SysTask::Write: {
                // A text operand is a format; the values after it fill its
                // specifiers, and values left over print in decimal
                // (SYSTEM_TASKS.md 3.1).
                std::string text;
                std::size_t i = 1;
                while (i < ops.size()) {
                    if (ops[i].kind != Operand::Kind::Text) {
                        text += formatValue(f, ops[i], {'d', -1});
                        ++i;
                        continue;
                    }
                    const auto& pieces = info.formats[i];
                    ++i;
                    for (const kernel::FormatPiece& piece : pieces) {
                        if (!piece.isSpec) {
                            text += piece.text;
                            continue;
                        }
                        if (i >= ops.size() || ops[i].kind == Operand::Kind::Text) {
                            fail(p, "a format specifier has no argument");
                            return false;
                        }
                        text += formatValue(f, ops[i], piece.spec);
                        ++i;
                    }
                }
                if (info.task == SysTask::Display) text += '\n';
                output_.write(text);
                return true;
            }
            case SysTask::Finish:
            case SysTask::Stop: {
                const auto level = static_cast<int>(value(f, ops[1]).a);
                sched_.requestEnd(info.task == SysTask::Finish ? kernel::EndReason::Finish
                                                               : kernel::EndReason::Stop,
                                  level);
                return true;
            }
            case SysTask::Time: {
                // The time in the caller's unit, rounded half away from zero
                // (SYSTEM_TASKS.md 5.1); the operand is 10^(unit - precision).
                const std::uint64_t div = value(f, ops[1]).a;
                const std::uint64_t now = sched_.time();
                const std::uint64_t q = now / div;
                const std::uint64_t r = now % div;
                temps_[in.result] = {q + (div > 1 && r >= div - r ? 1 : 0), 0};
                return true;
            }
            case SysTask::None:
            case SysTask::Unknown: break;
        }
        fail(p, "system task " + ops[0].text + " is not supported by the VM yet");
        return false;
    }
};

}  // namespace

RunResult run(const sir::Design& design, kernel::Context& context, const RunOptions& options) {
    return Interpreter(design, context, options).run();
}

}  // namespace haremasim::vm
