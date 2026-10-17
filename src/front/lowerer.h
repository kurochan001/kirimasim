// The lowering of slang's elaborated design to SIR (private to front).
//
// lower.cpp handles modules, variables and procedures, lower_stmt.cpp the
// statements and lower_expr.cpp the expressions. A construct that is not
// supported yet is reported as an error "unsupported: ... (not implemented
// yet)" (CLI.md 5) and the procedure that contains it is dropped; nothing
// is silently skipped.

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "kirimasim/sir/design.h"
#include "kirimasim/sir/timescale.h"
#include "slang/ast/Compilation.h"
#include "slang/ast/Statement.h"
#include "slang/ast/TimingControl.h"
#include "slang/ast/expressions/AssignmentExpressions.h"
#include "slang/ast/expressions/CallExpression.h"
#include "slang/ast/expressions/OperatorExpressions.h"
#include "slang/ast/statements/ConditionalStatements.h"
#include "slang/ast/statements/LoopStatements.h"
#include "slang/ast/statements/MiscStatements.h"
#include "slang/ast/symbols/BlockSymbols.h"
#include "slang/ast/symbols/InstanceSymbols.h"
#include "slang/ast/symbols/VariableSymbols.h"
#include "slang/ast/types/AllTypes.h"
#include "slang/numeric/ConstantValue.h"
#include "slang/text/SourceLocation.h"
#include "slang/text/SourceManager.h"

namespace kirimasim::front {

namespace ast = slang::ast;

/// Thrown after an "unsupported" error has been reported, to drop the
/// procedure being lowered.
struct Unsupported {};

/// The time unit and precision of a scope; 1ns/1ns when nothing sets them.
sir::TimeScale timeScaleOf(const ast::Scope& scope);
/// Whether a statement may wait (unknown statements count as waiting).
bool mayWait(const ast::Statement& s);
/// The symbol a simple or hierarchical name refers to, or nullptr for any
/// other expression.
const ast::Symbol* namedSymbol(const ast::Expression& e);
/// Whether a name refers to something kept in a slot: a net, or a static
/// variable or argument (SIR_SPEC.md 3.3). Automatic variables live in
/// frames.
bool inSlot(const ast::Symbol& s);

/// A variable, or a formal argument of a subroutine (also a variable).
bool isVariable(const ast::Symbol& s);
/// The elements of an assignment pattern of any of its three forms, in
/// order: slang expands the defaults and the names of a structured one;
/// a replicated one repeats its elements its constant count of times
/// (10.9.1), or gives nothing when the count is not known.
std::vector<const ast::Expression*> patternElements(const ast::Expression& e);
/// Whether e is an assignment pattern.
bool isPattern(const ast::Expression& e);

/// An unpacked-array element access: the array (a variable, a net or a
/// member of an unpacked structure) and the index expressions, outermost
/// dimension first.
struct ArrayAccess {
    const ast::Expression* base = nullptr;
    std::vector<const ast::Expression*> indices;
};
std::optional<ArrayAccess> arrayAccess(const ast::Expression& e);

/// The unpacked structure type that a type is, or nullptr.
const ast::UnpackedStructType* unpackedStruct(const ast::Type& t);
/// The unpacked structure type that a type is, or that a fixed-size
/// unpacked array (of any dimensions) has as its elements, or nullptr.
const ast::UnpackedStructType* structElement(const ast::Type& t);
/// An innermost member of an unpacked structure: its path from the
/// structure (".a.b"), its type, and the dimensions of the arrays of
/// structures around it, outermost first (SIR_SPEC.md 4.2). Anything else
/// is its own one leaf.
struct Leaf {
    std::string path;
    const ast::Type* type = nullptr;
    std::vector<sir::ArrayDim> dims;
};
/// The leaves of a type, in declaration order (SIR_SPEC.md 4.2).
void structLeaves(const ast::Type& t, const std::string& prefix, std::vector<Leaf>& out,
                  const std::vector<sir::ArrayDim>& outer = {});
/// The element type of a fixed-size unpacked array (of any dimensions), its
/// dimensions added to `dims` outermost first; any other type is itself.
const ast::Type& arrayElement(const ast::Type& type, std::vector<sir::ArrayDim>& dims);
/// The number of leaves of a type: 1 unless it is an unpacked structure
/// or an array of them.
std::uint32_t leafCount(const ast::Type& t);
/// The number of leaves before member f of a structure.
std::uint32_t leafOffset(const ast::UnpackedStructType& st, const ast::FieldSymbol& f);
/// Whether an unpacked structure type gives some member a default value
/// (7.2.2), itself or in a nested structure.
bool hasMemberDefaults(const ast::Type& t);

/// A value: an operand (a temp or a constant) and its type.
struct Val {
    sir::Operand op;
    sir::Type type;
};

class Lowerer {
public:
    Lowerer(const slang::SourceManager& sm, int precision) : sm_(sm), precision_(precision) {}

