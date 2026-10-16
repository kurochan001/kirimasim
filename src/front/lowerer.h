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
#include <string>
#include <string_view>
#include <vector>

#include "haremasim/sir/design.h"
#include "haremasim/sir/timescale.h"
#include "slang/ast/Compilation.h"
#include "slang/ast/Statement.h"
#include "slang/ast/TimingControl.h"
#include "slang/ast/expressions/AssignmentExpressions.h"
#include "slang/ast/expressions/CallExpression.h"
#include "slang/ast/statements/ConditionalStatements.h"
#include "slang/ast/statements/LoopStatements.h"
#include "slang/ast/symbols/BlockSymbols.h"
#include "slang/ast/symbols/InstanceSymbols.h"
#include "slang/ast/symbols/VariableSymbols.h"
#include "slang/numeric/ConstantValue.h"
#include "slang/text/SourceLocation.h"
#include "slang/text/SourceManager.h"

namespace haremasim::front {

namespace ast = slang::ast;

/// Thrown after an "unsupported" error has been reported, to drop the
/// procedure being lowered.
struct Unsupported {};

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
    sir::SourceLoc loc(slang::SourceLocation sl);

    /// The SIR value type of a slang type; unsupported for anything other
    /// than integral types of 1 to 64 bits.
    sir::Type typeOf(const ast::Type& t, slang::SourceLocation sl);

    // Statements (lower_stmt.cpp).
    void lowerStatement(const ast::Statement& s);
    void lowerIf(const ast::ConditionalStatement& s);
    void lowerFork(const ast::BlockStatement& b);
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
    /// Converts a value to another integral type (VALUE_SEMANTICS.md 2).
    Val coerce(Val v, sir::Type want, slang::SourceLocation sl);
    /// lowerExpr, then the truth value as a 1-bit value for br.if.
    Val lowerCondition(const ast::Expression& e);
    /// Writes a value to an lvalue expression with a blocking or
    /// nonblocking assignment.
    void lowerStore(const ast::Expression& lhs, const Val& v, bool nonblocking,
                    slang::SourceLocation sl);
    Val lowerAssignment(const ast::AssignmentExpression& a);
    /// A system task call used as a statement.
    void lowerSystemTask(const ast::CallExpression& call);

    // Constants.
    /// The value of an expression slang can evaluate as a constant.
    std::optional<slang::ConstantValue> constantValue(const ast::Expression& e);
    /// The same as a 64-bit integer, if it is one without x or z bits.
    std::optional<std::int64_t> constInt(const ast::Expression& e);
    Val constant(sir::Type t, std::uint64_t a, std::uint64_t b = 0);
    Val constantFrom(const slang::ConstantValue& cv, sir::Type t, slang::SourceLocation sl);

    // Building blocks.
    sir::TempId newTemp() { return proc().numTemps++; }
    sir::BlockId newBlock();
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
    sir::Design& design() { return design_; }
    int precision() const { return precision_; }
    sir::TimeScale timeScale() const { return b_.scale; }

    /// The slot of a static variable, created on first use.
    sir::SlotId slotOf(const ast::VariableSymbol& v, slang::SourceLocation sl);
    /// A frame variable for an automatic variable, created on first use.
    sir::FrameVarId frameVarOf(const ast::VariableSymbol& v, slang::SourceLocation sl);
    std::optional<sir::FrameVarId> findFrameVar(const ast::Symbol& s) const;
    /// A frame variable the front end needs for itself (no name).
    sir::FrameVarId newFrameVar(sir::Type t);

    /// The slot range of an event-control expression on a fixed signal
    /// (a static variable, or a constant bit of one), if it is one.
    std::optional<sir::SlotRange> eventSlot(const ast::Expression& e);

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
        std::vector<Loop> loops;
        sir::TimeScale scale;
        std::vector<sir::SlotRange> reads;  ///< For the sensitivity of always_comb.
        std::uint32_t forks = 0;            ///< Fork statements so far, for child names.
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
    std::map<std::string, std::uint32_t> slotNames_;  ///< Uses of each slot name.
    sir::InstanceId instance_ = 0;
    std::map<sir::InstanceId, std::uint32_t> staticInit_;  ///< Instance -> its static-init process.
    std::map<sir::InstanceId, Body> staticBody_;           ///< Its lowering state.
    const ast::Symbol* evalSymbol_ = nullptr;              ///< Where constants are evaluated.
    Body b_;

    void lowerTop(const ast::InstanceSymbol& inst);
    void checkWriters();
    void lowerProcedure(const ast::ProceduralBlockSymbol& pb, sir::InstanceId inst,
                        std::map<std::string, std::uint32_t>& counts);
};

}  // namespace haremasim::front
