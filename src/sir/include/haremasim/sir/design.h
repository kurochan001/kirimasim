// The SIR of one design: slot table, name table, instances, type table,
// constants, subroutines and processes (SIR_SPEC.md sections 2 to 8).
//
// The structures are plain data. The front end builds them; the verifier
// (verify.h) checks them; the text writer (text.h) prints them. Results of
// analysis are not part of the SIR (SIR_SPEC.md 1).

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "haremasim/sir/types.h"

namespace haremasim::sir {

using SlotId = std::uint32_t;
using InstanceId = std::uint32_t;
using ConstId = std::uint32_t;
using TempId = std::uint32_t;
using BlockId = std::uint32_t;
using TypeId = std::uint32_t;
using FrameVarId = std::uint32_t;
using SubroutineId = std::uint32_t;
using ProcessId = std::uint32_t;

inline constexpr std::uint32_t kNone = 0xffffffffU;

/// Position in the source. Line and column start at 1; 0 means "missing".
struct SourceLoc {
    std::uint32_t file = 0;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

/// Storage domain of a slot (SIR_SPEC.md 3.2). It follows from the declared
/// type alone; see domainOf().
enum class Domain : std::uint8_t { Two, Four, Real, Handle };

/// What a slot holds: a variable or a net of a given kind (SIR_SPEC.md 3.1).
enum class SlotKind : std::uint8_t {
    Var,
    Wire,
    Tri,
    Wand,
    Triand,
    Wor,
    Trior,
    Tri0,
    Tri1,
    Supply0,
    Supply1,
    Uwire,
};

/// Slot flags (SIR_SPEC.md 3.1).
enum SlotFlag : std::uint32_t {
    kObservable = 1U << 0,
    kForceTarget = 1U << 1,
    kMultiDriver = 1U << 2,
    kNeedsStrength = 1U << 3,
};

/// One dimension of a fixed-size unpacked array, as declared ([left:right]).
struct ArrayDim {
    std::int64_t left = 0;
    std::int64_t right = 0;
};

/// A row of the slot table. A fixed-size unpacked array is one slot whose
/// `type` is the element's value type; its elements lie one after another
/// inside the slot, the rightmost dimension changing fastest (SIR_SPEC.md
/// 4.2).
struct Slot {
    std::string name;  ///< Hierarchical name; for diagnostics and the text only.
    Type type;         ///< Value type (of one element, for an array).
    SlotKind kind = SlotKind::Var;
    std::vector<ArrayDim> dims;
    std::uint32_t flags = 0;
    InstanceId instance = 0;
    SubroutineId subroutine = kNone;  ///< Owning static subroutine, if any.
    TypeId declType = kNone;          ///< Type table row, when the value type is not enough.
    std::uint32_t offset = 0;         ///< Word offset inside the domain (assigned later).
};

/// A row of the name table: a hierarchical name and the slot it denotes.
/// Folded ports keep both names as aliases of one slot (SIR_SPEC.md 3.5).
struct NameEntry {
    std::string path;
    SlotId slot = 0;
};

/// A row of the instance table.
struct Instance {
    std::string scopeName;   ///< Hierarchical name of the instance.
    std::string definition;  ///< Module name (with parameter values, if any).
    InstanceId parent = kNone;
};

/// Kinds of type-table rows (SIR_SPEC.md 4.3).
enum class TypeEntryKind : std::uint8_t { Enum, PackedStruct, PackedUnion, UnpackedStruct, Array };

/// An item of an enum: its name and value (a constant of the base type).
struct EnumItem {
    std::string name;
    ConstId value = 0;
};

/// A field of a packed struct or union, or a member of an unpacked struct.
/// Packed fields have a bit position and width; unpacked members do not.
struct Field {
    std::string name;
    std::uint32_t lsb = 0;
    std::uint32_t width = 0;
    TypeId type = kNone;  ///< Type table row of the field, if it needs one.
};

/// A row of the type table (SIR_SPEC.md 4.3).
struct TypeEntry {
    TypeEntryKind kind = TypeEntryKind::Enum;
    std::string name;
    Type value;                   ///< Enum: base type. Packed: the whole vector.
    std::vector<EnumItem> items;  ///< Enum items in declaration order.
    std::vector<Field> fields;    ///< Packed fields or unpacked members.
    TypeId element = kNone;       ///< Array: the element's row, if it needs one.
    std::vector<ArrayDim> dims;   ///< Array: the dimensions.
};

/// A constant: an integral value of any width (four-state if the type is),
/// a real, or a string. For integral constants, words hold 64 bits each,
/// least significant first, and bWords holds the b half of the (a, b)
/// encoding (kernel/w4.h) of a four-state constant, or is empty. Bits above
/// the width are 0. A real is the IEEE 754 bit pattern in aWords[0] (32 bits
/// for shortreal). A string constant keeps its bytes in `bytes`.
struct Constant {
    Type type;
    std::vector<std::uint64_t> aWords;
    std::vector<std::uint64_t> bWords;
    std::string bytes;
};

/// Instruction opcodes (SIR_SPEC.md 5.2, 5.3 and 7.2).
enum class Op : std::uint8_t {
    // Reads and writes of slots (SIR_SPEC.md 5.3) and frame variables (7.4).
    Load,       ///< %r = load <slot range>
    Store,      ///< store <slot range>, <value>  (blocking assignment)
    Nba,        ///< nba <slot range>, <value>
    NbaDelay,   ///< nba.delay <slot range>, <value>, <delay>
    LoadElem,   ///< %r = load.elem <slot>, <index>...
    StoreElem,  ///< store.elem <slot>, <index>..., <value>
    NbaElem,    ///< nba.elem <slot>, <index>..., <value>
    StoreBits,  ///< store.bits <slot>, [<index>...,] <position>, <value>
    NbaBits,    ///< nba.bits <slot>, [<index>...,] <position>, <value>
    Fload,      ///< %r = fload %fN
    Fstore,     ///< fstore %fN, <value>
    // Arithmetic (integral, real and shortreal).
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Pow,
    Neg,
    // Relational (signedness from the operand type; also real and string).
    Lt,
    Le,
    Gt,
    Ge,
    // Equality.
    Eq,
    Ne,
    Ceq,  ///< ===
    Cne,  ///< !==
    Weq,  ///< ==?
    Wne,  ///< !=?
    /// %r = match.mask <value>, <value>, "casez" | "casex": the case-item
    /// match of casez and casex, which ignores z (casex: x and z) on both
    /// sides (VALUE_SEMANTICS.md 4.2). Always 0 or 1.
    MatchMask,
    // Logical. There are no short-circuit instructions: &&, ||, -> and ?:
    // with operands that have side effects are lowered to branches.
    Lnot,
    Land,
    Lor,
    Limp,   ///< -> once both sides are evaluated
    Leqv,   ///< <->
    Truth,  ///< Conversion of a value to a 1-bit truth value (1, 0 or x).
    // Bitwise.
    And,
    Or,
    Xor,
    Xnor,
    Not,
    // Reduction.
    Rand,
    Rnand,
    Ror,
    Rnor,
    Rxor,
    Rxnor,
    // Shift.
    Shl,
    Shr,
    Ashr,
    // Conditional operator (both sides already evaluated).
    Select,
    // Selects (VALUE_SEMANTICS.md 3.11).
    Bitsel,      ///< %r = bitsel <value>, <index>
    Partsel,     ///< %r = partsel <value>, <constant lsb>
    IdxPartsel,  ///< %r = idxpartsel <value>, <position>
    // Concatenation (VALUE_SEMANTICS.md 7.1 to 7.3).
    Concat,  ///< %r = concat <value>...  (most significant first)
    Repl,    ///< %r = repl <count>, <value>
    // Conversions.
    Zext,
    Sext,
    Trunc,
    To2,
    To4,
    Itor,    ///< integral -> real or shortreal
    Rtoi,    ///< real or shortreal -> integral
    Rconv,   ///< real <-> shortreal
    Itos,    ///< integral -> string
    Stoi,    ///< string -> integral
    StrGet,  ///< %r = str.get <string>, <index>
    // Events and process control (SCHEDULER_SPEC.md 9.4 and 10).
    Trigger,      ///< trigger <event slot>
    TriggerNb,    ///< trigger.nb <event slot>[, <delay>]
    Disable,      ///< disable <process or subroutine>
    DisableFork,  ///< disable.fork
    /// report.defer "<construct>", "<content>": puts a unique, unique0 or
    /// priority violation on the running process's queue of pending
    /// reports (SCHEDULER_SPEC.md 15; the texts are SYSTEM_TASKS.md 10.4).
    ReportDefer,
    // Calls.
    Call,     ///< [%r =] call @N, <argument>...
    Syscall,  ///< [%r =] syscall "$name", <operand>...
    Builtin,  ///< [%r =] builtin "name", <operand>...
    // Terminators. The continuation block of a wait comes last.
    Br,         ///< br bbN(<argument>...)
    BrIf,       ///< br.if %c, bbT(...), bbF(...)  (x and z count as false)
    Ret,        ///< ret [<value>]
    WaitDelay,  ///< wait.delay <ticks>, bbN
    WaitEvent,  ///< wait.event <event> [iff @G]..., bbN
    WaitCond,   ///< wait.cond @G, bbN
    WaitNamed,  ///< wait.named <event slot>, bbN
    Fork,       ///< fork @P..., "join" | "join_any" | "join_none", bbN
    WaitFork,   ///< wait.fork bbN
    CallTask,   ///< call.task @N, <argument>..., bbN
};

/// Edge of an event-control item.
enum class Edge : std::uint8_t { Any, Pos, Neg, Both };

/// An instruction operand.
///
/// A Block operand is followed by its `count` arguments. A Slot operand
/// covers bits [lsb, lsb + width) (width 0: all bits) of elements
/// [firstElem, firstElem + numElems) (numElems 0: all elements). In
/// wait.event a Slot operand may be followed by a Subroutine operand, the
/// item's iff guard.
struct Operand {
    enum class Kind : std::uint8_t { Temp, Const, Slot, Block, Text, Frame, Subroutine, Process };
    Kind kind = Kind::Temp;
    std::uint32_t id = 0;  ///< TempId, ConstId, SlotId, BlockId, FrameVarId, ...
    std::uint32_t lsb = 0;
    std::uint32_t width = 0;
    std::uint32_t firstElem = 0;
    std::uint32_t numElems = 0;
    std::uint32_t count = 0;  ///< Block: number of arguments that follow.
    Edge edge = Edge::Any;    ///< Slot in wait.event: the edge to wait for.
    std::string text;         ///< Text: system task or method name, or a word.