    void lower(const ast::RootSymbol& root);

    sir::Design takeDesign() { return std::move(design_); }
    const std::string& diagnostics() const { return diags_; }
    std::size_t numErrors() const { return numErrors_; }

    // --- Used by the statement and expression lowering. ---

    /// Reports an unsupported construct and throws Unsupported.
    [[noreturn]] void unsupported(slang::SourceLocation sl, std::string_view what);
    /// Reports an unsupported construct without throwing.
    void report(slang::SourceLocation sl, std::string_view what);
    /// Reports an error in the design that slang does not find and throws
    /// Unsupported, which drops the procedure.
    [[noreturn]] void error(slang::SourceLocation sl, std::string_view what);
    /// $sscanf and $value$plusargs: system functions with output arguments
    /// (SIR_SPEC.md 8).
    Val lowerScanCall(const ast::CallExpression& call, sir::Type rt, slang::SourceLocation sl);
    sir::SourceLoc loc(slang::SourceLocation sl);

    /// The SIR value type of a slang type; unsupported for anything other
    /// than integral types of 1 to 64 bits.
    sir::Type typeOf(const ast::Type& t, slang::SourceLocation sl);

    // Statements (lower_stmt.cpp).
    void lowerStatement(const ast::Statement& s);
    void lowerIf(const ast::ConditionalStatement& s);
    void lowerImmediateAssertion(const ast::ImmediateAssertionStatement& s);
    void lowerFork(const ast::BlockStatement& b);
    /// One target of assign, force, deassign or release: a slot range and
    /// the type of its value (SIR_SPEC.md 5.6).
    struct HeldTarget {
        sir::Operand range;
        sir::Type type;
    };
    std::vector<HeldTarget> heldTargets(const ast::Expression& lhs, slang::SourceLocation sl);
    void lowerOverride(const ast::ProceduralAssignStatement& s);
    void lowerUniqueIf(const ast::ConditionalStatement& s);
    /// The violation checks of unique, unique0 and priority on the 1-bit
    /// two-state match of each item or condition (SCHEDULER_SPEC.md 15.1).
    void reportViolations(const std::vector<Val>& matches, ast::UniquePriorityCheck check,
                          bool hasElse, bool isCase, slang::SourceLocation sl);
    void lowerCase(const ast::CaseStatement& s);
    void lowerTiming(const ast::TimingControl& t);
    void lowerForeach(const std::vector<const ast::ForeachLoopStatement::LoopDim*>& dims,
                      std::size_t k, const ast::Statement& body, slang::SourceLocation sl);
    /// The exit and continue blocks of loop number `loop`, made on first use.
    sir::BlockId breakTarget(std::size_t loop);
    sir::BlockId continueTarget(std::size_t loop);
    /// Ends loop number `loop`: carries on at its exit, or marks the code
    /// after it unreachable when nothing leaves it.
    void endLoop(std::size_t loop);
    /// br to a block, unless the current point is unreachable.
    void jump(sir::BlockId to, slang::SourceLocation sl);

