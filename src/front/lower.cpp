// Lowering of slang's elaborated design to SIR.
//
// This first version covers only the thin path of P1 (stage M3a): top
// modules whose members are initial procedures made of begin-end blocks,
// $display and $write with a string literal, and $finish. Everything else is
// reported as an unsupported construct (CLI.md 5), never silently dropped.

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haremasim/front/front.h"
#include "haremasim/sir/types.h"
#include "haremasim/sir/verify.h"
#include "slang/ast/Compilation.h"
#include "slang/ast/Statement.h"
#include "slang/ast/expressions/CallExpression.h"
#include "slang/ast/expressions/LiteralExpressions.h"
#include "slang/ast/statements/MiscStatements.h"
#include "slang/ast/symbols/BlockSymbols.h"
#include "slang/ast/symbols/CompilationUnitSymbols.h"
#include "slang/ast/symbols/InstanceSymbols.h"
#include "slang/diagnostics/TextDiagnosticClient.h"
#include "slang/driver/Driver.h"
#include "slang/text/SourceManager.h"

namespace haremasim::front {

namespace {

namespace ast = slang::ast;

class Lowerer {
public:
    explicit Lowerer(const slang::SourceManager& sm) : sm_(sm) {}

    void lower(const ast::RootSymbol& root) {
        for (const ast::InstanceSymbol* top : root.topInstances)
            lowerTop(*top);
    }

    sir::Design takeDesign() { return std::move(design_); }
    const std::string& diagnostics() const { return diags_; }
    std::size_t numErrors() const { return numErrors_; }

private:
    const slang::SourceManager& sm_;
    sir::Design design_;
    std::string diags_;
    std::size_t numErrors_ = 0;
    std::map<std::uint32_t, std::uint32_t> fileIds_;  // slang buffer id -> SIR file number

    sir::Process* proc_ = nullptr;
    sir::BlockId block_ = 0;

    sir::SourceLoc loc(slang::SourceLocation sl) {
        if (!sl.valid()) return {};
        const std::uint32_t buf = sl.buffer().getId();
        auto it = fileIds_.find(buf);
        if (it == fileIds_.end()) {
            const auto id = static_cast<std::uint32_t>(design_.sources.size());
            design_.sources.emplace_back(sm_.getFileName(sl));
            it = fileIds_.emplace(buf, id).first;
        }
        return {it->second, static_cast<std::uint32_t>(sm_.getLineNumber(sl)),
                static_cast<std::uint32_t>(sm_.getColumnNumber(sl))};
    }

    void unsupported(slang::SourceLocation sl, std::string_view what) {
        ++numErrors_;
        if (sl.valid()) {
            diags_ += std::string(sm_.getFileName(sl)) + ":" +
                      std::to_string(sm_.getLineNumber(sl)) + ":" +
                      std::to_string(sm_.getColumnNumber(sl)) + ": ";
        } else {
            diags_ += "haremasim: ";
        }
        diags_ += "error: unsupported: ";
        diags_ += what;
        diags_ += " (not implemented yet)\n";
    }

    void emit(sir::Op op, std::vector<sir::Operand> ops, slang::SourceLocation sl) {
        proc_->blocks[block_].instrs.push_back(
            {op, sir::voidType(), sir::kNone, std::move(ops), loc(sl)});
    }

    sir::ConstId constant32(std::uint64_t v) {
        const auto id = static_cast<sir::ConstId>(design_.constants.size());
        design_.constants.push_back({sir::u2(32), {v & 0xffffffffU}, {}});
        return id;
    }

    void lowerTop(const ast::InstanceSymbol& inst) {
        const auto instId = static_cast<sir::InstanceId>(design_.instances.size());
        design_.instances.push_back(
            {std::string(inst.name), std::string(inst.getDefinition().name), sir::kNone});
        std::uint32_t initials = 0;
        for (const ast::Symbol& member : inst.body.members()) {
            if (member.kind != ast::SymbolKind::ProceduralBlock) {
                unsupported(member.location,
                            "module member '" + std::string(ast::toString(member.kind)) + "'");
                continue;
            }
            const auto& pb = member.as<ast::ProceduralBlockSymbol>();
            if (pb.procedureKind != ast::ProceduralBlockKind::Initial) {
                unsupported(pb.location,
                            std::string(ast::toString(pb.procedureKind)) + " procedure");
                continue;
            }
            sir::Process p;
            p.name = std::string(inst.name) + ".initial" + std::to_string(initials++);
            p.kind = sir::ProcessKind::Initial;
            p.instance = instId;
            p.loc = loc(pb.location);
            p.blocks.resize(1);
            design_.processes.push_back(std::move(p));
            proc_ = &design_.processes.back();
            block_ = 0;
            lowerStatement(pb.getBody());
            emit(sir::Op::Ret, {}, pb.location);
            proc_ = nullptr;
        }
    }