    static Operand of(Kind k, std::uint32_t id) {
        Operand o;
        o.kind = k;
        o.id = id;
        return o;
    }
    static Operand temp(TempId t) { return of(Kind::Temp, t); }
    static Operand constant(ConstId c) { return of(Kind::Const, c); }
    static Operand slot(SlotId s, std::uint32_t lsb = 0, std::uint32_t width = 0) {
        Operand o = of(Kind::Slot, s);
        o.lsb = lsb;
        o.width = width;
        return o;
    }
    static Operand event(SlotId s, Edge e) {
        Operand o = of(Kind::Slot, s);
        o.edge = e;
        return o;
    }
    static Operand block(BlockId b, std::uint32_t args = 0) {
        Operand o = of(Kind::Block, b);
        o.count = args;
        return o;
    }
    static Operand frame(FrameVarId f) { return of(Kind::Frame, f); }
    static Operand subroutine(SubroutineId s) { return of(Kind::Subroutine, s); }
    static Operand process(ProcessId p) { return of(Kind::Process, p); }
    static Operand str(std::string s) {
        Operand o = of(Kind::Text, 0);
        o.text = std::move(s);
        return o;
    }
};

/// One instruction. `result` is kNone for instructions without a value.
struct Instr {
    Op op = Op::Ret;
    Type type;  ///< Result type (void when there is no result).
    TempId result = kNone;
    std::vector<Operand> operands;
    SourceLoc loc;
};

/// A typed parameter of a basic block (SIR_SPEC.md 5.1).
struct BlockParam {
    TempId temp = 0;
    Type type;
};

/// A basic block: instructions ending with exactly one terminator. Values
/// that meet at the block arrive as its parameters.
struct BasicBlock {
    std::vector<BlockParam> params;
    std::vector<Instr> instrs;
};

/// A row of a frame-variable table (SIR_SPEC.md 7.4).
struct FrameVar {
    std::string name;  ///< Declared name, or empty for a value the front end keeps.
    Type type;
    TypeId declType = kNone;
};

/// Process kinds (SIR_SPEC.md 7.1).
enum class ProcessKind : std::uint8_t {
    Cont,
    Comb,
    Latch,
    CombAt,
    Ff,
    FfPlain,
    Initial,
    Final,
    General,
    StaticInit,  ///< The static variable initializers of an instance (SIR_SPEC.md 7.1).
};

/// A part of a slot: elements [firstElem, firstElem + numElems) (numElems
/// 0: all) and bits [lsb, lsb + width) of each (width 0: all bits).
struct SlotRange {
    SlotId slot = 0;
    std::uint32_t firstElem = 0;
    std::uint32_t numElems = 0;
    std::uint32_t lsb = 0;
    std::uint32_t width = 0;
};

/// One item of a process's sensitivity: a slot range and the edge.
struct Trigger {
    SlotRange range;
    Edge edge = Edge::Any;
};

/// Which process or subroutine something belongs to.
struct Owner {
    enum class Kind : std::uint8_t { None, Process, Subroutine };
    Kind kind = Kind::None;
    std::uint32_t id = 0;
};

/// A process. Block 0 is the entry block.
struct Process {
    std::string name;
    ProcessKind kind = ProcessKind::Initial;
    InstanceId instance = 0;
    std::vector<Trigger> triggers;
    std::vector<SlotRange> writes;  ///< Everything the process may write (SIR_SPEC.md 3.4).
    std::vector<FrameVar> frame;
    std::vector<BasicBlock> blocks;
    std::uint32_t numTemps = 0;
    Owner parent;  ///< The process or task that forks this one, if any.
    SourceLoc loc;
};

/// Kinds of subroutines (SIR_SPEC.md 8). A guard is the iff condition of a
/// wait.event item or the expression of a wait.cond; it reads the frame of
/// the process or subroutine that waits (its `frameOf`).
enum class SubroutineKind : std::uint8_t { Function, Task, Guard };

/// Argument directions.
enum class Direction : std::uint8_t { Input, Output, Inout, Ref, ConstRef };

/// A formal argument: static ones live in a slot, automatic ones in a frame
/// variable.
struct Param {
    std::string name;
    Direction dir = Direction::Input;
    Type type;
    SlotId slot = kNone;
    FrameVarId frameVar = kNone;
};

/// A row of the subroutine table. Block 0 is the entry block.
struct Subroutine {
    std::string name;
    SubroutineKind kind = SubroutineKind::Function;
    bool automatic = false;
    bool mayWait = false;  ///< Only tasks; called with call.task when set.
    InstanceId instance = 0;
    std::vector<Param> params;
    Type returnType;  ///< void for tasks and void functions.
    std::vector<FrameVar> frame;
    std::vector<BasicBlock> blocks;
    std::uint32_t numTemps = 0;
    Owner frameOf;  ///< Guard: whose frame it reads.
    SourceLoc loc;
};

/// The SIR of one design.
struct Design {
    /// The global time precision, as a power of ten of a second (-9 is
    /// 1 ns): the smallest precision of all design elements. Delays and
    /// the simulation time count steps of it (SCHEDULER_SPEC.md 1).
    int timePrecision = -9;
    std::vector<std::string> sources;  ///< File number -> path.
    std::vector<Instance> instances;
    std::vector<Slot> slots;
    std::vector<NameEntry> names;
    std::vector<TypeEntry> types;
    std::vector<Constant> constants;
    std::vector<Subroutine> subroutines;
    std::vector<Process> processes;
};

/// The storage domain of a value type (SIR_SPEC.md 3.2).
Domain domainOf(Type t);
/// Number of elements of a slot (1 for a slot that is not an array).
std::uint64_t elementCount(const Slot& s);

/// Text name of an opcode in the SIR text format (load, br.if, ...).
const char* opName(Op op);
/// Whether the instruction ends a basic block.
bool isTerminator(Op op);
/// Whether the terminator can suspend the process (SIR_SPEC.md 7.2). A
/// fork is a wait unless it is join_none.
bool isWait(const Instr& in);
/// Text name of a process kind (cont, comb, ff-plain, ...).
const char* kindName(ProcessKind k);
/// Text name of a slot kind (var, wire, ...).
const char* kindName(SlotKind k);
/// Text name of a subroutine kind (function, task, guard).
const char* kindName(SubroutineKind k);
/// Text name of a type-table kind (enum, struct.packed, ...).
const char* kindName(TypeEntryKind k);
/// Text name of an argument direction (input, output, ...).
const char* directionName(Direction d);
/// Text name of an edge (any, posedge, negedge, edge).
const char* edgeName(Edge e);

}  // namespace haremasim::sir