    // Expressions (lower_expr.cpp).
    Val lowerExpr(const ast::Expression& e);
    Val lowerElementSelect(const ast::Expression& e);
    Val lowerRangeSelect(const ast::Expression& e);
    Val lowerUnary(const ast::Expression& e);
    Val lowerBinary(const ast::Expression& e);
    /// The bit position, from the least significant bit, of element `idx`
    /// of a packed range whose elements are `elemWidth` bits wide. A
    /// constant past the end when a constant index is outside the range.
    Val bitPosition(const ast::Expression& selector, const slang::ConstantRange& r,
                    std::uint32_t elemWidth);
    /// The bits a part-select covers: a run-time position, or a constant
    /// lsb, and the width.
    struct Part {
        std::optional<Val> pos;
        std::int64_t lsb = 0;
        std::uint32_t width = 0;
    };
    Part partOf(const ast::Expression& e);
    /// The number of the element that constant indices select in an
    /// unpacked array slot (SIR_SPEC.md 4.2), or nullopt when an index is
    /// not constant or is outside its range.
    /// The element number of constant declared indices, or nothing when one
    /// is outside its range.
    static std::optional<std::uint32_t> constElementOf(const sir::Slot& slot,
                                                       const std::vector<std::int64_t>& indices);
    std::optional<std::uint32_t> constElement(const sir::Slot& slot,
                                              const std::vector<const ast::Expression*>& indices);
    /// Converts a value to another integral type (VALUE_SEMANTICS.md 2).
    Val coerce(Val v, sir::Type want, slang::SourceLocation sl);
    /// lowerExpr, then the truth value as a 1-bit value for br.if.
    Val lowerCondition(const ast::Expression& e);
    /// Writes a value to an lvalue expression with a blocking or
    /// nonblocking assignment.
    void lowerStore(const ast::Expression& lhs, const Val& v, bool nonblocking,
                    slang::SourceLocation sl);
    /// An assignment target with its indices and positions evaluated, so
    /// that it can be read and written without evaluating them again
    /// (11.4.1, 11.4.2).
    struct Place {
        enum class Kind : std::uint8_t {
            Frame,       ///< An automatic variable.
            Slot,        ///< A whole net or static variable.
            Ref,         ///< What ref argument `frame` refers to (SIR_SPEC.md 8).
            Element,     ///< An element of an unpacked array.
            Bits,        ///< Bits of a variable or of an element.
            StringChar,  ///< A character of a string (base[0]).
            Concat,      ///< A concatenation of the places in base (11.4.12).
        };
        Kind kind = Kind::Slot;
        /// Where the variable of an Element or Bits place lives: in slot
        /// `slot`, in frame variable `slot`, or where ref argument `slot`
        /// refers to (SIR_SPEC.md 5.3, 7.4, 8).
        enum class Home : std::uint8_t { Slot, Frame, Ref };
        Home home = Home::Slot;
        sir::FrameVarId frame = 0;
        std::uint32_t leaf = 0;  ///< Ref: the leaf of a structure it refers to.
        sir::SlotId slot = 0;
        std::vector<sir::Operand> indices;  ///< Of an array element; of the character.
        std::optional<std::uint32_t> elem;  ///< The element, when the indices are constant.
        std::optional<Val> pos;             ///< Bits: a position known at run time,
        std::int64_t lsb = 0;               ///< or a constant one;
        std::uint32_t width = 0;            ///< and the width.
        std::vector<Place> base;            ///< StringChar: the string; Concat: the parts.
        sir::Type type;                     ///< The type of the value there.
    };
    Place placeOf(const ast::Expression& lhs, slang::SourceLocation sl);
    /// Bits lsb .. lsb + width - 1 of an integral place: a member of a
    /// packed structure, or a part of a member or part.
    Place narrow(Place base, std::int64_t lsb, std::uint32_t width, slang::SourceLocation sl);
    Val loadPlace(const Place& p, slang::SourceLocation sl);
    void storePlace(const Place& p, const Val& v, bool nonblocking, slang::SourceLocation sl);
    /// What a read or write of a place covers (SIR_SPEC.md 3.4).
    sir::SlotRange rangeOf(const Place& p) const;
    Val lowerAssignment(const ast::AssignmentExpression& a);
    /// An unpacked array in a slot, or a sub-array of one selected by
    /// constant indices (declared index values).
    struct ArrayTarget {
        sir::SlotId slot = 0;
        std::vector<std::int64_t> indices;
        Place::Home home = Place::Home::Slot;  ///< slot is a frame variable unless Slot.
    };
    /// An unpacked array: a slot, an array frame variable, or what an array
    /// ref argument refers to.
    struct ArrayBase {
        ArrayBase(sir::SlotId s) : id(s) {}  // NOLINT: a slot is the usual base
        ArrayBase(Place::Home h, std::uint32_t i) : home(h), id(i) {}
        Place::Home home = Place::Home::Slot;
        std::uint32_t id = 0;
    };
    static ArrayBase baseOf(const ArrayTarget& a) { return {a.home, a.slot}; }
    const std::vector<sir::ArrayDim>& dimsOf(ArrayBase b);
    sir::Type elemTypeOf(ArrayBase b);
    /// The value type of the variable an Element or Bits place is in (one
    /// element of an array).
    sir::Type homeType(const Place& p);
    std::optional<ArrayTarget> arrayTarget(const ast::Expression& e);
    /// The writes of an aggregate assignment: each place with the value it
    /// gets, all computed before any is written (VALUE_SEMANTICS.md 3.13).
    using Stores = std::vector<std::pair<Place, Val>>;
    void flushStores(const Stores& stores, bool nonblocking, slang::SourceLocation sl);
    /// The element of array s at constant declared indices.
    Place elementPlace(ArrayBase s, const std::vector<std::int64_t>& indices);
    /// Computes an unpacked array value (a pattern, another array, or nested
    /// ones) for the (sub-)array of s at `indices`, element by element.
    void assignArray(ArrayBase s, std::vector<std::int64_t> indices, const ast::Expression& rhs,
                     Stores& out, slang::SourceLocation sl);
    void copyArray(ArrayBase to, std::vector<std::int64_t> toIndices, ArrayBase from,
                   std::vector<std::int64_t> fromIndices, Stores& out, slang::SourceLocation sl);
    /// The number of the element at the given declared indices
    /// (SIR_SPEC.md 4.2).
    std::uint32_t elementNumber(const sir::Slot& slot,
                                const std::vector<std::int64_t>& indices) const;

