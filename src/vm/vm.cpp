// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/vm/vm.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <limits>
#include <map>
#include <new>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/dump.h"
#include "kirimasim/kernel/files.h"
#include "kirimasim/kernel/format.h"
#include "kirimasim/kernel/heap.h"
#include "kirimasim/kernel/memfile.h"
#include "kirimasim/kernel/pattern.h"
#include "kirimasim/kernel/random.h"
#include "kirimasim/kernel/scan.h"
#include "kirimasim/kernel/stack.h"
#include "kirimasim/kernel/strength.h"
#include "kirimasim/kernel/udp.h"
#include "kirimasim/kernel/urandom.h"
#include "kirimasim/kernel/w4ops.h"
#include "kirimasim/kernel/wide.h"
#include "kirimasim/sir/pattern.h"
#include "kirimasim/sir/timescale.h"
#include "kirimasim/sir/types.h"
#include <memory_resource>

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
    Exit,
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
    SFormatRuntime,
    Fatal,
    Error,
    Warning,
    Info,
    CountBits,
    TestPlusargs,
    Sscanf,
    ValuePlusargs,
    TimeFormat,
    MonitorOn,
    MonitorOff,
    FDisplay,
    FWrite,
    FOpen,
    FClose,
    FFlush,
    FGets,
    FGetc,
    Ungetc,
    FScanf,
    FRead,
    FTell,
    FSeek,
    Rewind,
    FEof,
    FError,
    ReadMem,
    WriteMem,
    Random,
    Dist,
    Urandom,
    UrandomRange,
    Stacktrace,
    DumpFile,
    DumpVars,
    DumpOff,
    DumpOn,
    DumpAll,
    DumpLimit,
    DumpFlush,
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
        case SysTask::Info:
        case SysTask::FDisplay:
        case SysTask::FWrite: return 2;
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
    // The b, o and h forms differ only in their radix (InstrInfo).
    if (name == "$display" || name == "$displayb" || name == "$displayo" || name == "$displayh")
        return SysTask::Display;
    if (name == "$write" || name == "$writeb" || name == "$writeo" || name == "$writeh")
        return SysTask::Write;
    if (name == "$swriteb" || name == "$swriteo" || name == "$swriteh") return SysTask::SFormatF;
    if (name == "$fdisplay" || name == "$fdisplayb" || name == "$fdisplayo" || name == "$fdisplayh")
        return SysTask::FDisplay;
    if (name == "$fwrite" || name == "$fwriteb" || name == "$fwriteo" || name == "$fwriteh")
        return SysTask::FWrite;
    if (name == "$fopen") return SysTask::FOpen;
    if (name == "$fclose") return SysTask::FClose;
    if (name == "$fflush") return SysTask::FFlush;
    if (name == "$fgets") return SysTask::FGets;
    if (name == "$fgetc") return SysTask::FGetc;
    if (name == "$ungetc") return SysTask::Ungetc;
    if (name == "$fscanf") return SysTask::FScanf;
    if (name == "$fread") return SysTask::FRead;
    if (name == "$ftell") return SysTask::FTell;
    if (name == "$fseek") return SysTask::FSeek;
    if (name == "$rewind") return SysTask::Rewind;
    if (name == "$feof") return SysTask::FEof;
    if (name == "$ferror") return SysTask::FError;
    if (name == "$readmemb" || name == "$readmemh") return SysTask::ReadMem;
    if (name == "$writememb" || name == "$writememh") return SysTask::WriteMem;
    if (name == "$random") return SysTask::Random;
    if (name.starts_with("$dist_")) return SysTask::Dist;
    if (name == "$stacktrace") return SysTask::Stacktrace;
    if (name == "$urandom") return SysTask::Urandom;
    if (name == "$urandom_range") return SysTask::UrandomRange;
    if (name == "$dumpfile") return SysTask::DumpFile;
    if (name == "$dumpvars") return SysTask::DumpVars;
    if (name == "$dumpoff") return SysTask::DumpOff;
    if (name == "$dumpon") return SysTask::DumpOn;
    if (name == "$dumpall") return SysTask::DumpAll;
    if (name == "$dumplimit") return SysTask::DumpLimit;
    if (name == "$dumpflush") return SysTask::DumpFlush;
    if (name == "$exit") return SysTask::Exit;
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
    if (name == "$sformatf.runtime") return SysTask::SFormatRuntime;
    if (name == "$fatal") return SysTask::Fatal;
    if (name == "$error") return SysTask::Error;
    if (name == "$warning") return SysTask::Warning;
    if (name == "$info") return SysTask::Info;
    if (name == "$countbits") return SysTask::CountBits;
    if (name == "$test$plusargs") return SysTask::TestPlusargs;
    if (name == "$sscanf") return SysTask::Sscanf;
    if (name == "$value$plusargs") return SysTask::ValuePlusargs;
    if (name == "$timeformat") return SysTask::TimeFormat;
    if (name == "$monitoron") return SysTask::MonitorOn;
    if (name == "$monitoroff") return SysTask::MonitorOff;
    for (const MathName& m : kMath)
        if (m.name == name) return m.fn1 != nullptr ? SysTask::Math1 : SysTask::Math2;
    return SysTask::Unknown;
}

/// The operands of a system task's syscall that its handler reads by
/// position: the name, then those.
std::size_t minOperands(SysTask t) {
    switch (t) {
        case SysTask::SFormatRuntime: return 5;
        case SysTask::FSeek: return 4;
        case SysTask::UrandomRange: return 3;
        case SysTask::Dist: return 3;
        case SysTask::Fatal:
        case SysTask::Math2:
        case SysTask::FGets:
        case SysTask::Ungetc:
        case SysTask::FScanf:
        case SysTask::FRead:
        case SysTask::FError:
        case SysTask::ReadMem:
        case SysTask::WriteMem:
        case SysTask::Sscanf:
        case SysTask::ValuePlusargs: return 3;
        case SysTask::Error:
        case SysTask::Warning:
        case SysTask::Info:
        case SysTask::CountBits:
        case SysTask::Finish:
        case SysTask::Stop:
        case SysTask::Time:
        case SysTask::Realtime:
        case SysTask::TestPlusargs:
        case SysTask::FDisplay:
        case SysTask::FWrite:
        case SysTask::FOpen:
        case SysTask::FClose:
        case SysTask::FGetc:
        case SysTask::FTell:
        case SysTask::Rewind:
        case SysTask::FEof:
        case SysTask::Rtoi:
        case SysTask::RealToBits:
        case SysTask::BitsToReal:
        case SysTask::ShortRealToBits:
        case SysTask::BitsToShortReal:
        case SysTask::Math1:
        case SysTask::DumpFile:
        case SysTask::DumpLimit:
        case SysTask::Stacktrace:
        case SysTask::Clog2: return 2;
        default: return 1;
    }
}