    void lowerStatement(const ast::Statement& s) {
        switch (s.kind) {
            case ast::StatementKind::Empty: return;
            case ast::StatementKind::List:
                for (const ast::Statement* item : s.as<ast::StatementList>().list)
                    lowerStatement(*item);
                return;
            case ast::StatementKind::Block: {
                const auto& b = s.as<ast::BlockStatement>();
                if (b.blockKind != ast::StatementBlockKind::Sequential) {
                    unsupported(s.sourceRange.start(), "fork-join block");
                    return;
                }
                if (b.blockSymbol != nullptr && !b.blockSymbol->members().empty()) {
                    unsupported(s.sourceRange.start(), "declarations in a block");
                    return;
                }
                lowerStatement(b.body);
                return;
            }
            case ast::StatementKind::ExpressionStatement:
                lowerExpressionStatement(s.as<ast::ExpressionStatement>());
                return;
            default:
                unsupported(s.sourceRange.start(),
                            "statement '" + std::string(ast::toString(s.kind)) + "'");
                return;
        }
    }

    void lowerExpressionStatement(const ast::ExpressionStatement& es) {
        const ast::Expression& e = es.expr;
        const slang::SourceLocation sl = e.sourceRange.start();
        if (e.kind != ast::ExpressionKind::Call || !e.as<ast::CallExpression>().isSystemCall()) {
            unsupported(sl, "expression statement");
            return;
        }
        const auto& call = e.as<ast::CallExpression>();
        const std::string name(call.getSubroutineName());
        const auto args = call.arguments();
        if (name == "$display" || name == "$write") {
            std::vector<sir::Operand> ops{sir::Operand::str(name)};
            if (args.size() > 1) {
                unsupported(sl, name + " with arguments after the format");
                return;
            }
            if (args.size() == 1) {
                if (args[0]->kind != ast::ExpressionKind::StringLiteral) {
                    unsupported(sl, name + " of something other than a string literal");
                    return;
                }
                const std::string text(args[0]->as<ast::StringLiteral>().getValue());
                for (std::size_t i = 0; i < text.size(); ++i) {
                    if (text[i] != '%') continue;
                    if (i + 1 < text.size() && text[i + 1] == '%') {
                        ++i;
                        continue;
                    }
                    unsupported(sl, name + " format specifiers");
                    return;
                }
                ops.push_back(sir::Operand::str(text));
            }
            emit(sir::Op::Syscall, std::move(ops), sl);
            return;
        }
        if (name == "$finish") {
            std::uint64_t n = 1;  // the default argument (20.2)
            if (!args.empty()) {
                const slang::ConstantValue* cv = args[0]->getConstant();
                const std::optional<std::uint64_t> v = cv != nullptr && cv->isInteger()
                                                           ? cv->integer().as<std::uint64_t>()
                                                           : std::nullopt;
                if (!v) {
                    unsupported(sl, "$finish with a non-constant argument");
                    return;
                }
                n = *v;
            }
            emit(sir::Op::Syscall, {sir::Operand::str(name), sir::Operand::constant(constant32(n))},
                 sl);
            return;
        }
        unsupported(sl, "system task " + name);
    }
};

}  // namespace

CompileResult compile(std::span<const std::string> args) {
    CompileResult result;
    slang::driver::Driver driver;
    driver.addStandardArgs();

    std::vector<const char*> argv;
    argv.push_back("haremasim");
    for (const auto& a : args)
        argv.push_back(a.c_str());
    if (!driver.parseCommandLine(static_cast<int>(argv.size()), argv.data())) return result;
    if (!driver.processOptions()) return result;

    driver.diagEngine.removeClient(driver.textDiagClient);
    auto client = std::make_shared<slang::TextDiagnosticClient>();
    driver.diagEngine.addClient(client);

    if (!driver.parseAllSources()) {
        result.numErrors = driver.diagEngine.getNumErrors();
        result.diagnostics = client->getString();
        return result;
    }
    std::unique_ptr<ast::Compilation> compilation = driver.createCompilation();
    driver.reportCompilation(*compilation, /* quiet */ true);
    result.numErrors = driver.diagEngine.getNumErrors();
    result.numWarnings = driver.diagEngine.getNumWarnings();
    result.diagnostics = client->getString();
    if (result.numErrors != 0) return result;

    Lowerer lowerer(*compilation->getSourceManager());
    lowerer.lower(compilation->getRoot());
    result.diagnostics += lowerer.diagnostics();
    if (lowerer.numErrors() != 0) {
        result.numErrors += lowerer.numErrors();
        result.unsupported = true;
        return result;
    }
    result.design = lowerer.takeDesign();
    for (const sir::VerifyError& e : sir::verify(result.design)) {
        result.verifyErrors +=
            "haremasim: internal error: SIR verifier: " + e.where + ": " + e.message + "\n";
    }
    result.success = result.verifyErrors.empty();
    return result;
}

}  // namespace haremasim::front