    // Subroutines (lower_sub.cpp).
    /// The subroutine row of a function or task for the current instance,
    /// lowered on its first call.
    sir::SubroutineId subroutineOf(const ast::SubroutineSymbol& sym, slang::SourceLocation sl);
    Val lowerCall(const ast::CallExpression& call);
    /// A ref to the actual of a ref argument (SIR_SPEC.md 8).
    Val refOf(const ast::Expression& actual, bool isConst, slang::SourceLocation sl);
    /// The operand of a fixed-size unpacked array argument (SIR_SPEC.md 8).
    sir::Operand arrayArgument(const ast::Expression& actual, const sir::Param& prm,
                               const std::vector<sir::ArrayDim>& dims, slang::SourceLocation sl);
    void lowerReturn(const ast::Expression* e, slang::SourceLocation sl);
    /// The format and value arguments of $display, $write and $sformatf.
    void lowerFormatArgs(std::span<const ast::Expression* const> args,
                         std::vector<sir::Operand>& ops, slang::SourceLocation sl);
    /// A system task call used as a statement.
    void lowerSystemTask(const ast::CallExpression& call);

    // Constants.
    /// The value of an expression slang can evaluate as a constant.
    std::optional<slang::ConstantValue> constantValue(const ast::Expression& e);
    /// The same as a 64-bit integer, if it is one without x or z bits.
    std::optional<std::int64_t> constInt(const ast::Expression& e);
    Val constant(sir::Type t, std::uint64_t a, std::uint64_t b = 0);
    Val realConstant(sir::Type t, double d);
    Val stringConstant(std::string bytes);
    /// Lowers a string method call (6.16) or a call of $sformatf.
    std::optional<Val> lowerStringCall(const ast::CallExpression& call, slang::SourceLocation sl);
    /// Lowers an enumeration method call (6.19.5).
    std::optional<Val> lowerEnumCall(const ast::CallExpression& call, slang::SourceLocation sl);
    Val constantFrom(const slang::ConstantValue& cv, sir::Type t, slang::SourceLocation sl);

    // Building blocks.
    sir::TempId newTemp() { return numTemps()++; }
    sir::BlockId newBlock();
    /// A new argument of block b (SIR_SPEC.md 5.1), where values join.
    Val blockParam(sir::BlockId b, sir::Type t);
    void setBlock(sir::BlockId b) {
        b_.block = b;
        b_.dead = false;
    }
    Val emit(sir::Op op, sir::Type t, std::vector<sir::Operand> ops, slang::SourceLocation sl);
    void emitVoid(sir::Op op, std::vector<sir::Operand> ops, slang::SourceLocation sl);
    /// Emits a terminator into the current block and marks the code after
    /// it unreachable until setBlock().
    void terminate(sir::Op op, std::vector<sir::Operand> ops, slang::SourceLocation sl);
    void emitInto(sir::BlockId b, sir::Op op, std::vector<sir::Operand> ops,
                  slang::SourceLocation sl);

