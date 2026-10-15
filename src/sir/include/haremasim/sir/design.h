// The SIR of one design: slot table, name table, instances, constants and
// processes (SIR_SPEC.md sections 2, 3, 5, 6 and 7).
//
// The structures are plain data. The front end builds them; the verifier
// (verify.h) checks them; the text writer (text.h) prints them.

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

inline constexpr std::uint32_t kNone = 0xffffffffU;

/// Position in the source. Line and column start at 1; 0 means "missing".
struct SourceLoc {
    std::uint32_t file = 0;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

/// Storage domain of a slot (SIR_SPEC.md 3.2).
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
    kProvenNoX = 1U << 4,
};

/// One dimension of a fixed-size unpacked array, as declared ([left:right]).
struct ArrayDim {
    std::int64_t left = 0;
    std::int64_t right = 0;
};

/// A row of the slot table.
struct Slot {
    std::string name;  ///< Hierarchical name used by the text format; unique.
    Type type;
    Domain domain = Domain::Four;
    std::uint32_t offset = 0;  ///< Word offset inside the domain (assigned later).
    SlotKind kind = SlotKind::Var;
    std::vector<ArrayDim> dims;
    std::uint32_t flags = 0;
    InstanceId instance = 0;
    std::uint32_t subroutine = kNone;  ///< Owning static subroutine, if any.
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

/// A constant: an integral value of any width, four-state if the type is.
/// Words hold 64 bits each, least significant first. For a four-state
/// constant, bWords holds the b half of the (a, b) encoding (kernel/w4.h);
/// for a two-state constant it is empty. Bits above the width are 0.
struct Constant {
    Type type;
    std::vector<std::uint64_t> aWords;
    std::vector<std::uint64_t> bWords;
};

/// Instruction opcodes (SIR_SPEC.md 5.2 and 7.2).
enum class Op : std::uint8_t {
    // Reads and writes.
    Load,   ///< %r = load <slot range>
    Store,  ///< store <slot range>, <value>  (blocking assignment)
    Nba,    ///< nba <slot range>, <value>
    // Arithmetic.
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Pow,
    Neg,
    // Relational (signedness from the operand type).
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
    // Logical. There are no short-circuit instructions: &&, ||, -> and ?:
    // with operands that have side effects are lowered to branches.
    Lnot,
    Land,
    Lor,
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
    // Conversions.
    Zext,
    Sext,
    Trunc,
    To2,
    To4,
    // Calls.
    Syscall,  ///< syscall "$name", operands...
    // Terminators.
    Br,         ///< br bbN
    BrIf,       ///< br.if %c, bbT, bbF  (x and z count as false)
    Ret,        ///< ret
    WaitDelay,  ///< wait.delay <value>, bbN
    WaitEvent,  ///< wait.event bbN, <edge slot range>...
};

/// Edge of an event-control item.
enum class Edge : std::uint8_t { Any, Pos, Neg, Both };

/// An instruction operand.
struct Operand {
    enum class Kind : std::uint8_t { Temp, Const, Slot, Block, Text };
    Kind kind = Kind::Temp;
    std::uint32_t id = 0;     ///< TempId, ConstId, SlotId or BlockId.
    std::uint32_t lsb = 0;    ///< Slot: first bit of the range.
    std::uint32_t width = 0;  ///< Slot: bits in the range; 0 means the whole slot.
    Edge edge = Edge::Any;    ///< Slot in wait.event: the edge to wait for.
    std::string text;         ///< Text: system task name or string literal.

    static Operand temp(TempId t) { return {Kind::Temp, t, 0, 0, Edge::Any, {}}; }
    static Operand constant(ConstId c) { return {Kind::Const, c, 0, 0, Edge::Any, {}}; }
    static Operand slot(SlotId s, std::uint32_t lsb = 0, std::uint32_t width = 0) {
        return {Kind::Slot, s, lsb, width, Edge::Any, {}};
    }
    static Operand event(SlotId s, Edge e) { return {Kind::Slot, s, 0, 0, e, {}}; }
    static Operand block(BlockId b) { return {Kind::Block, b, 0, 0, Edge::Any, {}}; }
    static Operand str(std::string s) { return {Kind::Text, 0, 0, 0, Edge::Any, std::move(s)}; }
};

/// One instruction. `result` is kNone for instructions without a value.
struct Instr {
    Op op = Op::Ret;
    Type type;  ///< Result type (void when there is no result).
    TempId result = kNone;
    std::vector<Operand> operands;
    SourceLoc loc;
};

/// A basic block: instructions ending with exactly one terminator.
struct BasicBlock {
    std::vector<Instr> instrs;
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
};

/// One item of a process's sensitivity: a slot range and the edge.
struct Trigger {
    SlotId slot = 0;
    std::uint32_t lsb = 0;
    std::uint32_t width = 0;  ///< 0 means the whole slot.
    Edge edge = Edge::Any;
};

/// A process. Block 0 is the entry block.
struct Process {
    std::string name;
    ProcessKind kind = ProcessKind::Initial;
    InstanceId instance = 0;
    std::vector<Trigger> triggers;
    std::vector<BasicBlock> blocks;
    std::uint32_t numTemps = 0;
    SourceLoc loc;
};

/// The SIR of one design.
struct Design {
    std::vector<std::string> sources;  ///< File number -> path.
    std::vector<Instance> instances;
    std::vector<Slot> slots;
    std::vector<NameEntry> names;
    std::vector<Constant> constants;
    std::vector<Process> processes;
};

/// Text name of an opcode in the SIR text format (load, br.if, ...).
const char* opName(Op op);
/// Text name of a process kind (cont, comb, ff-plain, ...).
const char* kindName(ProcessKind k);
/// Text name of a slot kind (var, wire, ...).
const char* kindName(SlotKind k);
/// Text name of an edge (any, posedge, negedge, edge).
const char* edgeName(Edge e);

}  // namespace haremasim::sir