/// What a run-time warning says of a descriptor that names a file that
/// is not open (SYSTEM_TASKS.md 6.1).
std::string descriptorText(std::uint32_t a) {
    std::string hex(8, '0');
    for (int i = 7, v = 0; i >= 0; --i, v += 4)
        hex[static_cast<std::size_t>(i)] = "0123456789abcdef"[(a >> v) & 0xFU];
    return std::string((a & 0x8000'0000U) != 0 ? "file descriptor" : "multichannel descriptor") +
           " 32'h" + hex;
}

std::string notOpenText(W4 d) {
    if (static_cast<std::uint32_t>(d.b) != 0) return "the descriptor has x or z bits";
    return descriptorText(static_cast<std::uint32_t>(d.a)) + " names a file that is not open";
}

/// The same for input, which takes an fd: an mcd names no file to read
/// (SYSTEM_TASKS.md 6.3).
std::string notReadableText(W4 d) {
    const auto a = static_cast<std::uint32_t>(d.a);
    if (static_cast<std::uint32_t>(d.b) == 0 && (a & 0x8000'0000U) == 0)
        return descriptorText(a) + " is not a file descriptor, which input takes";
    return notOpenText(d);
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

// shortreal is C's float and real C's double (VALUE_SEMANTICS.md 1, 7.4). A
// double out of float's range becomes an infinity, as IEEE 754 rounds it;
// C++ leaves that to the implementation only when the infinities are
// values of float, which they are with IEEE 754.
static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559,
              "shortreal and real need IEEE 754 float and double");

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
    /// How values without a specifier print: 'd', or the radix of a b, o
    /// or h form (SYSTEM_TASKS.md 3.1).
    char radix = 'd';
    std::uint32_t driver = 0;  ///< For drive.delay: its kernel driver.
    /// The kind a text operand names, read once: "force" of an override
    /// instruction, "casex" of match.mask, "final" of report.call.
    bool textFlag = false;
    /// Whether executeWide carries the instruction out: one it handles,
    /// with a result or an operand of more than one chunk. The types do
    /// not change while the design runs, so this is decided once.
    bool wide = false;
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

class Interpreter;

/// The frame of a process run by the VM (RUNTIME_ABI.md 4.3): where it
/// goes on when the kernel resumes it, and its frame variables.
/// A body the VM runs: process p is body p, and subroutine s is body
/// processCount() + s. It has its own type, so that neither a process row
/// nor a subroutine number can be given for one; Interpreter::bodyOf and
/// bodyOfProcess make them.
struct BodyTag;
using BodyId = sir::Id<BodyTag>;
/// A live activation's number, which refs to its frame variables carry
/// (Interpreter::enroll).
struct VarsTag;
using VarsId = sir::Id<VarsTag>;

struct Frame {
    Interpreter* vm = nullptr;
    sir::ProcessId process{0};                   ///< Row in the SIR process table.
    kernel::ProcessId kernelId = kernel::kNone;  ///< Id in the kernel, once registered.
    sir::BlockId block{0};                       ///< Block to resume at.
    std::vector<W4> vars;                        ///< Frame variables (SIR_SPEC.md 7.4).
    VarsId varsId{0};                            ///< Their number for refs (Interpreter::enroll).

    // Process control (SCHEDULER_SPEC.md 10).
    enum class Join : std::uint8_t { None, All, Any, WaitFork };
    Frame* parent = nullptr;  ///< The process that forked this one.
    /// The children whose records are kept: running, or ended with
    /// descendants left (SCHEDULER_SPEC.md 10.8).
    std::vector<Frame*> children;
    std::uint32_t liveChildren = 0;
    // Birth and group numbers are 64 bits: a frame that forks on every
    // clock edge makes more than 2^32 children in a long run (#36).
    std::uint64_t births = 0;   ///< Children made so far: the next one's birth number.
    std::uint64_t birth = 0;    ///< Its birth number among its parent's children.
    std::uint32_t place = 0;    ///< Its place in parent->children.
    std::uint32_t reports = 0;  ///< Its deferred assertion calls on the kernel's queues.
    bool finished = false;
    bool released = false;        ///< Its record was released; the frame waits to be reused.
    std::uint64_t group = 0;      ///< The parent's fork group it belongs to (0: none).
    std::uint64_t forkGroup = 0;  ///< This frame's latest join or join_any group.
    std::uint32_t groupSize = 0;
    std::uint32_t groupDone = 0;
    Join pending = Join::None;                  ///< What the frame waits for before going on.
    kernel::EventId joinEvent = kernel::kNone;  ///< Triggered when a child ends.
    /// An override process: whether it holds a target and so runs when what
    /// it reads changes (SIR_SPEC.md 5.6), and at which level.
    bool holding = false;
    kernel::Hold level = kernel::Hold::Assign;
    /// The generator of $urandom (SYSTEM_TASKS.md 13.2): the frame's own,
    /// for a procedure or a fork branch.
    kernel::Rng rng;
    bool ownRng = false;
    /// The guard of the wait.cond it waits at (SIR_SPEC.md 7.2).
    sir::SubroutineId cond{0};
    /// A guard or a watch of the event control it waits at, which the
    /// kernel calls with a pointer to this record (SIR_SPEC.md 7.2).
    struct Callback {
        Frame* frame = nullptr;
        sir::SubroutineId body{0};
    };
    std::vector<Callback> iffs;     ///< The items' iff guards.
    std::vector<Callback> watches;  ///< The items on expressions.

    /// Calls of tasks that may wait, innermost last (SIR_SPEC.md 7.4): each
    /// keeps its frame variables and the block to go on at. Their temps do
    /// not live across the call.task, so they start at 0.
    struct TaskCall {
        BodyId body{0};
        std::vector<W4> vars;
        VarsId varsId{0};
        sir::BlockId block{0};
        const sir::Instr* site = nullptr;  ///< The call.task in the caller.
        std::uint64_t children = 0;        ///< The frame's births when it was called.
    };
    std::vector<TaskCall> calls;

    // disable (SIR_SPEC.md 7.2, SCHEDULER_SPEC.md 10.4). Where the frame
    // stands: its task calls, and the calls run on the C++ stack (call)
    // within the innermost one, each with the frame's births then.
    struct SyncCall {
        BodyId body{0};
        std::uint64_t children = 0;
        const sir::Instr* site = nullptr;  ///< The call in the caller; none for a deferred one.
    };
    std::vector<SyncCall> syncCalls;
    /// The activations of scoped named blocks, innermost last: the block,
    /// its exit, and where the frame stood when it began.
    struct ScopeAct {
        std::uint32_t scope = 0;
        sir::BlockId exit{0};
        std::uint32_t calls = 0;
        std::uint32_t sync = 0;
        std::uint64_t children = 0;
    };
    std::vector<ScopeAct> scopes;
    /// Where a disable sends the running frame: the task calls and
    /// synchronous calls it keeps, and the block it goes on at, or kNone to
    /// go on after the call it ends.
    struct Unwind {
        std::uint32_t calls = 0;
        std::uint32_t sync = 0;
        sir::BlockId exit = sir::kNone;
    };
    std::optional<Unwind> unwind;
};
// The numbers that count forks over a whole run stay 64 bits (#36).
static_assert(sizeof(Frame::births) == 8 && sizeof(Frame::birth) == 8);
static_assert(sizeof(Frame::group) == 8 && sizeof(Frame::forkGroup) == 8);
static_assert(sizeof(Frame::TaskCall::children) == 8 && sizeof(Frame::SyncCall::children) == 8 &&
              sizeof(Frame::ScopeAct::children) == 8);

class Interpreter {
public:
    Interpreter(const sir::Design& d, kernel::Context& ctx, const RunOptions& options)
        : d_(d), sched_(ctx.scheduler()), store_(ctx.store()), output_(ctx.output()),
          files_(ctx.files()), dump_(ctx.dump()), randomSeed_(ctx.randomSeed()),
          strings_(ctx.strings()), heap_(ctx.heap()), counters_(ctx.counters()),
          stack_(ctx.stack()), options_(options) {
        auto patterns = sir::validatePatterns(d);
        if (!patterns.errors.empty()) loadError_ = "invalid pattern layout metadata";
        patternShapes_ = std::move(patterns.shapes);
        timeFormat_.unit = d.timePrecision;
        consts_.reserve(d.constants.size());
        constOff_.reserve(d.constants.size());
        for (const sir::Constant& c : d.constants) {
            constOff_.push_back(static_cast<std::uint32_t>(consts_.size()));
            if (c.type.kind == sir::TypeKind::String) {
                consts_.push_back({strings_.intern(c.bytes), 0});
                continue;
            }
            for (std::uint32_t k = 0; k < chunksOf(c.type); ++k)
                consts_.push_back(
                    {k < c.aWords.size() ? c.aWords[k] : 0, k < c.bWords.size() ? c.bWords[k] : 0});
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
        // A value of more than 64 bits takes consecutive W4 chunks, the
        // least significant first, like a slot element (kernel/wide.h). The
        // frame variables come in order, one chunk each when narrow, and the
        // elements of the arrays after them.
        layouts_.resize(bodies);
        for (std::uint32_t p = 0; p < bodies; ++p) {
            const auto& frame = bodyFrame(BodyId{p});
            Layout& l = layouts_[p];
            l.offset.assign(frame.size(), 0);
            l.chunks.assign(frame.size(), 1);
            l.size = 0;
            for (std::size_t i = 0; i < frame.size(); ++i) {
                const bool ref = frame[i].type.kind == sir::TypeKind::Ref;
                l.chunks[i] = ref ? 1 : chunksOf(frame[i].type);
                l.offset[i] = l.size;
                l.size += frame[i].dims.empty() || ref ? l.chunks[i] : 1;
                if (ref || sir::isContainer(frame[i].type)) l.roots.push_back(i);
            }
            for (std::size_t i = 0; i < frame.size(); ++i) {
                if (frame[i].dims.empty() || frame[i].type.kind == sir::TypeKind::Ref) continue;
                l.offset[i] = l.size;
                l.size += sir::elementCount(frame[i]) * l.chunks[i];
            }
        }
        for (std::uint32_t p = 0; p < bodies; ++p) {
            const auto& blocks = bodyBlocks(BodyId{p});
            const std::uint32_t n = numTemps(BodyId{p});
            tempTypes_[p].resize(n);
            info_[p].resize(blocks.size());
            for (std::size_t b = 0; b < blocks.size(); ++b) {
                for (const sir::BlockParam& bp : blocks[b].params)
                    if (bp.temp.index() < n) tempTypes_[p][bp.temp.index()] = bp.type;
                for (const sir::Instr& in : blocks[b].instrs) {
                    if (in.result != sir::kNone && in.result.index() < n)
                        tempTypes_[p][in.result.index()] = in.type;
                    info_[p][b].push_back(decode(in));
                    // Each drive.delay keeps its own pending update
                    // (SIR_SPEC.md 5.4).
                    if (in.op == Op::DriveDelay)
                        info_[p][b].back().driver = sched_.addDelayedDriver();
                }
            }
        }
        // The temps of a body, one chunk each when narrow.
        tempOff_.resize(bodies);
        tempTotal_.assign(bodies, 0);
        for (std::uint32_t p = 0; p < bodies; ++p) {
            std::uint32_t at = 0;
            tempOff_[p].resize(tempTypes_[p].size());
            for (std::size_t t = 0; t < tempTypes_[p].size(); ++t) {
                tempOff_[p][t] = at;
                at += chunksOf(tempTypes_[p][t]);
            }
            tempTotal_[p] = at;
        }
        // After every temp has its type, which a later block may give.
        for (std::uint32_t p = 0; p < bodies; ++p) {
            const auto& blocks = bodyBlocks(BodyId{p});
            for (std::size_t b = 0; b < blocks.size(); ++b) {
                for (std::size_t k = 0; k < blocks[b].instrs.size(); ++k) {
                    const sir::Instr& in = blocks[b].instrs[k];
                    info_[p][b][k].wide = wideHandles(in.op) && isWide(BodyId{p}, in);
                }
            }
        }
    }

    RunResult run() {
        if (!loadError_.empty()) return {kernel::EndReason::Internal, loadError_};
        stack_ = kernel::StackRange::ofThisThread();
        if (store_.size() != 0) return {kernel::EndReason::Internal, "the store is not empty"};
        struct RestoreHook {
            Interpreter& vm;
            kernel::Scheduler& scheduler;
            kernel::SafePointHook old;
            ~RestoreHook() {
                vm.dropHeapWaits();
                scheduler.setSafePointHook(old);
            }
        } hook{*this, sched_, sched_.setSafePointHook({})};
        kernelSlots_.clear();
        for (const sir::Slot& s : d_.slots) {
            const sir::Domain dom = sir::domainOf(s.type);
            kernelSlots_.push_back(store_.add(
                kernelDomain(dom), s.type.width, static_cast<std::uint32_t>(sir::elementCount(s)),
                dom == sir::Domain::Four ? kernelInit(sir::initialValue(s.kind))
                                         : kernel::Init::Zero));
            if (heapSlots_.size() <= kernelSlots_.back().index())
                heapSlots_.resize(kernelSlots_.back().index() + 1, false);
            heapSlots_[kernelSlots_.back().index()] = sir::isContainer(s.type);
            if (sir::isContainer(s.type))
                rootSlots_.push_back(
                    sir::SlotId{static_cast<std::uint32_t>(kernelSlots_.size() - 1)});
        }
        // The bits a driver drives are x until it first writes them
        // (VALUE_SEMANTICS.md 5.3, SIR_SPEC.md 3.5).
        for (const sir::SlotRange& r : sir::unknownAtStart(d_)) {
            const kernel::SlotLayout& l = store_.layout(kernelSlot(r.slot));
            const std::uint32_t lsb = r.width == 0 ? 0 : r.lsb;
            const std::uint32_t width = r.width == 0 ? l.width : r.width;
            const std::uint32_t n = r.numElems == 0 ? l.elements : r.numElems;
            for (std::uint32_t e = r.firstElem; e < r.firstElem + n; ++e) {
                std::uint64_t* p = store_.element(kernelSlot(r.slot), e);
                for (std::uint32_t done = 0; done < width;) {
                    const unsigned k = width - done < 64 ? width - done : 64U;
                    kernel::writeField(p, 2, lsb + done, k, kernel::mask(k));
                    kernel::writeField(p + 1, 2, lsb + done, k, kernel::mask(k));
                    done += k;
                }
            }
        }
        // Every element of an event slot holds its own event from the
        // start; the handle is the event's number plus 1, 0 being null
        // (SIR_SPEC.md 3.2.1).
        for (std::uint32_t i = 0; i < d_.slots.size(); ++i) {
            const sir::SlotId s{i};
            if (d_.slot(s).type.kind != sir::TypeKind::Event) continue;
            const auto n = static_cast<std::uint32_t>(sir::elementCount(d_.slot(s)));
            for (std::uint32_t e = 0; e < n; ++e)
                *store_.element(kernelSlot(s), e) = kernel::eventHandle(sched_.addEvent());
        }
        // The slots assign and force may hold (SCHEDULER_SPEC.md 14.4). A
        // net, or a variable that a continuous assignment drives, gets its
        // drivers' value back when released; what a function the assignment
        // calls writes is not driven (14.3, SIR_SPEC.md 3.4).
        std::vector<bool> driven(d_.slots.size(), false);
        for (const sir::Process& proc : d_.processes)
            for (const sir::SlotRange& w : proc.drives)
                driven[w.slot.index()] = true;
        for (std::uint32_t i = 0; i < d_.slots.size(); ++i)
            if ((d_.slots[i].flags & sir::kForceTarget) != 0)
                sched_.markHeld(kernelSlot(sir::SlotId{i}), d_.slots[i].kind != sir::SlotKind::Var,
                                driven[i]);
        // Static initializers first, in process order, run on the spot
        // rather than scheduled (SIR_SPEC.md 7.1, SCHEDULER_SPEC.md 6.1).
        // Each is a process of the kernel, with a pending queue whose
        // reports are settled in the Observed region of time 0 (15.3). A
        // run-time error of the design ends the run with the reason error,
        // after which no other process runs but final does (16.2).
        std::uint64_t initSteps = 0;
        // The generators of $urandom (SYSTEM_TASKS.md 13.2): one for each
        // instance and package, from the seed and its name; the procedures
        // take theirs from it in order, before any static initializer.
        initRngs_.clear();
        for (const sir::Instance& i : d_.instances)
            initRngs_.push_back({kernel::initialState(options_.seed, i.scopeName)});
        std::vector<kernel::Rng> procRngs(d_.processes.size());
        for (std::uint32_t p = 0; p < d_.processes.size(); ++p)
            if (isProcedure(d_.processes[p]))
                procRngs[p] = initRngs_[d_.processes[p].instance.index()].child();
        // The processes of $strobe and $monitor first: an initializer's
        // function may call them, and they wait on nothing until then.
        overrides_.assign(d_.processes.size(), nullptr);
        for (std::uint32_t p = 0; p < d_.processes.size(); ++p) {
            if (d_.processes[p].kind != sir::ProcessKind::Postponed) continue;
            Frame& f = newFrame(sir::ProcessId{p});
            f.kernelId = sched_.addProcess(&resume, &f, kernel::Group::Active);
            overrides_[p] = &f;
        }
        for (std::uint32_t p = 0; p < d_.processes.size() && !sched_.ending(); ++p) {
            if (d_.processes[p].kind != sir::ProcessKind::StaticInit) continue;
            Frame& f = newFrame(sir::ProcessId{p});
            f.kernelId = sched_.addProcess(&resume, &f, kernel::Group::Active);
            running_ = &f;
            const kernel::Status st = runProcess(f, initSteps);
            running_ = nullptr;
            if (st == kernel::Status::Error && sched_.reason() == kernel::EndReason::Internal)
                return {kernel::EndReason::Internal, sched_.error()};
            ended(f);
            sched_.ended(f.kernelId);
        }
        std::vector<kernel::ProgramId> programs(d_.instances.size(), kernel::kNone);
        for (std::size_t i = 0; i < d_.instances.size(); ++i)
            if (d_.instances[i].kind == sir::InstanceKind::Program)
                programs[i] = sched_.addProgram();
        std::vector<kernel::ProcessId> combs;
        std::vector<kernel::ProcessId> finals;
        for (std::uint32_t p = 0; p < d_.processes.size(); ++p) {
            const sir::Process& proc = d_.processes[p];
            if (proc.parent.kind != sir::Owner::Kind::None) continue;  // made by fork
            if (proc.kind == sir::ProcessKind::StaticInit) continue;   // already run
            if (proc.kind == sir::ProcessKind::Postponed) continue;    // registered above
            Frame& f = newFrame(sir::ProcessId{p});
            if (isProcedure(proc)) {
                f.rng = procRngs[p];
                f.ownRng = true;
            }
            switch (proc.kind) {
                case sir::ProcessKind::Initial:
                case sir::ProcessKind::General:
                // A continuous assignment is evaluated once at time 0, in
                // Active like the procedures (SCHEDULER_SPEC.md 6.1).
                case sir::ProcessKind::Cont:
                    if (proc.kind == sir::ProcessKind::Initial &&
                        programs[proc.instance.index()] != kernel::kNone)
                        f.kernelId =
                            sched_.addProgramInitial(&resume, &f, programs[proc.instance.index()]);
                    else
                        f.kernelId = sched_.addProcess(&resume, &f, processGroup(proc));
                    sched_.start(f.kernelId);
                    break;
                case sir::ProcessKind::Ff:
                case sir::ProcessKind::FfPlain:
                // always @* does not run at time 0 (SCHEDULER_SPEC.md 6.2).
                case sir::ProcessKind::CombAt:
                    f.kernelId = sched_.addProcess(&resume, &f, processGroup(proc));
                    waitTriggers(f, proc);
                    break;
                case sir::ProcessKind::Comb:
                case sir::ProcessKind::Latch:
                    // Started once at time 0 after every initial and always
                    // (SCHEDULER_SPEC.md 6.1).
                    f.kernelId = sched_.addProcess(&resume, &f, processGroup(proc));
                    combs.push_back(f.kernelId);
                    break;
                case sir::ProcessKind::Final:
                    // Run once each when the simulation ends.
                    f.kernelId = sched_.addProcess(&resume, &f, processGroup(proc));
                    finals.push_back(f.kernelId);
                    break;
                case sir::ProcessKind::Override:
                    // Runs from override.start to override.stop.
                    f.kernelId = sched_.addProcess(&resume, &f, processGroup(proc));
                    overrides_[p] = &f;
                    break;
                default:
                    return {kernel::EndReason::Internal, "process \"" + proc.name + "\": kind " +
                                                             sir::kindName(proc.kind) +
                                                             " is not supported by the VM yet"};
            }
        }
        dump_.setup(dumpSetup());
        // After the kernel's own counters, which the first process registers.
        cInstrs_ = counters_.add("vm.instrs");
        counters_.increment(cInstrs_, initSteps);
        for (const kernel::ProcessId k : combs)
            sched_.start(k);
        sched_.run();
        sched_.runFinal(finals);
        if (!boundsCounts_.empty()) {
            std::uint64_t count = 0;
            for (const auto& entry : boundsCounts_)
                count += entry.second;
            output_.message("kirimasim: info: bounds check: " + std::to_string(count) +
                            " invalid accesses at " + std::to_string(boundsCounts_.size()) +
                            " source positions");
        }
        dump_.finish(sched_);
        const kernel::EndReason reason = sched_.reason();
        return {reason, reason == kernel::EndReason::Internal ? sched_.error() : std::string()};
    }

    /// The monitor record of a wait.cond evaluates its guard (SCHEDULER_SPEC.md
    /// 9.5): 1 when true.
    static void evalCond(kirima_context* /*ctx*/, void* frame, std::uint64_t* out) {
        auto* f = static_cast<Frame*>(frame);
        Interpreter& vm = *f->vm;
        std::uint64_t steps = 0;
        out[0] = vm.guard(*f, f->cond, steps).value_or(false) ? 1U : 0U;
        vm.counters_.increment(vm.cInstrs_, steps);
    }

    /// An iff guard of an event-control item: nonzero when true.
    static int evalIff(kirima_context* /*ctx*/, void* iff) {
        const auto* g = static_cast<const Frame::Callback*>(iff);
        Interpreter& vm = *g->frame->vm;
        std::uint64_t steps = 0;
        const bool on = vm.guard(*g->frame, g->body, steps).value_or(false);
        vm.counters_.increment(vm.cInstrs_, steps);
        return on ? 1 : 0;
    }

    /// A watch's value, laid out like a slot element of its type.
    static void evalWatch(kirima_context* /*ctx*/, void* watch, std::uint64_t* out) {
        const auto* w = static_cast<const Frame::Callback*>(watch);
        Interpreter& vm = *w->frame->vm;
        std::uint64_t steps = 0;
        W4 ret{0, 0};
        if (vm.runWaiter(*w->frame, w->body, steps, ret)) {
            const sir::Type t = vm.d_.subroutine(w->body).returnType;
            const std::uint32_t c = chunksOf(t);
            const W4* v = c > 1 ? vm.retWide_.data() : &ret;
            const std::vector<std::uint64_t> words = packed(v, c, sir::isFourState(t));
            std::copy(words.begin(), words.end(), out);
        }
        vm.counters_.increment(vm.cInstrs_, steps);
    }

    /// A deferred assertion's call, confirmed (run) or dropped.
    static void deferredCall(kirima_context* /*ctx*/, void* frame, std::uint32_t token, int run) {
        auto* f = static_cast<Frame*>(frame);
        Interpreter& vm = *f->vm;
        Deferred d = std::move(vm.deferred_[token]);
        vm.deferred_[token] = {};
        vm.freeDeferred_.push_back(token);
        if (run != 0) {
            std::uint64_t steps = 0;
            vm.callWith(*f, d.subroutine, d.args, steps);
            vm.counters_.increment(vm.cInstrs_, steps);
        }
        --f->reports;
        vm.tryRelease(*f);
    }

    static kirima_status resume(kirima_context* /*ctx*/, void* frame) {
        auto* f = static_cast<Frame*>(frame);
        Interpreter& vm = *f->vm;
        vm.sweep();
        std::uint64_t steps = 0;
        vm.running_ = f;
        const kernel::Status st = vm.runProcess(*f, steps);
        vm.counters_.increment(vm.cInstrs_, steps);
        if (st == kernel::Status::Done || st == kernel::Status::Killed) vm.ended(*f);
        vm.running_ = nullptr;
        return static_cast<kirima_status>(st);
    }

private:
    const sir::Design& d_;
    kernel::Scheduler& sched_;
    kernel::Store& store_;
    kernel::Output& output_;
    kernel::Files& files_;
    kernel::Dump& dump_;
    std::int32_t& randomSeed_;  ///< $random with no argument (SYSTEM_TASKS.md 12.3).
    kernel::Strings& strings_;
    kernel::Heap& heap_;
    std::vector<sir::PatternShape> patternShapes_;
    std::vector<kernel::HeapTypeId> heapTypes_;
    bool heapReady_ = false;
    bool heapWaits_ = false;
    bool heapMonitorReported_ = false;
    std::set<std::array<std::uint32_t, 3>> heapWarned_;
    std::vector<bool> heapSlots_;
    std::vector<sir::SlotId> rootSlots_;
    std::vector<kernel::HeapRoot> roots_, publishedRoots_;
    std::vector<bool> rootActivationsSeen_;
    std::vector<VarsId> rootActivations_;
    sir::SourceLoc heapSite_;
    sir::InstanceId heapInstance_{0};
    /// The places that gave a run-time warning, which each gives once
    /// (UNSPECIFIED_BEHAVIOR.md 4).
    std::set<const sir::Instr*> warned_;
    /// One source access can lower to several instructions (an update,
    /// or the leaves of a structure). Bounds warnings name that access.
    std::map<std::array<std::uint32_t, 3>, std::uint64_t> boundsCounts_;
    kernel::Counters& counters_;
    kernel::StackRange& stack_;  ///< Read when the run starts (RUNTIME_ABI.md 5).
    kernel::CounterId cInstrs_{0};
    RunOptions options_;
    kernel::TimeFormat timeFormat_;
    std::string loadError_;
    std::vector<W4> consts_;
    std::vector<std::uint32_t> constOff_;  ///< By constant: its first chunk in consts_.
    /// By body and temp: where the temp's chunks start above the body's
    /// base; and by body, the chunks of all its temps.
    std::vector<std::vector<std::uint32_t>> tempOff_;
    std::vector<std::uint32_t> tempTotal_;
    std::vector<std::vector<sir::Type>> tempTypes_;          ///< [process][temp]
    std::vector<std::vector<std::vector<InstrInfo>>> info_;  ///< [process][block][instr]
    /// The temps of the running body and of the calls below it. Temps never
    /// live across a wait (SIR_SPEC.md 5.1), so a process starts at 0.
    std::vector<W4> tempStack_;
    /// The body being run: a process or a called subroutine, its frame
    /// variables and where its temps start.
    struct Act {
        BodyId body{0};
        std::vector<W4>* vars = nullptr;
        std::size_t base = 0;
        VarsId varsId{0};
    };
    /// Where the frame variables of a body lie (RUNTIME_ABI.md 4.3): a word
    /// pair for each, by number, then the elements of each array in order.
    struct Layout {
        std::size_t size = 0;
        /// By frame variable: where its value starts, or an array's first
        /// element; and the chunks of the value or of one element.
        std::vector<std::size_t> offset;
        std::vector<std::uint32_t> chunks;
        std::vector<std::size_t> roots;  ///< Typed container/reference fields only.
        /// Where frame variable fv starts among the activation's words, and
        /// its chunks: the one place a frame variable's number becomes a
        /// word offset (#1 took one for the other).
        [[nodiscard]] std::size_t wordOf(sir::FrameVarId fv) const { return offset[fv.index()]; }
        [[nodiscard]] std::uint32_t chunksOfVar(sir::FrameVarId fv) const {
            return chunks[fv.index()];
        }
    };
    std::vector<Layout> layouts_;              ///< By body.
    std::vector<kernel::SlotId> kernelSlots_;  ///< By SIR slot: the store's number.
    std::vector<kernel::UdpTable> udpTables_;  ///< By type-table row.
    /// The initialization generator of each row of the instance table
    /// (SYSTEM_TASKS.md 13.2).
    std::vector<kernel::Rng> initRngs_;
    /// The frame variables of each live activation, by number, for refs
    /// (SIR_SPEC.md 8): a ref to a frame variable is its activation's
    /// number and the word's place in it.
    struct VarSet {
        W4* words = nullptr;
        std::size_t size = 0;
        BodyId body{0};
        bool live = false;
    };
    std::vector<VarSet> varSets_;
    std::vector<VarsId> freeVarSets_;
    Act act_;
    std::uint32_t callDepth_ = 0;  ///< Calls running on the C++ stack.

    /// One more call of f would nest deeper than --max-call-depth allows,
    /// or the stack of this thread is nearly used up: reports it at the
    /// call and ends the run with resource_limit (SCHEDULER_SPEC.md 16.2,
    /// RUNTIME_ABI.md 5).
    bool tooDeep(const Frame& f, const sir::Instr& in) {
        const sir::InstanceId inst = instanceOf(d_.process(f.process));
        if (f.calls.size() + callDepth_ >= options_.maxCallDepth) {
            const std::string n = std::to_string(options_.maxCallDepth);
            limitAt(inst, in.loc,
                    "calls nested more than " + n + " deep (--max-call-depth=" + n + ")");
            return true;
        }
        if (!stack_.nearlyUsedUp()) return false;
        stackLimit(inst, in.loc);
        return true;
    }
    /// The stack of this thread is nearly used up where a call of the
    /// design would start (RUNTIME_ABI.md 5).
    void stackLimit(sir::InstanceId inst, sir::SourceLoc loc) {
        limitAt(inst, loc, "the stack of this thread is nearly used up");
    }
    /// A limit on resources reached at loc (CLI.md 2.7): the line of a
    /// run-time error of the design, then the end of the run, reason
    /// resource_limit (SCHEDULER_SPEC.md 16.2).
    void limitAt(sir::InstanceId inst, sir::SourceLoc loc, const std::string& what) {
        output_.message(runtimeLine(inst, loc, what));
        sched_.resourceLimit();
    }
    bool warnedRepl_ = false;  ///< The warning of VALUE_SEMANTICS.md 7.3 is given once.

    std::uint32_t processCount() const { return static_cast<std::uint32_t>(d_.processes.size()); }
    /// The body of subroutine s, and of process p: the one place these
    /// numbers become a body.
    [[nodiscard]] BodyId bodyOf(sir::SubroutineId s) const {
        return BodyId{processCount() + s.index()};
    }
    static BodyId bodyOfProcess(sir::ProcessId p) { return BodyId{p.index()}; }
    bool isSubBody(BodyId body) const { return body.index() >= processCount(); }
    const sir::Subroutine& subOf(BodyId body) const {
        return d_.subroutines[body.index() - processCount()];
    }
    const sir::Process& processOf(BodyId body) const { return d_.processes[body.index()]; }
    const std::vector<sir::BasicBlock>& bodyBlocks(BodyId body) const {
        return isSubBody(body) ? subOf(body).blocks : processOf(body).blocks;
    }
    const std::vector<sir::FrameVar>& bodyFrame(BodyId body) const {
        if (!isSubBody(body)) return processOf(body).frame;
        const sir::Subroutine& s = subOf(body);
        // A guard or a watch reads the frame of the body that waits (SIR_SPEC.md 7.2).
        if (!sir::readsWaiterFrame(s.kind)) return s.frame;
        return s.frameOf.kind == sir::Owner::Kind::Process
                   ? d_.process(s.frameOf.asProcess()).frame
                   : d_.subroutine(s.frameOf.asSubroutine()).frame;
    }
    std::uint32_t numTemps(BodyId body) const {
        return isSubBody(body) ? subOf(body).numTemps : processOf(body).numTemps;
    }
    /// The chunks a body's temps take on the temp stack.
    std::uint32_t bodyTemps(BodyId body) const { return tempTotal_[body.index()]; }
    W4& temp(sir::TempId id) {
        return tempStack_[act_.base + tempOff_[act_.body.index()][id.index()]];
    }
    /// The W4 chunks of a value of type t (kernel/wide.h).
    static std::uint32_t chunksOf(sir::Type t) {
        return sir::isIntegral(t) && t.width > 64 ? (t.width - 1) / 64 + 1 : 1;
    }
    /// Enters a body: its temps above the current ones.
    void enter(Act a) {
        act_ = a;
        if (tempStack_.size() < a.base + bodyTemps(a.body))
            tempStack_.resize(a.base + bodyTemps(a.body));
    }
    std::deque<Frame> frames_;  ///< A deque, so that frames never move.
    /// Released frames to reuse, frames to look at again, and the frame
    /// running now (SCHEDULER_SPEC.md 10.8).
    std::vector<Frame*> freeFrames_;
    std::vector<Frame*> later_;
    /// The $fmonitor frames, which outlive the call that made them until
    /// their files close (SYSTEM_TASKS.md 6.2).
    std::vector<Frame*> fileMonitorFrames_;
    std::vector<W4> retWide_;  ///< A function's result wider than 64 bits, at its ret.
    Frame* running_ = nullptr;
    std::vector<kernel::EventId> freeJoinEvents_;  ///< Join events of released frames.
    std::vector<kernel::WaitItem> items_;
    std::vector<kernel::NamedWaitItem> namedItems_;  ///< Scratch for wait.event.
    std::vector<kernel::MonitorSpec> monitors_;      ///< Scratch for wait.event.
    std::vector<kernel::StrengthDriver> drivers_;    ///< Scratch for resolve.
    std::vector<kernel::ScanValue> scanValues_;      ///< Scratch for $sscanf.
    /// By process: the frame of an override or postponed process.
    std::vector<Frame*> overrides_;
    Frame* monitor_ = nullptr;  ///< The postponed process of the $monitor in effect.
    /// The calls of deferred assertions on the kernel's queues: the function
    /// and its arguments' values (SIR_SPEC.md 5.7).
    struct Deferred {
        sir::SubroutineId subroutine{0};
        std::vector<std::vector<W4>> args;  ///< Each argument's chunks.
    };
    std::vector<Deferred> deferred_;
    std::vector<std::uint32_t> freeDeferred_;
    /// What a guard depends on, found on its first wait (SCHEDULER_SPEC.md
    /// 9.5): the slot parts it and the functions it calls read, and the
    /// event slots among them, whose events `triggered` may read.
    struct GuardDeps {
        bool heap = false;  ///< The body may read container structure or cell values.
        Interpreter* vm = nullptr;
        std::vector<sir::SlotId> privateContainers;
        /// What the body reads, as the SIR's ranges while they are found
        /// (collectReads), and as the kernel's parts once depsOf has left
        /// out the called functions' variables.
        std::vector<sir::SlotRange> reads;
        std::vector<kernel::SlotPart> slots;
        std::vector<sir::SlotId> events;
        /// What the body reads through a ref of the waiter's frame: the
        /// frame variable, the leaf of a structure, and whether any element
        /// of an array is read. Found when the wait starts (readsOf).
        struct RefRead {
            sir::FrameVarId var{0};
            std::uint32_t leaf = 0;
            bool anyElement = false;
        };
        std::vector<RefRead> refs;
    };
    std::vector<std::optional<GuardDeps>> guardDeps_;  ///< By body.

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
        switch (in.op) {
            case Op::OverrideStart:
            case Op::OverrideStop:
            case Op::StoreOverride:
                info.textFlag = !in.operands.empty() && in.operands[0].text == "force";
                return info;
            case Op::MatchMask:
                info.textFlag = in.operands.size() > 2 && in.operands[2].text == "casex";
                return info;
            case Op::ReportCall:
                info.textFlag = !in.operands.empty() && in.operands[0].text == "final";
                return info;
            default: break;
        }
        if (in.op != Op::Syscall || in.operands.empty()) return info;
        info.task = resolveSysTask(in.operands[0].text);
        // The handler reads these operands by position.
        if (in.operands.size() < minOperands(info.task) && loadError_.empty())
            loadError_ = "syscall " + in.operands[0].text + " with too few operands";
        const std::string& n = in.operands[0].text;
        if ((info.task == SysTask::Display || info.task == SysTask::Write ||
             info.task == SysTask::SFormatF || info.task == SysTask::FDisplay ||
             info.task == SysTask::FWrite) &&
            (n.back() == 'b' || n.back() == 'o' || n.back() == 'h'))
            info.radix = n.back();
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

    Frame& newFrame(sir::ProcessId process) {
        sweep();
        Frame* place = nullptr;
        if (freeFrames_.empty()) {
            place = &frames_.emplace_back();
        } else {
            place = freeFrames_.back();
            freeFrames_.pop_back();
            *place = Frame{};
        }
        Frame& f = *place;
        f.vm = this;
        f.process = process;
        f.vars = newVars(bodyOfProcess(process));
        f.varsId = enroll(f.vars, bodyOfProcess(process));
        return f;
    }

    // --- Process control (SCHEDULER_SPEC.md 10) ---

    /// Bookkeeping when a process ends, normally or killed: its parent may
    /// be waiting for it at a join or wait fork.
    void ended(Frame& c) {
        if (c.finished) return;
        c.finished = true;
        if (Frame* parent = c.parent; parent != nullptr) {
            --parent->liveChildren;
            if (c.group != 0 && c.group == parent->forkGroup) ++parent->groupDone;
            if (parent->pending != Frame::Join::None && !parent->finished &&
                parent->joinEvent != kernel::kNone)
                sched_.trigger(parent->joinEvent);
        }
        tryRelease(c);
    }

    /// Releases the record of a frame as soon as it may go (SCHEDULER_SPEC.md
    /// 10.8): it has ended, its children's records are released, and no
    /// report of it is left, then looks at its parent. The running frame,
    /// which the kernel still marks after it returns, and a frame whose
    /// reports the kernel still holds are looked at again later (sweep).
    void tryRelease(Frame& f) {
        if (f.released || !f.finished || !f.children.empty() || f.reports != 0) return;
        if (&f == running_ || sched_.hasPending(f.kernelId)) {
            later_.push_back(&f);
            return;
        }
        f.released = true;
        for (const Frame::TaskCall& c : f.calls)
            retire(c.varsId);
        retire(f.varsId);
        if (f.joinEvent != kernel::kNone) freeJoinEvents_.push_back(f.joinEvent);
        sched_.releaseProcess(f.kernelId);
        f.vars = {};
        f.calls = {};
        f.syncCalls = {};
        f.scopes = {};
        freeFrames_.push_back(&f);
        if (Frame* parent = f.parent; parent != nullptr) {
            Frame* last = parent->children.back();
            parent->children[f.place] = last;
            last->place = f.place;
            parent->children.pop_back();
            tryRelease(*parent);
        }
    }

    /// Looks again at the frames tryRelease left for later, and releases
    /// the frames of file monitors the kernel ended because their files
    /// closed (SYSTEM_TASKS.md 6.2).
    void sweep() {
        kernel::ProcessId endedProcess = kernel::kNone;
        while (sched_.takeEndedProgramProcess(endedProcess)) {
            for (Frame& f : frames_) {
                if (!f.released && f.kernelId == endedProcess) {
                    ended(f);
                    break;
                }
            }
        }
        while (!fileMonitorFrames_.empty() && sched_.takeEndedFileMonitor(endedProcess)) {
            for (std::size_t i = 0; i < fileMonitorFrames_.size(); ++i) {
                if (fileMonitorFrames_[i]->kernelId != endedProcess) continue;
                Frame& f = *fileMonitorFrames_[i];
                fileMonitorFrames_[i] = fileMonitorFrames_.back();
                fileMonitorFrames_.pop_back();
                ended(f);
                break;
            }
        }
        if (later_.empty()) return;
        // Retry only the entries present at the start. tryRelease may
        // append frames that still hold reports, or their parents; those
        // wait for the next sweep. Keep the queue's allocation for reuse.
        const std::size_t count = later_.size();
        for (std::size_t i = 0; i < count; ++i)
            tryRelease(*later_[i]);
        later_.erase(later_.begin(), later_.begin() + static_cast<std::ptrdiff_t>(count));
    }

    /// The children of a frame from birth number `from` on, in the order
    /// they were made: a copy, since ending them may release them.
    static std::vector<Frame*> childrenFrom(const Frame& f, std::uint64_t from) {
        std::vector<std::pair<std::uint64_t, Frame*>> born;
        for (Frame* c : f.children)
            if (c->birth >= from) born.emplace_back(c->birth, c);
        std::sort(born.begin(), born.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        std::vector<Frame*> kids;
        kids.reserve(born.size());
        for (const auto& [birth, c] : born)
            kids.push_back(c);
        return kids;
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
        if (f.joinEvent == kernel::kNone) {
            if (freeJoinEvents_.empty()) {
                f.joinEvent = sched_.addEvent();
            } else {
                f.joinEvent = freeJoinEvents_.back();
                freeJoinEvents_.pop_back();
            }
        }
        // Children ending is no flush point of the pending reports (15.4).
        sched_.waitNamed(f.kernelId, f.joinEvent, false);
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
            Frame& c = newFrame(ops[i].asProcess());
            // Each branch, in order, is seeded from the parent (18.14.2).
            c.rng = rngOf(f).child();
            c.ownRng = true;
            c.parent = &f;
            c.group = none ? 0 : f.forkGroup;
            c.birth = f.births++;
            c.place = static_cast<std::uint32_t>(f.children.size());
            c.kernelId = sched_.addChildProcess(&resume, &c, f.kernelId);
            f.children.push_back(&c);
            ++f.liveChildren;
            sched_.start(c.kernelId);
        }
    }

    /// disable fork: ends every descendant, also those of children that
    /// have already finished (9.6.3).
    void disableFork(Frame& f) {
        for (Frame* c : childrenFrom(f, 0)) {
            if (c->released) continue;
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

    /// A run-time error of the design (SCHEDULER_SPEC.md 16.2): one line in
    /// the form of the severity tasks, at the instruction's position and
    /// with the instance's name, then the end of the run, reason error.
    void designError(const Frame& f, const sir::Instr& in, const std::string& what) {
        designErrorIn(d_.process(f.process), in, what);
    }
    /// The same, given the process that runs.
    void designErrorIn(const sir::Process& p, const sir::Instr& in, const std::string& what) {
        output_.message(runtimeLine(instanceOf(p), in.loc, what));
        sched_.runtimeError();
    }
    /// The instance whose code runs: the subroutine's, or process p's.
    sir::InstanceId instanceOf(const sir::Process& p) const {
        return isSubBody(act_.body) ? subOf(act_.body).instance : p.instance;
    }
    /// The line of a run-time error of the design (SCHEDULER_SPEC.md
    /// 16.2): `<file>:<line>: error: <what> [<instance> @ <time>]`.
    std::string runtimeLine(sir::InstanceId inst, sir::SourceLoc loc, const std::string& what) {
        std::string line = loc.file < d_.sources.size() ? d_.sources[loc.file] : "?";
        line += ":" + std::to_string(loc.line) + ": error: " + what + " [" +
                d_.instance(inst).scopeName + " @ " + messageTime(sched_.time(), d_.timePrecision) +
                "]";
        return line;
    }

    /// Whether the instruction writes a variable or schedules an event,
    /// which the Postponed region forbids (SCHEDULER_SPEC.md 2.2). The
    /// variables of subroutines, frame variables and the static variables
    /// of functions, may be written: they hold the work of the functions
    /// the region calls.
    bool changesState(const sir::Instr& in) const {
        const auto& ops = in.operands;
        const auto ownSlot = [&](sir::SlotId s) {
            return s.index() < d_.slots.size() && d_.slot(s).subroutine != sir::kNone;
        };
        if (sir::isContainerMutation(in.op)) return !localRef(value(ops[1]));
        switch (in.op) {
            case Op::Store: return !ownSlot(ops[0].asSlot());
            case Op::StoreElem:
            case Op::StoreBits:
                return ops[0].kind != Operand::Kind::Frame && !ownSlot(ops[0].asSlot());
            case Op::StoreRef:
            case Op::StoreRefElem:  // the reference is the frame variable's value
            case Op::StoreRefBits: {
                const W4 r = value(ops[0]);
                return !localRef(r);
            }
            case Op::Nba:
            case Op::NbaDelay:
            case Op::DriveDelay:
            case Op::NbaElem:
            case Op::NbaElemDelay:
            case Op::NbaBits:
            case Op::NbaBitsDelay:
            case Op::NbaRef:
            case Op::Trigger:
            case Op::TriggerNb:
            case Op::Disable:
            case Op::DisableFork:
            case Op::DisableScope:
            case Op::DisableTask:
            case Op::OverrideStart:
            case Op::OverrideStop:
            case Op::StoreOverride:
            case Op::Fork: return true;
            default: return false;
        }
    }

    // --- Values ---

    sir::Type typeOf(const Operand& o) const { return typeIn(act_.body, o); }

    /// The type of an operand of an instruction of the given body.
    sir::Type typeIn(BodyId body, const Operand& o) const {
        switch (o.kind) {
            case Operand::Kind::Temp: return tempTypes_[body.index()][o.id];
            case Operand::Kind::Const: return d_.constants[o.id].type;
            case Operand::Kind::Frame: return bodyFrame(body)[o.id].type;
            case Operand::Kind::Slot: return d_.slot(o.asSlot()).type;
            default: return sir::voidType();
        }
    }

    W4 value(const Operand& o) const {
        switch (o.kind) {
            case Operand::Kind::Temp:
                return tempStack_[act_.base + tempOff_[act_.body.index()][o.id]];
            case Operand::Kind::Const: return consts_[constOff_[o.id]];
            case Operand::Kind::Frame:
                return (*act_.vars)[layouts_[act_.body.index()].wordOf(o.asFrame())];
            default: return {0, 0};
        }
    }

    /// A delay operand in ticks of the precision (SIR_SPEC.md 1): 0 when
    /// a bit is x or z (9.4.1), and nothing when it is 2^64 or more, which
    /// takes the time past the largest time (SCHEDULER_SPEC.md 8).
    std::optional<std::uint64_t> ticksOf(const Operand& o) const {
        const sir::Type t = typeOf(o);
        if (chunksOf(t) == 1) {
            const W4 d = value(o);
            return d.b != 0 ? 0 : d.a;
        }
        const std::vector<W4> c = chunks(o);
        if (kernel::wide::hasUnknown(c.data(), t.width)) return 0;
        for (std::size_t k = 1; k < c.size(); ++k)
            if (c[k].a != 0) return std::nullopt;
        return c[0].a;
    }

    /// The kernel's slot of a SIR slot, as the store numbered it when run()
    /// added the slots. Every slot the VM gives the kernel goes through here.
    [[nodiscard]] kernel::SlotId kernelSlot(sir::SlotId s) const { return kernelSlots_[s.index()]; }
    /// The k-th slot after s: the leaves of an unpacked structure are
    /// consecutive slots (SIR_SPEC.md 4.2).
    static sir::SlotId leafSlot(sir::SlotId s, std::uint32_t k) {
        return sir::SlotId{s.index() + k};
    }

    W4 readSlot(sir::SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width) const {
        const kernel::SlotLayout& l = store_.layout(kernelSlot(s));
        if (width == 0) width = l.width;
        const std::uint64_t* p = store_.element(kernelSlot(s), elem);
        if (l.domain == kernel::Domain::Four)
            return {kernel::readField(p, 2, lsb, width), kernel::readField(p + 1, 2, lsb, width)};
        return {kernel::readField(p, 1, lsb, width), 0};
    }

    void writeSlot(sir::SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
                   W4 v) {
        const std::uint64_t buf[2] = {v.a, v.b};
        sched_.write({.slot = kernelSlot(s), .elem = elem, .lsb = lsb, .width = width}, buf);
    }

    void nbaSlot(const Frame& f, sir::SlotId s, std::uint32_t elem, std::uint32_t lsb,
                 std::uint32_t width, W4 v, std::uint64_t delay = 0) {
        const std::uint64_t buf[2] = {v.a, v.b};
        sched_.scheduleNba(f.kernelId,
                           {.slot = kernelSlot(s), .elem = elem, .lsb = lsb, .width = width}, buf,
                           delay);
    }

    // Values of any width, in W4 chunks (kernel/wide.h). A two-state slot
    // keeps a words only; its chunks have b 0.

    /// Bits [lsb, lsb + width) (width 0: all) of a slot element.
    void readSlotWide(sir::SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
                      W4* out) const {
        const kernel::SlotLayout& l = store_.layout(kernelSlot(s));
        if (width == 0) width = l.width;
        const std::uint64_t* p = store_.element(kernelSlot(s), elem);
        const bool four = l.domain == kernel::Domain::Four;
        if (l.domain == kernel::Domain::Real || l.domain == kernel::Domain::Handle) {
            out[0] = {p[0], 0};
            return;
        }
        for (std::uint32_t k = 0; k * 64 < width; ++k) {
            const std::uint32_t n = width - k * 64 < 64 ? width - k * 64 : 64;
            out[k] = {kernel::readField(p, four ? 2 : 1, lsb + k * 64, n),
                      four ? kernel::readField(p + 1, 2, lsb + k * 64, n) : 0};
        }
    }

    /// The chunks as the words of a slot element of that many bits.
    static std::vector<std::uint64_t> slotWords(const W4* v, std::uint32_t chunks) {
        std::vector<std::uint64_t> buf(std::size_t{2} * chunks);
        for (std::size_t k = 0; k < chunks; ++k) {
            buf[2 * k] = v[k].a;
            buf[2 * k + 1] = v[k].b;
        }
        return buf;
    }

    /// Writes the bits [lsb, lsb + width) (width 0: all) of a slot element.
    void writeSlotWide(sir::SlotId s, std::uint32_t elem, std::uint32_t lsb, std::uint32_t width,
                       const W4* v) {
        const kernel::SlotLayout& l = store_.layout(kernelSlot(s));
        const std::uint32_t w = width == 0 ? l.width : width;
        if (l.domain != kernel::Domain::Four && l.domain != kernel::Domain::Two) {
            writeSlot(s, elem, lsb, width, v[0]);
            return;
        }
        const std::uint32_t c = (w + 63) / 64;
        const std::vector<std::uint64_t> buf = packed(v, c, l.domain == kernel::Domain::Four);
        sched_.write({.slot = kernelSlot(s), .elem = elem, .lsb = lsb, .width = width}, buf.data());
    }

    /// A value laid out for the kernel's write() into the slot range
    /// `target`: a and b words in turn for a four-state slot (store.h).
    std::vector<std::uint64_t> slotWords(const Operand& target, const Operand& value) const {
        const kernel::SlotLayout& l = store_.layout(kernelSlot(target.asSlot()));
        if (l.domain != kernel::Domain::Four && l.domain != kernel::Domain::Two) {
            const W4 x = this->value(value);
            return {x.a, x.b};
        }
        const std::uint32_t w = target.width == 0 ? l.width : target.width;
        std::vector<W4> v((w + 63) / 64);
        readOperand(value, v.data(), static_cast<std::uint32_t>(v.size()));
        return packed(v.data(), static_cast<std::uint32_t>(v.size()),
                      l.domain == kernel::Domain::Four);
    }

    void nbaSlotWide(const Frame& f, sir::SlotId s, std::uint32_t elem, std::uint32_t lsb,
                     std::uint32_t width, const W4* v, std::uint64_t delay = 0) {
        const kernel::SlotLayout& l = store_.layout(kernelSlot(s));
        const std::uint32_t w = width == 0 ? l.width : width;
        if (l.domain != kernel::Domain::Four && l.domain != kernel::Domain::Two) {
            nbaSlot(f, s, elem, lsb, width, v[0], delay);
            return;
        }
        const std::vector<std::uint64_t> buf =
            packed(v, (w + 63) / 64, l.domain == kernel::Domain::Four);
        sched_.scheduleNba(f.kernelId,
                           {.slot = kernelSlot(s), .elem = elem, .lsb = lsb, .width = width},
                           buf.data(), delay);
    }

    /// Chunks laid out like a slot element: a and b words interleaved when
    /// four-state, a words only otherwise.
    static std::vector<std::uint64_t> packed(const W4* v, std::uint32_t chunks, bool four) {
        if (four) return slotWords(v, chunks);
        std::vector<std::uint64_t> buf(chunks);
        for (std::uint32_t k = 0; k < chunks; ++k)
            buf[k] = v[k].a;
        return buf;
    }

    /// Reads any value operand into `chunks` chunks: a temp, a constant, a
    /// frame variable, or the range of a slot.
    void readOperand(const Operand& o, W4* out, std::uint32_t chunks) const {
        switch (o.kind) {
            case Operand::Kind::Slot:
                readSlotWide(o.asSlot(), o.firstElem, o.lsb, o.width, out);
                return;
            case Operand::Kind::Temp: {
                const W4* p = &tempStack_[act_.base + tempOff_[act_.body.index()][o.id]];
                for (std::uint32_t k = 0; k < chunks; ++k)
                    out[k] = p[k];
                return;
            }
            case Operand::Kind::Const: {
                const W4* p = &consts_[constOff_[o.id]];
                for (std::uint32_t k = 0; k < chunks; ++k)
                    out[k] = p[k];
                return;
            }
            case Operand::Kind::Frame: {
                const W4* p = &(*act_.vars)[layouts_[act_.body.index()].wordOf(o.asFrame())];
                for (std::uint32_t k = 0; k < chunks; ++k)
                    out[k] = p[k];
                return;
            }
            default: out[0] = {0, 0}; return;
        }
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
            const std::optional<std::int64_t> idx = indexOf(o);
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
    void storeBits(const Frame& f, sir::SlotId s, std::uint32_t elem,
                   std::optional<std::int64_t> pos, W4 v, std::uint32_t width, bool nba,
                   std::uint64_t delay = 0, std::uint32_t windowLsb = 0,
                   std::uint32_t windowWidth = 0) {
        const std::optional<kernel::Inside> in =
            pos ? kernel::inside(*pos, width, d_.slot(s).type.width, windowLsb, windowWidth)
                : std::nullopt;
        if (!in) return;
        const W4 part{(v.a >> in->skip) & kernel::mask(in->n),
                      (v.b >> in->skip) & kernel::mask(in->n)};
        if (nba)
            nbaSlot(f, s, elem, in->lo, in->n, part, delay);
        else
            writeSlot(s, elem, in->lo, in->n, part);
    }

    /// Whether a process waits on its triggers from before its body runs,
    /// so that a change it makes to what it reads starts it again: a
    /// continuous assignment, gate or UDP, and a procedural assign or force
    /// while it holds its targets, are processes sensitive to the elements
    /// of their right side (SCHEDULER_SPEC.md 5, 12; 4.9.1, 4.9.2). The
    /// others wait on their triggers once their body has run, as a
    /// procedure reaches its event control only then (9.4.2).
    static bool sensitiveWhileRunning(const sir::Process& p, const Frame& f) {
        return p.kind == sir::ProcessKind::Cont ||
               (p.kind == sir::ProcessKind::Override && f.holding);
    }

    void waitTriggers(Frame& f, const sir::Process& proc) {
        items_.clear();
        f.iffs.clear();
        f.iffs.reserve(proc.triggers.size());  // the kernel keeps pointers into it
        for (const sir::Trigger& t : proc.triggers) {
            kernel::WaitItem item;
            item.part = {kernelSlot(t.range.slot), t.range.firstElem, t.range.numElems, t.range.lsb,
                         t.range.width};
            item.edge = kernelEdge(t.edge);
            if (t.guard != sir::kNone) withIff(f, item, t.guard);
            items_.push_back(item);
        }
        sched_.waitEvent(f.kernelId, items_);
    }

    /// Gives an item, a subscription or a monitor record, its iff guard,
    /// which the kernel calls when the item's event occurs and only then
    /// (9.4.2.3). f.iffs has room for it.
    template <typename Item> static void withIff(Frame& f, Item& item, sir::SubroutineId g) {
        item.guard = &evalIff;
        item.guardFrame = &f.iffs.emplace_back(Frame::Callback{&f, g});
    }

    kernel::Group processGroup(const sir::Process& process) const {
        return d_.instance(process.instance).kind == sir::InstanceKind::Program
                   ? kernel::Group::Reactive
                   : kernel::Group::Active;
    }

    /// The monitor record of a watch, an item on an expression: evaluated
    /// again when what the watch reads changes (SIR_SPEC.md 7.2,
    /// SCHEDULER_SPEC.md 9.5). f.watches has room for it.
    kernel::MonitorSpec watchMonitor(Frame& f, const Operand& o) {
        const sir::Type t = d_.subroutines[o.id].returnType;
        kernel::MonitorSpec m;
        if (!readsOf(f, bodyOf(o.asSubroutine()), m)) return m;
        m.eval = &evalWatch;
        m.frame = &f.watches.emplace_back(Frame::Callback{&f, o.asSubroutine()});
        m.domain = sir::isFourState(t) ? kernel::Domain::Four : kernel::Domain::Two;
        m.width = t.width;
        m.edge = kernelEdge(o.edge);
        return m;
    }

    // --- Running ---

    /// Runs a process from where it stopped until it waits, returns or the
    /// simulation ends.
    /// Counts the instructions it runs in `steps` (vm.instrs).
    kernel::Status runProcess(Frame& f, std::uint64_t& steps) {
        const sir::Process& p = d_.process(f.process);
        // Woken at a join or wait fork: wait again until it is satisfied.
        if (f.pending != Frame::Join::None && waitJoin(f)) return kernel::Status::Waiting;
        const bool early = sensitiveWhileRunning(p, f);
        if (early) waitTriggers(f, p);
        W4 ret{0, 0};
        for (;;) {
            Exit e = Exit::Ret;
            if (f.calls.empty()) {
                enter({bodyOfProcess(f.process), &f.vars, 0, f.varsId});
                e = runBody(f, f.block, steps, ret);
            } else {
                Frame::TaskCall& c = f.calls.back();
                enter({c.body, &c.vars, 0, c.varsId});
                e = runBody(f, c.block, steps, ret);
            }
            if (e == Exit::Entered) continue;  // a task was called
            if (e == Exit::Disabled && f.unwind.has_value()) {
                // The task calls a disable ended go, without their outputs.
                const Frame::Unwind u = *f.unwind;
                f.unwind.reset();
                while (f.calls.size() > u.calls) {
                    retire(f.calls.back().varsId);
                    f.calls.pop_back();
                }
                if (u.exit != sir::kNone) resumeAt(f) = u.exit;
                continue;
            }
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
                f.block = sir::kEntryBlock;
                if (!early) waitTriggers(f, p);
                return kernel::Status::Waiting;
            case sir::ProcessKind::Override:
                // Idle once it holds nothing, until override.start again.
                f.block = sir::kEntryBlock;
                if (f.holding && !early) waitTriggers(f, p);
                return kernel::Status::Waiting;
            case sir::ProcessKind::Postponed:
                // Runs again from its start, each time the kernel runs it.
                f.block = sir::kEntryBlock;
                return kernel::Status::Waiting;
            default: return kernel::Status::Done;
        }
    }

    /// How running a body stops: like kernel::Status, or at its ret.
    enum class Exit : std::uint8_t {
        Waiting = static_cast<std::uint8_t>(kernel::Status::Waiting),
        Error = static_cast<std::uint8_t>(kernel::Status::Error),
        Stop = static_cast<std::uint8_t>(kernel::Status::Stop),
        Killed = static_cast<std::uint8_t>(kernel::Status::Killed),
        Disabled = 0xfd,  ///< A disable sends the frame to f.unwind.
        Entered = 0xfe,   ///< call.task made a new innermost call.
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
        while (!f.scopes.empty() && f.scopes.back().calls > f.calls.size())
            f.scopes.pop_back();
        if (f.calls.empty())
            writeOuts(s, c.site->operands, outs, f.vars, bodyOfProcess(f.process));
        else
            writeOuts(s, c.site->operands, outs, f.calls.back().vars, f.calls.back().body);
    }

    /// call.task: copies the inputs in and makes the call the innermost
    /// one; the caller goes on at the block the instruction names. False
    /// when the call would nest too deep.
    bool enterTask(Frame& f, const sir::Instr& in) {
        if (tooDeep(f, in)) return false;
        const auto& ops = in.operands;
        const sir::Subroutine& s = d_.subroutines[ops[0].id];
        Frame::TaskCall c;
        c.body = bodyOf(ops[0].asSubroutine());
        c.site = &in;
        c.children = f.births;
        c.vars = newVars(c.body);
        bindArgs(c.body, s, readArgs(s, ops), c.vars);
        c.varsId = enroll(c.vars, c.body);
        resumeAt(f) = ops.back().asBlock();
        f.calls.push_back(std::move(c));
        return true;
    }

    // --- Frames and arguments (SIR_SPEC.md 7.4, 8) ---

    /// The frame variables of a new activation of a body: a subroutine's at
    /// their defaults, a process's at 0 until its declarations run; array
    /// elements at their defaults.
    std::vector<W4> newVars(BodyId body) const {
        const auto& frame = bodyFrame(body);
        const Layout& l = layouts_[body.index()];
        std::vector<W4> vars(l.size, W4{0, 0});
        for (std::size_t i = 0; i < frame.size(); ++i) {
            const bool array = !frame[i].dims.empty() && frame[i].type.kind != sir::TypeKind::Ref;
            if (!array) {
                if (isSubBody(body)) fillDefault(&vars[l.offset[i]], frame[i].type);
                continue;
            }
            const std::uint64_t n = sir::elementCount(frame[i]);
            for (std::uint64_t e = 0; e < n; ++e)
                fillDefault(&vars[l.offset[i] + e * l.chunks[i]], frame[i].type);
        }
        return vars;
    }

    /// Gives an activation's frame variables a number for refs.
    VarsId enroll(std::vector<W4>& vars, BodyId body) {
        const VarSet entry{vars.data(), vars.size(), body, true};
        if (freeVarSets_.empty()) {
            varSets_.push_back(entry);
            return VarsId{static_cast<std::uint32_t>(varSets_.size() - 1)};
        }
        const VarsId id = freeVarSets_.back();
        freeVarSets_.pop_back();
        varSets_[id.index()] = entry;
        return id;
    }
    void retire(VarsId id) {
        varSets_[id.index()] = {};
        freeVarSets_.push_back(id);
    }

    /// Element e of array frame variable fv of the running body: its first
    /// chunk.
    W4& frameElem(sir::FrameVarId fv, std::uint64_t e) {
        const Layout& l = layouts_[act_.body.index()];
        return (*act_.vars)[l.wordOf(fv) + e * l.chunksOfVar(fv)];
    }
    /// Frame variable fv of the running body: its first chunk.
    W4& frameVar(sir::FrameVarId fv) {
        return (*act_.vars)[layouts_[act_.body.index()].wordOf(fv)];
    }

    static bool isArrayParam(const sir::Subroutine& s, const sir::Param& p, const sir::Design& d) {
        return p.frameVar != sir::kNone
                   ? !s.frame[p.frameVar.index()].dims.empty() &&
                         s.frame[p.frameVar.index()].type.kind != sir::TypeKind::Ref
                   : !d.slot(p.slot).dims.empty();
    }
    static bool isRefParam(const sir::Param& p) {
        return p.dir == sir::Direction::Ref || p.dir == sir::Direction::ConstRef;
    }

    /// The elements of an array argument, read in the running body: a run
    /// of elements of a slot, or an array frame variable; each element's
    /// chunks in order.
    std::vector<W4> readArray(const Operand& o) {
        std::vector<W4> out;
        if (o.kind == Operand::Kind::Slot) {
            const std::uint32_t c = chunksOf(d_.slot(o.asSlot()).type);
            const std::uint64_t n =
                o.numElems != 0 ? o.numElems : sir::elementCount(d_.slot(o.asSlot()));
            out.resize(n * c);
            for (std::uint64_t e = 0; e < n; ++e)
                readSlotWide(o.asSlot(), static_cast<std::uint32_t>(o.firstElem + e), 0, 0,
                             &out[e * c]);
            return out;
        }
        const Layout& l = layouts_[act_.body.index()];
        const std::uint64_t n =
            sir::elementCount(bodyFrame(act_.body)[o.asFrame().index()]) * l.chunks[o.id];
        for (std::uint64_t k = 0; k < n; ++k)
            out.push_back((*act_.vars)[l.wordOf(o.asFrame()) + k]);
        return out;
    }

    /// What each argument passes in, read in the caller before anything is
    /// written: a ref, an array's elements, or a value (none for an
    /// output), as chunks.
    std::vector<std::vector<W4>> readArgs(const sir::Subroutine& s,
                                          const std::vector<Operand>& ops) {
        std::vector<std::vector<W4>> args(s.params.size());
        for (std::size_t i = 0; i < s.params.size(); ++i) {
            const sir::Param& prm = s.params[i];
            const Operand& o = ops[1 + i];
            if (isRefParam(prm)) {
                args[i].push_back(value(o));
            } else if (prm.dir == sir::Direction::Output) {
                continue;
            } else if (isArrayParam(s, prm, d_)) {
                args[i] = readArray(o);
            } else {
                args[i].resize(chunksOf(prm.type));
                readOperand(o, args[i].data(), chunksOf(prm.type));
            }
        }
        return args;
    }

    /// Copies the arguments into the callee's formals (4.9.7). A static
    /// output keeps its value.
    void bindArgs(BodyId body, const sir::Subroutine& s, const std::vector<std::vector<W4>>& args,
                  std::vector<W4>& vars) {
        const Layout& l = layouts_[body.index()];
        for (std::size_t i = 0; i < s.params.size(); ++i) {
            const sir::Param& prm = s.params[i];
            if (args[i].empty()) continue;  // an output
            if (prm.frameVar == sir::kNone) {
                const std::uint32_t c = chunksOf(d_.slot(prm.slot).type);
                for (std::size_t e = 0; e * c < args[i].size(); ++e)
                    writeSlotWide(prm.slot, static_cast<std::uint32_t>(e), 0, 0, &args[i][e * c]);
            } else {
                for (std::size_t k = 0; k < args[i].size(); ++k)
                    vars[l.wordOf(prm.frameVar) + k] = args[i][k];
            }
        }
    }

    /// The values of the outputs and inouts at the callee's end, as chunks.
    std::vector<std::vector<W4>> readOuts(BodyId body, const sir::Subroutine& s,
                                          const std::vector<W4>& vars) const {
        const Layout& l = layouts_[body.index()];
        std::vector<std::vector<W4>> outs(s.params.size());
        for (std::size_t i = 0; i < s.params.size(); ++i) {
            const sir::Param& prm = s.params[i];
            if (prm.dir == sir::Direction::Input || isRefParam(prm)) continue;
            if (prm.frameVar == sir::kNone) {
                const sir::Slot& slot = d_.slot(prm.slot);
                const std::uint32_t c = chunksOf(slot.type);
                const std::uint64_t n = sir::elementCount(slot);
                outs[i].resize(n * c);
                for (std::uint64_t e = 0; e < n; ++e)
                    readSlotWide(prm.slot, static_cast<std::uint32_t>(e), 0, 0, &outs[i][e * c]);
                continue;
            }
            const sir::FrameVar& fv = s.frame[prm.frameVar.index()];
            const std::uint64_t n =
                (fv.dims.empty() ? 1 : sir::elementCount(fv)) * l.chunksOfVar(prm.frameVar);
            for (std::uint64_t k = 0; k < n; ++k)
                outs[i].push_back(vars[l.wordOf(prm.frameVar) + k]);
        }
        return outs;
    }

    /// Copies the outputs and inouts back to the caller's places like
    /// blocking assignments (4.9.7).
    void writeOuts(const sir::Subroutine& s, const std::vector<Operand>& ops,
                   const std::vector<std::vector<W4>>& outs, std::vector<W4>& callerVars,
                   BodyId callerBody) {
        const Layout& l = layouts_[callerBody.index()];
        for (std::size_t i = 0; i < s.params.size(); ++i) {
            if (outs[i].empty()) continue;
            const Operand& o = ops[1 + i];
            const std::uint32_t c = chunksOf(s.params[i].type);
            if (o.kind == Operand::Kind::Slot) {
                if (!isArrayParam(s, s.params[i], d_)) {
                    writeSlotWide(o.asSlot(), o.firstElem, o.lsb, o.width, outs[i].data());
                    continue;
                }
                for (std::size_t e = 0; e * c < outs[i].size(); ++e)
                    writeSlotWide(o.asSlot(), static_cast<std::uint32_t>(o.firstElem + e), 0, 0,
                                  &outs[i][e * c]);
                continue;
            }
            for (std::size_t k = 0; k < outs[i].size(); ++k)
                callerVars[l.wordOf(o.asFrame()) + k] = outs[i][k];
        }
    }

    // --- References (SIR_SPEC.md 8) ---

    /// Frame refs pack the activation in a's high half and its word in
    /// the low half; bit 0 of b is 1. Slot refs pack the slot in a's low
    /// half and the element above bit 0 of b. Actual container type + 1
    /// lives above the frame tag in b, or in a's high half for a slot.
    /// Cell refs use bit 63 of b, the cell token in a and layout + 1 in
    /// b's bits 1..32; bit 0 records whether its origin is local.
    /// A ref to word `at` of activation `vars`; nothing when the word's
    /// place does not fit the low half, which it would overrun into the
    /// activation's number.
    static std::optional<W4> frameRef(VarsId vars, std::uint64_t at) {
        if (at > 0xffffffffU) return std::nullopt;
        return W4{(std::uint64_t{vars.index()} << 32) | at, 1};
    }
    /// The frame ref `words` words further on, on the same terms.
    static std::optional<W4> refStep(W4 r, std::uint64_t words) {
        auto moved =
            frameRef(VarsId{static_cast<std::uint32_t>(r.a >> 32)}, (r.a & 0xffffffffU) + words);
        if (moved) moved->b = r.b;  // Preserve the actual container type, if present.
        return moved;
    }
    static W4 slotRef(sir::SlotId s, std::uint32_t e) { return {s.index(), std::uint64_t{e} << 1}; }
    static bool isHeapRef(W4 r) { return (r.b >> 63) != 0; }
    static bool isFrameRef(W4 r) { return !isHeapRef(r) && (r.b & 1U) != 0; }
    W4& refWord(W4 r, std::uint64_t off) {
        return varSets_[r.a >> 32].words[(r.a & 0xffffffffU) + off];  // frameRef's halves
    }
    static sir::SlotId refSlot(W4 r) { return sir::SlotId{static_cast<std::uint32_t>(r.a)}; }
    static std::uint32_t refElem(W4 r) { return static_cast<std::uint32_t>(r.b >> 1); }

    // --- Typed container operands (SIR_SPEC.md 5.8) ---

    void rootContainer(std::uint64_t bits) {
        if (bits != 0) roots_.push_back(kernel::HeapRoot::container({bits}));
    }
    bool rootActivation(VarsId id) {
        if (id.index() >= varSets_.size() || !varSets_[id.index()].live) return false;
        if (!rootActivationsSeen_[id.index()]) {
            rootActivationsSeen_[id.index()] = true;
            rootActivations_.push_back(id);
        }
        return true;
    }
    bool rootReference(W4 ref) {
        if (isHeapRef(ref)) {
            if (ref.a != 0) roots_.push_back(kernel::HeapRoot::cell({ref.a}));
            return true;
        }
        if (!isFrameRef(ref)) return true;  // The typed slots are already roots.
        const VarsId vars{static_cast<std::uint32_t>(ref.a >> 32)};
        if (vars.index() >= varSets_.size() || (ref.a & 0xffffffffU) >= varSets_[vars.index()].size)
            return false;
        // An interior ref keeps its activation live. Trace its typed
        // fields, never arbitrary words that happen to resemble handles.
        return rootActivation(vars);
    }
    static bool rootPendingValue(void* frame, kernel::SlotId slot,
                                 std::span<const std::uint64_t> values) noexcept {
        auto& vm = *static_cast<Interpreter*>(frame);
        if (slot.index() >= vm.heapSlots_.size() || !vm.heapSlots_[slot.index()]) return true;
        try {
            for (const auto value : values)
                vm.rootContainer(value);
            return true;
        } catch (const std::bad_alloc&) {
            return false;
        } catch (const std::length_error&) {
            return false;
        }
    }
    void collectionLimit() noexcept {
        try {
            limitAt(heapInstance_, heapSite_, "container collection resource limit");
        } catch (...) {
            // Failure to allocate the diagnostic must not escape the
            // kernel callback, or prevent the resource-limit result.
            sched_.resourceLimit();
        }
    }
    static void collectSafePoint(void* frame) noexcept {
        auto& vm = *static_cast<Interpreter*>(frame);
        if (!vm.heapReady_ || vm.running_ != nullptr || vm.callDepth_ != 0) return;
        try {
            vm.collectRoots();
        } catch (const std::bad_alloc&) {
            vm.collectionLimit();
        } catch (const std::length_error&) {
            vm.collectionLimit();
        }
    }
    void collectRoots() {
        roots_.clear();
        rootActivations_.clear();
        rootActivationsSeen_.assign(varSets_.size(), false);
        for (const auto slot : rootSlots_) {
            const auto count = sir::elementCount(d_.slot(slot));
            for (std::uint64_t e = 0; e < count; ++e)
                rootContainer(*store_.element(kernelSlot(slot), static_cast<std::uint32_t>(e)));
        }
        for (const Frame& f : frames_) {
            if (f.released || f.finished || sched_.finished(f.kernelId)) continue;
            if (!rootActivation(f.varsId)) {
                sched_.fail("invalid activation at a heap safe point");
                return;
            }
            for (const auto& c : f.calls)
                if (!rootActivation(c.varsId)) {
                    sched_.fail("invalid task activation at a heap safe point");
                    return;
                }
        }
        for (const Deferred& call : deferred_) {
            const auto& params = d_.subroutine(call.subroutine).params;
            for (std::size_t i = 0; i < call.args.size(); ++i) {
                for (const auto value : call.args[i]) {
                    if (isRefParam(params[i]) || params[i].type.kind == sir::TypeKind::Ref) {
                        if (!rootReference(value)) {
                            sched_.fail("invalid deferred reference at a heap safe point");
                            return;
                        }
                    } else if (sir::isContainer(params[i].type))
                        rootContainer(value.a);
                }
            }
        }
        // References may keep an otherwise finished activation alive.
        // Follow those edges iteratively, including cycles of refs.
        for (std::size_t i = 0; i < rootActivations_.size(); ++i) {
            const VarSet vars = varSets_[rootActivations_[i].index()];
            const auto& fields = bodyFrame(vars.body);
            const auto& layout = layouts_[vars.body.index()];
            for (const auto field : layout.roots) {
                const auto& fv = fields[field];
                const auto at = layout.offset[field];
                if (fv.type.kind == sir::TypeKind::Ref) {
                    if (!rootReference(vars.words[at])) {
                        sched_.fail("invalid frame reference at a heap safe point");
                        return;
                    }
                } else {
                    const auto count = fv.dims.empty() ? 1 : sir::elementCount(fv);
                    for (std::uint64_t e = 0; e < count; ++e)
                        rootContainer(vars.words[at + e].a);
                }
            }
        }
        if (!sched_.visitPendingValues(&rootPendingValue, this)) {
            collectionLimit();
            return;
        }
        const bool same = roots_.size() == publishedRoots_.size() &&
                          std::equal(roots_.begin(), roots_.end(), publishedRoots_.begin(),
                                     [](const auto& a, const auto& b) {
                                         return a.kind == b.kind && a.bits == b.bits;
                                     });
        if (same && !heap_.needsCollection()) return;
        const auto status = heap_.collect(roots_);
        if (status.error == kernel::HeapError::ResourceLimit) {
            collectionLimit();
            return;
        }
        if (!status) {
            sched_.fail("invalid typed root at a heap safe point");
            return;
        }
        publishedRoots_.swap(roots_);
    }

    bool heapStatus(Frame& f, const sir::Instr& in, kernel::HeapStatus status) {
        using E = kernel::HeapError;
        if (status.error == E::ResourceLimit) {
            limitAt(instanceOf(d_.process(f.process)), in.loc, "container resource limit");
            return false;
        }
        if (status.error == E::InvalidSize) {
            designError(f, in, "dynamic-array size is negative or contains X/Z bits");
            return false;
        }
        if (!status) {
            fail(d_.process(f.process), "invalid typed container operand or reference");
            return false;
        }
        if (status.warning != kernel::HeapWarning::None &&
            heapWarned_.insert({in.loc.file, in.loc.line, in.loc.column}).second) {
            const std::string_view text = status.warning == kernel::HeapWarning::BoundExceeded
                                              ? "queue operation exceeds its declared bound"
                                          : status.warning == kernel::HeapWarning::EmptyPop
                                              ? "pop from an empty queue"
                                              : "queue method index is invalid";
            warnOnce(in, text);
        }
        return true;
    }

    bool prepareHeap(Frame& f, const sir::Instr& in) {
        if (heapReady_) return true;
        try {
            heapTypes_.assign(d_.types.size(), kernel::kNone);
            for (std::size_t i = 0; i < d_.types.size(); ++i) {
                const sir::TypeEntry& te = d_.types[i];
                if (te.kind != sir::TypeEntryKind::DynArray && te.kind != sir::TypeEntryKind::Queue)
                    continue;
                const sir::Type e = te.value;
                const unsigned width = sir::isIntegral(e)                   ? e.width
                                       : e.kind == sir::TypeKind::ShortReal ? 32
                                                                            : 64;
                std::vector<kernel::HeapWord> words;
                for (std::uint64_t bit = 0; bit < width; bit += 64) {
                    const std::uint64_t mask = kernel::mask(
                        static_cast<unsigned>(std::min<std::uint64_t>(64, width - bit)));
                    const bool four = sir::isFourState(e);
                    words.push_back(
                        {mask, four, four ? W4{mask, mask} : W4{0, 0},
                         sir::isContainer(e) ? heapTypes_[e.detail.index()] : kernel::kNone});
                }
                const auto l = heap_.addLayout(words);
                if (!heapStatus(f, in, l.status)) return false;
                const auto t = heap_.addType({te.kind == sir::TypeEntryKind::DynArray
                                                  ? kernel::ContainerKind::DynamicArray
                                                  : kernel::ContainerKind::Queue,
                                              l.value, te.maxElements});
                if (!heapStatus(f, in, t.status)) return false;
                heapTypes_[i] = t.value;
            }
            heapReady_ = true;
            sched_.setSafePointHook({&collectSafePoint, this});
            return true;
        } catch (const std::bad_alloc&) {
            return heapStatus(f, in, {kernel::HeapError::ResourceLimit});
        } catch (const std::length_error&) {
            return heapStatus(f, in, {kernel::HeapError::ResourceLimit});
        }
    }

    static kernel::HeapLayoutId refLayout(W4 r) {
        return kernel::HeapLayoutId{static_cast<std::uint32_t>(r.b >> 1) - 1};
    }
    bool localRef(W4 r) const {
        return isHeapRef(r) ? (r.b & 1U) != 0
                            : isFrameRef(r) || d_.slot(refSlot(r)).subroutine != sir::kNone;
    }
    W4 typedRef(W4 r, sir::Type type) const {
        if (!sir::isContainer(type)) return r;
        const auto encoded = std::uint64_t{heapTypes_[type.detail.index()].index()} + 1;
        if (isFrameRef(r))
            r.b |= encoded << 1;
        else
            r.a |= encoded << 32;
        return r;
    }
    kernel::HeapResult<kernel::HeapTypeId> receiverType(W4 r) const {
        if (isHeapRef(r)) {
            const auto w = heap_.describeWord(refLayout(r), 0);
            return {w.value.nested, w.status};
        }
        const auto encoded = static_cast<std::uint32_t>(isFrameRef(r) ? r.b >> 1 : r.a >> 32);
        return {encoded ? kernel::HeapTypeId{encoded - 1} : kernel::HeapTypeId{kernel::kNone}, {}};
    }
    kernel::HeapResult<kernel::HeapTypeId> valueHeapType(sir::Type nominal,
                                                         kernel::ContainerToken value) const {
        auto t = heap_.actualType(value);
        if (t && t.value == kernel::kNone) t.value = heapTypes_[nominal.detail.index()];
        return t;
    }
    bool readReceiver(Frame& f, const sir::Instr& in, W4 r, kernel::ContainerToken& out) {
        W4 value{};
        if (isHeapRef(r)) {
            if (!heapStatus(f, in,
                            heap_.readCell(refLayout(r), kernel::CellToken{r.a}, {&value, 1})))
                return false;
        } else {
            value = isFrameRef(r) ? refWord(r, 0) : readSlot(refSlot(r), refElem(r), 0, 0);
        }
        out = {value.a};
        return true;
    }
    bool storeReceiver(Frame& f, const sir::Instr& in, W4 r, kernel::ContainerToken value) {
        if (isHeapRef(r))
            return heapStatus(f, in,
                              heap_.storeNested(refLayout(r), kernel::CellToken{r.a}, value));
        if (isFrameRef(r))
            refWord(r, 0) = {value.bits, 0};
        else
            writeSlot(refSlot(r), refElem(r), 0, 0, {value.bits, 0});
        return true;
    }

    bool executeContainer(Frame& f, const sir::Instr& in) {
        using kernel::ContainerToken;
        using kernel::HeapStatus;
        try {
            heapSite_ = in.loc;
            heapInstance_ = instanceOf(d_.process(f.process));
            if (!prepareHeap(f, in)) return false;
            const auto& ops = in.operands;
            const auto token = [&](std::size_t i) { return ContainerToken{value(ops[i]).a}; };
            const auto put = [&](ContainerToken t) { temp(in.result) = {t.bits, 0}; };
            if (in.op == Op::ContainerNew || in.op == Op::ContainerCopy ||
                in.op == Op::ContainerSnapshot) {
                const auto t = heapTypes_[in.type.detail.index()];
                const auto result = in.op == Op::ContainerNew
                                        ? heap_.construct(t, indexOf(ops[0]), token(1))
                                    : in.op == Op::ContainerCopy ? heap_.copy(t, token(0))
                                                                 : heap_.snapshot(token(0));
                if (!heapStatus(f, in, result.status)) return false;
                put(result.value);
                return true;
            }
            if (in.op == Op::ContainerPack) {
                const sir::Type element = d_.types[in.type.detail.index()].value;
                const std::size_t c = chunksOf(element);
                std::vector<W4> words;
                if (ops.size() > words.max_size() / c)
                    return heapStatus(f, in, {kernel::HeapError::ResourceLimit});
                words.resize(ops.size() * c);
                for (std::size_t i = 0; i < ops.size(); ++i)
                    readOperand(ops[i], words.data() + i * c, static_cast<std::uint32_t>(c));
                const auto result = heap_.fromValues(heapTypes_[in.type.detail.index()], words);
                if (!heapStatus(f, in, result.status)) return false;
                put(result.value);
                return true;
            }
            if (in.op == Op::ContainerSize || in.op == Op::ContainerRead) {
                const auto t = valueHeapType(typeOf(ops[0]), token(0));
                if (!heapStatus(f, in, t.status)) return false;
                if (in.op == Op::ContainerSize) {
                    const auto result = heap_.size(t.value, token(0));
                    if (!heapStatus(f, in, result.status)) return false;
                    temp(in.result) = {result.value, 0};
                    return true;
                }
                return heapStatus(f, in,
                                  heap_.read(t.value, token(0), indexOf(ops[1]),
                                             {&temp(in.result), chunksOf(in.type)}));
            }
            const W4 ref = value(ops[1]);
            const auto t = receiverType(ref);
            if (!heapStatus(f, in, t.status)) return false;
            const auto desc = heap_.describeType(t.value);
            if (!heapStatus(f, in, desc.status)) return false;
            ContainerToken current;
            if (!readReceiver(f, in, ref, current)) return false;
            if (in.op == Op::ContainerCell) {
                const auto cell = heap_.cell(t.value, current, indexOf(ops[2]));
                if (!heapStatus(f, in, cell.status)) return false;
                temp(in.result) = {cell.value.bits,
                                   (std::uint64_t{1} << 63) |
                                       ((std::uint64_t{desc.value.element.index()} + 1) << 1) |
                                       (localRef(ref) ? 1U : 0U)};
                return true;
            }
            if (isHeapRef(ref) && ref.a == 0) {
                if (in.result != sir::kNone) fillDefault(&temp(in.result), in.type);
                return true;  // Invalid element writes leave storage unchanged.
            }
            if (in.op == Op::ContainerAssign) {
                const auto result = heap_.copy(t.value, token(2));
                if (!heapStatus(f, in, result.status)) return false;
                return storeReceiver(f, in, ref, result.value);
            }
            HeapStatus status;
            if (in.op == Op::ContainerClear)
                status = heap_.clear(t.value, current);
            else if (in.op == Op::ContainerSwapValues)
                status = heap_.swapValues(t.value, current, indexOf(ops[2]), indexOf(ops[3]));
            else if (in.op == Op::QueueErase)
                status = heap_.erase(t.value, current, indexOf(ops[2]));
            else if (in.op == Op::QueuePopFront || in.op == Op::QueuePopBack)
                status = heap_.pop(t.value, current, in.op == Op::QueuePopFront,
                                   {&temp(in.result), chunksOf(in.type)});
            else {
                const auto data = chunks(ops.back());
                if (in.op == Op::ContainerWrite)
                    status = heap_.write(t.value, current, indexOf(ops[2]), data);
                else if (in.op == Op::QueueInsert)
                    status = heap_.insert(t.value, current, indexOf(ops[2]), data);
                else if (in.op == Op::QueuePushFront || in.op == Op::QueuePushBack)
                    status = heap_.push(t.value, current, in.op == Op::QueuePushFront, data);
                else
                    return heapStatus(f, in, {kernel::HeapError::InvalidType});
            }
            if (!heapStatus(f, in, status)) return false;
            return storeReceiver(f, in, ref, current);
        } catch (const std::bad_alloc&) {
            return heapStatus(f, in, {kernel::HeapError::ResourceLimit});
        } catch (const std::length_error&) {
            return heapStatus(f, in, {kernel::HeapError::ResourceLimit});
        }
    }

    bool executeHeapRef(Frame& f, const sir::Instr& in) {
        try {
            const auto& ops = in.operands;
            const W4 ref = value(ops[0]);
            const bool load = in.op == Op::LoadRef || in.op == Op::LoadRefElem;
            if (in.op == Op::NbaRef) {
                designError(f, in,
                            "nonblocking assignment to a dynamically sized array element is "
                            "illegal (IEEE 1800-2023 10.4.2)");
                return false;
            }
            if (ref.a == 0) {
                if (load) fillDefault(&temp(in.result), in.type);
                return true;
            }
            const auto layout = refLayout(ref);
            std::vector<W4> data(heap_.words(layout));
            if (!heapStatus(f, in, heap_.readCell(layout, kernel::CellToken{ref.a}, data)))
                return false;
            const bool whole = in.op == Op::LoadRef || in.op == Op::StoreRef;
            std::size_t off = 0, valueAt = ops.size() - 1;
            sir::Type target = load ? in.type : typeOf(ops.back());
            if (whole) {
                if (ops.size() - (load ? 0 : 1) == 2) off = value(ops[1]).a;
            } else {
                const sir::FrameVar& fv = bodyFrame(act_.body)[ops[0].asFrame().index()];
                target = fv.target;
                if (!fv.dims.empty()) {
                    const auto e = elementOf(fv.dims, ops, 1);
                    if (!e) {
                        if (load) fillDefault(&temp(in.result), in.type);
                        return true;
                    }
                    off = std::size_t{*e} * chunksOf(target);
                }
                valueAt = fv.dims.size() + 1;
            }
            const std::size_t count = chunksOf(target);
            if (off > data.size() || count > data.size() - off)
                return heapStatus(f, in, {kernel::HeapError::InvalidType});
            if (load) {
                std::copy_n(data.data() + off, chunksOf(in.type), &temp(in.result));
                return true;
            }
            if (in.op == Op::StoreRefBits) {
                const auto pos = indexOf(ops[valueAt]);
                if (!pos) return true;
                const unsigned width = typeOf(ops[valueAt + 1]).width;
                const auto at = kernel::inside(*pos, width, target.width, ops[0].lsb, ops[0].width);
                if (!at) return true;
                const auto input = chunks(ops[valueAt + 1]);
                std::vector<W4> part(kernel::wide::chunks(at->n));
                kernel::wide::bits(part.data(), at->n, input.data(), width, at->skip);
                kernel::wide::put(data.data() + off, at->lo, part.data(), at->n);
            } else if (sir::isContainer(target)) {
                return heapStatus(
                    f, in,
                    heap_.storeNested(layout, kernel::CellToken{ref.a}, {value(ops[valueAt]).a}));
            } else {
                readOperand(ops[valueAt], data.data() + off, static_cast<std::uint32_t>(count));
            }
            return heapStatus(f, in, heap_.writeCell(layout, kernel::CellToken{ref.a}, data));
        } catch (const std::bad_alloc&) {
            return heapStatus(f, in, {kernel::HeapError::ResourceLimit});
        } catch (const std::length_error&) {
            return heapStatus(f, in, {kernel::HeapError::ResourceLimit});
        }
    }

    /// Runs the current body (act_) from block b until a wait, its ret, an
    /// error or the end of the simulation. A ret's value goes to `ret`.
    Exit runBody(Frame& f, sir::BlockId b, std::uint64_t& steps, W4& ret) {
        // A confirmed report can run after its originating process ended.
        // Stop only when this execution is newly canceled by the kernel.
        const bool wasStopped = sched_.programStopped(f.kernelId);
        const sir::Process& p = d_.process(f.process);
        const BodyId body = act_.body;
        for (;;) {
            const auto& instrs = bodyBlocks(body)[b.index()].instrs;
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
                // Nothing may be written or scheduled from the Postponed
                // region (4.4.2.9, SCHEDULER_SPEC.md 2.2): a run-time error.
                if (sched_.inPostponed() && changesState(in)) [[unlikely]] {
                    designError(f, in, "a write or an event in the Postponed region");
                    return Exit::Error;
                }
                const auto& ops = in.operands;
                switch (in.op) {
                    // SIGINT is looked at on each branch, which includes the
                    // back edges of loops (RUNTIME_ABI.md 4.4).
                    case Op::Br:
                        b = takeBranch(ops, 0);
                        jumped = true;
                        if (sched_.takeInterrupt()) return Exit::Stop;
                        break;
                    case Op::BrIf: {
                        const std::size_t second = 2 + ops[1].count;
                        b = takeBranch(ops, isTrue(value(ops[0])) ? 1 : second);
                        jumped = true;
                        if (sched_.takeInterrupt()) return Exit::Stop;
                        break;
                    }
                    case Op::Ret:
                        if (!ops.empty()) {
                            ret = value(ops[0]);
                            // A value wider than 64 bits goes back in retWide_.
                            if (const std::uint32_t c = chunksOf(typeOf(ops[0])); c > 1) {
                                retWide_.resize(c);
                                readOperand(ops[0], retWide_.data(), c);
                            }
                        }
                        return Exit::Ret;
                    case Op::CallTask: return enterTask(f, in) ? Exit::Entered : Exit::Error;
                    case Op::Call: {
                        const Exit e = call(f, in, steps);
                        if (e == Exit::Disabled && unwindsHere(f)) {
                            b = goOn(f, b, jumped);  // a disable ended the call
                            break;
                        }
                        if (e != Exit::Ret) return e;
                        if (sched_.ending()) return Exit::Stop;
                        if (!wasStopped && sched_.programStopped(f.kernelId)) return Exit::Killed;
                        break;
                    }
                    case Op::ScopeEnter:
                        f.scopes.push_back({ops[0].id, ops[1].asBlock(),
                                            static_cast<std::uint32_t>(f.calls.size()),
                                            static_cast<std::uint32_t>(f.syncCalls.size()),
                                            f.births});
                        break;
                    case Op::ScopeLeave: leaveScope(f, ops[0].id); break;
                    case Op::DisableScope:
                    case Op::DisableTask: {
                        const Exit e = disable(f, in);
                        if (e == Exit::Ret) break;
                        if (e == Exit::Disabled && unwindsHere(f)) {
                            b = goOn(f, b, jumped);
                            break;
                        }
                        return e;
                    }
                    case Op::Fork:
                    case Op::WaitFork: {
                        if (in.op == Op::Fork)
                            fork(f, ops);
                        else
                            f.pending = Frame::Join::WaitFork;
                        resumeAt(f) = ops.back().asBlock();
                        if (f.pending != Frame::Join::None && waitJoin(f)) return Exit::Waiting;
                        b = resumeAt(f);
                        jumped = true;
                        break;
                    }
                    case Op::DisableFork: disableFork(f); break;
                    case Op::WaitDelay: {
                        const std::optional<std::uint64_t> t = ticksOf(ops[0]);
                        if (t) sched_.waitDelay(f.kernelId, *t);
                        if (!t || sched_.takeOverflow()) {
                            designError(f, in, "the delay takes the time past the largest time");
                            return Exit::Error;
                        }
                        resumeAt(f) = ops[1].asBlock();
                        return Exit::Waiting;
                    }
                    case Op::WaitEvent: {
                        items_.clear();
                        monitors_.clear();
                        namedItems_.clear();
                        // The kernel keeps pointers into these.
                        f.iffs.clear();
                        f.iffs.reserve(ops.size());
                        f.watches.clear();
                        f.watches.reserve(ops.size());
                        enum class LastItem { Slot, Watch, Named };
                        LastItem last = LastItem::Slot;
                        for (std::size_t i = 0; i + 1 < ops.size(); ++i) {
                            const Operand& o = ops[i];
                            if (o.kind == Operand::Kind::Subroutine &&
                                d_.subroutines[o.id].kind == sir::SubroutineKind::Watch) {
                                monitors_.push_back(watchMonitor(f, o));
                                if (sched_.ending()) return Exit::Error;
                                last = LastItem::Watch;
                            } else if (o.kind == Operand::Kind::Subroutine) {  // the item's iff
                                if (last == LastItem::Watch)
                                    withIff(f, monitors_.back(), o.asSubroutine());
                                else if (last == LastItem::Named)
                                    withIff(f, namedItems_.back(), o.asSubroutine());
                                else
                                    withIff(f, items_.back(), o.asSubroutine());
                            } else if (d_.slot(o.asSlot()).type.kind == sir::TypeKind::Event) {
                                const std::optional<kernel::EventId> e = eventOf(o);
                                if (!e) {
                                    fail(p, "wait on a null event");
                                    return Exit::Error;
                                }
                                namedItems_.push_back({*e});
                                last = LastItem::Named;
                            } else {
                                kernel::WaitItem item;
                                item.part = {kernelSlot(o.asSlot()), o.firstElem, o.numElems, o.lsb,
                                             o.width};
                                item.edge = kernelEdge(o.edge);
                                items_.push_back(item);
                                last = LastItem::Slot;
                            }
                        }
                        sched_.waitAny(f.kernelId, items_, monitors_, namedItems_);
                        if (reportHeapMonitorLimit(instanceOf(p), in.loc)) return Exit::Stop;
                        resumeAt(f) = ops.back().asBlock();
                        return Exit::Waiting;
                    }
                    case Op::WaitCond: {
                        const std::optional<bool> waits = waitCond(f, ops[0].asSubroutine(), steps);
                        if (reportHeapMonitorLimit(instanceOf(p), in.loc)) return Exit::Stop;
                        if (!waits) return sched_.ending() ? Exit::Stop : Exit::Error;
                        if (*waits) {
                            resumeAt(f) = ops[1].asBlock();
                            return Exit::Waiting;
                        }
                        b = ops[1].asBlock();
                        jumped = true;
                        break;
                    }
                    case Op::WaitNamed: {
                        const std::optional<kernel::EventId> e = eventOf(ops[0]);
                        if (!e) {
                            fail(p, "wait on a null event");
                            return Exit::Error;
                        }
                        sched_.waitNamed(f.kernelId, *e);
                        resumeAt(f) = ops[1].asBlock();
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
                            const std::optional<std::uint64_t> t =
                                ops.size() > 1 ? ticksOf(ops[1]) : std::optional<std::uint64_t>(0);
                            if (!t) {
                                designError(f, in,
                                            "the delay takes the time past the largest time");
                                return Exit::Error;
                            }
                            sched_.triggerNb(f.kernelId, *e, *t);
                        }
                        break;
                    }
                    case Op::Builtin: builtin(in, info_[body.index()][b.index()][k]); break;
                    case Op::DriveDelay: {
                        const std::vector<std::uint64_t> buf = slotWords(ops[0], ops[1]);
                        const std::optional<std::uint64_t> rise = ticksOf(ops[2]);
                        const std::optional<std::uint64_t> fall = ticksOf(ops[3]);
                        const std::optional<std::uint64_t> off = ticksOf(ops[4]);
                        if (!rise || !fall || !off) {
                            designError(f, in, "the delay takes the time past the largest time");
                            return Exit::Error;
                        }
                        sched_.driveDelayed(info_[body.index()][b.index()][k].driver,
                                            {.slot = kernelSlot(ops[0].asSlot()),
                                             .elem = ops[0].firstElem,
                                             .lsb = ops[0].lsb,
                                             .width = ops[0].width},
                                            buf.data(), *rise, *fall, *off);
                        break;
                    }
                    case Op::Resolve: resolve(in, info_[body.index()][b.index()][k]); break;
                    case Op::Syscall: {
                        const InstrInfo& info = info_[body.index()][b.index()][k];
                        if (info.task == SysTask::Stacktrace) {
                            stacktrace(f, in);
                            break;
                        }
                        if (info.task == SysTask::Urandom || info.task == SysTask::UrandomRange) {
                            urandom(f, in, info.task == SysTask::UrandomRange);
                            break;
                        }
                        if (!syscall(f, p, in, info)) return Exit::Error;
                        if (sched_.ending()) return Exit::Stop;
                        if (!wasStopped && sched_.programStopped(f.kernelId)) return Exit::Killed;
                        break;
                    }
                    case Op::OverrideStart: {
                        const Exit e = overrideStart(in, info_[body.index()][b.index()][k], steps);
                        if (e != Exit::Ret) return e;
                        if (sched_.ending()) return Exit::Stop;
                        if (!wasStopped && sched_.programStopped(f.kernelId)) return Exit::Killed;
                        break;
                    }
                    case Op::Strobe:
                        if (ops.size() < 2)
                            sched_.strobe(overrides_[ops[0].id]->kernelId);
                        else  // $fstrobe: the files of its descriptor now (SYSTEM_TASKS.md 6.2)
                            sched_.strobe(overrides_[ops[0].id]->kernelId,
                                          fileTargets(in, value(ops[1]), "$fstrobe"));
                        break;
                    case Op::Monitor: {
                        if (ops.size() >= 2) {
                            // $fmonitor: one more monitor, with a frame of
                            // its own, printing to the files of its
                            // descriptor now (SYSTEM_TASKS.md 6.2).
                            kernel::FileTargets to = fileTargets(in, value(ops[1]), "$fmonitor");
                            Frame& m = newFrame(ops[0].asProcess());
                            m.kernelId = sched_.addProcess(&resume, &m, kernel::Group::Active);
                            const auto& deps = depsOf(bodyOfProcess(ops[0].asProcess()));
                            sched_.addFileMonitor(
                                m.kernelId, deps.slots, std::move(to),
                                {deps.heap,
                                 deps.privateContainers.empty() ? nullptr : &keepHeapRead, &deps});
                            // It lives until its files close; sweep frees
                            // the frame when the kernel says it ended.
                            fileMonitorFrames_.push_back(&m);
                            break;
                        }
                        // The new monitor starts afresh and shows what it
                        // reads (SYSTEM_TASKS.md 4).
                        Frame& m = *overrides_[ops[0].id];
                        freshVars(m);
                        monitor_ = &m;
                        const auto& deps = depsOf(bodyOfProcess(ops[0].asProcess()));
                        sched_.setMonitor(m.kernelId, deps.slots,
                                          {deps.heap,
                                           deps.privateContainers.empty() ? nullptr : &keepHeapRead,
                                           &deps});
                        break;
                    }
                    case Op::OverrideStop: {
                        const kernel::Hold level = holdLevel(info_[body.index()][b.index()][k]);
                        for (std::size_t i = 1; i < ops.size(); ++i)
                            sched_.release(level, partOf(ops[i]));
                        dropIdle(level);
                        break;
                    }
                    case Op::StoreOverride: {
                        const std::vector<std::uint64_t> buf = slotWords(ops[1], ops[2]);
                        sched_.writeHeld(holdLevel(info_[body.index()][b.index()][k]), f.kernelId,
                                         {.slot = kernelSlot(ops[1].asSlot()),
                                          .elem = ops[1].firstElem,
                                          .lsb = ops[1].lsb,
                                          .width = ops[1].width},
                                         buf.data());
                        break;
                    }
                    default:
                        if (!execute(f, p, in, info_[body.index()][b.index()][k]))
                            return Exit::Error;
                        if (sched_.ending()) return Exit::Stop;
                        if (!wasStopped && sched_.programStopped(f.kernelId)) return Exit::Killed;
                        break;
                }
                // An NBA, a ->> or a driver's update with a delay past the
                // largest time (SCHEDULER_SPEC.md 8): a run-time error.
                if (sched_.takeOverflow()) {
                    designError(f, in, "the delay takes the time past the largest time");
                    return Exit::Error;
                }
            }
            if (!jumped) {
                fail(p, "block bb" + std::to_string(b.index()) + " has no terminator");
                return Exit::Error;
            }
        }
    }

    // --- Procedural continuous assignments (SIR_SPEC.md 5.6) ---

    static kernel::Hold holdLevel(const InstrInfo& info) {
        return info.textFlag ? kernel::Hold::Force : kernel::Hold::Assign;
    }

    [[nodiscard]] kernel::SlotPart partOf(const Operand& o) const {
        return {kernelSlot(o.asSlot()), o.firstElem, o.numElems, o.lsb, o.width};
    }

    /// override.start: the targets go to the override process, which runs
    /// once now and then whenever what it reads changes. A process that no
    /// longer holds anything stops (10.6.1, 10.6.2).
    Exit overrideStart(const sir::Instr& in, const InstrInfo& info, std::uint64_t& steps) {
        const auto& ops = in.operands;
        const kernel::Hold level = holdLevel(info);
        Frame& o = *overrides_[ops[1].id];
        for (std::size_t i = 2; i < ops.size(); ++i)
            sched_.hold(level, o.kernelId, partOf(ops[i]));
        dropIdle(level);
        // It holds its targets from now, and so waits on its triggers
        // before it runs (sensitiveWhileRunning).
        o.holding = true;
        o.level = level;
        waitTriggers(o, d_.process(ops[1].asProcess()));
        const Act caller = act_;
        enter({bodyOfProcess(ops[1].asProcess()), &o.vars, caller.base + bodyTemps(caller.body),
               o.varsId});
        W4 ret{0, 0};
        const Exit e = runBody(o, sir::kEntryBlock, steps, ret);
        act_ = caller;
        return e;
    }

    // --- wait (expr) (SIR_SPEC.md 7.2) ---

    /// Runs guard g on the frame of the activation that waits: the process,
    /// or its innermost task call. Empty on an error.
    std::optional<bool> guard(Frame& f, sir::SubroutineId g, std::uint64_t& steps) {
        W4 ret{0, 0};
        if (!runWaiter(f, g, steps, ret)) return std::nullopt;
        return isTrue(ret);
    }

    /// Runs guard or watch g like guard(), its value in ret (and in
    /// retWide_ when wider than 64 bits). False on an error.
    bool runWaiter(Frame& f, sir::SubroutineId g, std::uint64_t& steps, W4& ret) {
        // The kernel calls it in the middle of a write, which may be deep
        // in the calls of another process (SCHEDULER_SPEC.md 9.5).
        if (stack_.nearlyUsedUp()) {
            stackLimit(d_.subroutine(g).instance, d_.subroutine(g).loc);
            return false;
        }
        const Act caller = act_;
        std::vector<W4>& vars = f.calls.empty() ? f.vars : f.calls.back().vars;
        const VarsId varsId = f.calls.empty() ? f.varsId : f.calls.back().varsId;
        enter({bodyOf(g), &vars, caller.base + bodyTemps(caller.body), varsId});
        const Exit e = runBody(f, sir::kEntryBlock, steps, ret);
        act_ = caller;
        return e == Exit::Ret;
    }

    /// The instructions whose first operand is the slot they write or the
    /// event they trigger.
    static bool isWriteTarget(Op op) {
        switch (op) {
            case Op::Store:
            case Op::Nba:
            case Op::NbaDelay:
            case Op::DriveDelay:
            case Op::StoreElem:
            case Op::NbaElem:
            case Op::NbaElemDelay:
            case Op::StoreBits:
            case Op::NbaBits:
            case Op::NbaBitsDelay:
            case Op::Trigger:
            case Op::TriggerNb: return true;
            default: return false;
        }
    }

    /// The slot parts a body reads, with what the functions it calls read.
    /// depsOf leaves out the variables of the functions it calls.
    void collectReads(BodyId body, GuardDeps& deps, std::vector<bool>& seen) const {
        if (seen[body.index()]) return;
        const bool own = std::none_of(seen.begin(), seen.end(), [](bool b) { return b; });
        seen[body.index()] = true;
        for (const sir::BasicBlock& bb : bodyBlocks(body)) {
            for (const sir::Instr& in : bb.instrs) {
                if (in.op == Op::FormatPattern && !in.operands.empty() &&
                    in.operands[0].id < patternShapes_.size() &&
                    patternShapes_[in.operands[0].id].heap)
                    deps.heap = true;
                if (sir::isContainerOp(in.op) || in.op == Op::LoadRef || in.op == Op::LoadRefElem)
                    deps.heap = true;
                if (in.op == Op::Call)
                    collectReads(bodyOf(in.operands[0].asSubroutine()), deps, seen);
                // A ref of the body's own frame (that of the waiter); the
                // refs of the functions it calls are bound by the call.
                const bool forwardedRef =
                    in.op == Op::RefMake && in.operands[0].kind == Operand::Kind::Frame &&
                    bodyFrame(body)[in.operands[0].id].type.kind == sir::TypeKind::Ref;
                // Forwarding an enclosing ref also reads its binding. An
                // empty container has no heap identity to observe; replacing
                // its variable must still reevaluate the called expression.
                if (own && (in.op == Op::LoadRef || in.op == Op::LoadRefElem || forwardedRef) &&
                    in.operands[0].kind == Operand::Kind::Frame) {
                    std::uint32_t leaf = 0;
                    if (in.op == Op::LoadRef && in.operands.size() == 2 &&
                        in.operands[1].kind == Operand::Kind::Const) {
                        const sir::Constant& c = d_.constants[in.operands[1].id];
                        leaf = c.aWords.empty() ? 0 : static_cast<std::uint32_t>(c.aWords[0]);
                    }
                    deps.refs.push_back({in.operands[0].asFrame(), leaf, in.op == Op::LoadRefElem});
                }
                // The target of a write or a trigger is not read: an
                // expression whose function triggers an event does not wait
                // on it, which would wake it again for its own trigger.
                const bool target = isWriteTarget(in.op);
                for (std::size_t i = 0; i < in.operands.size(); ++i) {
                    const Operand& o = in.operands[i];
                    if (o.kind != Operand::Kind::Slot || (target && i == 0)) continue;
                    const sir::Slot& slot = d_.slot(o.asSlot());
                    const bool whole = in.op != Op::Load;
                    deps.reads.push_back({o.asSlot(), whole ? 0 : o.firstElem,
                                          whole ? 0 : o.numElems, whole ? 0 : o.lsb,
                                          whole ? 0 : o.width});
                    if (slot.type.kind == sir::TypeKind::Event) deps.events.push_back(o.asSlot());
                }
            }
        }
    }

    /// What a body (a guard, or a postponed process) reads, found on its
    /// first use.
    const GuardDeps& depsOf(BodyId body) {
        if (guardDeps_.empty()) guardDeps_.resize(processCount() + d_.subroutines.size());
        std::optional<GuardDeps>& cached = guardDeps_[body.index()];
        if (cached) return *cached;
        GuardDeps deps;
        deps.vm = this;
        std::vector<bool> seen(processCount() + d_.subroutines.size(), false);
        collectReads(body, deps, seen);
        // The variables of a function the body calls are left out: the call
        // sets them again. Those of another function, read by a
        // hierarchical name, are what the body depends on (SCHEDULER_SPEC.md
        // 9.5).
        const auto called = [&](sir::SlotId s) {
            const sir::SubroutineId g = d_.slot(s).subroutine;
            return g != sir::kNone && seen[bodyOf(g).index()];
        };
        std::erase_if(deps.reads, [&](const sir::SlotRange& r) { return called(r.slot); });
        std::erase_if(deps.events, called);
        for (std::uint32_t i = 0; i < d_.slots.size(); ++i)
            if (sir::isContainer(d_.slots[i].type) && called(sir::SlotId{i}))
                deps.privateContainers.push_back(sir::SlotId{i});
        // The kernel's parts, made from the SIR's slots in one place (#39).
        for (const sir::SlotRange& r : deps.reads)
            deps.slots.push_back({kernelSlot(r.slot), r.firstElem, r.numElems, r.lsb, r.width});
        return cached.emplace(std::move(deps));
    }

    /// The frame variables of a process back to their values at its start.
    void freshVars(Frame& f) {
        const std::vector<W4> fresh = newVars(bodyOfProcess(f.process));
        std::copy(fresh.begin(), fresh.end(), f.vars.begin());
    }

    /// What a monitor record of frame f depends on: the slot parts guard or
    /// watch `body` reads, those its refs refer to now, and for
    /// ev.triggered the events its event slots hold now (15.5.3). A ref to
    /// an automatic variable is left out: only its own process writes it.
    /// The body reads the frame of the activation that waits, so a ref is
    /// found where the body's own reads find it, by its layout (SIR_SPEC.md
    /// 7.4).
    static kernel::HeapResult<bool> keepHeapRead(const void* frame,
                                                 kernel::HeapRoot root) noexcept {
        const auto& deps = *static_cast<const GuardDeps*>(frame);
        const auto& vm = *deps.vm;
        try {
            std::vector<kernel::HeapRoot> privateRoots;
            for (const auto slot : deps.privateContainers)
                for (std::uint64_t e = 0; e < sir::elementCount(vm.d_.slot(slot)); ++e)
                    privateRoots.push_back(kernel::HeapRoot::container(
                        {*vm.store_.element(vm.kernelSlot(slot), static_cast<std::uint32_t>(e))}));
            const auto privateRead = vm.heap_.reaches(privateRoots, root);
            return {!privateRead.value, privateRead.status};
        } catch (const std::bad_alloc&) {
            return {false, {kernel::HeapError::ResourceLimit}};
        } catch (const std::length_error&) {
            return {false, {kernel::HeapError::ResourceLimit}};
        }
    }

    void dropHeapWaits() noexcept {
        for (const auto& f : frames_) {
            if (f.released || f.kernelId == kernel::kNone) continue;
            sched_.detachDisplayMonitor(f.kernelId);
            if (heapWaits_) sched_.cancelWait(f.kernelId);
        }
    }

    bool reportHeapMonitorLimit(sir::InstanceId instance, sir::SourceLoc loc) {
        if (!sched_.heapMonitorLimited()) return false;
        if (!heapMonitorReported_) {
            const auto failed = sched_.failedHeapMonitorProcess();
            for (const auto& frame : frames_)
                if (!frame.released && frame.kernelId == failed) {
                    const auto& process = d_.process(frame.process);
                    instance = instanceOf(process);
                    loc = process.loc;
                    break;
                }
            heapMonitorReported_ = true;
            limitAt(instance, loc, "container monitor resource limit");
        }
        return true;
    }

    bool readsOf(const Frame& f, BodyId body, kernel::MonitorSpec& m) {
        const GuardDeps& deps = depsOf(body);
        m.heap = deps.heap;
        if (!deps.privateContainers.empty()) {
            m.heapReadFilter = &keepHeapRead;
            m.heapReadFrame = &deps;
        }
        m.slots = deps.slots;
        const std::vector<W4>& vars = f.calls.empty() ? f.vars : f.calls.back().vars;
        for (const GuardDeps::RefRead& rr : deps.refs) {
            const W4 r = vars[layouts_[body.index()].wordOf(rr.var)];
            if (isHeapRef(r)) {
                m.heap = true;
                continue;
            }
            if (isFrameRef(r)) continue;
            m.slots.push_back({kernelSlot(leafSlot(refSlot(r), rr.leaf)),
                               rr.anyElement ? 0 : refElem(r), rr.anyElement ? 0U : 1U, 0, 0});
        }
        for (const sir::SlotId s : deps.events) {
            const auto n = static_cast<std::uint32_t>(sir::elementCount(d_.slot(s)));
            for (std::uint32_t e = 0; e < n; ++e)
                if (const std::uint64_t h = *store_.element(kernelSlot(s), e); h != 0)
                    m.events.push_back(kernel::eventOfHandle(h));
        }
        heapWaits_ = heapWaits_ || m.heap;
        return true;
    }

    /// wait.cond: goes on when the guard is true, else waits on a monitor
    /// record that wakes the process when the guard becomes true
    /// (SCHEDULER_SPEC.md 9.3, 9.5). Returns whether it waits; empty on an
    /// error.
    std::optional<bool> waitCond(Frame& f, sir::SubroutineId g, std::uint64_t& steps) {
        const std::optional<bool> now = guard(f, g, steps);
        if (!now || *now) return now ? std::optional<bool>(false) : std::nullopt;
        kernel::MonitorSpec m;
        if (!readsOf(f, bodyOf(g), m)) return std::nullopt;
        m.eval = &evalCond;
        m.frame = &f;
        m.domain = kernel::Domain::Two;
        m.width = 1;
        m.untilTrue = true;
        f.cond = g;
        sched_.waitMonitor(f.kernelId, m);
        return true;
    }

    // --- disable (SIR_SPEC.md 7.2, SCHEDULER_SPEC.md 10.4) ---

    /// Whether the disable that sends the running frame on goes on in the
    /// body running now.
    static bool unwindsHere(const Frame& f) {
        return f.unwind.has_value() && f.unwind->calls == f.calls.size() &&
               f.unwind->sync == f.syncCalls.size();
    }

    /// Where the running body goes on after a disable: its exit block, or
    /// the instruction after the call it ended (block b).
    static sir::BlockId goOn(Frame& f, sir::BlockId b, bool& jumped) {
        const sir::BlockId exit = f.unwind.has_value() ? f.unwind->exit : sir::kNone;
        f.unwind.reset();
        if (exit == sir::kNone) return b;
        jumped = true;
        return exit;
    }

    /// scope.leave: the innermost activation of the block at this level
    /// ends, with any left inside it.
    static void leaveScope(Frame& f, std::uint32_t scope) {
        for (std::size_t i = f.scopes.size(); i-- > 0;) {
            const Frame::ScopeAct& a = f.scopes[i];
            if (a.scope == scope && a.calls == f.calls.size() && a.sync == f.syncCalls.size()) {
                f.scopes.resize(i);
                return;
            }
        }
    }

    /// Ends the children a frame made from birth number `from` on, with
    /// their descendants.
    void killChildren(Frame& f, std::uint64_t from) {
        for (Frame* c : childrenFrom(f, from)) {
            if (c->released) continue;
            if (!c->finished) {
                sched_.kill(c->kernelId);
                ended(*c);
            }
            disableFork(*c);
        }
    }

    /// disable.scope and disable.task: every activation of the block, or
    /// every call of the task, ends in every process. Each process drops
    /// the task calls made in it without their outputs, ends the children
    /// it made in it, drops its wait and goes on at the block's exit or
    /// after the task's call. Returns Ret when the running frame goes on
    /// after the instruction, Disabled when it goes on at f.unwind, Killed
    /// when it ended itself, or Error for a disable from a function of
    /// what is outside it.
    Exit disable(Frame& f, const sir::Instr& in) {
        const bool task = in.op == Op::DisableTask;
        // The task's body, or the scope's number (#N).
        const BodyId taskBody = task ? bodyOf(in.operands[0].asSubroutine()) : BodyId{};
        const std::uint32_t scope = task ? 0 : in.operands[0].id;
        const bool inFunction =
            isSubBody(act_.body) && subOf(act_.body).kind == sir::SubroutineKind::Function;
        bool self = false;
        for (Frame& g : frames_) {
            if (g.finished) continue;
            std::optional<Frame::Unwind> to;
            std::uint64_t children = 0;
            std::size_t keep = g.scopes.size();  ///< The activations that stay.
            if (!task) {
                for (std::size_t i = 0; i < g.scopes.size() && !to; ++i) {
                    const Frame::ScopeAct& a = g.scopes[i];
                    if (a.scope != scope) continue;
                    to = Frame::Unwind{a.calls, a.sync, a.exit};
                    children = a.children;
                    keep = i;
                }
            } else {
                for (std::size_t i = 0; i < g.calls.size() && !to; ++i)
                    if (g.calls[i].body == taskBody) {
                        to = Frame::Unwind{static_cast<std::uint32_t>(i), 0, sir::kNone};
                        children = g.calls[i].children;
                    }
                for (std::size_t j = 0; &g == &f && j < f.syncCalls.size() && !to; ++j)
                    if (f.syncCalls[j].body == taskBody) {
                        to = Frame::Unwind{static_cast<std::uint32_t>(f.calls.size()),
                                           static_cast<std::uint32_t>(j), sir::kNone};
                        children = f.syncCalls[j].children;
                    }
                // The activations begun inside the call.
                while (
                    to && keep > 0 &&
                    (g.scopes[keep - 1].calls > to->calls ||
                     (g.scopes[keep - 1].calls == to->calls && g.scopes[keep - 1].sync > to->sync)))
                    --keep;
            }
            if (!to) continue;
            // A function may end only its own blocks (SCHEDULER_SPEC.md 10.4).
            if (inFunction &&
                (&g != &f || to->calls != f.calls.size() || to->sync != f.syncCalls.size())) {
                designError(f, in, "disable in a function of a block or task outside the function");
                return Exit::Error;
            }
            g.scopes.resize(keep);
            killChildren(g, children);
            if (&g == &f) {
                f.unwind = to;
                self = true;
                continue;
            }
            while (g.calls.size() > to->calls) {
                retire(g.calls.back().varsId);
                g.calls.pop_back();
            }
            if (to->exit != sir::kNone) resumeAt(g) = to->exit;
            g.pending = Frame::Join::None;
            sched_.start(g.kernelId);
        }
        if (f.finished) return Exit::Killed;
        return self ? Exit::Disabled : Exit::Ret;
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
    /// The default of type t in its chunks.
    static void fillDefault(W4* p, sir::Type t) {
        if (chunksOf(t) == 1) {
            *p = defaultValue(t);
            return;
        }
        if (sir::isFourState(t)) {
            kernel::wide::allX(p, t.width);
        } else {
            for (std::uint32_t k = 0; k < chunksOf(t); ++k)
                p[k] = {0, 0};
        }
    }

    /// call: runs a function, or a task that does not wait, to its ret.
    /// Inputs and inouts are copied in, outputs and inouts copied back to
    /// the caller's places like blocking assignments (SIR_SPEC.md 8, 4.9.7).
    Exit call(Frame& f, const sir::Instr& in, std::uint64_t& steps) {
        const auto& ops = in.operands;
        const sir::Subroutine& s = d_.subroutine(ops[0].asSubroutine());
        if (tooDeep(f, in)) return Exit::Error;
        const Act caller = act_;
        const BodyId body = bodyOf(ops[0].asSubroutine());
        std::vector<W4> vars = newVars(body);
        // The arguments are read in the caller before anything is written.
        const std::vector<std::vector<W4>> args = readArgs(s, ops);
        enter({body, &vars, caller.base + bodyTemps(caller.body), enroll(vars, body)});
        bindArgs(body, s, args, vars);
        W4 ret{0, 0};
        ++callDepth_;
        f.syncCalls.push_back({body, f.births, &in});
        const Exit e = runBody(f, sir::kEntryBlock, steps, ret);
        std::vector<W4> wide;
        if (in.result != sir::kNone && chunksOf(in.type) > 1) wide = std::move(retWide_);
        f.syncCalls.pop_back();
        while (!f.scopes.empty() && f.scopes.back().calls == f.calls.size() &&
               f.scopes.back().sync > f.syncCalls.size())
            f.scopes.pop_back();
        --callDepth_;
        const std::vector<std::vector<W4>> outs = readOuts(body, s, vars);
        retire(act_.varsId);
        act_ = caller;
        if (e == Exit::Waiting) {
            fail(d_.process(f.process),
                 "task " + s.name + " waited; tasks that wait are not supported by the VM yet");
            return Exit::Error;
        }
        if (e != Exit::Ret) return e;
        writeOuts(s, ops, outs, *act_.vars, act_.body);
        if (in.result != sir::kNone) {
            if (const std::uint32_t c = chunksOf(in.type); c > 1) {
                W4* r = &temp(in.result);
                for (std::uint32_t k = 0; k < c; ++k)
                    r[k] = k < wide.size() ? wide[k] : W4{0, 0};
            } else {
                temp(in.result) = ret;
            }
        }
        return Exit::Ret;
    }

    /// Runs function s, of input arguments only, with these values, for
    /// frame f (a deferred assertion's call, SIR_SPEC.md 5.7).
    Exit callWith(Frame& f, sir::SubroutineId s, const std::vector<std::vector<W4>>& args,
                  std::uint64_t& steps) {
        const sir::Subroutine& sub = d_.subroutine(s);
        if (stack_.nearlyUsedUp()) {
            stackLimit(sub.instance, sub.loc);
            return Exit::Error;
        }
        const Act caller = act_;
        const BodyId body = bodyOf(s);
        std::vector<W4> vars = newVars(body);
        bindArgs(body, sub, args, vars);
        enter({body, &vars, caller.base + bodyTemps(caller.body), enroll(vars, body)});
        W4 ret{0, 0};
        ++callDepth_;
        f.syncCalls.push_back({body, f.births});
        const Exit e = runBody(f, sir::kEntryBlock, steps, ret);
        f.syncCalls.pop_back();
        --callDepth_;
        retire(act_.varsId);
        act_ = caller;
        return e;
    }

    /// Passes the arguments of the block operand at ops[i] to its
    /// parameters and returns the block.
    sir::BlockId takeBranch(const std::vector<Operand>& ops, std::size_t i) {
        const Operand& target = ops[i];
        const auto& params = bodyBlocks(act_.body)[target.id].params;
        if (target.count != 0) {
            // Read every chunk of every argument before writing any
            // parameter; a parameter wider than 64 bits takes all of its
            // chunks (SIR_SPEC.md 5.1).
            std::uint32_t total = 0;
            for (std::uint32_t a = 0; a < target.count; ++a)
                total += chunksOf(params[a].type);
            W4 vals[16];
            std::vector<W4> many;
            W4* v = total <= 16 ? vals : (many.resize(total), many.data());
            std::uint32_t at = 0;
            for (std::uint32_t a = 0; a < target.count; ++a) {
                const std::uint32_t c = chunksOf(params[a].type);
                readOperand(ops[i + 1 + a], v + at, c);
                at += c;
            }
            at = 0;
            for (std::uint32_t a = 0; a < target.count; ++a) {
                const std::uint32_t c = chunksOf(params[a].type);
                W4* p = &temp(params[a].temp);
                for (std::uint32_t k = 0; k < c; ++k)
                    p[k] = v[at + k];
                at += c;
            }
        }
        return target.asBlock();
    }

    /// Runs one instruction that is not a terminator or a system call.
    /// resolve (SIR_SPEC.md 3.5): each driver is a value and its strengths
    /// for 1 and for 0 (VALUE_SEMANTICS.md 5.4).
    void resolve(const sir::Instr& in, const InstrInfo& info) {
        const auto& ops = in.operands;
        const unsigned w = in.type.width;
        if (const std::uint32_t c = chunksOf(in.type);
            c > 1 || (w > 32 && std::any_of(ops.begin() + 1, ops.end(), [&](const Operand& o) {
                          return typeOf(o).width > 64;
                      }))) {
            // Side halves can start between words. Extract each half at
            // the net's full width, not at the current chunk's width.
            std::vector<std::vector<W4>> values;
            std::vector<std::vector<W4>> sides;
            std::vector<std::pair<std::uint8_t, std::uint8_t>> strengths;
            for (std::size_t i = 1; i + 2 < ops.size();) {
                values.push_back(chunks(ops[i]));
                strengths.emplace_back(static_cast<std::uint8_t>(value(ops[i + 1]).a),
                                       static_cast<std::uint8_t>(value(ops[i + 2]).a));
                i += 3;
                sides.push_back(i < ops.size() && !sir::isFourState(typeOf(ops[i]))
                                    ? chunks(ops[i++])
                                    : std::vector<W4>{});
            }
            const auto sideWord = [](const std::vector<W4>& mask, std::uint32_t bit) {
                const auto word = bit / 64;
                const auto shift = bit % 64;
                std::uint64_t result = mask[word].a >> shift;
                if (shift != 0 && word + 1 < mask.size())
                    result |= mask[word + 1].a << (64 - shift);
                return result;
            };
            W4* r = &temp(in.result);
            for (std::uint32_t k = 0; k < c; ++k) {
                drivers_.clear();
                for (std::size_t d = 0; d < values.size(); ++d) {
                    kernel::StrengthDriver driver{values[d][k], strengths[d].first,
                                                  strengths[d].second};
                    if (!sides[d].empty()) {
                        driver.one = sideWord(sides[d], 64 * k);
                        driver.zero = sideWord(sides[d], w + 64 * k);
                    }
                    drivers_.push_back(driver);
                }
                r[k] = kernel::resolve4(info.netKind, drivers_.data(),
                                        static_cast<unsigned>(drivers_.size()),
                                        w - 64 * k < 64 ? w - 64 * k : 64);
            }
            return;
        }
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

    // --- Values wider than 64 bits (kernel/wide.h) ---

    /// Whether an instruction has an integral operand or result wider than
    /// 64 bits; those run in executeWide.
    bool isWide(BodyId body, const sir::Instr& in) const {
        if (chunksOf(in.type) > 1) return true;
        for (const Operand& o : in.operands) {
            if (o.kind != Operand::Kind::Temp && o.kind != Operand::Kind::Const &&
                o.kind != Operand::Kind::Frame && o.kind != Operand::Kind::Slot)
                continue;
            if (chunksOf(typeIn(body, o)) > 1) return true;
        }
        return false;
    }

    /// The chunks of operand i, at its own width.
    std::vector<W4> chunks(const Operand& o) const {
        std::vector<W4> x(chunksOf(typeOf(o)));
        readOperand(o, x.data(), static_cast<std::uint32_t>(x.size()));
        return x;
    }

    /// The chunks of operand o resized to width w: extended by its own
    /// signedness, or truncated.
    std::vector<W4> chunksAt(const Operand& o, unsigned w) const {
        const sir::Type t = typeOf(o);
        const std::vector<W4> x = chunks(o);
        std::vector<W4> r(kernel::wide::chunks(w));
        kernel::wide::resize(r.data(), w, x.data(), t.width, sir::isSigned(t));
        return r;
    }

    /// Writes a result of the instruction's type; a two-state type has no
    /// x or z (VALUE_SEMANTICS.md 1.2).
    void setWide(const sir::Instr& in, const std::vector<W4>& r) {
        W4* p = &temp(in.result);
        const sir::Type rt = in.type;
        const std::uint32_t c = chunksOf(rt);
        if (sir::isIntegral(rt) && !sir::isFourState(rt))
            kernel::wide::to2(p, r.data(), rt.width);
        else
            for (std::uint32_t k = 0; k < c; ++k)
                p[k] = k < r.size() ? r[k] : W4{0, 0};
    }

    /// An index or position operand's value, read at its whole width, or
    /// nothing for x, z or a value that does not fit in 64 signed bits
    /// (VALUE_SEMANTICS.md 3.11). Every index the VM reads comes here.
    std::optional<std::int64_t> indexOf(const Operand& o) const {
        const sir::Type t = typeOf(o);
        if (chunksOf(t) == 1) return kernel::position4(value(o), t.width, sir::isSigned(t));
        const std::vector<W4> x = chunks(o);
        return kernel::wide::position(x.data(), t.width, sir::isSigned(t));
    }

    bool executeWide(Frame& f, const sir::Process& p, const sir::Instr& in, const InstrInfo& info,
                     std::uint64_t delay) {
        namespace kw = kernel::wide;
        const auto& ops = in.operands;
        const sir::Type rt = in.type;
        const unsigned w = rt.width;
        const auto t = [&](std::size_t i) { return typeOf(ops[i]); };
        const auto get = [&](std::size_t i) { return chunks(ops[i]); };
        std::vector<W4> r(chunksOf(rt));
        const auto set1 = [&](W4 x) { temp(in.result) = x; };
        const auto binary = [&](auto fn) {
            const std::vector<W4> x = get(0);
            const std::vector<W4> y = get(1);
            fn(r.data(), x.data(), y.data(), w);
            setWide(in, r);
        };
        const auto compare = [&](auto fn) {
            const unsigned ow = t(0).width;
            const std::vector<W4> x = get(0);
            const std::vector<W4> y = get(1);
            set1(fn(x.data(), y.data(), ow));
        };
        switch (in.op) {
            case Op::Load:
                readSlotWide(ops[0].asSlot(), ops[0].firstElem, ops[0].lsb, ops[0].width, r.data());
                setWide(in, r);
                return true;
            case Op::Store: {
                const std::vector<W4> x = get(1);
                writeSlotWide(ops[0].asSlot(), ops[0].firstElem, ops[0].lsb, ops[0].width,
                              x.data());
                return true;
            }
            case Op::Nba:
            case Op::NbaDelay: {
                const std::vector<W4> x = get(1);
                const std::optional<std::uint64_t> delay =
                    in.op == Op::NbaDelay ? ticksOf(ops[2]) : std::optional<std::uint64_t>(0);
                if (!delay) {
                    designError(f, in, "the delay takes the time past the largest time");
                    return false;
                }
                nbaSlotWide(f, ops[0].asSlot(), ops[0].firstElem, ops[0].lsb, ops[0].width,
                            x.data(), *delay);
                return true;
            }
            case Op::Fload: {
                const W4* src = &frameVar(ops[0].asFrame());
                for (std::size_t k = 0; k < r.size(); ++k)
                    r[k] = src[k];
                setWide(in, r);
                return true;
            }
            case Op::Fstore: {
                const std::vector<W4> x = get(1);
                W4* dst = &frameVar(ops[0].asFrame());
                for (std::size_t k = 0; k < x.size(); ++k)
                    dst[k] = x[k];
                return true;
            }
            case Op::LoadElem: {
                if (options_.boundsCheck) checkBounds(in);
                const bool frame = ops[0].kind == Operand::Kind::Frame;
                const std::vector<sir::ArrayDim>& dims =
                    frame ? bodyFrame(act_.body)[ops[0].asFrame().index()].dims
                          : d_.slot(ops[0].asSlot()).dims;
                const std::optional<std::uint32_t> e = elementOf(dims, ops, 1);
                if (!e) {
                    fillDefault(r.data(), rt);  // VALUE_SEMANTICS.md 3.11
                } else if (frame) {
                    const W4* src = &frameElem(ops[0].asFrame(), *e);
                    for (std::size_t k = 0; k < r.size(); ++k)
                        r[k] = src[k];
                } else {
                    readSlotWide(ops[0].asSlot(), *e, 0, 0, r.data());
                }
                setWide(in, r);
                return true;
            }
            case Op::StoreElem:
            case Op::NbaElem:
            case Op::NbaElemDelay: {
                if (options_.boundsCheck) checkBounds(in);
                const bool frame = ops[0].kind == Operand::Kind::Frame;
                const std::vector<sir::ArrayDim>& dims =
                    frame ? bodyFrame(act_.body)[ops[0].asFrame().index()].dims
                          : d_.slot(ops[0].asSlot()).dims;
                const std::optional<std::uint32_t> e = elementOf(dims, ops, 1);
                if (!e) return true;  // an invalid index writes nothing
                const std::vector<W4> x = get(dims.size() + 1);
                if (frame) {
                    W4* dst = &frameElem(ops[0].asFrame(), *e);
                    for (std::size_t k = 0; k < x.size(); ++k)
                        dst[k] = x[k];
                } else if (in.op == Op::StoreElem) {
                    writeSlotWide(ops[0].asSlot(), *e, 0, 0, x.data());
                } else {
                    nbaSlotWide(f, ops[0].asSlot(), *e, 0, 0, x.data(), delay);
                }
                return true;
            }
            case Op::StoreBits:
            case Op::NbaBits:
            case Op::NbaBitsDelay: {
                if (options_.boundsCheck) checkBounds(in);
                // A part at a run-time position: only the bits inside the
                // element (VALUE_SEMANTICS.md 3.11).
                const bool frame = ops[0].kind == Operand::Kind::Frame;
                const std::vector<sir::ArrayDim>& dims =
                    frame ? bodyFrame(act_.body)[ops[0].asFrame().index()].dims
                          : d_.slot(ops[0].asSlot()).dims;
                std::uint32_t e = 0;
                if (!dims.empty()) {
                    const std::optional<std::uint32_t> el = elementOf(dims, ops, 1);
                    if (!el) return true;
                    e = *el;
                }
                const std::size_t n = dims.size();
                const std::optional<std::int64_t> pos = indexOf(ops[n + 1]);
                if (!pos) return true;
                const sir::Type et = frame ? bodyFrame(act_.body)[ops[0].asFrame().index()].type
                                           : d_.slot(ops[0].asSlot()).type;
                const unsigned vw = t(n + 2).width;
                const std::optional<kernel::Inside> at =
                    kernel::inside(*pos, vw, et.width, ops[0].lsb, ops[0].width);
                if (!at) return true;
                const std::vector<W4> x = get(n + 2);
                std::vector<W4> part(kw::chunks(at->n));
                kw::bits(part.data(), at->n, x.data(), vw, at->skip);
                if (frame) {
                    W4* cell = dims.empty() ? &frameVar(ops[0].asFrame())
                                            : &frameElem(ops[0].asFrame(), e);
                    kw::put(cell, at->lo, part.data(), at->n);
                } else if (in.op == Op::StoreBits) {
                    writeSlotWide(ops[0].asSlot(), e, at->lo, at->n, part.data());
                } else {
                    nbaSlotWide(f, ops[0].asSlot(), e, at->lo, at->n, part.data(), delay);
                }
                return true;
            }
            case Op::Add: binary(kw::add); return true;
            case Op::Sub: binary(kw::sub); return true;
            case Op::Mul: binary(kw::mul); return true;
            case Op::Div:
                binary([&](W4* o, const W4* x, const W4* y, unsigned ww) {
                    kw::div(o, x, y, ww, sir::isSigned(rt));
                });
                return true;
            case Op::Mod:
                binary([&](W4* o, const W4* x, const W4* y, unsigned ww) {
                    kw::mod(o, x, y, ww, sir::isSigned(rt));
                });
                return true;
            case Op::Pow: {
                const std::vector<W4> x = get(0);
                const std::vector<W4> y = get(1);
                kw::pow(r.data(), x.data(), w, sir::isSigned(t(0)), y.data(), t(1).width,
                        sir::isSigned(t(1)));
                setWide(in, r);
                return true;
            }
            case Op::Neg: {
                const std::vector<W4> x = get(0);
                kw::neg(r.data(), x.data(), w);
                setWide(in, r);
                return true;
            }
            case Op::And: binary(kw::bitAnd); return true;
            case Op::Or: binary(kw::bitOr); return true;
            case Op::Xor: binary(kw::bitXor); return true;
            case Op::Xnor: binary(kw::bitXnor); return true;
            case Op::Not: {
                const std::vector<W4> x = get(0);
                kw::bitNot(r.data(), x.data(), w);
                setWide(in, r);
                return true;
            }
            case Op::Lt:
                compare([&](const W4* x, const W4* y, unsigned ow) {
                    return kw::lt(x, y, ow, sir::isSigned(t(0)));
                });
                return true;
            case Op::Le:
                compare([&](const W4* x, const W4* y, unsigned ow) {
                    return kw::le(x, y, ow, sir::isSigned(t(0)));
                });
                return true;
            case Op::Gt:
                compare([&](const W4* x, const W4* y, unsigned ow) {
                    return kw::gt(x, y, ow, sir::isSigned(t(0)));
                });
                return true;
            case Op::Ge:
                compare([&](const W4* x, const W4* y, unsigned ow) {
                    return kw::ge(x, y, ow, sir::isSigned(t(0)));
                });
                return true;
            case Op::Eq: compare(kw::eq); return true;
            case Op::Ne: compare(kw::ne); return true;
            case Op::Ceq: compare(kw::ceq); return true;
            case Op::Cne: compare(kw::cne); return true;
            case Op::Weq: compare(kw::weq); return true;
            case Op::Wne: compare(kw::wne); return true;
            case Op::MatchMask: {
                const bool ignoreX = info.textFlag;  // casex
                compare([&](const W4* x, const W4* y, unsigned ow) {
                    return kw::caseMatch(x, y, ow, ignoreX);
                });
                return true;
            }
            case Op::Truth: {
                const std::vector<W4> x = get(0);
                set1(kw::truth(x.data(), t(0).width));
                return true;
            }
            case Op::Lnot: {
                const std::vector<W4> x = get(0);
                set1(kernel::lnot4(kw::truth(x.data(), t(0).width)));
                return true;
            }
            case Op::Land:
            case Op::Lor:
            case Op::Limp:
            case Op::Leqv: {
                const std::vector<W4> x = get(0);
                const std::vector<W4> y = get(1);
                const W4 a = kw::truth(x.data(), t(0).width);
                const W4 b = kw::truth(y.data(), t(1).width);
                switch (in.op) {
                    case Op::Land: set1(kernel::land4(a, b)); break;
                    case Op::Lor: set1(kernel::lor4(a, b)); break;
                    case Op::Limp: set1(kernel::lor4(kernel::lnot4(a), b)); break;
                    default: {
                        const W4 both = kernel::land4(a, b);
                        const W4 neither = kernel::land4(kernel::lnot4(a), kernel::lnot4(b));
                        set1(kernel::lor4(both, neither));
                        break;
                    }
                }
                return true;
            }
            case Op::Rand:
            case Op::Rnand:
            case Op::Ror:
            case Op::Rnor:
            case Op::Rxor:
            case Op::Rxnor: {
                const std::vector<W4> x = get(0);
                const unsigned xw = t(0).width;
                W4 v{0, 0};
                switch (in.op) {
                    case Op::Rand: v = kw::redAnd(x.data(), xw); break;
                    case Op::Rnand: v = kernel::negate1(kw::redAnd(x.data(), xw)); break;
                    case Op::Ror: v = kw::redOr(x.data(), xw); break;
                    case Op::Rnor: v = kernel::negate1(kw::redOr(x.data(), xw)); break;
                    case Op::Rxor: v = kw::redXor(x.data(), xw); break;
                    default: v = kernel::negate1(kw::redXor(x.data(), xw)); break;
                }
                set1(v);
                return true;
            }
            case Op::Shl:
            case Op::Shr:
            case Op::Ashr: {
                const std::vector<W4> x = get(0);
                const std::vector<W4> amount = get(1);
                if (in.op == Op::Shl)
                    kw::shl(r.data(), x.data(), w, amount.data(), t(1).width);
                else
                    kw::ashr(r.data(), x.data(), w, amount.data(), t(1).width,
                             in.op == Op::Ashr && sir::isSigned(rt));
                setWide(in, r);
                return true;
            }
            case Op::Select: {
                const std::vector<W4> x = get(1);
                const std::vector<W4> y = get(2);
                kw::select(r.data(), value(ops[0]), x.data(), y.data(), w);
                setWide(in, r);
                return true;
            }
            case Op::Bitsel: {
                if (options_.boundsCheck) checkBounds(in);
                const std::optional<std::int64_t> pos = indexOf(ops[1]);
                const std::vector<W4> x = get(0);
                if (!pos || *pos < 0 || *pos >= t(0).width) {
                    set1(kernel::kW4X);
                    if (!sir::isFourState(rt)) set1({0, 0});
                    return true;
                }
                W4 bit{0, 0};
                kw::bits(&bit, 1, x.data(), t(0).width, *pos);
                set1(sir::isFourState(rt) ? bit : kernel::to2(bit));
                return true;
            }
            case Op::Partsel:
            case Op::IdxPartsel: {
                if (options_.boundsCheck) checkBounds(in);
                const std::optional<std::int64_t> pos =
                    in.op == Op::Partsel
                        ? std::optional<std::int64_t>(static_cast<std::int64_t>(value(ops[1]).a))
                        : indexOf(ops[1]);
                if (!pos) {
                    kw::allX(r.data(), w);
                } else {
                    const std::vector<W4> x = get(0);
                    kw::bits(r.data(), w, x.data(), t(0).width, *pos);
                }
                setWide(in, r);
                return true;
            }
            case Op::Concat: {
                // Most significant first (VALUE_SEMANTICS.md 7.1).
                unsigned at = w;
                for (std::size_t i = 0; i < ops.size(); ++i) {
                    const unsigned ow = t(i).width;
                    at -= ow;
                    const std::vector<W4> x = get(i);
                    kw::put(r.data(), at, x.data(), ow);
                }
                setWide(in, r);
                return true;
            }
            case Op::Repl: {
                const std::uint64_t n = value(ops[0]).a;
                const unsigned ow = t(1).width;
                const std::vector<W4> x = get(1);
                for (std::uint64_t i = 0; i < n; ++i)
                    kw::put(r.data(), static_cast<unsigned>(i * ow), x.data(), ow);
                setWide(in, r);
                return true;
            }
            case Op::Zext:
            case Op::Sext:
            case Op::Trunc:
            case Op::To2:
            case Op::To4: {
                const std::vector<W4> x = get(0);
                kw::resize(r.data(), w, x.data(), t(0).width, in.op == Op::Sext);
                if (in.op == Op::To2) kw::to2(r.data(), r.data(), w);
                if (chunksOf(rt) == 1)
                    set1(r[0]);
                else
                    setWide(in, r);
                return true;
            }
            case Op::Itor: {
                const std::vector<W4> x = get(0);
                temp(in.result) =
                    fromReal(kw::toReal(x.data(), t(0).width, sir::isSigned(t(0))), rt);
                return true;
            }
            case Op::Rtoi:
                kw::fromReal(r.data(), w, toReal(value(ops[0]), t(0)), sir::isFourState(rt));
                setWide(in, r);
                return true;
            default: break;
        }
        fail(p, std::string("instruction ") + sir::opName(in.op) +
                    " on values wider than 64 bits is not supported by the VM yet");
        return false;
    }

    /// Reads and writes through a ref of a value wider than 64 bits: a
    /// slot element, or the chunks of frame words a ref steps through.
    /// Returns false for a narrower value, which the caller handles.
    bool refWide(Frame& f, const sir::Process& p, const sir::Instr& in) {
        namespace kw = kernel::wide;
        const auto& ops = in.operands;
        const bool load = in.op == Op::LoadRef || in.op == Op::LoadRefElem;
        if (in.op == Op::LoadRef || in.op == Op::StoreRef || in.op == Op::NbaRef) {
            const sir::Type vt = load ? in.type : typeOf(ops.back());
            const std::uint32_t c = chunksOf(vt);
            if (c == 1) return false;
            const W4 r = value(ops[0]);
            const std::size_t n = ops.size() - (load ? 0 : 1);
            const std::uint64_t k = n == 2 ? value(ops[1]).a : 0;
            if (load) {
                W4* out = &temp(in.result);
                if (isFrameRef(r)) {
                    for (std::uint32_t j = 0; j < c; ++j)
                        out[j] = refWord(r, k + j);
                } else {
                    readSlotWide(leafSlot(refSlot(r), static_cast<std::uint32_t>(k)), refElem(r), 0,
                                 0, out);
                }
                return true;
            }
            const std::vector<W4> x = chunks(ops.back());
            if (isFrameRef(r)) {
                if (in.op == Op::NbaRef) {
                    fail(p, "nonblocking assignment to an automatic variable through a ref");
                    return true;
                }
                for (std::uint32_t j = 0; j < c; ++j)
                    refWord(r, k + j) = x[j];
            } else if (in.op == Op::StoreRef) {
                writeSlotWide(leafSlot(refSlot(r), static_cast<std::uint32_t>(k)), refElem(r), 0, 0,
                              x.data());
            } else {
                nbaSlotWide(f, leafSlot(refSlot(r), static_cast<std::uint32_t>(k)), refElem(r), 0,
                            0, x.data());
            }
            return true;
        }
        const sir::FrameVar& fv = bodyFrame(act_.body)[ops[0].asFrame().index()];
        const std::uint32_t c = chunksOf(fv.target);
        if (c == 1) return false;
        const W4 r = frameVar(ops[0].asFrame());
        std::uint32_t e = 0;
        if (!fv.dims.empty()) {
            const std::optional<std::uint32_t> el = elementOf(fv.dims, ops, 1);
            if (!el) {
                // An invalid index reads the default and writes nothing
                // (VALUE_SEMANTICS.md 3.11).
                if (load) fillDefault(&temp(in.result), in.type);
                return true;
            }
            e = *el;
        }
        const std::size_t n = fv.dims.size();
        if (in.op == Op::LoadRefElem) {
            W4* out = &temp(in.result);
            if (isFrameRef(r)) {
                for (std::uint32_t j = 0; j < c; ++j)
                    out[j] = refWord(r, std::uint64_t{e} * c + j);
            } else {
                readSlotWide(refSlot(r), refElem(r) + e, 0, 0, out);
            }
            return true;
        }
        if (in.op == Op::StoreRefElem) {
            const std::vector<W4> x = chunks(ops[n + 1]);
            if (isFrameRef(r)) {
                for (std::uint32_t j = 0; j < c; ++j)
                    refWord(r, std::uint64_t{e} * c + j) = x[j];
            } else {
                writeSlotWide(refSlot(r), refElem(r) + e, 0, 0, x.data());
            }
            return true;
        }
        // A part at a run-time position: only the bits inside the element
        // (VALUE_SEMANTICS.md 3.11).
        const std::optional<std::int64_t> pos = indexOf(ops[n + 1]);
        if (!pos) return true;
        const unsigned vw = typeOf(ops[n + 2]).width;
        const std::optional<kernel::Inside> at =
            kernel::inside(*pos, vw, fv.target.width, ops[0].lsb, ops[0].width);
        if (!at) return true;
        const std::vector<W4> x = chunks(ops[n + 2]);
        std::vector<W4> part(kw::chunks(at->n));
        kw::bits(part.data(), at->n, x.data(), vw, at->skip);
        if (isFrameRef(r))
            kw::put(&refWord(r, std::uint64_t{e} * c), at->lo, part.data(), at->n);
        else
            writeSlotWide(refSlot(r), refElem(r) + e, at->lo, at->n, part.data());
        return true;
    }

    /// The instructions executeWide carries out.
    static bool wideHandles(Op op) {
        switch (op) {
            case Op::Add:
            case Op::And:
            case Op::Ashr:
            case Op::Bitsel:
            case Op::Ceq:
            case Op::Cne:
            case Op::Concat:
            case Op::Div:
            case Op::Eq:
            case Op::Fload:
            case Op::Fstore:
            case Op::Ge:
            case Op::Gt:
            case Op::IdxPartsel:
            case Op::Itor:
            case Op::Land:
            case Op::Le:
            case Op::Leqv:
            case Op::Limp:
            case Op::Lnot:
            case Op::Load:
            case Op::LoadElem:
            case Op::Lor:
            case Op::Lt:
            case Op::MatchMask:
            case Op::Mod:
            case Op::Mul:
            case Op::Nba:
            case Op::NbaBits:
            case Op::NbaBitsDelay:
            case Op::NbaDelay:
            case Op::NbaElem:
            case Op::NbaElemDelay:
            case Op::Ne:
            case Op::Neg:
            case Op::Not:
            case Op::Or:
            case Op::Partsel:
            case Op::Pow:
            case Op::Rand:
            case Op::Repl:
            case Op::Rnand:
            case Op::Rnor:
            case Op::Ror:
            case Op::Rtoi:
            case Op::Rxnor:
            case Op::Rxor:
            case Op::Select:
            case Op::Sext:
            case Op::Shl:
            case Op::Shr:
            case Op::Store:
            case Op::StoreBits:
            case Op::StoreElem:
            case Op::Sub:
            case Op::To2:
            case Op::To4:
            case Op::Trunc:
            case Op::Truth:
            case Op::Weq:
            case Op::Wne:
            case Op::Xnor:
            case Op::Xor:
            case Op::Zext: return true;
            default: return false;
        }
    }

    /// Decode the tier's operands for the existing index/bit-range checks.
    /// This observes the access; the ordinary instructions still perform
    /// it with the same defaults and clipping (VALUE_SEMANTICS.md 3.11).
    void checkBounds(const sir::Instr& in) {
        const auto& ops = in.operands;
        bool invalid = false;
        switch (in.op) {
            case Op::Bitsel:
            case Op::Partsel:
            case Op::IdxPartsel:
                invalid =
                    !kernel::fullyInside(indexOf(ops[1]), in.type.width, typeOf(ops[0]).width);
                break;
            case Op::LoadElem:
            case Op::StoreElem:
            case Op::NbaElem:
            case Op::NbaElemDelay:
            case Op::StoreBits:
            case Op::NbaBits:
            case Op::NbaBitsDelay:
            case Op::LoadRefElem:
            case Op::StoreRefElem:
            case Op::StoreRefBits: {
                const bool ref = in.op == Op::LoadRefElem || in.op == Op::StoreRefElem ||
                                 in.op == Op::StoreRefBits;
                const bool frame = ops[0].kind == Operand::Kind::Frame;
                const sir::FrameVar* fv =
                    frame ? &bodyFrame(act_.body)[ops[0].asFrame().index()] : nullptr;
                const std::vector<sir::ArrayDim>& dims =
                    frame ? fv->dims : d_.slot(ops[0].asSlot()).dims;
                invalid = !elementOf(dims, ops, 1);
                const bool bits = in.op == Op::StoreBits || in.op == Op::NbaBits ||
                                  in.op == Op::NbaBitsDelay || in.op == Op::StoreRefBits;
                if (!invalid && bits) {
                    const sir::Type type = ref     ? fv->target
                                           : frame ? fv->type
                                                   : d_.slot(ops[0].asSlot()).type;
                    invalid = !kernel::fullyInside(indexOf(ops[dims.size() + 1]),
                                                   typeOf(ops[dims.size() + 2]).width, type.width,
                                                   ops[0].lsb, ops[0].width);
                }
                break;
            }
            default: return;
        }
        if (invalid) {
            std::uint64_t& count = boundsCounts_[{in.loc.file, in.loc.line, in.loc.column}];
            if (++count == 1)
                warnOnce(in, "index or packed selection is out of range or contains X/Z bits "
                             "(--bounds-check=warn)");
        }
    }

    bool execute(Frame& f, const sir::Process& p, const sir::Instr& in, const InstrInfo& info) {
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
        const auto publishHeap = [&](bool ok) {
            sched_.flushHeapChanges();
            reportHeapMonitorLimit(instanceOf(p), in.loc);
            return ok && !sched_.ending();
        };
        if (in.op == Op::FormatPattern) return publishHeap(executePattern(f, p, in));
        if (sir::isContainerOp(in.op)) return publishHeap(executeContainer(f, in));
        if ((in.op == Op::LoadRef || in.op == Op::StoreRef || in.op == Op::NbaRef ||
             in.op == Op::LoadRefElem || in.op == Op::StoreRefElem || in.op == Op::StoreRefBits) &&
            isHeapRef(value(ops[0])))
            return publishHeap(executeHeapRef(f, in));
        const bool delayedTarget = in.op == Op::NbaElemDelay || in.op == Op::NbaBitsDelay;
        const std::optional<std::uint64_t> delay =
            delayedTarget ? ticksOf(ops.back()) : std::optional<std::uint64_t>(0);
        if (!delay) {
            designError(f, in, "the delay takes the time past the largest time");
            return false;
        }
        if (info.wide) return executeWide(f, p, in, info, *delay);
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
                case Op::Itos:
                    put(kernel::wide::toText(chunks(ops[0]).data(), t(0).width));
                    return true;
                case Op::Stoi:
                    if (chunksOf(rt) > 1) {
                        std::vector<W4> r(chunksOf(rt));
                        kernel::wide::fromText(r.data(), w, str(0));
                        setWide(in, r);
                    } else {
                        W4 r{0, 0};
                        kernel::wide::fromText(&r, w, str(0));
                        set(r);
                    }
                    return true;
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
                    const std::optional<std::int64_t> n = indexOf(ops[0]);
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
                    const std::optional<std::int64_t> i = indexOf(ops[1]);
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
                case Op::Rtoi: {
                    // VALUE_SEMANTICS.md 7.4, at any width.
                    W4 x{0, 0};
                    kernel::wide::fromReal(&x, w, r(0), sir::isFourState(rt));
                    set(x);
                    return true;
                }
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
                set(readSlot(o.asSlot(), o.firstElem, o.lsb, o.width));
                return true;
            }
            case Op::Store:
            case Op::Nba: {
                const Operand& o = ops[0];
                if (in.op == Op::Store)
                    writeSlot(o.asSlot(), o.firstElem, o.lsb, o.width, v(1));
                else
                    nbaSlot(f, o.asSlot(), o.firstElem, o.lsb, o.width, v(1));
                return true;
            }
            case Op::NbaDelay: {
                // The update in the NBA region of the time slot `delay` ticks
                // ahead (SCHEDULER_SPEC.md 13); x or z ticks are 0 (9.4.1).
                const Operand& o = ops[0];
                const std::optional<std::uint64_t> d = ticksOf(ops[2]);
                if (!d) {
                    designError(f, in, "the delay takes the time past the largest time");
                    return false;
                }
                nbaSlot(f, o.asSlot(), o.firstElem, o.lsb, o.width, v(1), *d);
                return true;
            }
            case Op::LoadElem: {
                if (options_.boundsCheck) checkBounds(in);
                const bool frame = ops[0].kind == Operand::Kind::Frame;
                const std::optional<std::uint32_t> e =
                    elementOf(frame ? bodyFrame(act_.body)[ops[0].asFrame().index()].dims
                                    : d_.slot(ops[0].asSlot()).dims,
                              ops, 1);
                // An invalid index reads the element type's default
                // (VALUE_SEMANTICS.md 3.11).
                if (!e)
                    set(defaultValue(rt));
                else
                    set(frame ? frameElem(ops[0].asFrame(), *e)
                              : readSlot(ops[0].asSlot(), *e, 0, 0));
                return true;
            }
            case Op::StoreElem:
            case Op::NbaElem:
            case Op::NbaElemDelay: {
                if (options_.boundsCheck) checkBounds(in);
                const bool frame = ops[0].kind == Operand::Kind::Frame;
                const std::optional<std::uint32_t> e =
                    elementOf(frame ? bodyFrame(act_.body)[ops[0].asFrame().index()].dims
                                    : d_.slot(ops[0].asSlot()).dims,
                              ops, 1);
                if (!e) return true;  // an invalid index writes nothing
                const std::size_t valueAt = ops.size() - (delayedTarget ? 2 : 1);
                if (frame)
                    frameElem(ops[0].asFrame(), *e) = v(valueAt);
                else if (in.op == Op::StoreElem)
                    writeSlot(ops[0].asSlot(), *e, 0, 0, v(valueAt));
                else
                    nbaSlot(f, ops[0].asSlot(), *e, 0, 0, v(valueAt), *delay);
                return true;
            }
            case Op::StoreBits:
            case Op::NbaBits:
            case Op::NbaBitsDelay: {
                if (options_.boundsCheck) checkBounds(in);
                const bool frame = ops[0].kind == Operand::Kind::Frame;
                const std::vector<sir::ArrayDim>& dims =
                    frame ? bodyFrame(act_.body)[ops[0].asFrame().index()].dims
                          : d_.slot(ops[0].asSlot()).dims;
                std::uint32_t e = 0;
                if (!dims.empty()) {
                    const std::optional<std::uint32_t> el = elementOf(dims, ops, 1);
                    if (!el) return true;
                    e = *el;
                }
                const std::size_t n = dims.size();
                if (frame) {
                    W4& cell =
                        dims.empty() ? frameVar(ops[0].asFrame()) : frameElem(ops[0].asFrame(), e);
                    if (const std::optional<std::int64_t> pos = indexOf(ops[n + 1]))
                        cell = kernel::writeBits4(
                            cell, bodyFrame(act_.body)[ops[0].asFrame().index()].type.width, *pos,
                            v(n + 2), t(n + 2).width, ops[0].lsb, ops[0].width);
                    return true;
                }
                storeBits(f, ops[0].asSlot(), e, indexOf(ops[n + 1]), v(n + 2), t(n + 2).width,
                          in.op != Op::StoreBits, *delay, ops[0].lsb, ops[0].width);
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
                constexpr unsigned inlineInputs = 32;
                std::uint8_t prev[inlineInputs];
                std::uint8_t cur[inlineInputs];
                std::uint8_t r = kernel::kUdpX;
                if (n > inlineInputs && in.op == Op::Udp) {
                    r = kernel::udpOutputRead(table, [&](unsigned i) { return code(v(1ULL + i)); });
                } else if (n > inlineInputs) {
                    r = kernel::udpStepRead(
                        table, code(v(1)), [&](unsigned i) { return code(v(2ULL + i)); },
                        [&](unsigned i) { return code(v(2ULL + n + i)); });
                } else if (in.op == Op::Udp) {
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
                const sir::Type target = o.kind == Operand::Kind::Slot
                                             ? d_.slot(o.asSlot()).type
                                             : bodyFrame(act_.body)[o.asFrame().index()].type;
                if (sir::isContainer(target) && !prepareHeap(f, in)) return false;
                if (o.kind == Operand::Kind::Slot) {
                    std::uint32_t e = 0;
                    if (ops.size() > 1) {
                        const std::optional<std::uint32_t> el =
                            elementOf(d_.slot(o.asSlot()).dims, ops, 1);
                        if (!el) {
                            fail(p, "ref to an element outside its array");
                            return false;
                        }
                        e = *el;
                    }
                    set(typedRef(slotRef(o.asSlot(), e), target));
                    return true;
                }
                const sir::FrameVar& fv = bodyFrame(act_.body)[o.asFrame().index()];
                if (fv.type.kind == sir::TypeKind::Ref) {
                    // A ref argument passed on, or an element of the array
                    // it refers to.
                    W4 r = frameVar(o.asFrame());
                    if (ops.size() > 1) {
                        const std::optional<std::uint32_t> el = elementOf(fv.dims, ops, 1);
                        if (!el) {
                            fail(p, "ref to an element outside its array");
                            return false;
                        }
                        if (isFrameRef(r)) {
                            const std::optional<W4> moved =
                                refStep(r, std::uint64_t{*el} * chunksOf(fv.target));
                            if (!moved) {
                                fail(p, "ref past the words of its frame");
                                return false;
                            }
                            r = *moved;
                        } else {
                            r.b += std::uint64_t{*el} << 1;
                        }
                    }
                    set(r);
                    return true;
                }
                // A ref steps through the words of a value's chunks; from
                // one leaf of a structure to the next one word at a time
                // (front keeps wider members out of those).
                std::size_t at = layouts_[act_.body.index()].wordOf(o.asFrame());
                if (!fv.dims.empty()) {
                    if (ops.size() > 1) {
                        const std::optional<std::uint32_t> el = elementOf(fv.dims, ops, 1);
                        if (!el) {
                            fail(p, "ref to an element outside its array");
                            return false;
                        }
                        at += std::size_t{*el} * chunksOf(fv.type);
                    }
                }
                const std::optional<W4> ref = frameRef(act_.varsId, at);
                if (!ref) {
                    fail(p, "ref past the words of its frame");
                    return false;
                }
                set(typedRef(*ref, target));
                return true;
            }
            case Op::LoadRef:
            case Op::StoreRef:
            case Op::NbaRef: {
                if (refWide(f, p, in)) return true;
                // The leaf of a structure: the next slots or frame words.
                const W4 r = v(0);
                const std::size_t n = ops.size() - (in.op == Op::LoadRef ? 0 : 1);
                const std::uint64_t k = n == 2 ? v(1).a : 0;
                if (in.op == Op::LoadRef) {
                    set(isFrameRef(r)
                            ? refWord(r, k)
                            : readSlot(leafSlot(refSlot(r), static_cast<std::uint32_t>(k)),
                                       refElem(r), 0, 0));
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
                    writeSlot(leafSlot(refSlot(r), static_cast<std::uint32_t>(k)), refElem(r), 0, 0,
                              x);
                } else {
                    nbaSlot(f, leafSlot(refSlot(r), static_cast<std::uint32_t>(k)), refElem(r), 0,
                            0, x);
                }
                return true;
            }
            case Op::LoadRefElem:
            case Op::StoreRefElem:
            case Op::StoreRefBits: {
                if (options_.boundsCheck) checkBounds(in);
                if (refWide(f, p, in)) return true;
                const sir::FrameVar& fv = bodyFrame(act_.body)[ops[0].asFrame().index()];
                const W4 r = frameVar(ops[0].asFrame());
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
                    if (const std::optional<std::int64_t> pos = indexOf(ops[n + 1]))
                        cell = kernel::writeBits4(cell, fv.target.width, *pos, v(n + 2),
                                                  t(n + 2).width, ops[0].lsb, ops[0].width);
                } else {
                    storeBits(f, refSlot(r), refElem(r) + e, indexOf(ops[n + 1]), v(n + 2),
                              t(n + 2).width, false, 0, ops[0].lsb, ops[0].width);
                }
                return true;
            }
            case Op::ReportCall: {
                // The kernel runs or drops it (SCHEDULER_SPEC.md 15.2).
                Deferred d;
                d.subroutine = ops[1].asSubroutine();
                for (std::size_t i = 2; i < ops.size(); ++i) {
                    std::vector<W4> a(chunksOf(t(i)));
                    readOperand(ops[i], a.data(), static_cast<std::uint32_t>(a.size()));
                    d.args.push_back(std::move(a));
                }
                std::uint32_t token = 0;
                if (!freeDeferred_.empty()) {
                    token = freeDeferred_.back();
                    freeDeferred_.pop_back();
                    deferred_[token] = std::move(d);
                } else {
                    token = static_cast<std::uint32_t>(deferred_.size());
                    deferred_.push_back(std::move(d));
                }
                ++f.reports;
                sched_.deferCall(f.kernelId, info.textFlag, &deferredCall, &f, token);  // final
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
            case Op::Fload: set(frameVar(ops[0].asFrame())); return true;
            case Op::Fstore: frameVar(ops[0].asFrame()) = v(1); return true;
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
                set(kernel::caseMatch4(v(0), v(1), info.textFlag));  // casex
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
            // Selects, concatenation and replication (VALUE_SEMANTICS.md
            // 3.11, 7.1, 7.2); X for no position, which to2 makes 0 for a
            // two-state result.
            case Op::Bitsel: {
                if (options_.boundsCheck) checkBounds(in);
                const std::optional<std::int64_t> pos = indexOf(ops[1]);
                set(pos ? kernel::bits4(v(0), t(0).width, *pos, 1) : kernel::kW4X);
                return true;
            }
            case Op::Partsel:
                if (options_.boundsCheck) checkBounds(in);
                set(kernel::bits4(v(0), t(0).width, static_cast<std::int64_t>(v(1).a), w));
                return true;
            case Op::IdxPartsel: {
                if (options_.boundsCheck) checkBounds(in);
                const std::optional<std::int64_t> pos = indexOf(ops[1]);
                set(pos ? kernel::bits4(v(0), t(0).width, *pos, w) : kernel::allX(w));
                return true;
            }
            case Op::Concat: {
                W4 r{0, 0};
                for (std::size_t i = 0; i < ops.size(); ++i)
                    r = kernel::concat4(r, v(i), t(i).width);
                set(r);
                return true;
            }
            case Op::Repl: set(kernel::repl4(v(1), t(1).width, v(0).a)); return true;
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

    /// The format of a scan: constant text or a value evaluated by front.
    /// Unknown integral bits give EOF without reading (SYSTEM_TASKS.md 6.3).
    std::optional<std::string> scanFormat(const Operand& o) const {
        if (o.kind == Operand::Kind::Text) return o.text;
        const sir::Type type = typeOf(o);
        if (type.kind == sir::TypeKind::String) return std::string(strings_.get(value(o).a));
        const std::vector<W4> words = chunks(o);
        if (kernel::wide::hasUnknown(words.data(), type.width)) return std::nullopt;
        return kernel::wide::toText(words.data(), type.width);
    }

    /// Maps the tier's output type to the shared format checker's type.
    kernel::ScanTarget scanTarget(sir::Type type) const {
        return sir::isIntegral(type)                ? kernel::ScanTarget::Integral
               : isRealType(type)                   ? kernel::ScanTarget::Real
               : type.kind == sir::TypeKind::String ? kernel::ScanTarget::String
                                                    : kernel::ScanTarget::Other;
    }

    /// Resolves the tier's types for the shared scan-format checker.
    kernel::ScanPlan scanPlan(const sir::Instr& in, std::string_view format) const {
        if (in.operands[2].kind == Operand::Kind::Text) return {std::string(format), {}, false};
        std::vector<kernel::ScanTarget> targets;
        for (std::size_t i = 3; i < in.operands.size(); ++i) {
            targets.push_back(scanTarget(typeOf(in.operands[i])));
        }
        return kernel::prepareScanFormat(format, targets);
    }

    /// Output widths supplied by SIR; the kernel decides transfer sizes.
    std::vector<std::uint32_t> scanWidths(const sir::Instr& in) const {
        std::vector<std::uint32_t> widths;
        for (std::size_t i = 3; i < in.operands.size(); ++i)
            widths.push_back(typeOf(in.operands[i]).width);
        return widths;
    }

    /// A value read by $sscanf or $value$plusargs in the type of its target
    /// (SYSTEM_TASKS.md 6.3, 9): the low bits of an integral value, a lone
    /// x or z in every bit, a string's characters, a real.
    W4 scanned(const kernel::ScanValue& v, sir::Type t) {
        W4 r{0, 0};
        scanned(v, t, &r);
        return r;
    }
    /// The same into the chunks of an integral target of any width: the
    /// value extended with 0, or negated for a decimal minus sign, and cut
    /// to the width.
    void scanned(const kernel::ScanValue& v, sir::Type t, W4* out) {
        const std::uint32_t c = chunksOf(t);
        if (c == 1 || !sir::isIntegral(t)) {
            *out = scannedWord(v, t);
            return;
        }
        std::vector<W4> r(c);
        if (v.conv == 's') {
            kernel::wide::fromText(r.data(), t.width, v.text);
        } else if (v.fill) {
            for (W4& k : r)
                k = {v.bits.a != 0 ? ~std::uint64_t{0} : 0, ~std::uint64_t{0}};
        } else {
            for (std::uint32_t k = 0; k < c && k < v.wide.size(); ++k)
                r[k] = v.wide[k];
            if (v.negative) {
                std::vector<W4> zero(c);
                kernel::wide::sub(r.data(), zero.data(), std::vector<W4>(r).data(), t.width);
            }
        }
        if (t.width % 64 != 0) {
            r.back().a &= kernel::mask(t.width % 64);
            r.back().b &= kernel::mask(t.width % 64);
        }
        if (!sir::isFourState(t))
            kernel::wide::to2(out, r.data(), t.width);
        else
            std::copy(r.begin(), r.end(), out);
    }
    W4 scannedWord(const kernel::ScanValue& v, sir::Type t) {
        if (t.kind == sir::TypeKind::String)
            return {strings_.intern(v.conv == 's' ? v.text : std::string()), 0};
        if (isRealType(t)) return fromReal(v.conv == 'f' ? v.real : 0.0, t);
        W4 r = v.bits;
        if (v.conv == 's') kernel::wide::fromText(&r, t.width, v.text);
        if (v.fill) r = {v.bits.a != 0 ? ~std::uint64_t{0} : 0, ~std::uint64_t{0}};
        r = {r.a & kernel::mask(t.width), r.b & kernel::mask(t.width)};
        return sir::isFourState(t) ? r : kernel::to2(r);
    }

    /// The event an element of an event slot holds; nothing for null.
    std::optional<kernel::EventId> eventOf(const sir::Operand& o) const {
        const std::uint64_t h = readSlot(o.asSlot(), o.firstElem, 0, 0).a;
        if (h == 0) return std::nullopt;
        return kernel::eventOfHandle(h);
    }

    void builtin(const sir::Instr& in, const InstrInfo& info) {
        const auto& ops = in.operands;
        const auto v = [&](std::size_t i) { return value(ops[i]); };
        const auto t = [&](std::size_t i) { return typeOf(ops[i]); };
        if (info.method == Method::Triggered) {
            // False for a null event (15.5.3).
            const std::uint64_t h = v(1).a;
            const bool on = h != 0 && sched_.triggered(kernel::eventOfHandle(h));
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
        const auto index = [&](std::size_t i) { return indexOf(ops[i]); };
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
                const std::vector<std::uint64_t> words = formatWords(ops[2]);
                const kernel::IntValue iv{words.data(), at.width, sir::isFourState(at),
                                          sir::isSigned(at)};
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

    /// Translate explicit SIR presentation layouts into borrowed kernel views.
    /// Values are captured by the instruction's typed operands, not AST names.
    bool executePattern(Frame& f, const sir::Process& p, const sir::Instr& in) {
        using Kind = sir::PatternLayout::Kind;
        using View = kernel::PatternValue;
        struct Node {
            explicit Node(std::pmr::memory_resource* memory)
                : children(memory), names(memory), enumerators(memory), enumWords(memory),
                  words(memory), raw(memory) {}
            View view;
            std::pmr::vector<View> children;
            std::pmr::vector<kernel::PatternName> names;
            std::pmr::vector<kernel::PatternEnumName> enumerators;
            std::pmr::vector<std::pmr::vector<std::uint64_t>> enumWords;
            std::pmr::vector<std::uint64_t> words;
            std::pmr::vector<W4> raw;
        };
        struct Work {
            Node* node;
            sir::PatternId layout;
            std::size_t next = 0;
            bool opened = false;
            kernel::HeapTypeId heapType = kernel::kNone;
            kernel::ContainerToken token;
        };
        try {
            auto* memory = heap_.memoryResource();
            std::pmr::deque<Node> nodes(memory);
            std::pmr::vector<Work> work(memory);
            nodes.emplace_back(memory);
            work.push_back({&nodes.back(), sir::PatternId{in.operands[0].id}});
            std::size_t input = 2;
            while (!work.empty()) {
                auto& at = work.back();
                Node& node = *at.node;
                const auto& layout = d_.patterns[at.layout.index()];
                const bool packed =
                    layout.kind == Kind::PackedRecord || layout.kind == Kind::PackedUnion;
                const bool aggregate = layout.kind == Kind::Record || layout.kind == Kind::Union ||
                                       layout.kind == Kind::Array ||
                                       layout.kind == Kind::TaggedUnion;
                if (!at.opened) {
                    at.opened = true;
                    if (!aggregate && node.raw.empty()) {
                        const auto data = chunks(in.operands[input++]);
                        node.raw.assign(data.begin(), data.end());
                    }
                    std::size_t count = 0;
                    switch (layout.kind) {
                        case Kind::Integral: node.view.kind = View::Kind::Integral; break;
                        case Kind::Real:
                            node.view.kind = View::Kind::Real;
                            node.view.real = toReal(node.raw.front(), layout.value);
                            break;
                        case Kind::String:
                            node.view.kind = View::Kind::String;
                            node.view.text = strings_.get(node.raw.front().a);
                            break;
                        case Kind::Enumeration: node.view.kind = View::Kind::Enumeration; break;
                        case Kind::PackedRecord:
                        case Kind::Record:
                            node.view.kind = View::Kind::Record;
                            count = layout.members.size();
                            break;
                        case Kind::PackedUnion:
                        case Kind::Union:
                            node.view.kind = View::Kind::Union;
                            count = 1;
                            break;
                        case Kind::Array:
                            node.view.kind = View::Kind::Array;
                            if (layout.count > node.children.max_size()) throw std::bad_alloc{};
                            count = static_cast<std::size_t>(layout.count);
                            break;
                        case Kind::TaggedUnion:
                            node.view.kind = View::Kind::TaggedUnion;
                            node.view.tag = {layout.tag, layout.tagEscaped};
                            count = 1;
                            break;
                        case Kind::Container: {
                            node.view.kind = View::Kind::Array;
                            if (!prepareHeap(f, in)) return false;
                            at.token = kernel::ContainerToken{node.raw.front().a};
                            const auto actual = valueHeapType(layout.value, at.token);
                            if (!heapStatus(f, in, actual.status)) return false;
                            at.heapType = actual.value;
                            const auto size = heap_.size(at.heapType, at.token);
                            if (!heapStatus(f, in, size.status)) return false;
                            if (size.value > node.children.max_size() ||
                                size.value > static_cast<std::uint64_t>(
                                                 std::numeric_limits<std::int64_t>::max()))
                                throw std::bad_alloc{};
                            count = static_cast<std::size_t>(size.value);
                            break;
                        }
                    }
                    if (layout.kind == Kind::Integral || layout.kind == Kind::Enumeration) {
                        for (const auto& word : node.raw) {
                            node.words.push_back(sir::isFourState(layout.value) ? word.a
                                                                                : word.a & ~word.b);
                            if (sir::isFourState(layout.value)) node.words.push_back(word.b);
                        }
                        node.view.integral = {node.words.data(), layout.value.width,
                                              sir::isFourState(layout.value),
                                              sir::isSigned(layout.value)};
                    }
                    if (layout.kind == Kind::Enumeration) {
                        node.enumWords.resize(layout.enumerators.size());
                        node.enumerators.resize(layout.enumerators.size());
                        for (std::size_t i = 0; i < layout.enumerators.size(); ++i) {
                            const auto& item = layout.enumerators[i];
                            const auto& constant = d_.constant(item.value);
                            for (std::size_t j = 0; j < constant.aWords.size(); ++j) {
                                node.enumWords[i].push_back(constant.aWords[j]);
                                if (sir::isFourState(layout.value))
                                    node.enumWords[i].push_back(constant.bWords[j]);
                            }
                            node.enumerators[i] = {{node.enumWords[i].data(), layout.value.width,
                                                    sir::isFourState(layout.value),
                                                    sir::isSigned(layout.value)},
                                                   {item.name, item.escaped}};
                        }
                        node.view.enumerators = node.enumerators;
                    }
                    node.children.resize(count);
                    node.view.children = node.children;
                    if (layout.kind == Kind::Record || layout.kind == Kind::Union || packed) {
                        for (std::size_t i = 0; i < count; ++i)
                            node.names.push_back(
                                {layout.members[i].name, layout.members[i].escaped});
                        node.view.names = node.names;
                    }
                }
                if (at.next == node.children.size()) {
                    const View completed = node.view;
                    work.pop_back();
                    if (!work.empty()) work.back().node->children[work.back().next - 1] = completed;
                    continue;
                }
                const auto index = at.next++;
                const bool member =
                    layout.kind == Kind::Record || layout.kind == Kind::Union || packed;
                const auto childLayout = member ? layout.members[index].layout : layout.element;
                nodes.emplace_back(memory);
                Node& child = nodes.back();
                if (packed) {
                    const auto type = d_.patterns[childLayout.index()].value;
                    child.raw.resize(chunksOf(type));
                    kernel::wide::bits(child.raw.data(), type.width, node.raw.data(),
                                       layout.value.width, layout.members[index].lsb);
                    if (!sir::isFourState(type))
                        kernel::wide::to2(child.raw.data(), child.raw.data(), type.width);
                } else if (layout.kind == Kind::Container) {
                    const auto type = d_.patterns[childLayout.index()].value;
                    child.raw.resize(chunksOf(type));
                    if (!heapStatus(f, in,
                                    heap_.read(at.heapType, at.token,
                                               static_cast<std::int64_t>(index), child.raw)))
                        return false;
                }
                work.push_back({&child, childLayout});
            }
            const auto result = kernel::formatPattern(nodes.front().view,
                                                      in.operands[1].text == "positional", memory);
            if (result.error == kernel::PatternError::ResourceLimit) throw std::bad_alloc{};
            if (!result || input != in.operands.size()) {
                fail(p, "invalid pattern formatting value");
                return false;
            }
            formattedResult(in, result.text);
            return true;
        } catch (const std::bad_alloc&) {
            limitAt(instanceOf(p), in.loc, "pattern formatting allocation limit reached");
        } catch (const std::length_error&) {
            limitAt(instanceOf(p), in.loc, "pattern formatting size limit reached");
        }
        return false;
    }

    /// An integral operand's words laid out like a slot element, as
    /// kernel::IntValue takes them: a and b words in turn when four-state.
    std::vector<std::uint64_t> formatWords(const Operand& o) const {
        const sir::Type t = typeOf(o);
        const bool four = sir::isFourState(t);
        std::vector<std::uint64_t> words;
        for (const W4& c : chunks(o)) {
            words.push_back(c.a);
            words.push_back(four ? c.b : 0);
        }
        if (!four) {
            // Two-state: the a words only.
            for (std::size_t k = 0; k * 2 < words.size(); ++k)
                words[k] = words[k * 2];
            words.resize((words.size() + 1) / 2);
        }
        return words;
    }

    /// An operand as a real: a real's value, an integral one converted
    /// (VALUE_SEMANTICS.md 1.5) at any width.
    double realOf(const Operand& o) const {
        const sir::Type t = typeOf(o);
        if (sir::isIntegral(t) && chunksOf(t) > 1)
            return kernel::wide::toReal(chunks(o).data(), t.width, sir::isSigned(t));
        return toReal(value(o), t);
    }

    std::string formatValue(const Operand& o, const kernel::FormatSpec& spec) const {
        const sir::Type t = typeOf(o);
        const W4 x = value(o);
        if (t.kind == sir::TypeKind::String) return kernel::formatString(strings_.get(x.a), spec);
        if (isRealType(t) && spec.conv == 't')
            return kernel::formatTimeReal(toReal(x, t), d_.timePrecision, timeFormat_, spec.width,
                                          spec.left);
        if (isRealType(t)) {
            // front makes every other specifier take an integer; without
            // one, a real shows like %g (SYSTEM_TASKS.md 3.6).
            const bool realSpec = spec.conv == 'e' || spec.conv == 'f' || spec.conv == 'g';
            return kernel::formatReal(toReal(x, t), realSpec ? spec : kernel::FormatSpec{});
        }
        const std::vector<std::uint64_t> words = formatWords(o);
        const kernel::IntValue iv{words.data(), t.width, sir::isFourState(t), sir::isSigned(t)};
        if (spec.conv == 't')
            return kernel::formatTime(iv, d_.timePrecision, timeFormat_, spec.width, spec.left);
        return kernel::formatInt(iv, spec);
    }

    /// Marshal formatted bytes into the result type supplied by front.
    void formattedResult(const sir::Instr& in, std::string bytes) {
        if (in.type.kind == sir::TypeKind::String) {
            temp(in.result) = {strings_.intern(kernel::formattedString(std::move(bytes))), 0};
            return;
        }
        std::vector<W4> words(chunksOf(in.type));
        kernel::wide::fromText(words.data(), in.type.width, bytes);
        setWide(in, words);
    }

    /// The text of the format operands from `i` on: a text operand is a
    /// format; the values after it fill its specifiers, and values left
    /// over print in decimal (SYSTEM_TASKS.md 3.1).
    std::optional<std::string> formatOperands(const sir::Process& p, const sir::Instr& in,
                                              const InstrInfo& info, std::size_t i) {
        const auto& ops = in.operands;
        std::string text;
        while (i < ops.size()) {
            if (ops[i].kind != Operand::Kind::Text) {
                kernel::FormatSpec spec;
                spec.conv = info.radix;
                text += formatValue(ops[i], spec);
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
                text += formatValue(ops[i], piece.spec);
                ++i;
            }
        }
        return text;
    }

    /// A run-time warning at in's place, given once for each place
    /// (UNSPECIFIED_BEHAVIOR.md 4).
    void warnOnce(const sir::Instr& in, std::string_view text) {
        if (!warned_.insert(&in).second) return;
        std::string line = in.loc.file < d_.sources.size() ? d_.sources[in.loc.file] : "?";
        line += ":" + std::to_string(in.loc.line) + ":" + std::to_string(in.loc.column);
        line += ": warning: ";
        line += text;
        output_.message(line);
    }

    /// Whether a process is a procedure, which has a generator of its own:
    /// an initial, always or final procedure (SYSTEM_TASKS.md 13.2).
    static bool isProcedure(const sir::Process& p) {
        if (p.parent.kind != sir::Owner::Kind::None) return false;  // a fork branch
        switch (p.kind) {
            case sir::ProcessKind::Initial:
            case sir::ProcessKind::Final:
            case sir::ProcessKind::General:
            case sir::ProcessKind::Comb:
            case sir::ProcessKind::Latch:
            case sir::ProcessKind::CombAt:
            case sir::ProcessKind::Ff:
            case sir::ProcessKind::FfPlain: return true;
            default: return false;
        }
    }

    /// The generator a process draws from (SYSTEM_TASKS.md 13.2): its own,
    /// for a procedure or a fork branch, or else its instance's
    /// initialization generator.
    kernel::Rng& rngOf(Frame& f) {
        return f.ownRng ? f.rng : initRngs_[d_.process(f.process).instance.index()];
    }

    /// $urandom([seed]) and $urandom_range(maxval, minval) (SYSTEM_TASKS.md
    /// 13.3): a seed starts the generator again from its low 32 bits.
    void urandom(Frame& f, const sir::Instr& in, bool range) {
        kernel::Rng& g = rngOf(f);
        const auto& ops = in.operands;
        if (range) {
            const auto maxval = static_cast<std::uint32_t>(value(ops[1]).a);
            const auto minval = static_cast<std::uint32_t>(value(ops[2]).a);
            temp(in.result) = {kernel::urandomRange(g, maxval, minval), 0};
            return;
        }
        if (ops.size() > 1) g.state = static_cast<std::uint32_t>(value(ops[1]).a);
        temp(in.result) = {g.draw(), 0};
    }

    /// $stacktrace (SYSTEM_TASKS.md 10.3): where it is called, with the
    /// scope front gives, then each call that led there, innermost first:
    /// the task or function, or the module instance of the procedure, and
    /// the place of the call. A task prints the lines; a function returns
    /// them, with no newline after the last.
    void stacktrace(const Frame& f, const sir::Instr& in) {
        const auto where = [&](const sir::SourceLoc& l) {
            return (l.file < d_.sources.size() ? d_.sources[l.file] : std::string("?")) + ":" +
                   std::to_string(l.line);
        };
        const auto scopeOf = [&](BodyId body) {
            return isSubBody(body) ? subOf(body).name
                                   : d_.instance(processOf(body).instance).scopeName;
        };
        // The bodies from the process in, each with the call that entered it.
        std::vector<std::pair<BodyId, const sir::Instr*>> chain{
            {bodyOfProcess(f.process), nullptr}};
        for (const Frame::TaskCall& c : f.calls)
            chain.emplace_back(c.body, c.site);
        for (const Frame::SyncCall& c : f.syncCalls)
            chain.emplace_back(c.body, c.site);
        std::string text = in.operands[1].text + " (" + where(in.loc) + ")";
        for (std::size_t k = chain.size() - 1; k-- > 0;) {
            const sir::Instr* site = chain[k + 1].second;
            text += "\n" + scopeOf(chain[k].first) + " (" +
                    (site != nullptr ? where(site->loc) : std::string("?")) + ")";
        }
        if (in.result != sir::kNone) {
            temp(in.result) = {strings_.intern(text), 0};
            return;
        }
        output_.write(text + "\n");
    }

    /// What the waveform needs from the design and the run (WAVEFORM.md):
    /// the dump table, made for the kernel, and the options.
    kernel::DumpSetup dumpSetup() const {
        kernel::DumpSetup s;
        for (const sir::DumpScope& r : d_.dumpScopes) {
            kernel::DumpScopeKind k = kernel::DumpScopeKind::Module;
            switch (r.kind) {
                case sir::DumpScopeKind::Module: k = kernel::DumpScopeKind::Module; break;
                case sir::DumpScopeKind::Begin: k = kernel::DumpScopeKind::Begin; break;
                case sir::DumpScopeKind::Fork: k = kernel::DumpScopeKind::Fork; break;
                case sir::DumpScopeKind::Task: k = kernel::DumpScopeKind::Task; break;
                case sir::DumpScopeKind::Function: k = kernel::DumpScopeKind::Function; break;
            }
            s.scopes.push_back({r.name, r.path, k, r.parent == sir::kNone ? UINT32_MAX : r.parent});
        }
        for (const sir::DumpVar& v : d_.dumpVars) {
            kernel::DumpType t = kernel::DumpType::Reg;
            switch (v.type) {
                case sir::DumpType::Reg: t = kernel::DumpType::Reg; break;
                case sir::DumpType::Integer: t = kernel::DumpType::Integer; break;
                case sir::DumpType::Time: t = kernel::DumpType::Time; break;
                case sir::DumpType::Real: t = kernel::DumpType::Real; break;
                case sir::DumpType::Realtime: t = kernel::DumpType::Realtime; break;
                case sir::DumpType::Event: t = kernel::DumpType::Event; break;
                case sir::DumpType::Wire: t = kernel::DumpType::Wire; break;
                case sir::DumpType::Tri: t = kernel::DumpType::Tri; break;
                case sir::DumpType::Wand: t = kernel::DumpType::Wand; break;
                case sir::DumpType::Triand: t = kernel::DumpType::Triand; break;
                case sir::DumpType::Wor: t = kernel::DumpType::Wor; break;
                case sir::DumpType::Trior: t = kernel::DumpType::Trior; break;
                case sir::DumpType::Tri0: t = kernel::DumpType::Tri0; break;
                case sir::DumpType::Tri1: t = kernel::DumpType::Tri1; break;
                case sir::DumpType::Supply0: t = kernel::DumpType::Supply0; break;
                case sir::DumpType::Supply1: t = kernel::DumpType::Supply1; break;
            }
            s.vars.push_back({v.scope, v.name, v.range, t, v.width, kernelSlot(v.slot),
                              d_.slot(v.slot).type.kind == sir::TypeKind::ShortReal});
        }
        s.date = options_.date;
        s.version = options_.version;
        s.timescale = sir::timeText(1, d_.timePrecision);
        s.waveFile = options_.waveFile;
        s.scopePatterns = options_.waveScopes;
        s.glitches = options_.waveGlitches;
        return s;
    }

    /// The warnings of a $dumpfile or $dumpvars that is ignored
    /// (UNSPECIFIED_BEHAVIOR.md D3, WAVEFORM.md 4, 5).
    void dumpWarning(const sir::Instr& in, std::string_view task, kernel::DumpCall r) {
        if (r == kernel::DumpCall::Late)
            warnOnce(in, std::string(task) +
                             (task == "$dumpfile"
                                  ? ": dumping has started; the call is ignored"
                                  : ": dumping started at an earlier time; the call is ignored"));
        else if (r == kernel::DumpCall::CommandLine)
            warnOnce(in, std::string(task) + ": --wave sets what is dumped; the call is ignored");
    }

    /// The files d names, with a warning when some of it is not open
    /// (SYSTEM_TASKS.md 6.1).
    kernel::FileTargets fileTargets(const sir::Instr& in, W4 d, std::string_view task) {
        kernel::FileTargets to = files_.targets(d);
        if (to.notOpen)
            warnOnce(in, std::string(task) + ": " + notOpenText(d) + "; nothing is written there");
        return to;
    }

    /// $readmemb, $readmemh, $writememb and $writememh (file, memory[,
    /// start[, finish]]): the memory is a slot, an array frame variable,
    /// or a ref to either, described by its formal's shape and target type,
    /// and the kernel decides which of its elements get which words
    /// (SIR_SPEC.md 8, SYSTEM_TASKS.md 7). A file that cannot be opened,
    /// and one with text that is no word, are run-time errors of the
    /// design; an address out of the range and a count of words that does
    /// not match give warnings.
    bool memoryFile(const sir::Process& p, const sir::Instr& in, bool load) {
        const auto& ops = in.operands;
        const std::string_view task = ops[0].text;
        const bool hex = task.back() == 'h';
        const std::string name(strings_.get(value(ops[1]).a));
        const Operand& m = ops[2];
        const bool slot = m.kind == Operand::Kind::Slot;
        const sir::FrameVar* frame = slot ? nullptr : &bodyFrame(act_.body)[m.asFrame().index()];
        const bool ref = frame != nullptr && frame->type.kind == sir::TypeKind::Ref;
        const W4 reference = ref ? frameVar(m.asFrame()) : W4{};
        const std::vector<sir::ArrayDim>& adims = slot ? d_.slot(m.asSlot()).dims : frame->dims;
        const sir::Type et = slot ? d_.slot(m.asSlot()).type : ref ? frame->target : frame->type;
        std::vector<kernel::MemDim> dims;
        dims.reserve(adims.size());
        for (const sir::ArrayDim& d : adims)
            dims.push_back({d.left, d.right});
        // A memory of an enumerated type: its row, then the addresses
        // (SIR_SPEC.md 8).
        const bool isEnum = ops.size() > 3 && ops[3].kind == Operand::Kind::Type;
        const std::size_t first = isEnum ? 4 : 3;
        std::optional<std::int64_t> address[2];
        for (std::size_t k = 0; k < 2 && first + k < ops.size(); ++k) {
            const W4 a = value(ops[first + k]);
            if (a.b != 0) {
                designErrorIn(p, in, std::string(task) + ": an address argument has x or z bits");
                return true;
            }
            address[k] = static_cast<std::int64_t>(a.a);
        }
        const std::uint32_t c = chunksOf(et);
        if (!load) {
            // One word per line in hexadecimal or binary, every digit of
            // the width (SYSTEM_TASKS.md 7.2).
            std::string text;
            std::vector<W4> v(c);
            std::vector<std::uint64_t> words;
            kernel::FormatSpec spec;
            spec.conv = hex ? 'h' : 'b';
            for (const std::uint32_t e : kernel::dumpOrder(dims, address[0], address[1])) {
                if (slot)
                    readSlotWide(m.asSlot(), e, 0, 0, v.data());
                else if (ref && !isFrameRef(reference))
                    readSlotWide(refSlot(reference), refElem(reference) + e, 0, 0, v.data());
                else
                    for (std::uint32_t k = 0; k < c; ++k)
                        v[k] = ref ? refWord(reference, std::uint64_t{e} * c + k)
                                   : (&frameElem(m.asFrame(), e))[k];
                words = packed(v.data(), c, sir::isFourState(et));
                text +=
                    kernel::formatInt({words.data(), et.width, sir::isFourState(et), false}, spec);
                text += '\n';
            }
            std::FILE* f = std::fopen(name.c_str(), "w");
            if (f == nullptr) {
                designErrorIn(p, in,
                              std::string(task) + ": cannot open \"" + name + "\" for writing");
                return true;
            }
            std::fwrite(text.data(), 1, text.size(), f);
            std::fclose(f);
            return true;
        }
        const std::optional<std::string> text = kernel::readFile(name);
        if (!text) {
            designErrorIn(p, in, std::string(task) + ": cannot open \"" + name + "\"");
            return true;
        }
        std::vector<std::vector<W4>> items;
        if (isEnum)
            for (const sir::EnumItem& item : d_.types[ops[3].id].items) {
                const std::uint32_t o = constOff_[item.value.index()];
                items.emplace_back(consts_.begin() + o, consts_.begin() + o + c);
            }
        const kernel::MemLoad l = kernel::loadMemory(*text, hex, dims, et.width, address[0],
                                                     address[1], !sir::isFourState(et), items);
        for (const kernel::MemWord& w : l.words) {
            if (slot)
                writeSlotWide(m.asSlot(), w.elem, 0, 0, w.value.data());
            else if (ref && !isFrameRef(reference))
                writeSlotWide(refSlot(reference), refElem(reference) + w.elem, 0, 0,
                              w.value.data());
            else
                for (std::uint32_t k = 0; k < c; ++k)
                    (ref ? refWord(reference, std::uint64_t{w.elem} * c + k)
                         : (&frameElem(m.asFrame(), w.elem))[k]) = w.value[k];
        }
        if (!l.range.empty()) warnOnce(in, std::string(task) + ": " + l.range);
        if (!l.count.empty()) warnOnce(in, std::string(task) + ": " + l.count);
        if (!l.error.empty())
            designErrorIn(p, in, std::string(task) + ": \"" + name + "\" " + l.error);
        return true;
    }

    bool syscall(Frame& f, const sir::Process& p, const sir::Instr& in, const InstrInfo& info) {
        const auto& ops = in.operands;
        switch (info.task) {
            case SysTask::Fatal:
            case SysTask::Error:
            case SysTask::Warning:
            case SysTask::Info: {
                // <file>:<line>: <severity>: <message> [<scope> @ <time>]
                // among the messages (SYSTEM_TASKS.md 10.1); $fatal then ends the
                // run like $finish with its level (20.10).
                const std::optional<std::string> message =
                    formatOperands(p, in, info, formatStart(info.task));
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
                const sir::Type ct = typeOf(ops[1]);
                temp(in.result) = {
                    chunksOf(ct) > 1
                        ? kernel::wide::countBits(chunks(ops[1]).data(), ct.width, zeros, ones, zs,
                                                  xs)
                        : kernel::countBits4(value(ops[1]), ct.width, zeros, ones, zs, xs),
                    0};
                return true;
            }
            case SysTask::TimeFormat: {
                // The unit, digits, suffix and minimum width of every %t
                // after it; with no arguments, the defaults (SYSTEM_TASKS.md
                // 5.4).
                kernel::TimeFormat tf;
                tf.unit = d_.timePrecision;
                if (ops.size() == 5) {
                    const auto sint = [&](std::size_t i) {
                        const W4 w = value(ops[i]);
                        const unsigned bits = typeOf(ops[i]).width;
                        std::uint64_t a = w.a;
                        if (bits < 64 && ((a >> (bits - 1)) & 1U) != 0)
                            a |= ~std::uint64_t{0} << bits;
                        return static_cast<std::int64_t>(a);
                    };
                    tf.unit = static_cast<int>(sint(1));
                    tf.decimals = static_cast<int>(sint(2));
                    tf.suffix = std::string(strings_.get(value(ops[3]).a));
                    tf.minWidth = static_cast<int>(sint(4));
                    if (tf.unit < -15 || tf.unit > 2 || tf.decimals < 0 || tf.minWidth < 0) {
                        fail(p, "$timeformat with these arguments");
                        return false;
                    }
                }
                timeFormat_ = tf;
                return true;
            }
            case SysTask::MonitorOn:
                // The monitor shows its values once more, changed or not
                // (SYSTEM_TASKS.md 4): it starts again from a fresh frame.
                if (monitor_ != nullptr) freshVars(*monitor_);
                sched_.setMonitoring(true);
                return true;
            case SysTask::MonitorOff: sched_.setMonitoring(false); return true;
            case SysTask::DumpFile:
                // (call as written[, name]): dump.vcd when no name is given
                // (WAVEFORM.md 4).
                dumpWarning(in, "$dumpfile",
                            dump_.file(ops.size() > 2 ? std::string(strings_.get(value(ops[2]).a))
                                                      : std::string("dump.vcd"),
                                       ops[1].text));
                return true;
            case SysTask::DumpVars: {
                // ([levels[, "scope" | "var", row]...]): rows of the dump
                // table (SIR_SPEC.md 2), kNone for something not dumped.
                std::vector<std::uint32_t> scopes;
                std::vector<std::uint32_t> vars;
                for (std::size_t i = 2; i + 1 < ops.size(); i += 2) {
                    const auto row = static_cast<std::uint32_t>(value(ops[i + 1]).a);
                    if (row != sir::kNone) (ops[i].text == "scope" ? scopes : vars).push_back(row);
                }
                const auto levels =
                    ops.size() > 1 ? static_cast<std::uint32_t>(value(ops[1]).a) : 0U;
                dumpWarning(in, "$dumpvars",
                            dump_.vars(sched_.time(), levels, scopes, vars, ops.size() <= 2));
                return true;
            }
            case SysTask::DumpOff: dump_.off(); return true;
            case SysTask::DumpOn: dump_.on(); return true;
            case SysTask::DumpAll: dump_.all(); return true;
            case SysTask::DumpLimit: dump_.limit(value(ops[1]).a); return true;
            case SysTask::DumpFlush: dump_.flush(); return true;
            case SysTask::SFormatRuntime: {
                // front supplies scope, cell, tick scale, format, raw values.
                const std::optional<std::string> format = scanFormat(ops[4]);
                if (!format) {
                    designErrorIn(p, in,
                                  "string format: unsupported: format with X/Z bits "
                                  "(not implemented yet)");
                    return false;
                }
                std::vector<std::vector<std::uint64_t>> storage(ops.size() - 5);
                std::vector<kernel::FormatValue> values;
                values.reserve(storage.size());
                for (std::size_t i = 5; i < ops.size(); ++i) {
                    kernel::FormatValue arg;
                    if (ops[i].kind == Operand::Kind::Text) {
                        arg.kind = kernel::FormatValue::Kind::Empty;
                    } else {
                        const sir::Type type = typeOf(ops[i]);
                        if (type.kind == sir::TypeKind::String) {
                            arg.kind = kernel::FormatValue::Kind::String;
                            arg.text = strings_.get(value(ops[i]).a);
                        } else if (isRealType(type)) {
                            arg.kind = kernel::FormatValue::Kind::Real;
                            arg.real = toReal(value(ops[i]), type);
                        } else {
                            storage[i - 5] = formatWords(ops[i]);
                            arg.integral = {storage[i - 5].data(), type.width,
                                            sir::isFourState(type), sir::isSigned(type)};
                        }
                    }
                    values.push_back(arg);
                }
                const kernel::StringFormatResult result =
                    kernel::formatStringCall(*format, values, ops[1].text, ops[2].text,
                                             value(ops[3]).a, d_.timePrecision, timeFormat_);
                if (!result.error.empty()) {
                    designErrorIn(p, in,
                                  "string format: unsupported: " + result.error +
                                      " (not implemented yet)");
                    return false;
                }
                if (result.argumentMismatch)
                    warnOnce(in,
                             "string format has a different number of arguments than conversions");
                formattedResult(in, result.text);
                return true;
            }
            case SysTask::Display:
            case SysTask::Write:
            case SysTask::SFormatF: {
                const std::optional<std::string> formatted =
                    formatOperands(p, in, info, formatStart(info.task));
                if (!formatted) return false;
                std::string text = *formatted;
                if (info.task == SysTask::SFormatF) {
                    formattedResult(in, std::move(text));
                    return true;
                }
                if (info.task == SysTask::Display) text += '\n';
                // The print of $fstrobe or $fmonitor (SYSTEM_TASKS.md 6.2).
                if (const kernel::FileTargets* to = sched_.printingTo())
                    files_.write(*to, text);
                else
                    output_.write(text);
                return true;
            }
            case SysTask::FDisplay:
            case SysTask::FWrite: {
                const std::optional<std::string> formatted =
                    formatOperands(p, in, info, formatStart(info.task));
                if (!formatted) return false;
                std::string text = *formatted;
                if (info.task == SysTask::FDisplay) text += '\n';
                files_.write(fileTargets(in, value(ops[1]), ops[0].text), text);
                return true;
            }
            case SysTask::FOpen: {
                // An mcd with one argument, an fd with a type (SYSTEM_TASKS.md 6.1).
                const std::string name(strings_.get(value(ops[1]).a));
                std::uint32_t d = 0;
                if (ops.size() > 2) {
                    const std::string_view type = strings_.get(value(ops[2]).a);
                    bool badType = false;
                    d = files_.openFd(name, type, badType);
                    if (badType)
                        warnOnce(in, "$fopen: \"" + std::string(type) +
                                         "\" is not a type of Table 21-6; nothing is opened");
                } else {
                    d = files_.openMcd(name);
                }
                temp(in.result) = {d, 0};
                return true;
            }
            case SysTask::FGets: {
                // The line goes to the frame variable, which front assigns
                // to the target when the count is above 0 (SIR_SPEC.md 8). An
                // integral target holds width / 8 characters, placed as a
                // string literal is (SYSTEM_TASKS.md 6.3).
                const sir::Type t = typeOf(ops[1]);
                const bool text = t.kind == sir::TypeKind::String;
                std::string line;
                bool notOpen = false;
                if (text || t.width >= 8)
                    files_.readLine(value(ops[2]), text ? 0 : t.width / 8, line, notOpen);
                if (notOpen) warnOnce(in, "$fgets: " + notReadableText(value(ops[2])));
                if (!line.empty()) {
                    if (text)
                        frameVar(ops[1].asFrame()) = {strings_.intern(line), 0};
                    else
                        kernel::wide::fromText(&frameVar(ops[1].asFrame()), t.width, line);
                }
                temp(in.result) = {line.size(), 0};
                return true;
            }
            case SysTask::FGetc: {
                bool notOpen = false;
                const int c = files_.getChar(value(ops[1]), notOpen);
                if (notOpen) warnOnce(in, "$fgetc: " + notReadableText(value(ops[1])));
                temp(in.result) = {static_cast<std::uint32_t>(c), 0};
                return true;
            }
            case SysTask::Ungetc: {
                bool notOpen = false;
                const int r = files_.ungetChar(static_cast<int>(value(ops[1]).a & 0xffU),
                                               value(ops[2]), notOpen);
                if (notOpen) warnOnce(in, "$ungetc: " + notReadableText(value(ops[2])));
                temp(in.result) = {static_cast<std::uint32_t>(r), 0};
                return true;
            }
            case SysTask::FScanf: {
                // $fscanf(fd, format, frame variables...): as $sscanf, from
                // the file (SYSTEM_TASKS.md 6.3).
                const std::optional<std::string> format = scanFormat(ops[2]);
                if (!format) {
                    temp(in.result) = {0xffffffffU, 0};
                    return true;
                }
                const kernel::ScanPlan plan = scanPlan(in, *format);
                if (!plan.error.empty()) {
                    designErrorIn(p, in,
                                  "$fscanf: unsupported: " + plan.error + " (not implemented yet)");
                    return false;
                }
                if (plan.missingTargets)
                    warnOnce(in, "$fscanf has fewer arguments than its format has conversions");
                bool notOpen = false;
                scanValues_.clear();
                const int n =
                    files_.scan(value(ops[1]), plan.format, scanValues_, notOpen, scanWidths(in));
                if (notOpen) warnOnce(in, "$fscanf: " + notReadableText(value(ops[1])));
                for (std::size_t i = 0; i < scanValues_.size() && 3 + i < ops.size(); ++i)
                    scanned(scanValues_[i], typeOf(ops[3 + i]), &frameVar(ops[3 + i].asFrame()));
                temp(in.result) = {static_cast<std::uint32_t>(n), 0};
                return true;
            }
            case SysTask::FRead: {
                // $fread(frame variable, fd): the frame variable holds the
                // target's value; the bytes read replace its bits from the
                // most significant byte of ceil(width / 8) on, as 2-state
                // values, and the rest keep theirs (SYSTEM_TASKS.md 6.3).
                const sir::Type t = typeOf(ops[1]);
                const unsigned nbytes = (t.width + 7) / 8;
                std::string bytes;
                bool notOpen = false;
                files_.readBytes(value(ops[2]), nbytes, bytes, notOpen);
                if (notOpen) warnOnce(in, "$fread: " + notReadableText(value(ops[2])));
                W4* v = &frameVar(ops[1].asFrame());
                for (std::size_t k = 0; k < bytes.size(); ++k) {
                    const W4 byte{static_cast<unsigned char>(bytes[k]), 0};
                    const auto low = static_cast<unsigned>((nbytes - 1 - k) * 8);
                    if (low < t.width)
                        kernel::wide::put(v, low, &byte, std::min(8U, t.width - low));
                }
                temp(in.result) = {bytes.size(), 0};
                return true;
            }
            case SysTask::FTell: {
                bool notOpen = false;
                const std::int64_t p = files_.tell(value(ops[1]), notOpen);
                if (notOpen) warnOnce(in, "$ftell: " + notReadableText(value(ops[1])));
                temp(in.result) = {static_cast<std::uint32_t>(p), 0};
                return true;
            }
            case SysTask::FSeek:
            case SysTask::Rewind: {
                // $rewind is $fseek(fd, 0, 0) (21.3.5); an offset or an
                // operation with x or z bits is an error.
                bool notOpen = false;
                std::int64_t offset = 0;
                std::int64_t from = 0;
                bool known = true;
                if (info.task == SysTask::FSeek) {
                    const W4 o = value(ops[2]);
                    const W4 f = value(ops[3]);
                    known = o.b == 0 && f.b == 0;
                    offset = static_cast<std::int32_t>(o.a);
                    from = static_cast<std::int32_t>(f.a);
                }
                const int r = files_.seek(value(ops[1]), offset, known ? from : -1, notOpen);
                if (notOpen) warnOnce(in, ops[0].text + ": " + notReadableText(value(ops[1])));
                temp(in.result) = {static_cast<std::uint32_t>(r), 0};
                return true;
            }
            case SysTask::FEof: {
                bool notOpen = false;
                const int e = files_.atEnd(value(ops[1]), notOpen);
                if (notOpen) warnOnce(in, "$feof: " + notReadableText(value(ops[1])));
                temp(in.result) = {static_cast<std::uint32_t>(e), 0};
                return true;
            }
            case SysTask::FError: {
                // $ferror(fd, frame variable): the text goes to the frame
                // variable, which front assigns to the target; no warning,
                // as it is how a design asks (SYSTEM_TASKS.md 6.4).
                std::string text;
                const int e = files_.error(value(ops[1]), text);
                const sir::Type t = typeOf(ops[2]);
                if (t.kind == sir::TypeKind::String)
                    frameVar(ops[2].asFrame()) = {strings_.intern(text), 0};
                else
                    kernel::wide::fromText(&frameVar(ops[2].asFrame()), t.width, text);
                temp(in.result) = {static_cast<std::uint32_t>(e), 0};
                return true;
            }
            case SysTask::ReadMem:
            case SysTask::WriteMem: return memoryFile(p, in, info.task == SysTask::ReadMem);
            case SysTask::Random:
            case SysTask::Dist: {
                // (frame variable of the seed, arguments...): the seed is
                // the frame variable's low 32 bits, which the function
                // updates; $random alone uses the Context's seed (SYSTEM_TASKS.md 12).
                const bool own = ops.size() > 1;
                std::int32_t local =
                    own ? static_cast<std::int32_t>(frameVar(ops[1].asFrame()).a) : randomSeed_;
                const auto arg = [&](std::size_t i) {
                    return static_cast<std::int32_t>(value(ops[i]).a);
                };
                const std::string_view name = ops[0].text;
                kernel::DistResult r;
                if (info.task == SysTask::Random)
                    r.value = kernel::randomInt(local);
                else if (name == "$dist_uniform")
                    r = kernel::distUniform(local, arg(2), arg(3));
                else if (name == "$dist_normal")
                    r = kernel::distNormal(local, arg(2), arg(3));
                else if (name == "$dist_exponential")
                    r = kernel::distExponential(local, arg(2));
                else if (name == "$dist_poisson")
                    r = kernel::distPoisson(local, arg(2));
                else if (name == "$dist_chi_square")
                    r = kernel::distChiSquare(local, arg(2));
                else if (name == "$dist_t")
                    r = kernel::distT(local, arg(2));
                else
                    r = kernel::distErlang(local, arg(2), arg(3));
                if (r.badArgument)
                    warnOnce(in, std::string(name) + ": " +
                                     (name == "$dist_erlang" ? "k_stage"
                                      : name == "$dist_chi_square" || name == "$dist_t"
                                          ? "the degree of freedom"
                                          : "the mean") +
                                     " is 0 or less; the result is 0");
                if (own)
                    frameVar(ops[1].asFrame()) = {static_cast<std::uint32_t>(local), 0};
                else
                    randomSeed_ = local;
                temp(in.result) = {static_cast<std::uint32_t>(r.value), 0};
                return true;
            }
            case SysTask::FClose:
                if (!files_.close(value(ops[1])))
                    warnOnce(in, "$fclose: " + notOpenText(value(ops[1])));
                return true;
            case SysTask::FFlush:
                if (ops.size() < 2)
                    files_.flushAll();
                else if (!files_.flush(value(ops[1])))
                    warnOnce(in, "$fflush: " + notOpenText(value(ops[1])));
                return true;
            case SysTask::Exit: sched_.exitProgram(f.kernelId); return true;
            case SysTask::Finish:
            case SysTask::Stop: {
                // 1 and 2 report the time and the place of the call, at the
                // call; 2 also the statistics, which come at the end of the
                // run (SYSTEM_TASKS.md 10.2).
                const auto level = static_cast<int>(value(ops[1]).a);
                if (level >= 1) {
                    std::string line =
                        in.loc.file < d_.sources.size() ? d_.sources[in.loc.file] : "?";
                    line += ":" + std::to_string(in.loc.line) + ": ";
                    line += info.task == SysTask::Finish ? "$finish" : "$stop";
                    line += " at " + sir::timeText(sched_.time(), d_.timePrecision);
                    output_.message(line);
                }
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
                const std::optional<std::string> format = scanFormat(ops[2]);
                if (!format) {
                    temp(in.result) = {0xffffffffU, 0};
                    return true;
                }
                const kernel::ScanPlan plan = scanPlan(in, *format);
                if (!plan.error.empty()) {
                    designErrorIn(p, in,
                                  "$sscanf: unsupported: " + plan.error + " (not implemented yet)");
                    return false;
                }
                if (plan.missingTargets)
                    warnOnce(in, "$sscanf has fewer arguments than its format has conversions");
                std::string input;
                const sir::Type it = typeOf(ops[1]);
                const std::vector<W4> iv = chunks(ops[1]);
                int n = -1;
                if (it.kind == sir::TypeKind::String) {
                    input = strings_.get(iv[0].a);
                } else if (!kernel::wide::hasUnknown(iv.data(), it.width)) {
                    // The bytes from the most significant one, NULs kept.
                    for (unsigned k = (it.width + 7) / 8; k-- > 0;)
                        input.push_back(static_cast<char>((iv[k / 8].a >> (k % 8 * 8)) & 0xffU));
                } else {
                    temp(in.result) = {0xffffffffU, 0};
                    return true;
                }
                scanValues_.clear();
                n = kernel::scanString(input, plan.format, scanValues_, scanWidths(in));
                for (std::size_t i = 0; i < scanValues_.size() && 3 + i < ops.size(); ++i)
                    scanned(scanValues_[i], typeOf(ops[3 + i]), &frameVar(ops[3 + i].asFrame()));
                temp(in.result) = {static_cast<std::uint32_t>(n), 0};
                return true;
            }
            case SysTask::ValuePlusargs: {
                // The first plusarg that starts with the name before the %:
                // the rest by the one conversion (SYSTEM_TASKS.md 9).
                const std::optional<std::string> fmt = scanFormat(ops[1]);
                if (!fmt) {
                    designErrorIn(p, in,
                                  "$value$plusargs: unsupported: format with X/Z bits "
                                  "(not implemented yet)");
                    return false;
                }
                const kernel::PlusargPlan plan =
                    kernel::preparePlusargFormat(*fmt, scanTarget(typeOf(ops[2])));
                if (!plan.error.empty()) {
                    designErrorIn(p, in,
                                  "$value$plusargs: unsupported: " + plan.error +
                                      " (not implemented yet)");
                    return false;
                }
                temp(in.result) = {0, 0};
                for (const std::string& a : options_.plusargs) {
                    if (!std::string_view(a).starts_with(plan.prefix)) continue;
                    const kernel::ScanValue v =
                        kernel::plusargValue(a.substr(plan.prefix.size()), plan.conv);
                    scanned(v, typeOf(ops[2]), &frameVar(ops[2].asFrame()));
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
                        // Truncated, then as a conversion (SYSTEM_TASKS.md 8).
                        kernel::wide::fromReal(&r, rt.width, toReal(arg(1), argType(1)),
                                               sir::isFourState(rt), true);
                        break;
                    case SysTask::RealToBits:
                    case SysTask::ShortRealToBits:
                    case SysTask::BitsToReal:
                    case SysTask::BitsToShortReal: r = {arg(1).a & ~arg(1).b, 0}; break;
                    case SysTask::Math1: r = fromReal(info.fn1(realOf(ops[1])), rt); break;
                    case SysTask::Math2:
                        r = fromReal(info.fn2(realOf(ops[1]), realOf(ops[2])), rt);
                        break;
                    case SysTask::Clog2: {
                        // ceil(log2(n)), and 0 for 0 and 1 (20.8.1); x for an
                        // unknown argument.
                        const std::vector<W4> n = chunks(ops[1]);
                        if (kernel::wide::hasUnknown(n.data(), argType(1).width)) {
                            r = kernel::allX(rt.width);
                        } else {
                            // The highest 1 and whether any 1 is below it.
                            std::uint64_t top = 0;
                            bool found = false;
                            bool below = false;
                            for (std::size_t c = n.size(); c-- > 0;) {
                                if (n[c].a == 0) continue;
                                if (!found) {
                                    top = 64 * c + 63 -
                                          static_cast<std::uint64_t>(std::countl_zero(n[c].a));
                                    found = true;
                                    below = (n[c].a & (n[c].a - 1)) != 0;
                                } else {
                                    below = true;
                                }
                            }
                            r = {!found ? 0 : top + (below ? 1 : 0), 0};
                        }
                        break;
                    }
                    default: break;
                }
                if (sir::isIntegral(rt) && !sir::isFourState(rt)) r = kernel::to2(r);
                temp(in.result) = r;
                return true;
            }
            case SysTask::Stacktrace:  // runBody, which has the frame
            case SysTask::Urandom:
            case SysTask::UrandomRange:
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