    sir::Process& proc() { return design_.processes[b_.proc]; }
    // The parts of the body being lowered, a process or a subroutine.
    std::vector<sir::BasicBlock>& blocks() {
        return b_.isSub ? design_.subroutines[b_.proc].blocks : proc().blocks;
    }
    std::vector<sir::FrameVar>& frameVars() {
        return b_.isSub ? design_.subroutines[b_.proc].frame : proc().frame;
    }
    std::uint32_t& numTemps() {
        return b_.isSub ? design_.subroutines[b_.proc].numTemps : proc().numTemps;
    }
    sir::Design& design() { return design_; }
    int precision() const { return precision_; }
    sir::TimeScale timeScale() const { return b_.scale; }

    /// The hierarchical name of the scope a symbol is declared in, for %m
    /// (SYSTEM_TASKS.md 3.8).
    std::string scopeNameOf(const ast::Symbol& sym) const;
    /// The slot of a net or static variable, created on first use.
    sir::SlotId slotOf(const ast::ValueSymbol& v, slang::SourceLocation sl);
    /// The slots of a static variable of an unpacked structure type, one
    /// per leaf in order, created on first use: the first one.
    sir::SlotId structSlots(const ast::ValueSymbol& v, slang::SourceLocation sl);
    /// A new slot for variable v, or for the member of it at path `member`
    /// (empty for v itself) of the given type, inside arrays of structures
    /// of dimensions `outer`.
    sir::SlotId newSlot(const ast::ValueSymbol& v, const std::string& member, const ast::Type& type,
                        const std::vector<sir::ArrayDim>& outer, slang::SourceLocation sl);
    /// The frame variables of an automatic variable of an unpacked
    /// structure type, one per leaf in order: the first one.
    sir::FrameVarId structFrameVars(const ast::VariableSymbol& v, slang::SourceLocation sl);
    /// Where the value an expression names lives: a frame variable or a
    /// slot. For an unpacked structure, its first leaf, the others
    /// following in order (SIR_SPEC.md 4.2).
    struct Storage {
        bool frame = false;
        std::uint32_t id = 0;
        /// In an array of structures: the indices of the element, outermost
        /// first, evaluated once (SIR_SPEC.md 4.2), and their values when
        /// all are constant.
        std::vector<sir::Operand> elem;
        bool dynamic = false;
        std::vector<std::int64_t> constIdx;
    };
    /// The storage `off` leaves after s, in the same element.
    static Storage leafAt(Storage s, std::uint32_t off);
    /// s with one more constant element index.
    Storage withIndex(Storage s, std::int64_t index);
    /// Whether the leaf at s is an unpacked array beyond its element indices.
    bool isArrayLeaf(const Storage& s);
    /// The array at s, a leaf that is one.
    ArrayBase storageBase(const Storage& s);
    /// The constant element indices of s; reported when they are not.
    std::vector<std::int64_t> fixedIndices(const Storage& s, slang::SourceLocation sl);
    /// The element of an unpacked array named by base and indices, its
    /// indices evaluated in order.
    Place arrayElementPlace(const ast::Expression& base,
                            const std::vector<const ast::Expression*>& indices,
                            slang::SourceLocation sl);
    /// The storage of a variable, a net or a member of an unpacked
    /// structure; nothing for other expressions.
    std::optional<Storage> storageOf(const ast::Expression& e, slang::SourceLocation sl);
    /// Whether an expression is a member of an unpacked structure.
    static bool isUnpackedMember(const ast::Expression& e);
    /// The place of one leaf: a frame variable or a whole slot.
    Place storagePlace(Storage st);
    /// An assignment with a delay or event control inside it (9.4.5).
    Val lowerDelayedAssignment(const ast::AssignmentExpression& a);
    /// The stream of a streaming concatenation used as a value (11.4.14).
    Val lowerStream(const ast::StreamingConcatenationExpression& s, slang::SourceLocation sl);
    /// v with its blocks of `size` bits in reverse order, sliced from the
    /// right (packing) or from the left (unpacking).
    Val reverseBlocks(const Val& v, std::uint64_t size, bool fromRight, slang::SourceLocation sl);
    /// An integral value, or an unpacked array or structure, as a stream of
    /// bits (11.4.14.1).
    Val bitstreamOf(const ast::Expression& e, slang::SourceLocation sl);
    /// The single places of a target in the order of bitstreamOf.
    void bitstreamPlaces(const ast::Expression& e, std::vector<Place>& out,
                         slang::SourceLocation sl);
    /// The element places of the (sub-)array of s at indices, in order.
    void arrayPlaces(ArrayBase s, std::vector<std::int64_t> indices, std::vector<Place>& out);
    /// The single places of the leaves of type t at s, in order.
    void storagePlaces(const Storage& s, const ast::Type& t, std::vector<Place>& out,
                       slang::SourceLocation sl);
    /// The concatenation of integral places, four-state like `like`.
    Place concatPlace(std::vector<Place> parts, sir::Type like, slang::SourceLocation sl);
    /// A streaming concatenation as the target of an assignment (11.4.14.3).
    void lowerUnpack(const ast::StreamingConcatenationExpression& s, const ast::Expression& rhs,
                     bool nonblocking, slang::SourceLocation sl);
    /// An array reduction method: sum, product, and, or, xor (7.12.3).
    Val lowerArrayReduction(const ast::CallExpression& call, const std::string& name, sir::Type rt,
                            slang::SourceLocation sl);
    /// Whether a type is an unpacked array or structure (11.2.2).
    static bool isAggregate(const ast::Type& t);
    /// The single values of an aggregate expression of type t, in order.
    void aggregateValues(const ast::Expression& e, const ast::Type& t, std::vector<Val>& out,
                         slang::SourceLocation sl);
    /// The elements of the (sub-)array of s at indices, in order.
    void arrayValues(ArrayBase s, std::vector<std::int64_t> indices, std::vector<Val>& out,
                     slang::SourceLocation sl);
    /// ==, !=, === and !== of unpacked arrays and structures (3.14).
    Val compareAggregates(const ast::BinaryExpression& b, slang::SourceLocation sl);
    /// a op b for a comparison op of values of any types that compare:
    /// strings, reals, or integral values at their common type. 1 bit.
    Val compareValues(sir::Op op, Val l, Val r, slang::SourceLocation sl);
    /// a op b for two 1-bit values (And, Or).
    Val bitOp(sir::Op op, const Val& a, const Val& b, slang::SourceLocation sl);
    /// Whether v is a member of the set of an inside operator or a case
    /// inside item (11.4.13, 12.5.4): 1, 0 or x.
    Val lowerInside(const Val& v, std::span<const ast::Expression* const> set,
                    slang::SourceLocation sl);
    Val insideMatch(const Val& v, const ast::Expression& e, slang::SourceLocation sl);
    /// Whether v matches an element of the (sub-)array of s at indices.
    Val arrayMatch(const Val& v, ArrayBase s, std::vector<std::int64_t> indices,
                   slang::SourceLocation sl);
    /// Computes an unpacked structure value for the leaves at dst, member
    /// by member (SIR_SPEC.md 4.2).
    void assignStruct(Storage dst, const ast::Type& t, const ast::Expression& rhs, Stores& out,
                      slang::SourceLocation sl);
    /// Computes a value of type t, a structure, an array of them, an array
    /// or a single value, for the leaves at dst.
    void assignAggregate(Storage dst, const ast::Type& t, const ast::Expression& rhs, Stores& out,
                         slang::SourceLocation sl);
    /// Copies the leaves of type t at src to those at dst.
    void copyAggregate(Storage dst, Storage src, const ast::Type& t, Stores& out,
                       slang::SourceLocation sl);
    /// The single values of the leaves of type t at s, in order (3.14).
    void storageValues(const Storage& s, const ast::Type& t, std::vector<Val>& out,
                       slang::SourceLocation sl);
    /// The default values (7.2.2) of the members of an unpacked structure
    /// at dst, where the type has them.
    void memberDefaults(const Storage& dst, const ast::Type& t, Stores& out,
                        slang::SourceLocation sl);
    /// A frame variable for an automatic variable, created on first use.
    sir::FrameVarId frameVarOf(const ast::VariableSymbol& v, slang::SourceLocation sl);
    std::optional<sir::FrameVarId> findFrameVar(const ast::Symbol& s) const;
    /// A frame variable the front end needs for itself (no name).
    sir::FrameVarId newFrameVar(sir::Type t);

