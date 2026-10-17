#include "kirimasim/vm/vm.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/format.h"
#include "kirimasim/kernel/scan.h"
#include "kirimasim/kernel/strength.h"
#include "kirimasim/kernel/udp.h"
#include "kirimasim/kernel/w4ops.h"
#include "kirimasim/sir/types.h"

namespace kirimasim::vm {

namespace {

using kernel::W4;
using sir::Op;
using sir::Operand;

/// System tasks and functions the VM knows. Names are resolved to these
/// when the design is loaded, so nothing compares names while the design
/// runs (SIR_SPEC.md 1).
enum class SysTask : std::uint8_t {
    None,
    Display,
    Write,
    Finish,
    Stop,
    Time,
    Realtime,
    Rtoi,
    RealToBits,
    BitsToReal,
    ShortRealToBits,
    BitsToShortReal,
    Math1,
    Math2,
    Clog2,
    SFormatF,
    Fatal,
    Error,
    Warning,
    Info,
    CountBits,
    TestPlusargs,
    Sscanf,
    ValuePlusargs,
    Unknown,
};

/// The operand where the format and its values start: after the scope of
/// a severity task, and after the level of $fatal (SYSTEM_TASKS.md 10.1).
std::size_t formatStart(SysTask t) {
    switch (t) {
        case SysTask::Display:
        case SysTask::Write:
        case SysTask::SFormatF: return 1;
        case SysTask::Error:
        case SysTask::Warning:
        case SysTask::Info: return 2;
        case SysTask::Fatal: return 3;
        default: return 0;
    }
}

/// The string methods (6.16), resolved from their names at load.
enum class Method : std::uint8_t {
    None,
    Len,
    Getc,
    Putc,
    ToUpper,
    ToLower,
    Compare,
    ICompare,
    Substr,
    AtoI,
    AtoHex,
    AtoOct,
    AtoBin,
    AtoReal,
    ItoA,
    HexToA,
    OctToA,
    BinToA,
    RealToA,
    Triggered,
};

Method resolveMethod(std::string_view n) {
    static constexpr std::pair<std::string_view, Method> kNames[] = {
        {"len", Method::Len},
        {"getc", Method::Getc},
        {"putc", Method::Putc},
        {"toupper", Method::ToUpper},
        {"tolower", Method::ToLower},
        {"compare", Method::Compare},
        {"icompare", Method::ICompare},
        {"substr", Method::Substr},
        {"atoi", Method::AtoI},
        {"atohex", Method::AtoHex},
        {"atooct", Method::AtoOct},
        {"atobin", Method::AtoBin},
        {"atoreal", Method::AtoReal},
        {"itoa", Method::ItoA},
        {"hextoa", Method::HexToA},
        {"octtoa", Method::OctToA},
        {"bintoa", Method::BinToA},
        {"realtoa", Method::RealToA},
        {"triggered", Method::Triggered},
    };
    for (const auto& [name, m] : kNames)
        if (name == n) return m;
    return Method::None;
}

/// The bytes of an integral value as a string (6.16): most significant
/// first, zero bytes dropped; x and z bits read as 0.
std::string integerToString(W4 v, unsigned width) {
    const std::uint64_t a = v.a & ~v.b;
    std::string s;
    for (unsigned k = (width + 7) / 8; k-- > 0;) {
        const auto c = static_cast<char>(k * 8 < 64 ? (a >> (k * 8)) & 0xffU : 0);
        if (c != 0) s.push_back(c);
    }
    return s;
}

/// A string as an integral value of `width` bits: the last character in
/// the low byte (6.16).
W4 stringToInteger(std::string_view s, unsigned width) {
    std::uint64_t a = 0;
    for (const char c : s)
        a = (a << 8) | static_cast<unsigned char>(c);
    return {a & kernel::mask(width), 0};
}

/// atoi and friends (6.16.9): digits of the base and underscores only, no
/// sign and no leading space (VALUE_SEMANTICS.md 7.6).
std::uint64_t parseDigits(std::string_view s, unsigned base) {
    std::uint64_t n = 0;
    for (const char c : s) {
        if (c == '_') continue;
        unsigned d = 0;
        if (c >= '0' && c <= '9')
            d = static_cast<unsigned>(c - '0');
        else if (c >= 'a' && c <= 'f')
            d = static_cast<unsigned>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            d = static_cast<unsigned>(c - 'A' + 10);
        else
            break;
        if (d >= base) break;
        n = n * base + d;
    }
    return n;
}

using Math1Fn = double (*)(double);
using Math2Fn = double (*)(double, double);

/// The real functions of 20.8, which call the host's C library
/// (SYSTEM_TASKS.md 8).
struct MathName {
    std::string_view name;
    Math1Fn fn1;
    Math2Fn fn2;
};
// clang-format off
constexpr MathName kMath[] = {
    {"$ln", [](double x) { return std::log(x); }, nullptr},
    {"$log10", [](double x) { return std::log10(x); }, nullptr},
    {"$exp", [](double x) { return std::exp(x); }, nullptr},
    {"$sqrt", [](double x) { return std::sqrt(x); }, nullptr},
    {"$floor", [](double x) { return std::floor(x); }, nullptr},
    {"$ceil", [](double x) { return std::ceil(x); }, nullptr},
    {"$sin", [](double x) { return std::sin(x); }, nullptr},
    {"$cos", [](double x) { return std::cos(x); }, nullptr},
    {"$tan", [](double x) { return std::tan(x); }, nullptr},
    {"$asin", [](double x) { return std::asin(x); }, nullptr},
    {"$acos", [](double x) { return std::acos(x); }, nullptr},
    {"$atan", [](double x) { return std::atan(x); }, nullptr},
    {"$sinh", [](double x) { return std::sinh(x); }, nullptr},
    {"$cosh", [](double x) { return std::cosh(x); }, nullptr},
    {"$tanh", [](double x) { return std::tanh(x); }, nullptr},
    {"$asinh", [](double x) { return std::asinh(x); }, nullptr},
    {"$acosh", [](double x) { return std::acosh(x); }, nullptr},
    {"$atanh", [](double x) { return std::atanh(x); }, nullptr},
    {"$pow", nullptr, [](double x, double y) { return std::pow(x, y); }},
    {"$atan2", nullptr, [](double x, double y) { return std::atan2(x, y); }},
    {"$hypot", nullptr, [](double x, double y) { return std::hypot(x, y); }},
};
// clang-format on

SysTask resolveSysTask(std::string_view name) {
    if (name == "$display") return SysTask::Display;
    if (name == "$write") return SysTask::Write;
    if (name == "$finish") return SysTask::Finish;
    if (name == "$stop") return SysTask::Stop;
    if (name == "$time") return SysTask::Time;
    if (name == "$realtime") return SysTask::Realtime;
    if (name == "$rtoi") return SysTask::Rtoi;
    if (name == "$realtobits") return SysTask::RealToBits;
    if (name == "$bitstoreal") return SysTask::BitsToReal;
    if (name == "$shortrealtobits") return SysTask::ShortRealToBits;
    if (name == "$bitstoshortreal") return SysTask::BitsToShortReal;
    if (name == "$clog2") return SysTask::Clog2;
    if (name == "$sformatf") return SysTask::SFormatF;
    if (name == "$fatal") return SysTask::Fatal;
    if (name == "$error") return SysTask::Error;
    if (name == "$warning") return SysTask::Warning;
    if (name == "$info") return SysTask::Info;
    if (name == "$countbits") return SysTask::CountBits;
    if (name == "$test$plusargs") return SysTask::TestPlusargs;
    if (name == "$sscanf") return SysTask::Sscanf;
    if (name == "$value$plusargs") return SysTask::ValuePlusargs;
    for (const MathName& m : kMath)
        if (m.name == name) return m.fn1 != nullptr ? SysTask::Math1 : SysTask::Math2;
    return SysTask::Unknown;
}

bool isRealType(sir::Type t) {
    return t.kind == sir::TypeKind::Real || t.kind == sir::TypeKind::ShortReal;
}

/// A real value: a double in the a word, or a float in its low 32 bits for
/// shortreal (SIR_SPEC.md 3.2, 6).
double toReal(W4 v, sir::Type t) {
    if (t.kind == sir::TypeKind::ShortReal)
        return std::bit_cast<float>(static_cast<std::uint32_t>(v.a));
    return std::bit_cast<double>(v.a);
}

W4 fromReal(double d, sir::Type t) {
    if (t.kind == sir::TypeKind::ShortReal)
        return {std::bit_cast<std::uint32_t>(static_cast<float>(d)), 0};
    return {std::bit_cast<std::uint64_t>(d), 0};
}

/// Integer to real (VALUE_SEMANTICS.md 1.5): x and z bits are 0.
double integerToReal(W4 v, sir::Type t) {
    const std::uint64_t a = v.a & ~v.b;
    if (sir::isSigned(t)) return static_cast<double>(kernel::asSigned(a, t.width));
    return static_cast<double>(a);
}

/// Real to a 64-bit integer, rounded (1.5) or truncated ($rtoi, 20.5); NaN
/// and values outside 64 bits give 0 (7.4).
std::uint64_t realToInteger(double d, bool truncate) {
    if (std::isnan(d) || d >= 9223372036854775808.0 || d < -9223372036854775808.0) return 0;
    return static_cast<std::uint64_t>(
        static_cast<std::int64_t>(truncate ? std::trunc(d) : std::round(d)));
}

/// The truth of a value as a 1-bit W4 (3.5, 7.5: a real is true unless 0.0).
W4 truthOf(W4 v, sir::Type t) {
    if (isRealType(t)) return {toReal(v, t) != 0.0 ? 1U : 0U, 0};
    return kernel::truth4(v);
}

/// What the VM works out once per instruction when it loads the design.
struct InstrInfo {
    SysTask task = SysTask::None;
    Method method = Method::None;
    Math1Fn fn1 = nullptr;
    Math2Fn fn2 = nullptr;
    /// For $display and $write: the parsed format of each text operand
    /// (empty for the others).
    std::vector<std::vector<kernel::FormatPiece>> formats;
    kernel::NetKind netKind = kernel::NetKind::Wire;  ///< For resolve.
    std::uint32_t driver = 0;                         ///< For drive.delay: its kernel driver.
};

/// The kind of net a resolve instruction names (SIR_SPEC.md 3.5).
std::optional<kernel::NetKind> netKindOf(std::string_view name) {
    static constexpr std::pair<std::string_view, kernel::NetKind> kKinds[] = {
        {"wire", kernel::NetKind::Wire},       {"tri", kernel::NetKind::Tri},
        {"wand", kernel::NetKind::Wand},       {"triand", kernel::NetKind::Triand},
        {"wor", kernel::NetKind::Wor},         {"trior", kernel::NetKind::Trior},
        {"tri0", kernel::NetKind::Tri0},       {"tri1", kernel::NetKind::Tri1},
        {"supply0", kernel::NetKind::Supply0}, {"supply1", kernel::NetKind::Supply1},
        {"uwire", kernel::NetKind::Uwire}};
    for (const auto& [n, k] : kKinds)
        if (n == name) return k;
    return std::nullopt;
}

kernel::Domain kernelDomain(sir::Domain d) {
    switch (d) {
        case sir::Domain::Two: return kernel::Domain::Two;
        case sir::Domain::Four: return kernel::Domain::Four;
        case sir::Domain::Real: return kernel::Domain::Real;
        case sir::Domain::Handle: return kernel::Domain::Handle;
    }
    return kernel::Domain::Four;
}

kernel::Init kernelInit(sir::InitValue v) {
    switch (v) {
        case sir::InitValue::Zero: return kernel::Init::Zero;
        case sir::InitValue::One: return kernel::Init::One;
        case sir::InitValue::X: return kernel::Init::X;
        case sir::InitValue::Z: return kernel::Init::Z;
    }
    return kernel::Init::X;
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

/// A time in the form of messages (SYSTEM_TASKS.md 10.1): a whole number in
/// the unit of the global precision, then the unit, with no space: 15000ps.
std::string messageTime(std::uint64_t ticks, int precision) {
    static constexpr const char* kUnits[] = {"s", "ms", "us", "ns", "ps", "fs"};
    int base = precision;
    while (base % 3 != 0)
        --base;
    std::string text = std::to_string(ticks);
    if (ticks != 0) text.append(static_cast<std::size_t>(precision - base), '0');
    const int index = -base / 3;
    text += index >= 0 && index < 6 ? kUnits[index] : "?";
    return text;
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

/// v (an element of `ew` bits) with `width` bits of `part` written at the
/// run-time position posv: only the bits inside the element
/// (VALUE_SEMANTICS.md 3.11).
W4 mergeBits(W4 v, std::uint32_t ew, W4 posv, sir::Type post, W4 part, std::uint32_t width) {
    const std::optional<std::int64_t> pos = indexValue(posv, post);
    if (!pos) return v;
    std::int64_t lo = *pos;
    std::int64_t hi = *pos + width;  // exclusive
    if (lo >= static_cast<std::int64_t>(ew) || hi <= 0) return v;
    std::uint32_t skip = 0;
    if (lo < 0) {
        skip = static_cast<std::uint32_t>(-lo);
        lo = 0;
    }
    if (hi > static_cast<std::int64_t>(ew)) hi = ew;
    const std::uint64_t m = kernel::mask(static_cast<unsigned>(hi - lo)) << lo;
    v.a = (v.a & ~m) | (((part.a >> skip) << lo) & m);
    v.b = (v.b & ~m) | (((part.b >> skip) << lo) & m);
    return v;
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
    std::uint32_t varsId = 0;        ///< Their number for refs (Interpreter::enroll).

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
    /// An override process: whether it holds a target and so runs when what
    /// it reads changes (SIR_SPEC.md 5.6), and at which level.
    bool holding = false;
    kernel::Hold level = kernel::Hold::Assign;

    /// Calls of tasks that may wait, innermost last (SIR_SPEC.md 7.4): each
    /// keeps its frame variables and the block to go on at. Their temps do
    /// not live across the call.task, so they start at 0.
    struct TaskCall {
        std::uint32_t body = 0;
        std::vector<W4> vars;
        std::uint32_t varsId = 0;
        sir::BlockId block = 0;
        const sir::Instr* site = nullptr;  ///< The call.task in the caller.
    };
    std::vector<TaskCall> calls;
};

class Interpreter {
public:
    Interpreter(const sir::Design& d, kernel::Context& ctx, const RunOptions& options)
        : d_(d), sched_(ctx.scheduler()), store_(ctx.store()), output_(ctx.output()),
          strings_(ctx.strings()), counters_(ctx.counters()), options_(options) {
        timeFormat_.unit = d.timePrecision;
        consts_.reserve(d.constants.size());
        for (const sir::Constant& c : d.constants) {
            if (c.type.kind == sir::TypeKind::String) {
                consts_.push_back({strings_.intern(c.bytes), 0});
                continue;
            }
            consts_.push_back(
                {c.aWords.empty() ? 0 : c.aWords[0], c.bWords.empty() ? 0 : c.bWords[0]});
        }
        // Bodies: processes first, then subroutines (bodyBlocks).
        const std::size_t bodies = d.processes.size() + d.subroutines.size();
        info_.resize(bodies);
        tempTypes_.resize(bodies);
        // The UDP tables, compiled once (VALUE_SEMANTICS.md 5.9).
        udpTables_.resize(d.types.size());
        for (std::size_t i = 0; i < d.types.size(); ++i) {
            const sir::TypeEntry& te = d.types[i];
            if (te.kind != sir::TypeEntryKind::Udp) continue;
            udpTables_[i].inputs = te.inputs;
            for (const sir::UdpRow& r : te.rows)
                udpTables_[i].rows.push_back(kernel::compileUdpRow(r.inputs, r.state, r.output));
        }
        layouts_.resize(bodies);
        for (std::uint32_t p = 0; p < bodies; ++p) {
            const auto& frame = bodyFrame(p);
            Layout& l = layouts_[p];
            l.size = frame.size();
            l.elements.assign(frame.size(), 0);
            for (std::size_t i = 0; i < frame.size(); ++i) {
                if (frame[i].dims.empty() || frame[i].type.kind == sir::TypeKind::Ref) continue;
                l.elements[i] = l.size;
                l.size += sir::elementCount(frame[i]);
            }
        }
        for (std::uint32_t p = 0; p < bodies; ++p) {
            const auto& blocks = bodyBlocks(p);
            const std::uint32_t n = bodyTemps(p);
            tempTypes_[p].resize(n);
            info_[p].resize(blocks.size());
            for (std::size_t b = 0; b < blocks.size(); ++b) {
                for (const sir::BlockParam& bp : blocks[b].params)
                    if (bp.temp < n) tempTypes_[p][bp.temp] = bp.type;
                for (const sir::Instr& in : blocks[b].instrs) {
                    if (in.result != sir::kNone && in.result < n)
                        tempTypes_[p][in.result] = in.type;
                    info_[p][b].push_back(decode(in));
                    // Each drive.delay keeps its own pending update
                    // (SIR_SPEC.md 5.4).
                    if (in.op == Op::DriveDelay)
                        info_[p][b].back().driver = sched_.addDelayedDriver();
                }
            }
        }
    }

    RunResult run() {
        if (!loadError_.empty()) return {kernel::EndReason::Internal, loadError_};
        if (store_.size() != 0) return {kernel::EndReason::Internal, "the store is not empty"};
        for (const sir::Slot& s : d_.slots) {
            const sir::Domain dom = sir::domainOf(s.type);
            store_.add(kernelDomain(dom), s.type.width,
                       static_cast<std::uint32_t>(sir::elementCount(s)),
                       dom == sir::Domain::Four ? kernelInit(sir::initialValue(s.kind))
                                                : kernel::Init::Zero);
        }
        // Every element of an event slot holds its own event from the
        // start; the handle is the event's number plus 1, 0 being null
        // (SIR_SPEC.md 3.2.1).
        for (sir::SlotId s = 0; s < d_.slots.size(); ++s) {
            if (d_.slots[s].type.kind != sir::TypeKind::Event) continue;
            const auto n = static_cast<std::uint32_t>(sir::elementCount(d_.slots[s]));
            for (std::uint32_t e = 0; e < n; ++e)
                *store_.element(s, e) = std::uint64_t{sched_.addEvent()} + 1;
        }
        // The slots assign and force may hold (SCHEDULER_SPEC.md 14.4). A
        // net, or a variable that a continuous assignment drives, gets its
        // drivers' value back when released.
        std::vector<bool> driven(d_.slots.size(), false);
        for (const sir::Process& proc : d_.processes)
            if (proc.kind == sir::ProcessKind::Cont)
                for (const sir::SlotRange& w : proc.writes)
                    driven[w.slot] = true;
        for (sir::SlotId s = 0; s < d_.slots.size(); ++s)
            if ((d_.slots[s].flags & sir::kForceTarget) != 0)
                sched_.markHeld(s, d_.slots[s].kind != sir::SlotKind::Var, driven[s]);
        // Static initializers first, in process order, run on the spot
        // rather than scheduled (SIR_SPEC.md 7.1, SCHEDULER_SPEC.md 6.1).
        std::uint64_t initSteps = 0;
        for (std::uint32_t p = 0; p < d_.processes.size(); ++p) {
            if (d_.processes[p].kind != sir::ProcessKind::StaticInit) continue;
            Frame& f = newFrame(p);
            const kernel::Status st = runProcess(f, initSteps);
            if (st == kernel::Status::Error) return {kernel::EndReason::Internal, sched_.error()};
        }
        std::vector<kernel::ProcessId> combs;
        std::vector<kernel::ProcessId> finals;
        overrides_.assign(d_.processes.size(), nullptr);
        for (std::uint32_t p = 0; p < d_.processes.size(); ++p) {
            const sir::Process& proc = d_.processes[p];
            if (proc.parent.kind != sir::Owner::Kind::None) continue;  // made by fork
            if (proc.kind == sir::ProcessKind::StaticInit) continue;   // already run
            Frame& f = newFrame(p);
            switch (proc.kind) {
                case sir::ProcessKind::Initial:
                case sir::ProcessKind::General:
                // A continuous assignment is evaluated once at time 0, in
                // Active like the procedures (SCHEDULER_SPEC.md 6.1).
                case sir::ProcessKind::Cont:
                    f.kernelId = sched_.addProcess(&resume, &f, kernel::Group::Active);
                    sched_.start(f.kernelId);
                    break;
                case sir::ProcessKind::Ff:
                case sir::ProcessKind::FfPlain:
                // always @* does not run at time 0 (SCHEDULER_SPEC.md 6.2).
                case sir::ProcessKind::CombAt:
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
                case sir::ProcessKind::Override:
                    // Runs from override.start to override.stop.
                    f.kernelId = sched_.addProcess(&resume, &f, kernel::Group::Active);
                    overrides_[p] = &f;
                    break;
                default:
                    return {kernel::EndReason::Internal, "process \"" + proc.name + "\": kind " +
                                                             sir::kindName(proc.kind) +
                                                             " is not supported by the VM yet"};
            }
        }
        // After the kernel's own counters, which the first process registers.
        cInstrs_ = counters_.add("vm.instrs");
        counters_.increment(cInstrs_, initSteps);
        for (const kernel::ProcessId k : combs)
            sched_.start(k);
        sched_.run();
        sched_.runFinal(finals);
        const kernel::EndReason reason = sched_.reason();
        return {reason, reason == kernel::EndReason::Internal ? sched_.error() : std::string()};
    }

    static kirima_status resume(kirima_context* /*ctx*/, void* frame) {
        auto* f = static_cast<Frame*>(frame);
        Interpreter& vm = *f->vm;
        std::uint64_t steps = 0;
        const kernel::Status st = vm.runProcess(*f, steps);
        vm.counters_.increment(vm.cInstrs_, steps);
        if (st == kernel::Status::Done) vm.ended(*f);
        return static_cast<kirima_status>(st);
    }

private:
    const sir::Design& d_;
    kernel::Scheduler& sched_;
    kernel::Store& store_;
    kernel::Output& output_;
    kernel::Strings& strings_;
    kernel::Counters& counters_;
    kernel::CounterId cInstrs_ = 0;
    RunOptions options_;
    kernel::TimeFormat timeFormat_;
    std::string loadError_;
    std::vector<W4> consts_;
    std::vector<std::vector<sir::Type>> tempTypes_;          ///< [process][temp]
    std::vector<std::vector<std::vector<InstrInfo>>> info_;  ///< [process][block][instr]
    /// The temps of the running body and of the calls below it. Temps never
    /// live across a wait (SIR_SPEC.md 5.1), so a process starts at 0.
    std::vector<W4> tempStack_;
    /// The body being run: a process or a called subroutine, its frame
    /// variables and where its temps start.
    struct Act {
        std::uint32_t body = 0;
        std::vector<W4>* vars = nullptr;
        std::size_t base = 0;
        std::uint32_t varsId = 0;
    };
    /// Where the frame variables of a body lie (RUNTIME_ABI.md 4.3): a word
    /// pair for each, by number, then the elements of each array in order.
    struct Layout {
        std::size_t size = 0;
        std::vector<std::size_t> elements;  ///< By frame variable: an array's first element.
    };
    std::vector<Layout> layouts_;              ///< By body.
    std::vector<kernel::UdpTable> udpTables_;  ///< By type-table row.
    /// The frame variables of each live activation, by number, for refs
    /// (SIR_SPEC.md 8): a ref to a frame variable is its activation's
    /// number and the word's place in it.
    std::vector<W4*> varSets_;
    std::vector<std::uint32_t> freeVarSets_;
    Act act_;
    std::uint32_t callDepth_ = 0;
    bool warnedRepl_ = false;  ///< The warning of VALUE_SEMANTICS.md 7.3 is given once.

    std::uint32_t processCount() const { return static_cast<std::uint32_t>(d_.processes.size()); }
    bool isSubBody(std::uint32_t body) const { return body >= processCount(); }
    const sir::Subroutine& subOf(std::uint32_t body) const {
        return d_.subroutines[body - processCount()];
    }
    const std::vector<sir::BasicBlock>& bodyBlocks(std::uint32_t body) const {
        return isSubBody(body) ? subOf(body).blocks : d_.processes[body].blocks;
    }
    const std::vector<sir::FrameVar>& bodyFrame(std::uint32_t body) const {
        return isSubBody(body) ? subOf(body).frame : d_.processes[body].frame;
    }
    std::uint32_t bodyTemps(std::uint32_t body) const {
        return isSubBody(body) ? subOf(body).numTemps : d_.processes[body].numTemps;
    }
    W4& temp(sir::TempId id) { return tempStack_[act_.base + id]; }
    /// Enters a body: its temps above the current ones.
    void enter(Act a) {
        act_ = a;
        if (tempStack_.size() < a.base + bodyTemps(a.body))
            tempStack_.resize(a.base + bodyTemps(a.body));
    }
    std::deque<Frame> frames_;                     ///< A deque, so that frames never move.
    std::vector<kernel::WaitItem> items_;          ///< Scratch for wait.event.
    std::vector<kernel::StrengthDriver> drivers_;  ///< Scratch for resolve.
    std::vector<kernel::ScanValue> scanValues_;    ///< Scratch for $sscanf.
    std::vector<Frame*> overrides_;                ///< By process: an override process's frame.

    InstrInfo decode(const sir::Instr& in) {
        InstrInfo info;
        if (in.op == Op::Resolve && !in.operands.empty()) {
            const std::optional<kernel::NetKind> k = netKindOf(in.operands[0].text);
            if (!k && loadError_.empty()) loadError_ = "resolve of net kind " + in.operands[0].text;
            info.netKind = k.value_or(kernel::NetKind::Wire);
            return info;
        }
        if (in.op == Op::Builtin && !in.operands.empty()) {
            info.method = resolveMethod(in.operands[0].text);
            if (info.method == Method::None && loadError_.empty())
                loadError_ = "method " + in.operands[0].text + " is not supported by the VM yet";
            return info;
        }
        if (in.op != Op::Syscall || in.operands.empty()) return info;
        info.task = resolveSysTask(in.operands[0].text);
        for (const MathName& m : kMath) {
            if (m.name != in.operands[0].text) continue;
            info.fn1 = m.fn1;
            info.fn2 = m.fn2;
        }
        const std::size_t start = formatStart(info.task);
        if (start == 0) return info;
        info.formats.resize(in.operands.size());
        for (std::size_t i = start; i < in.operands.size(); ++i) {
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
        f.vars = newVars(process);
        f.varsId = enroll(f.vars);
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

    sir::Type typeOf(const Operand& o) const {
        switch (o.kind) {
            case Operand::Kind::Temp: return tempTypes_[act_.body][o.id];
            case Operand::Kind::Const: return d_.constants[o.id].type;
            case Operand::Kind::Frame: return bodyFrame(act_.body)[o.id].type;
            case Operand::Kind::Slot: return d_.slots[o.id].type;
            default: return sir::voidType();
        }
    }

    W4 value(const Operand& o) const {
        switch (o.kind) {
            case Operand::Kind::Temp: return tempStack_[act_.base + o.id];
            case Operand::Kind::Const: return consts_[o.id];
            case Operand::Kind::Frame: return (*act_.vars)[o.id];
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
                 std::uint32_t width, W4 v, std::uint64_t delay = 0) {
        const std::uint64_t buf[2] = {v.a, v.b};
        sched_.scheduleNba(f.kernelId, s, elem, lsb, width, buf, delay);
    }

    /// The element number of an unpacked-array access from the index
    /// operands ops[first .. first + dims), or nothing for an invalid index
    /// (VALUE_SEMANTICS.md 3.11).
    std::optional<std::uint32_t> elementOf(const std::vector<sir::ArrayDim>& dims,
                                           const std::vector<Operand>& ops,
                                           std::size_t first) const {
        std::uint64_t elem = 0;
        for (std::size_t d = 0; d < dims.size(); ++d) {
            const Operand& o = ops[first + d];
            const std::optional<std::int64_t> idx = indexValue(value(o), typeOf(o));
            if (!idx) return std::nullopt;
            const std::int64_t l = dims[d].left;
            const std::int64_t r = dims[d].right;
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
        const sir::Process& p = d_.processes[f.process];
        // Woken at a join or wait fork: wait again until it is satisfied.
        if (f.pending != Frame::Join::None && waitJoin(f)) return kernel::Status::Waiting;
        W4 ret{0, 0};
        for (;;) {
            Exit e = Exit::Ret;
            if (f.calls.empty()) {
                enter({f.process, &f.vars, 0, f.varsId});
                e = runBody(f, f.block, steps, ret);
            } else {
                Frame::TaskCall& c = f.calls.back();
                enter({c.body, &c.vars, 0, c.varsId});
                e = runBody(f, c.block, steps, ret);
            }
            if (e == Exit::Entered) continue;  // a task was called
            if (e != Exit::Ret) return static_cast<kernel::Status>(e);
            if (f.calls.empty()) break;
            returnFromTask(f);  // and go on in its caller
        }
        switch (p.kind) {
            case sir::ProcessKind::Cont:
            case sir::ProcessKind::Comb:
            case sir::ProcessKind::Latch:
            case sir::ProcessKind::Ff:
            case sir::ProcessKind::FfPlain:
            case sir::ProcessKind::CombAt:
                f.block = 0;
                waitTriggers(f, p);
                return kernel::Status::Waiting;
            case sir::ProcessKind::Override:
                // Idle once it holds nothing, until override.start again.
                f.block = 0;
                if (f.holding) waitTriggers(f, p);
                return kernel::Status::Waiting;
            default: return kernel::Status::Done;
        }
    }

    /// How running a body stops: like kernel::Status, or at its ret.
    enum class Exit : std::uint8_t {
        Waiting = static_cast<std::uint8_t>(kernel::Status::Waiting),
        Error = static_cast<std::uint8_t>(kernel::Status::Error),
        Stop = static_cast<std::uint8_t>(kernel::Status::Stop),
        Entered = 0xfe,  ///< call.task made a new innermost call.
        Ret = 0xff,
    };

    /// Where the innermost activation of a frame goes on after a wait.
    static sir::BlockId& resumeAt(Frame& f) {
        return f.calls.empty() ? f.block : f.calls.back().block;
    }

    /// The end of a task called with call.task: outputs and inouts back to
    /// the caller's places (4.9.7), then the call is gone.
    void returnFromTask(Frame& f) {
        Frame::TaskCall c = std::move(f.calls.back());
        f.calls.pop_back();
        const sir::Subroutine& s = subOf(c.body);
        const std::vector<std::vector<W4>> outs = readOuts(c.body, s, c.vars);
        retire(c.varsId);
        if (f.calls.empty())
            writeOuts(s, c.site->operands, outs, f.vars, f.process);
        else
            writeOuts(s, c.site->operands, outs, f.calls.back().vars, f.calls.back().body);
    }

    /// call.task: copies the inputs in and makes the call the innermost
    /// one; the caller goes on at the block the instruction names.
    void enterTask(Frame& f, const sir::Instr& in) {
        const auto& ops = in.operands;
        const sir::Subroutine& s = d_.subroutines[ops[0].id];
        Frame::TaskCall c;
        c.body = processCount() + ops[0].id;
        c.site = &in;
        c.vars = newVars(c.body);
        bindArgs(c.body, s, readArgs(s, ops), c.vars);
        c.varsId = enroll(c.vars);
        resumeAt(f) = ops.back().id;
        f.calls.push_back(std::move(c));
    }

    // --- Frames and arguments (SIR_SPEC.md 7.4, 8) ---

    /// The frame variables of a new activation of a body: a subroutine's at
    /// their defaults, a process's at 0 until its declarations run; array
    /// elements at their defaults.
    std::vector<W4> newVars(std::uint32_t body) const {
        const auto& frame = bodyFrame(body);
        const Layout& l = layouts_[body];
        std::vector<W4> vars(l.size, W4{0, 0});
        for (std::size_t i = 0; i < frame.size(); ++i) {
            if (isSubBody(body)) vars[i] = defaultValue(frame[i].type);
            if (frame[i].dims.empty() || frame[i].type.kind == sir::TypeKind::Ref) continue;
            const std::uint64_t n = sir::elementCount(frame[i]);
            for (std::uint64_t e = 0; e < n; ++e)
                vars[l.elements[i] + e] = defaultValue(frame[i].type);
        }
        return vars;
    }

    /// Gives an activation's frame variables a number for refs.
    std::uint32_t enroll(std::vector<W4>& vars) {
        if (freeVarSets_.empty()) {
            varSets_.push_back(vars.data());
            return static_cast<std::uint32_t>(varSets_.size() - 1);
        }
        const std::uint32_t id = freeVarSets_.back();
        freeVarSets_.pop_back();
        varSets_[id] = vars.data();
        return id;
    }
    void retire(std::uint32_t id) {
        varSets_[id] = nullptr;
        freeVarSets_.push_back(id);
    }

    /// Element e of array frame variable fv of the running body.
    W4& frameElem(sir::FrameVarId fv, std::uint64_t e) {
        return (*act_.vars)[layouts_[act_.body].elements[fv] + e];
    }

    static bool isArrayParam(const sir::Subroutine& s, const sir::Param& p, const sir::Design& d) {
        return p.frameVar != sir::kNone ? !s.frame[p.frameVar].dims.empty() &&
                                              s.frame[p.frameVar].type.kind != sir::TypeKind::Ref
                                        : !d.slots[p.slot].dims.empty();
    }
    static bool isRefParam(const sir::Param& p) {
        return p.dir == sir::Direction::Ref || p.dir == sir::Direction::ConstRef;
    }

    /// The elements of an array argument, read in the running body: a run
    /// of elements of a slot, or an array frame variable.
    std::vector<W4> readArray(const Operand& o) {
        std::vector<W4> out;
        if (o.kind == Operand::Kind::Slot) {
            const std::uint64_t n =
                o.numElems != 0 ? o.numElems : sir::elementCount(d_.slots[o.id]);
            for (std::uint64_t e = 0; e < n; ++e)
                out.push_back(readSlot(o.id, static_cast<std::uint32_t>(o.firstElem + e), 0, 0));
            return out;
        }
        const std::uint64_t n = sir::elementCount(bodyFrame(act_.body)[o.id]);
        for (std::uint64_t e = 0; e < n; ++e)
            out.push_back(frameElem(o.id, e));
        return out;
    }

    /// What each argument passes in, read in the caller before anything is
    /// written: a ref, an array's elements, or a value (none for an
    /// output).
    std::vector<std::vector<W4>> readArgs(const sir::Subroutine& s,
                                          const std::vector<Operand>& ops) {
        std::vector<std::vector<W4>> args(s.params.size());
        for (std::size_t i = 0; i < s.params.size(); ++i) {
            const sir::Param& prm = s.params[i];
            const Operand& o = ops[1 + i];
            if (isRefParam(prm))
                args[i].push_back(value(o));
            else if (prm.dir == sir::Direction::Output)
                continue;
            else if (isArrayParam(s, prm, d_))
                args[i] = readArray(o);
            else
                args[i].push_back(o.kind == Operand::Kind::Slot
                                      ? readSlot(o.id, o.firstElem, o.lsb, o.width)
                                      : value(o));
        }
        return args;
    }

    /// Copies the arguments into the callee's formals (4.9.7). A static
    /// output keeps its value.
    void bindArgs(std::uint32_t body, const sir::Subroutine& s,
                  const std::vector<std::vector<W4>>& args, std::vector<W4>& vars) {
        for (std::size_t i = 0; i < s.params.size(); ++i) {
            const sir::Param& prm = s.params[i];
            if (args[i].empty()) continue;  // an output
            if (prm.frameVar == sir::kNone) {
                for (std::size_t e = 0; e < args[i].size(); ++e)
                    writeSlot(prm.slot, static_cast<std::uint32_t>(e), 0, 0, args[i][e]);
            } else if (isArrayParam(s, prm, d_)) {
                for (std::size_t e = 0; e < args[i].size(); ++e)
                    vars[layouts_[body].elements[prm.frameVar] + e] = args[i][e];
            } else {
                vars[prm.frameVar] = args[i][0];
            }
        }
    }

    /// The values of the outputs and inouts at the callee's end.
    std::vector<std::vector<W4>> readOuts(std::uint32_t body, const sir::Subroutine& s,
                                          const std::vector<W4>& vars) const {
        std::vector<std::vector<W4>> outs(s.params.size());
        for (std::size_t i = 0; i < s.params.size(); ++i) {
            const sir::Param& prm = s.params[i];
            if (prm.dir == sir::Direction::Input || isRefParam(prm)) continue;
            if (!isArrayParam(s, prm, d_)) {
                outs[i].push_back(prm.frameVar != sir::kNone ? vars[prm.frameVar]
                                                             : readSlot(prm.slot, 0, 0, 0));
                continue;
            }
            if (prm.frameVar == sir::kNone) {
                const std::uint64_t n = sir::elementCount(d_.slots[prm.slot]);
                for (std::uint64_t e = 0; e < n; ++e)
                    outs[i].push_back(readSlot(prm.slot, static_cast<std::uint32_t>(e), 0, 0));
            } else {
                const std::uint64_t n = sir::elementCount(s.frame[prm.frameVar]);
                for (std::uint64_t e = 0; e < n; ++e)
                    outs[i].push_back(vars[layouts_[body].elements[prm.frameVar] + e]);
            }
        }
        return outs;
    }

    /// Copies the outputs and inouts back to the caller's places like
    /// blocking assignments (4.9.7).
    void writeOuts(const sir::Subroutine& s, const std::vector<Operand>& ops,
                   const std::vector<std::vector<W4>>& outs, std::vector<W4>& callerVars,
                   std::uint32_t callerBody) {
        for (std::size_t i = 0; i < s.params.size(); ++i) {
            if (outs[i].empty()) continue;
            const Operand& o = ops[1 + i];
            if (!isArrayParam(s, s.params[i], d_)) {
                if (o.kind == Operand::Kind::Slot)
                    writeSlot(o.id, o.firstElem, o.lsb, o.width, outs[i][0]);
                else
                    callerVars[o.id] = outs[i][0];
                continue;
            }
            for (std::size_t e = 0; e < outs[i].size(); ++e) {
                if (o.kind == Operand::Kind::Slot)
                    writeSlot(o.id, static_cast<std::uint32_t>(o.firstElem + e), 0, 0, outs[i][e]);
                else
                    callerVars[layouts_[callerBody].elements[o.id] + e] = outs[i][e];
            }
        }
    }

    // --- References (SIR_SPEC.md 8) ---

    /// A ref is a frame variable's word or element (tag 1: the activation's
    /// number in the high half of a, the word's place in the low half), or
    /// an element of a slot (the slot in a, the element in b above the
    /// tag).
    static W4 frameRef(std::uint32_t vars, std::size_t at) {
        return {(std::uint64_t{vars} << 32) | static_cast<std::uint64_t>(at), 1};
    }
    static W4 slotRef(sir::SlotId s, std::uint32_t e) { return {s, std::uint64_t{e} << 1}; }
    static bool isFrameRef(W4 r) { return (r.b & 1U) != 0; }
    W4& refWord(W4 r, std::uint64_t off) { return varSets_[r.a >> 32][(r.a & 0xffffffffU) + off]; }
    static sir::SlotId refSlot(W4 r) { return static_cast<sir::SlotId>(r.a); }
    static std::uint32_t refElem(W4 r) { return static_cast<std::uint32_t>(r.b >> 1); }

    /// Runs the current body (act_) from block b until a wait, its ret, an
    /// error or the end of the simulation. A ret's value goes to `ret`.
    Exit runBody(Frame& f, sir::BlockId b, std::uint64_t& steps, W4& ret) {
        const sir::Process& p = d_.processes[f.process];
        const std::uint32_t body = act_.body;
        for (;;) {
            const auto& instrs = bodyBlocks(body)[b].instrs;
            bool jumped = false;
            for (std::size_t k = 0; k < instrs.size() && !jumped; ++k) {
                const sir::Instr& in = instrs[k];
                if (++steps > options_.maxSteps && options_.maxSteps != 0) {
                    const std::string n = std::to_string(options_.maxSteps);
                    std::string note = "process \"";
                    note += p.name;
                    note += "\" ran ";
                    note += n;
                    note += " instructions without waiting (--max-steps=";
                    note += n;
                    note += ")";
                    sched_.setEndNote(std::move(note));
                    sched_.requestEnd(kernel::EndReason::ZeroTimeLimit, 1);
                    return Exit::Stop;
                }
                const auto& ops = in.operands;
                switch (in.op) {
                    case Op::Br:
                        b = takeBranch(f, ops, 0);
                        jumped = true;
                        break;
                    case Op::BrIf: {
                        const std::size_t second = 2 + ops[1].count;
                        b = takeBranch(f, ops, isTrue(value(ops[0])) ? 1 : second);
                        jumped = true;
                        break;
                    }
                    case Op::Ret:
                        if (!ops.empty()) ret = value(ops[0]);
                        return Exit::Ret;
                    case Op::CallTask: enterTask(f, in); return Exit::Entered;
                    case Op::Call: {
                        const Exit e = call(f, in, steps);
                        if (e != Exit::Ret) return e;
                        if (sched_.ending()) return Exit::Stop;
                        break;
                    }
                    case Op::Fork:
                    case Op::WaitFork: {
                        if (in.op == Op::Fork)
                            fork(f, ops);
                        else
                            f.pending = Frame::Join::WaitFork;
                        resumeAt(f) = ops.back().id;
                        if (f.pending != Frame::Join::None && waitJoin(f)) return Exit::Waiting;
                        b = resumeAt(f);
                        jumped = true;
                        break;
                    }
                    case Op::DisableFork: disableFork(f); break;
                    case Op::WaitDelay: {
                        const W4 t = value(ops[0]);
                        sched_.waitDelay(f.kernelId, t.b != 0 ? 0 : t.a);  // x or z: 0 (9.4.1)
                        resumeAt(f) = ops[1].id;
                        return Exit::Waiting;
                    }
                    case Op::WaitEvent: {
                        items_.clear();
                        for (std::size_t i = 0; i + 1 < ops.size(); ++i) {
                            const Operand& o = ops[i];
                            if (o.kind != Operand::Kind::Slot) {
                                fail(p, "wait.event with an iff guard is not supported by the "
                                        "VM yet");
                                return Exit::Error;
                            }
                            kernel::WaitItem item;
                            item.part = {o.id, o.firstElem, o.numElems, o.lsb, o.width};
                            item.edge = kernelEdge(o.edge);
                            items_.push_back(item);
                        }
                        sched_.waitEvent(f.kernelId, items_);
                        resumeAt(f) = ops.back().id;
                        return Exit::Waiting;
                    }
                    case Op::WaitNamed: {
                        const std::optional<kernel::EventId> e = eventOf(ops[0]);
                        if (!e) {
                            fail(p, "wait on a null event");
                            return Exit::Error;
                        }
                        sched_.waitNamed(f.kernelId, *e);
                        resumeAt(f) = ops[1].id;
                        return Exit::Waiting;
                    }
                    case Op::Trigger:
                    case Op::TriggerNb: {
                        const std::optional<kernel::EventId> e = eventOf(ops[0]);
                        if (!e) {
                            fail(p, "trigger of a null event");
                            return Exit::Error;
                        }
                        if (in.op == Op::Trigger) {
                            sched_.trigger(*e);
                        } else {
                            const W4 t = ops.size() > 1 ? value(ops[1]) : W4{0, 0};
                            sched_.triggerNb(f.kernelId, *e, t.b != 0 ? 0 : t.a);
                        }
                        break;
                    }
                    case Op::Builtin: builtin(in, info_[body][b][k]); break;
                    case Op::DriveDelay: {
                        const W4 x = value(ops[1]);
                        const std::uint64_t buf[2] = {x.a, x.b};
                        // A delay with x or z bits is 0 (9.4.1).
                        const auto ticks = [&](std::size_t i) {
                            const W4 d = value(ops[i]);
                            return d.b != 0 ? std::uint64_t{0} : d.a;
                        };
                        sched_.driveDelayed(info_[body][b][k].driver, ops[0].id, ops[0].firstElem,
                                            ops[0].lsb, ops[0].width, buf, ticks(2), ticks(3),
                                            ticks(4));
                        break;
                    }
                    case Op::Resolve: resolve(in, info_[body][b][k]); break;
                    case Op::Syscall: {
                        const InstrInfo& info = info_[body][b][k];
                        if (!syscall(f, p, in, info)) return Exit::Error;
                        if (sched_.ending()) return Exit::Stop;
                        break;
                    }
                    case Op::OverrideStart: {
                        const Exit e = overrideStart(in, steps);
                        if (e != Exit::Ret) return e;
                        if (sched_.ending()) return Exit::Stop;
                        break;
                    }
                    case Op::OverrideStop: {
                        const kernel::Hold level = holdLevel(ops[0]);
                        for (std::size_t i = 1; i < ops.size(); ++i)
                            sched_.release(level, partOf(ops[i]));
                        dropIdle(level);
                        break;
                    }
                    case Op::StoreOverride: {
                        const W4 v = value(ops[2]);
                        const std::uint64_t buf[2] = {v.a, v.b};
                        sched_.writeHeld(holdLevel(ops[0]), f.kernelId, ops[1].id, ops[1].firstElem,
                                         ops[1].lsb, ops[1].width, buf);
                        break;
                    }
                    default:
                        if (!execute(f, p, in)) return Exit::Error;
                        if (sched_.ending()) return Exit::Stop;
                        break;
                }
            }
            if (!jumped) {
                fail(p, "block bb" + std::to_string(b) + " has no terminator");
                return Exit::Error;
            }
        }
    }

    // --- Procedural continuous assignments (SIR_SPEC.md 5.6) ---

    static kernel::Hold holdLevel(const Operand& o) {
        return o.text == "force" ? kernel::Hold::Force : kernel::Hold::Assign;
    }

    static kernel::SlotPart partOf(const Operand& o) {
        return {o.id, o.firstElem, o.numElems, o.lsb, o.width};
    }

    /// override.start: the targets go to the override process, which runs
    /// once now and then whenever what it reads changes. A process that no
    /// longer holds anything stops (10.6.1, 10.6.2).
    Exit overrideStart(const sir::Instr& in, std::uint64_t& steps) {
        const auto& ops = in.operands;
        const kernel::Hold level = holdLevel(ops[0]);
        Frame& o = *overrides_[ops[1].id];
        for (std::size_t i = 2; i < ops.size(); ++i)
            sched_.hold(level, o.kernelId, partOf(ops[i]));
        dropIdle(level);
        const Act caller = act_;
        enter({ops[1].id, &o.vars, caller.base + bodyTemps(caller.body), o.varsId});
        W4 ret{0, 0};
        const Exit e = runBody(o, 0, steps, ret);
        act_ = caller;
        if (e != Exit::Ret) return e;
        o.holding = true;
        o.level = level;
        waitTriggers(o, d_.processes[ops[1].id]);
        return Exit::Ret;
    }

    /// Stops the override processes of a level that hold nothing now.
    void dropIdle(kernel::Hold level) {
        for (Frame* o : overrides_) {
            if (o == nullptr || !o->holding || o->level != level) continue;
            if (sched_.holdsAny(level, o->kernelId)) continue;
            o->holding = false;
            sched_.cancelWait(o->kernelId);
        }
    }

    /// A value of a type's default (VALUE_SEMANTICS.md 1.4): x for
    /// four-state, 0 for the others.
    static W4 defaultValue(sir::Type t) {
        return sir::isFourState(t) ? kernel::allX(t.width) : W4{0, 0};
    }

    /// call: runs a function, or a task that does not wait, to its ret.
    /// Inputs and inouts are copied in, outputs and inouts copied back to
    /// the caller's places like blocking assignments (SIR_SPEC.md 8, 4.9.7).
    Exit call(Frame& f, const sir::Instr& in, std::uint64_t& steps) {
        const auto& ops = in.operands;
        const sir::Subroutine& s = d_.subroutines[ops[0].id];
        if (callDepth_ >= 10000) {
            fail(d_.processes[f.process], "calls nested more than 10000 deep");
            return Exit::Error;
        }
        const Act caller = act_;
        const std::uint32_t body = processCount() + ops[0].id;
        std::vector<W4> vars = newVars(body);
        // The arguments are read in the caller before anything is written.
        const std::vector<std::vector<W4>> args = readArgs(s, ops);
        enter({body, &vars, caller.base + bodyTemps(caller.body), enroll(vars)});
        bindArgs(body, s, args, vars);
        W4 ret{0, 0};
        ++callDepth_;
        const Exit e = runBody(f, 0, steps, ret);
        --callDepth_;
        const std::vector<std::vector<W4>> outs = readOuts(body, s, vars);
        retire(act_.varsId);
        act_ = caller;
        if (e == Exit::Waiting) {
            fail(d_.processes[f.process],
                 "task " + s.name + " waited; tasks that wait are not supported by the VM yet");
            return Exit::Error;
        }
        if (e != Exit::Ret) return e;
        writeOuts(s, ops, outs, *act_.vars, act_.body);
        if (in.result != sir::kNone) temp(in.result) = ret;
        return Exit::Ret;
    }

    /// Passes the arguments of the block operand at ops[i] to its
    /// parameters and returns the block.
    sir::BlockId takeBranch(const Frame& f, const std::vector<Operand>& ops, std::size_t i) {
        const Operand& target = ops[i];
        const auto& params = bodyBlocks(act_.body)[target.id].params;
        if (target.count != 0) {
            // Read every argument before writing any parameter.
            W4 vals[16];
            std::vector<W4> many;
            W4* v = target.count <= 16 ? vals : (many.resize(target.count), many.data());
            for (std::uint32_t a = 0; a < target.count; ++a)
                v[a] = value(ops[i + 1 + a]);
            for (std::uint32_t a = 0; a < target.count; ++a)
                temp(params[a].temp) = v[a];
        }
        return target.id;
    }

    /// Runs one instruction that is not a terminator or a system call.
    /// resolve (SIR_SPEC.md 3.5): each driver is a value and its strengths
    /// for 1 and for 0 (VALUE_SEMANTICS.md 5.4).
    void resolve(const sir::Instr& in, const InstrInfo& info) {
        const auto& ops = in.operands;
        const unsigned w = in.type.width;
        drivers_.clear();
        for (std::size_t i = 1; i + 2 < ops.size();) {
            kernel::StrengthDriver d{value(ops[i]), static_cast<std::uint8_t>(value(ops[i + 1]).a),
                                     static_cast<std::uint8_t>(value(ops[i + 2]).a)};
            i += 3;
            // A two-state operand after the strengths is the driver's side
            // slot: the sides it drives on the low w bits for 1, on the
            // next w for 0 (SIR_SPEC.md 3.5). Driver values are four-state.
            if (i < ops.size() && !sir::isFourState(typeOf(ops[i]))) {
                const std::uint64_t side = value(ops[i++]).a;
                d.one = side & kernel::mask(w);
                d.zero = w < 64 ? side >> w : 0;
            }
            drivers_.push_back(d);
        }
        temp(in.result) = kernel::resolve4(info.netKind, drivers_.data(),
                                           static_cast<unsigned>(drivers_.size()), w);
    }

    bool execute(Frame& f, const sir::Process& p, const sir::Instr& in) {
        const auto& ops = in.operands;
        const sir::Type rt = in.type;
        const unsigned w = rt.width;
        const auto v = [&](std::size_t i) { return value(ops[i]); };
        const auto t = [&](std::size_t i) { return typeOf(ops[i]); };
        const auto set = [&](W4 r) {
            // A two-state result has no x or z (VALUE_SEMANTICS.md 1.2).
            if (sir::isIntegral(rt) && !sir::isFourState(rt)) r = kernel::to2(r);
            temp(in.result) = r;
        };
        if (!ops.empty() &&
            (rt.kind == sir::TypeKind::String || t(0).kind == sir::TypeKind::String)) {
            // Strings (VALUE_SEMANTICS.md 7.3, 7.6): handles of the
            // context's interned strings.
            const auto str = [&](std::size_t i) { return strings_.get(v(i).a); };
            const auto put = [&](std::string_view text) {
                temp(in.result) = {strings_.intern(text), 0};
            };
            const auto cmp = [&](bool b) { temp(in.result) = {b ? 1U : 0U, 0}; };
            switch (in.op) {
                case Op::Itos: put(integerToString(v(0), t(0).width)); return true;
                case Op::Stoi: set(stringToInteger(str(0), w)); return true;
                case Op::Concat: {
                    std::string text;
                    for (std::size_t i = 0; i < ops.size(); ++i)
                        text += str(i);
                    put(text);
                    return true;
                }
                case Op::Repl: {
                    // A negative, x or z count gives the empty string, with
                    // one warning per run (VALUE_SEMANTICS.md 7.3).
                    const std::optional<std::int64_t> n = indexValue(v(0), t(0));
                    if ((!n || *n < 0) && !warnedRepl_) {
                        warnedRepl_ = true;
                        std::string line =
                            in.loc.file < d_.sources.size() ? d_.sources[in.loc.file] : "?";
                        line +=
                            ":" + std::to_string(in.loc.line) + ":" + std::to_string(in.loc.column);
                        line += ": warning: string replication count is negative, x or z; the "
                                "result is "
                                "the empty string";
                        output_.message(line);
                    }
                    std::string text;
                    for (std::int64_t i = 0; n && i < *n; ++i)
                        text += str(1);
                    put(text);
                    return true;
                }
                case Op::Select: {
                    // An unknown condition with unequal strings gives the
                    // empty string (VALUE_SEMANTICS.md 3.9, table 7-1).
                    const W4 c = kernel::truth4(v(0));
                    if (c.b == 0)
                        temp(in.result) = (c.a & 1U) != 0 ? v(1) : v(2);
                    else
                        temp(in.result) = str(1) == str(2) ? v(1) : W4{0, 0};  // 0 is ""
                    return true;
                }
                case Op::Eq: cmp(str(0) == str(1)); return true;
                case Op::Ne: cmp(str(0) != str(1)); return true;
                case Op::Lt: cmp(str(0) < str(1)); return true;
                case Op::Le: cmp(str(0) <= str(1)); return true;
                case Op::Gt: cmp(str(0) > str(1)); return true;
                case Op::Ge: cmp(str(0) >= str(1)); return true;
                case Op::StrGet: {
                    // Outside the string: 0 (table 6-9).
                    const std::string_view text = str(0);
                    const std::optional<std::int64_t> i = indexValue(v(1), t(1));
                    const bool inside = i && *i >= 0 && static_cast<std::size_t>(*i) < text.size();
                    temp(in.result) = {
                        inside ? static_cast<unsigned char>(text[static_cast<std::size_t>(*i)])
                               : 0U,
                        0};
                    return true;
                }
                default: break;  // loads, stores and moves copy the handle
            }
        }
        if (!ops.empty() && (isRealType(rt) || isRealType(t(0)))) {
            // Real operations (VALUE_SEMANTICS.md 7.5): real in double,
            // shortreal in float.
            const auto r = [&](std::size_t i) { return toReal(v(i), t(i)); };
            const bool single = rt.kind == sir::TypeKind::ShortReal;
            const auto put = [&](double d) {
                temp(in.result) = fromReal(single ? static_cast<float>(d) : d, rt);
            };
            const auto cmp = [&](bool b) { temp(in.result) = {b ? 1U : 0U, 0}; };
            const bool fl = t(0).kind == sir::TypeKind::ShortReal;
            const auto a2 = [&](std::size_t i) {
                return fl ? static_cast<double>(static_cast<float>(r(i))) : r(i);
            };
            switch (in.op) {
                case Op::Add:
                    put(single ? double(float(r(0)) + float(r(1))) : r(0) + r(1));
                    return true;
                case Op::Sub:
                    put(single ? double(float(r(0)) - float(r(1))) : r(0) - r(1));
                    return true;
                case Op::Mul:
                    put(single ? double(float(r(0)) * float(r(1))) : r(0) * r(1));
                    return true;
                case Op::Div:
                    put(single ? double(float(r(0)) / float(r(1))) : r(0) / r(1));
                    return true;
                case Op::Pow: put(std::pow(r(0), r(1))); return true;
                case Op::Neg: put(-r(0)); return true;
                case Op::Lt: cmp(a2(0) < a2(1)); return true;
                case Op::Le: cmp(a2(0) <= a2(1)); return true;
                case Op::Gt: cmp(a2(0) > a2(1)); return true;
                case Op::Ge: cmp(a2(0) >= a2(1)); return true;
                case Op::Eq: cmp(a2(0) == a2(1)); return true;
                case Op::Ne: cmp(a2(0) != a2(1)); return true;
                case Op::Truth: temp(in.result) = truthOf(v(0), t(0)); return true;
                case Op::Lnot: temp(in.result) = kernel::lnot4(truthOf(v(0), t(0))); return true;
                case Op::Itor: put(integerToReal(v(0), t(0))); return true;
                case Op::Rconv: put(r(0)); return true;
                case Op::Rtoi:
                    set(W4{realToInteger(r(0), false) & kernel::mask(w), 0});
                    return true;
                case Op::Select: {
                    // An unknown condition gives the value both sides share,
                    // or 0.0 (VALUE_SEMANTICS.md 3.9, table 7-1).
                    const W4 c = kernel::truth4(v(0));
                    if (c.b == 0) {
                        temp(in.result) = (c.a & 1U) != 0 ? v(1) : v(2);
                    } else {
                        temp(in.result) = r(1) == r(2) ? v(1) : fromReal(0.0, rt);
                    }
                    return true;
                }
                default: break;  // loads, stores and moves copy the bits as they are
            }
        }
        switch (in.op) {
            case Op::Land:
            case Op::Lor:
            case Op::Limp:
            case Op::Leqv:
                if (isRealType(t(0)) || isRealType(t(1))) {
                    const W4 x = truthOf(v(0), t(0));
                    const W4 y = truthOf(v(1), t(1));
                    W4 res = in.op == Op::Land  ? kernel::land4(x, y)
                             : in.op == Op::Lor ? kernel::lor4(x, y)
                             : in.op == Op::Limp
                                 ? kernel::lor4(kernel::lnot4(x), y)
                                 : ((x.b | y.b) != 0 ? kernel::kW4X : W4{x.a == y.a ? 1U : 0U, 0});
                    set(res);
                    return true;
                }
                break;
            default: break;
        }
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
            case Op::NbaDelay: {
                // The update in the NBA region of the time slot `delay` ticks
                // ahead (SCHEDULER_SPEC.md 13); x or z ticks are 0 (9.4.1).
                const Operand& o = ops[0];
                const W4 d = v(2);
                nbaSlot(f, o.id, o.firstElem, o.lsb, o.width, v(1), d.b != 0 ? 0 : d.a);
                return true;
            }
            case Op::LoadElem: {
                const bool frame = ops[0].kind == Operand::Kind::Frame;
                const std::optional<std::uint32_t> e = elementOf(
                    frame ? bodyFrame(act_.body)[ops[0].id].dims : d_.slots[ops[0].id].dims, ops,
                    1);
                // An invalid index reads the element type's default
                // (VALUE_SEMANTICS.md 3.11).
                if (!e)
                    set(defaultValue(rt));
                else
                    set(frame ? frameElem(ops[0].id, *e) : readSlot(ops[0].id, *e, 0, 0));
                return true;
            }
            case Op::StoreElem:
            case Op::NbaElem: {
                const bool frame = ops[0].kind == Operand::Kind::Frame;
                const std::optional<std::uint32_t> e = elementOf(
                    frame ? bodyFrame(act_.body)[ops[0].id].dims : d_.slots[ops[0].id].dims, ops,
                    1);
                if (!e) return true;  // an invalid index writes nothing
                if (frame)
                    frameElem(ops[0].id, *e) = v(ops.size() - 1);
                else if (in.op == Op::StoreElem)
                    writeSlot(ops[0].id, *e, 0, 0, v(ops.size() - 1));
                else
                    nbaSlot(f, ops[0].id, *e, 0, 0, v(ops.size() - 1));
                return true;
            }
            case Op::StoreBits:
            case Op::NbaBits: {
                const bool frame = ops[0].kind == Operand::Kind::Frame;
                const std::vector<sir::ArrayDim>& dims =
                    frame ? bodyFrame(act_.body)[ops[0].id].dims : d_.slots[ops[0].id].dims;
                std::uint32_t e = 0;
                if (!dims.empty()) {
                    const std::optional<std::uint32_t> el = elementOf(dims, ops, 1);
                    if (!el) return true;
                    e = *el;
                }
                const std::size_t n = dims.size();
                if (frame) {
                    W4& cell = dims.empty() ? (*act_.vars)[ops[0].id] : frameElem(ops[0].id, e);
                    cell = mergeBits(cell, bodyFrame(act_.body)[ops[0].id].type.width, v(n + 1),
                                     t(n + 1), v(n + 2), t(n + 2).width);
                    return true;
                }
                storeBits(f, ops[0].id, e, v(n + 1), t(n + 1), v(n + 2), t(n + 2).width,
                          in.op == Op::NbaBits);
                return true;
            }
            case Op::Udp:
            case Op::UdpSeq: {
                // A value of a UDP: 0, 1, or x for x and z.
                const auto code = [](W4 x) -> std::uint8_t {
                    return (x.b & 1U) != 0 ? kernel::kUdpX : static_cast<std::uint8_t>(x.a & 1U);
                };
                const kernel::UdpTable& table = udpTables_[ops[0].id];
                const unsigned n = table.inputs;
                std::uint8_t prev[kernel::kUdpMaxInputs];
                std::uint8_t cur[kernel::kUdpMaxInputs];
                std::uint8_t r = kernel::kUdpX;
                if (in.op == Op::Udp) {
                    for (unsigned i = 0; i < n; ++i)
                        cur[i] = code(v(1 + i));
                    r = kernel::udpOutput(table, cur);
                } else {
                    for (unsigned i = 0; i < n; ++i) {
                        prev[i] = code(v(2 + i));
                        cur[i] = code(v(2 + n + i));
                    }
                    r = kernel::udpStep(table, code(v(1)), prev, cur);
                }
                set(r == kernel::kUdpX ? W4{1, 1} : W4{r, 0});
                return true;
            }
            case Op::RefMake: {
                const Operand& o = ops[0];
                if (o.kind == Operand::Kind::Slot) {
                    std::uint32_t e = 0;
                    if (ops.size() > 1) {
                        const std::optional<std::uint32_t> el =
                            elementOf(d_.slots[o.id].dims, ops, 1);
                        if (!el) {
                            fail(p, "ref to an element outside its array");
                            return false;
                        }
                        e = *el;
                    }
                    set(slotRef(o.id, e));
                    return true;
                }
                const sir::FrameVar& fv = bodyFrame(act_.body)[o.id];
                if (fv.type.kind == sir::TypeKind::Ref) {
                    // A ref argument passed on, or an element of the array
                    // it refers to.
                    W4 r = (*act_.vars)[o.id];
                    if (ops.size() > 1) {
                        const std::optional<std::uint32_t> el = elementOf(fv.dims, ops, 1);
                        if (!el) {
                            fail(p, "ref to an element outside its array");
                            return false;
                        }
                        if (isFrameRef(r))
                            r.a += *el;
                        else
                            r.b += std::uint64_t{*el} << 1;
                    }
                    set(r);
                    return true;
                }
                std::size_t at = o.id;
                if (!fv.dims.empty()) {
                    at = layouts_[act_.body].elements[o.id];
                    if (ops.size() > 1) {
                        const std::optional<std::uint32_t> el = elementOf(fv.dims, ops, 1);
                        if (!el) {
                            fail(p, "ref to an element outside its array");
                            return false;
                        }
                        at += *el;
                    }
                }
                set(frameRef(act_.varsId, at));
                return true;
            }
            case Op::LoadRef:
            case Op::StoreRef:
            case Op::NbaRef: {
                // The leaf of a structure: the next slots or frame words.
                const W4 r = v(0);
                const std::size_t n = ops.size() - (in.op == Op::LoadRef ? 0 : 1);
                const std::uint64_t k = n == 2 ? v(1).a : 0;
                if (in.op == Op::LoadRef) {
                    set(isFrameRef(r)
                            ? refWord(r, k)
                            : readSlot(refSlot(r) + static_cast<sir::SlotId>(k), refElem(r), 0, 0));
                    return true;
                }
                const W4 x = v(ops.size() - 1);
                if (isFrameRef(r)) {
                    if (in.op == Op::NbaRef) {
                        fail(p, "nonblocking assignment to an automatic variable through a ref");
                        return false;
                    }
                    refWord(r, k) = x;
                } else if (in.op == Op::StoreRef) {
                    writeSlot(refSlot(r) + static_cast<sir::SlotId>(k), refElem(r), 0, 0, x);
                } else {
                    nbaSlot(f, refSlot(r) + static_cast<sir::SlotId>(k), refElem(r), 0, 0, x);
                }
                return true;
            }
            case Op::LoadRefElem:
            case Op::StoreRefElem:
            case Op::StoreRefBits: {
                const sir::FrameVar& fv = bodyFrame(act_.body)[ops[0].id];
                const W4 r = (*act_.vars)[ops[0].id];
                std::uint32_t e = 0;
                if (!fv.dims.empty()) {
                    const std::optional<std::uint32_t> el = elementOf(fv.dims, ops, 1);
                    if (!el) {
                        // An invalid index reads the default and writes
                        // nothing (VALUE_SEMANTICS.md 3.11).
                        if (in.op == Op::LoadRefElem) set(defaultValue(rt));
                        return true;
                    }
                    e = *el;
                }
                const std::size_t n = fv.dims.size();
                if (in.op == Op::LoadRefElem) {
                    set(isFrameRef(r) ? refWord(r, e) : readSlot(refSlot(r), refElem(r) + e, 0, 0));
                } else if (in.op == Op::StoreRefElem) {
                    if (isFrameRef(r))
                        refWord(r, e) = v(n + 1);
                    else
                        writeSlot(refSlot(r), refElem(r) + e, 0, 0, v(n + 1));
                } else if (isFrameRef(r)) {
                    W4& cell = refWord(r, e);
                    cell = mergeBits(cell, fv.target.width, v(n + 1), t(n + 1), v(n + 2),
                                     t(n + 2).width);
                } else {
                    storeBits(f, refSlot(r), refElem(r) + e, v(n + 1), t(n + 1), v(n + 2),
                              t(n + 2).width, false);
                }
                return true;
            }
            case Op::ReportDefer: {
                // SYSTEM_TASKS.md 10.4; the kernel holds it until the
                // Observed region (SCHEDULER_SPEC.md 15.3).
                std::string line = in.loc.file < d_.sources.size() ? d_.sources[in.loc.file] : "?";
                line += ":";
                line += std::to_string(in.loc.line);
                line += ": warning: ";
                line += ops[0].text;
                line += " violation: ";
                line += ops[1].text;
                line += " [";
                line += ops[2].text;
                line += " @ ";
                line += messageTime(sched_.time(), d_.timePrecision);
                line += "]";
                sched_.deferReport(f.kernelId, std::move(line));
                return true;
            }
            case Op::Fload: set((*act_.vars)[ops[0].id]); return true;
            case Op::Fstore: (*act_.vars)[ops[0].id] = v(1); return true;
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

    /// The string methods (6.16, VALUE_SEMANTICS.md 7.6). The ones that
    /// change the string return the new one; front stores it.
    /// A value read by $sscanf or $value$plusargs in the type of its target
    /// (SYSTEM_TASKS.md 6.3, 9): the low bits of an integral value, a lone
    /// x or z in every bit, a string's characters, a real.
    W4 scanned(const kernel::ScanValue& v, sir::Type t) {
        if (t.kind == sir::TypeKind::String)
            return {strings_.intern(v.conv == 's' ? v.text : std::string()), 0};
        if (isRealType(t)) return fromReal(v.conv == 'f' ? v.real : 0.0, t);
        W4 r = v.conv == 's' ? stringToInteger(v.text, t.width) : v.bits;
        if (v.fill) r = {v.bits.a != 0 ? ~std::uint64_t{0} : 0, ~std::uint64_t{0}};
        r = {r.a & kernel::mask(t.width), r.b & kernel::mask(t.width)};
        return sir::isFourState(t) ? r : kernel::to2(r);
    }

    /// The event an element of an event slot holds; nothing for null.
    std::optional<kernel::EventId> eventOf(const sir::Operand& o) const {
        const std::uint64_t h = readSlot(o.id, o.firstElem, 0, 0).a;
        if (h == 0) return std::nullopt;
        return static_cast<kernel::EventId>(h - 1);
    }

    void builtin(const sir::Instr& in, const InstrInfo& info) {
        const auto& ops = in.operands;
        const auto v = [&](std::size_t i) { return value(ops[i]); };
        const auto t = [&](std::size_t i) { return typeOf(ops[i]); };
        if (info.method == Method::Triggered) {
            // False for a null event (15.5.3).
            const std::uint64_t h = v(1).a;
            const bool on = h != 0 && sched_.triggered(static_cast<kernel::EventId>(h - 1));
            temp(in.result) = {on ? 1U : 0U, 0};
            return;
        }
        const std::string_view self = strings_.get(v(1).a);
        const sir::Type rt = in.type;
        const auto put = [&](std::string_view text) {
            temp(in.result) = {strings_.intern(text), 0};
        };
        const auto num = [&](std::uint64_t n) {
            W4 r{n & kernel::mask(rt.width), 0};
            temp(in.result) = r;
        };
        const auto index = [&](std::size_t i) { return indexValue(v(i), t(i)); };
        switch (info.method) {
            case Method::Len: num(self.size()); return;
            case Method::Getc: {
                const std::optional<std::int64_t> i = index(2);
                const bool ok = i && *i >= 0 && static_cast<std::size_t>(*i) < self.size();
                num(ok ? static_cast<unsigned char>(self[static_cast<std::size_t>(*i)]) : 0U);
                return;
            }
            case Method::Putc: {
                // Outside the string, or the character 0: no change (6.16.3).
                std::string text(self);
                const std::optional<std::int64_t> i = index(2);
                const auto c = static_cast<char>(v(3).a & ~v(3).b & 0xffU);
                if (i && *i >= 0 && static_cast<std::size_t>(*i) < text.size() && c != 0)
                    text[static_cast<std::size_t>(*i)] = c;
                put(text);
                return;
            }
            case Method::ToUpper:
            case Method::ToLower: {
                std::string text(self);
                for (char& c : text)
                    c = static_cast<char>(info.method == Method::ToUpper
                                              ? std::toupper(static_cast<unsigned char>(c))
                                              : std::tolower(static_cast<unsigned char>(c)));
                put(text);
                return;
            }
            case Method::Compare:
            case Method::ICompare: {
                // -1, 0 or 1 (VALUE_SEMANTICS.md 7.6).
                std::string a(self);
                std::string b(strings_.get(v(2).a));
                if (info.method == Method::ICompare) {
                    for (char& c : a)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    for (char& c : b)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                const int r = a.compare(b);
                num(static_cast<std::uint64_t>(static_cast<std::int64_t>(r < 0   ? -1
                                                                         : r > 0 ? 1
                                                                                 : 0)));
                return;
            }
            case Method::Substr: {
                // Characters i to j; empty when out of range (6.16.8).
                const std::optional<std::int64_t> i = index(2);
                const std::optional<std::int64_t> j = index(3);
                const auto n = static_cast<std::int64_t>(self.size());
                if (!i || !j || *i < 0 || *j < *i || *j >= n) {
                    put("");
                    return;
                }
                put(self.substr(static_cast<std::size_t>(*i),
                                static_cast<std::size_t>(*j - *i + 1)));
                return;
            }
            case Method::AtoI: num(parseDigits(self, 10)); return;
            case Method::AtoHex: num(parseDigits(self, 16)); return;
            case Method::AtoOct: num(parseDigits(self, 8)); return;
            case Method::AtoBin: num(parseDigits(self, 2)); return;
            case Method::AtoReal: {
                const std::string text(self);
                char* end = nullptr;
                const double d = std::strtod(text.c_str(), &end);
                temp(in.result) = fromReal(d, rt);
                return;
            }
            case Method::ItoA:
            case Method::HexToA:
            case Method::OctToA:
            case Method::BinToA: {
                // Like $sformatf with %0d, %0h, %0o and %0b (6.16.12).
                const char conv = info.method == Method::ItoA     ? 'd'
                                  : info.method == Method::HexToA ? 'h'
                                  : info.method == Method::OctToA ? 'o'
                                                                  : 'b';
                const sir::Type at = t(2);
                const W4 x = v(2);
                const std::uint64_t words[2] = {x.a, x.b};
                const kernel::IntValue iv{words, at.width, sir::isFourState(at), sir::isSigned(at)};
                put(kernel::formatInt(iv, kernel::FormatSpec{conv, 0, {}}));
                return;
            }
            case Method::RealToA:  // like %g (VALUE_SEMANTICS.md 7.6)
                put(kernel::formatReal(toReal(v(2), t(2)), kernel::FormatSpec{}));
                return;
            case Method::None:
            case Method::Triggered: return;  // handled above
        }
    }

    std::string formatValue(const Frame& f, const Operand& o,
                            const kernel::FormatSpec& spec) const {
        const sir::Type t = typeOf(o);
        const W4 x = value(o);
        if (t.kind == sir::TypeKind::String) {
            // The characters, padded to an explicit width (3.5).
            std::string text(strings_.get(x.a));
            if (spec.width > 0 && text.size() < static_cast<std::size_t>(spec.width))
                text.insert(0, static_cast<std::size_t>(spec.width) - text.size(), ' ');
            return text;
        }
        if (isRealType(t)) {
            // front makes every other specifier take an integer; without
            // one, a real shows like %g (SYSTEM_TASKS.md 3.6).
            const bool realSpec = spec.conv == 'e' || spec.conv == 'f' || spec.conv == 'g';
            return kernel::formatReal(toReal(x, t), realSpec ? spec : kernel::FormatSpec{});
        }
        const std::uint64_t words[2] = {x.a, x.b};
        const kernel::IntValue iv{words, t.width, sir::isFourState(t), sir::isSigned(t)};
        if (spec.conv == 't')
            return kernel::formatTime(iv, d_.timePrecision, timeFormat_, spec.width);
        return kernel::formatInt(iv, spec);
    }

    /// The text of the format operands from `i` on: a text operand is a
    /// format; the values after it fill its specifiers, and values left
    /// over print in decimal (SYSTEM_TASKS.md 3.1).
    std::optional<std::string> formatOperands(Frame& f, const sir::Process& p, const sir::Instr& in,
                                              const InstrInfo& info, std::size_t i) {
        const auto& ops = in.operands;
        std::string text;
        while (i < ops.size()) {
            if (ops[i].kind != Operand::Kind::Text) {
                text += formatValue(f, ops[i], kernel::FormatSpec{});
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
                    return std::nullopt;
                }
                text += formatValue(f, ops[i], piece.spec);
                ++i;
            }
        }
        return text;
    }

    bool syscall(Frame& f, const sir::Process& p, const sir::Instr& in, const InstrInfo& info) {
        const auto& ops = in.operands;
        switch (info.task) {
            case SysTask::Fatal:
            case SysTask::Error:
            case SysTask::Warning:
            case SysTask::Info: {
                // <file>:<line>: <severity>: <message> [<scope> @ <time>]
                // on stderr (SYSTEM_TASKS.md 10.1); $fatal then ends the
                // run like $finish with its level (20.10).
                const std::optional<std::string> message =
                    formatOperands(f, p, in, info, formatStart(info.task));
                if (!message) return false;
                std::string line = in.loc.file < d_.sources.size() ? d_.sources[in.loc.file] : "?";
                line += ":" + std::to_string(in.loc.line) + ": ";
                line += info.task == SysTask::Fatal     ? "fatal"
                        : info.task == SysTask::Error   ? "error"
                        : info.task == SysTask::Warning ? "warning"
                                                        : "info";
                line += ": " + *message + " [" + ops[1].text + " @ " +
                        messageTime(sched_.time(), d_.timePrecision) + "]";
                output_.message(line);
                if (info.task == SysTask::Fatal)
                    sched_.requestEnd(kernel::EndReason::Fatal, static_cast<int>(value(ops[2]).a));
                return true;
            }
            case SysTask::CountBits: {
                // $countbits (20.9): the values to count are those of the
                // control bits.
                bool zeros = false, ones = false, zs = false, xs = false;
                for (std::size_t i = 2; i < ops.size(); ++i) {
                    const W4 c = value(ops[i]);
                    const bool a = (c.a & 1U) != 0;
                    const bool b = (c.b & 1U) != 0;
                    zeros = zeros || (!a && !b);
                    ones = ones || (a && !b);
                    zs = zs || (!a && b);
                    xs = xs || (a && b);
                }
                temp(in.result) = {
                    kernel::countBits4(value(ops[1]), typeOf(ops[1]).width, zeros, ones, zs, xs),
                    0};
                return true;
            }
            case SysTask::Display:
            case SysTask::Write:
            case SysTask::SFormatF: {
                const std::optional<std::string> formatted =
                    formatOperands(f, p, in, info, formatStart(info.task));
                if (!formatted) return false;
                std::string text = *formatted;
                if (info.task == SysTask::SFormatF) {
                    temp(in.result) = {strings_.intern(text), 0};
                    return true;
                }
                if (info.task == SysTask::Display) text += '\n';
                output_.write(text);
                return true;
            }
            case SysTask::Finish:
            case SysTask::Stop: {
                const auto level = static_cast<int>(value(ops[1]).a);
                sched_.requestEnd(info.task == SysTask::Finish ? kernel::EndReason::Finish
                                                               : kernel::EndReason::Stop,
                                  level);
                return true;
            }
            case SysTask::Time: {
                // The time in the caller's unit, rounded half away from zero
                // (SYSTEM_TASKS.md 5.1); the operand is 10^(unit - precision).
                const std::uint64_t div = value(ops[1]).a;
                const std::uint64_t now = sched_.time();
                const std::uint64_t q = now / div;
                const std::uint64_t r = now % div;
                temp(in.result) = {q + (div > 1 && r >= div - r ? 1 : 0), 0};
                return true;
            }
            case SysTask::Sscanf: {
                // $sscanf(input, format, frame variables...): the values read
                // go to the frame variables, which the front copies to the
                // targets (SIR_SPEC.md 8, SYSTEM_TASKS.md 6.3). An input with
                // x or z bits gives -1.
                std::string input;
                const sir::Type it = typeOf(ops[1]);
                const W4 iv = value(ops[1]);
                int n = -1;
                if (it.kind == sir::TypeKind::String) {
                    input = strings_.get(iv.a);
                } else if (iv.b == 0) {
                    // The bytes from the most significant one, NULs kept.
                    for (unsigned k = (it.width + 7) / 8; k-- > 0;)
                        input.push_back(
                            static_cast<char>(k * 8 < 64 ? (iv.a >> (k * 8)) & 0xffU : 0));
                } else {
                    temp(in.result) = {0xffffffffU, 0};
                    return true;
                }
                scanValues_.clear();
                n = kernel::scanString(input, ops[2].text, scanValues_);
                for (std::size_t i = 0; i < scanValues_.size() && 3 + i < ops.size(); ++i)
                    (*act_.vars)[ops[3 + i].id] = scanned(scanValues_[i], typeOf(ops[3 + i]));
                temp(in.result) = {static_cast<std::uint32_t>(n), 0};
                return true;
            }
            case SysTask::ValuePlusargs: {
                // The first plusarg that starts with the name before the %:
                // the rest by the one conversion (SYSTEM_TASKS.md 9).
                const std::string_view fmt = ops[1].text;
                const std::size_t pct = fmt.find('%');
                const std::string_view name = fmt.substr(0, pct);
                char conv = 'd';
                for (std::size_t k = pct + 1; k < fmt.size(); ++k) {
                    if (fmt[k] >= '0' && fmt[k] <= '9') continue;
                    conv = static_cast<char>(std::tolower(static_cast<unsigned char>(fmt[k])));
                    break;
                }
                if (conv == 'x') conv = 'h';
                if (conv == 'e' || conv == 'g') conv = 'f';
                temp(in.result) = {0, 0};
                for (const std::string& a : options_.plusargs) {
                    if (!std::string_view(a).starts_with(name)) continue;
                    const kernel::ScanValue v = kernel::plusargValue(a.substr(name.size()), conv);
                    (*act_.vars)[ops[2].id] = scanned(v, typeOf(ops[2]));
                    temp(in.result) = {1, 0};
                    break;
                }
                return true;
            }
            case SysTask::TestPlusargs: {
                // 1 if some plusarg starts with the string (SYSTEM_TASKS.md 9).
                const std::string_view prefix = strings_.get(value(ops[1]).a);
                bool found = false;
                for (const std::string& a : options_.plusargs)
                    found = found || std::string_view(a).starts_with(prefix);
                temp(in.result) = {found ? 1U : 0U, 0};
                return true;
            }
            case SysTask::Realtime: {
                const auto div = static_cast<double>(value(ops[1]).a);
                temp(in.result) = fromReal(static_cast<double>(sched_.time()) / div, in.type);
                return true;
            }
            case SysTask::Rtoi:
            case SysTask::RealToBits:
            case SysTask::BitsToReal:
            case SysTask::ShortRealToBits:
            case SysTask::BitsToShortReal:
            case SysTask::Math1:
            case SysTask::Math2:
            case SysTask::Clog2: {
                const sir::Type rt = in.type;
                const auto arg = [&](std::size_t i) { return value(ops[i]); };
                const auto argType = [&](std::size_t i) { return typeOf(ops[i]); };
                W4 r{0, 0};
                switch (info.task) {
                    case SysTask::Rtoi:
                        r = {realToInteger(toReal(arg(1), argType(1)), true) &
                                 kernel::mask(rt.width),
                             0};
                        break;
                    case SysTask::RealToBits:
                    case SysTask::ShortRealToBits:
                    case SysTask::BitsToReal:
                    case SysTask::BitsToShortReal: r = {arg(1).a & ~arg(1).b, 0}; break;
                    case SysTask::Math1:
                        r = fromReal(info.fn1(toReal(arg(1), argType(1))), rt);
                        break;
                    case SysTask::Math2:
                        r = fromReal(
                            info.fn2(toReal(arg(1), argType(1)), toReal(arg(2), argType(2))), rt);
                        break;
                    case SysTask::Clog2: {
                        // ceil(log2(n)), and 0 for 0 and 1 (20.8.1); x for an
                        // unknown argument.
                        const W4 n = arg(1);
                        if (n.b != 0) {
                            r = kernel::allX(rt.width);
                        } else {
                            std::uint64_t k = 0;
                            while (k < 64 && (std::uint64_t{1} << k) < n.a)
                                ++k;
                            r = {k, 0};
                        }
                        break;
                    }
                    default: break;
                }
                if (sir::isIntegral(rt) && !sir::isFourState(rt)) r = kernel::to2(r);
                temp(in.result) = r;
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

}  // namespace kirimasim::vm