    /// The slot range of an event-control expression on a fixed signal
    /// (a static variable, or a constant bit of one), if it is one.
    std::optional<sir::SlotRange> eventSlot(const ast::Expression& e);
    /// The event slot element that a named-event expression denotes.
    sir::Operand eventOperand(const ast::Expression& e, slang::SourceLocation sl);
    /// `ev.triggered` (15.5.3).
    Val lowerTriggered(const ast::Expression& ev, sir::Type rt, slang::SourceLocation sl);
    /// The ticks of a constant delay (9.4.1).
    std::uint64_t delayTicks(const ast::Expression& e, slang::SourceLocation sl);

    /// Puts the initializer of a static variable into the static-init
    /// process of the current instance (SIR_SPEC.md 7.1).
    void lowerStaticInit(const ast::VariableSymbol& v, sir::SlotId slot, slang::SourceLocation sl);

    void noteWrite(const sir::SlotRange& r);
    void noteRead(const sir::SlotRange& r);

    struct Loop {
        sir::BlockId breakTo = sir::kNone;
        sir::BlockId continueTo = sir::kNone;
    };

    /// The state of the procedure being lowered.
    struct Body {
        std::uint32_t proc = 0;
        sir::BlockId block = 0;
        bool dead = false;  ///< The current point is unreachable.
        std::map<const ast::Symbol*, sir::FrameVarId> frameVars;
        std::map<const ast::Symbol*, sir::FrameVarId> structFrames;  ///< The first leaf's.
        std::vector<Loop> loops;
        /// The named blocks around the current point, innermost last, for
        /// disable (9.6.2): the block, its end once a disable jumps there,
        /// and whether it starts processes it does not wait for.
        struct Named {
            const ast::Symbol* sym = nullptr;
            std::optional<sir::BlockId> exit;
            bool spawns = false;
        };
        std::vector<Named> named;
        sir::TimeScale scale;
        std::vector<sir::SlotRange> reads;        ///< For the sensitivity of always_comb.
        std::uint32_t forks = 0;                  ///< Fork statements so far, for child names.
        bool isSub = false;                       ///< proc is a subroutine number.
        const ast::Symbol* subSymbol = nullptr;   ///< The subroutine, when isSub.
        std::optional<sir::FrameVarId> retFrame;  ///< A function's return variable, automatic.
        std::optional<sir::SlotId> retSlot;       ///< The same, static.
        std::vector<Val> lvalueRefs;  ///< The targets' values of the assignment operators.
        /// The hierarchical name of the scope being lowered, for the
        /// messages of $error and the like (SYSTEM_TASKS.md 10.1).
        std::string scope;
    };
    Body& body() { return b_; }

private:
    const slang::SourceManager& sm_;
    int precision_;
    sir::Design design_;
    std::string diags_;
    std::size_t numErrors_ = 0;
    std::map<std::uint32_t, std::uint32_t> fileIds_;  // slang buffer id -> SIR file number
    std::map<const ast::Symbol*, sir::SlotId> slots_;
    std::map<const ast::Symbol*, sir::SlotId> structSlots_;  ///< The first leaf's slot.
    /// The values of the iterators of the with clauses being lowered.
    std::map<const ast::Symbol*, Val> iterators_;
    std::map<std::string, std::uint32_t> slotNames_;  ///< Uses of each slot name.
    sir::InstanceId instance_ = 0;
    std::map<const ast::InstanceSymbol*, sir::InstanceId> instanceIds_;
    /// Row of the instance table -> its symbol: an instance, a package or
    /// a compilation unit.
    std::vector<const ast::Symbol*> instanceSyms_;
    /// Packages and compilation units -> their rows (SIR_SPEC.md 2).
    std::map<const ast::Symbol*, sir::InstanceId> scopeRows_;
    std::uint32_t units_ = 0;  ///< Compilation units with rows so far.
    /// The cont processes whose drive strength is not (strong1, strong0):
    /// process -> (strength1, strength0).
    std::map<std::uint32_t, std::pair<std::uint8_t, std::uint8_t>> strengths_;
    /// The cont processes of port connections between nets.
    std::set<std::uint32_t> netPorts_;
    /// The side computation of a three-state gate (SIR_SPEC.md 3.5):
    /// instructions [first, first + count) of its process's entry block,
    /// which store to a side slot not made yet (kNone). resolveNets points
    /// them at the side slot of the gate's driver when its net resolves,
    /// and removes them when it does not. `known` is false when the output
    /// is a place whose bits are not fixed; resolving it is unsupported.
    struct GateSide {
        std::size_t first = 0;
        std::size_t count = 0;
        bool known = true;
    };
    std::map<std::uint32_t, GateSide> sides_;
    /// The port connections whose nets are merged (mergePorts), and the
    /// slots they share.
    std::set<const ast::PortConnection*> mergedPorts_;
    std::set<sir::SlotId> mergedSlots_;
    /// Process names used so far, "<scope>.<kind>" -> the next number.
    std::map<std::string, std::uint32_t> nameCounts_;
    std::map<sir::InstanceId, std::uint32_t> staticInit_;  ///< Instance -> its static-init process.
    std::map<sir::InstanceId, Body> staticBody_;           ///< Its lowering state.
    std::map<sir::InstanceId, slang::SourceLocation> staticLoc_;  ///< Its first initializer.
    /// Subroutines lowered so far, per instance and symbol.
    std::map<std::pair<sir::InstanceId, const ast::Symbol*>, sir::SubroutineId> subs_;
    std::map<sir::SubroutineId, std::vector<sir::SlotRange>> subWrites_;
    std::map<sir::SubroutineId, std::vector<sir::SlotRange>> subReads_;
    /// Calls made by each body: (is a subroutine, number) -> callees.
    std::map<std::pair<bool, std::uint32_t>, std::vector<sir::SubroutineId>> callees_;
    /// The reads of each always_comb and always_latch, for its sensitivity.
    std::map<std::uint32_t, std::vector<sir::SlotRange>> combReads_;
    const ast::Symbol* evalSymbol_ = nullptr;  ///< Where constants are evaluated.
    Body b_;

    // The instance tree is walked twice, depth first and in source order
    // (SIR_SPEC.md 1): first for the instance table, the slots of nets and
    // variables and the static initializers, then for the processes.
    void declareInstance(const ast::InstanceSymbol& inst, sir::InstanceId parent);
    /// The rows of the packages and compilation units that hold variables,
    /// nets or subroutines, in source order, with their slots and static
    /// initializers, before any instance (SIR_SPEC.md 2, 3.3, 7.1).
    void declarePackages(const ast::RootSymbol& root);
    /// The package or compilation unit a symbol is declared in, if any.
    static const ast::Symbol* packageOf(const ast::Symbol& s);
    /// The row of the package or compilation unit a symbol is declared in,
    /// if it is declared in one; made on first use.
    std::optional<sir::InstanceId> rowOf(const ast::Symbol& s);
    /// The row of a package or compilation unit, made on first use.
    sir::InstanceId rowFor(const ast::Symbol& scope);
    void declareScope(const ast::Scope& scope);
    void lowerInstance(const ast::InstanceSymbol& inst);
    void lowerScope(const ast::Scope& scope, const std::string& prefix);
    /// The implicit continuous assignments of a child's port connections
    /// (4.9.6, SIR_SPEC.md 3.5).
    void lowerPorts(const ast::InstanceSymbol& child);
    /// A built-in gate or pull (VALUE_SEMANTICS.md 5.8), or one element of an
    /// array of them.
    void lowerPrimitive(const ast::PrimitiveInstanceSymbol& g, const std::string& prefix);
    /// Merges the child's nets with the parent's where a port connects
    /// whole nets of the same type (SIR_SPEC.md 3.5, 23.3.3.7).
    void mergePorts(const ast::InstanceSymbol& child);
    /// A cont process (SIR_SPEC.md 7.1): `body` lowers the right-hand side
    /// and the store; the process waits on what it reads.
    /// `strength` is the drive strength for 1 and for 0 (VALUE_SEMANTICS.md
    /// 5.3), used when the net it drives needs resolution.
    template <typename F>
    void lowerCont(const std::string& name, slang::SourceLocation sl, const ast::Symbol& evalAt,
                   F&& body, std::pair<std::uint8_t, std::uint8_t> strength = {6, 6});
    /// Makes the process wait on what its body read, not on what the
    /// subroutines it calls read inside: the operands of a continuous
    /// assignment (10.3.2) and the implicit sensitivity of @* (9.4.2.2,
    /// SCHEDULER_SPEC.md 9.2).
    void triggerOnReads();
    /// The next process name for a kind of process in a scope.
    std::string processName(const std::string& prefix, const std::string& kind);
    /// Drops the processes from number `first` on, after an unsupported
    /// construct, with the static-init processes made meanwhile.
    void dropProcesses(std::size_t first);
    /// "<file>:<line>:<column>" of a SIR source position.
    std::string whereOf(const sir::SourceLoc& l) const;
    /// The nets that need resolution (VALUE_SEMANTICS.md 5.5) get a driver
    /// slot per driver and a process that resolves them (SIR_SPEC.md 3.5).
    void resolveNets();
    /// After every body is lowered: the write sets and the always_comb
    /// sensitivity, including what called subroutines read and write.
    void finishDesign();
    void lowerProcedure(const ast::ProceduralBlockSymbol& pb, const std::string& prefix);
};

}  // namespace kirimasim::front
