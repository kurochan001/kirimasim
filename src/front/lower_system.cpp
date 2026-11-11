// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Lowering of method-style built-ins, format arguments and system tasks to SIR.

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "lower_expr_helpers.h"
#include "lowerer.h"
#include "slang/ast/ASTVisitor.h"
#include "slang/ast/EvalContext.h"
#include "slang/ast/ValuePath.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/LiteralExpressions.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/OperatorExpressions.h"
#include "slang/ast/expressions/SelectExpressions.h"
#include "slang/ast/symbols/MemberSymbols.h"
#include "slang/ast/symbols/ParameterSymbols.h"
#include "slang/ast/types/AllTypes.h"
#include "slang/numeric/SVInt.h"

namespace kirimasim::front {

// --- System tasks ------------------------------------------------------------

std::optional<Val> Lowerer::lowerEnumCall(const ast::CallExpression& call,
                                          slang::SourceLocation sl) {
    // The enumeration methods (6.19.5): the value is the first argument.
    const auto args = call.arguments();
    if (!isBuiltinMethod(call, ast::SymbolKind::EnumType) || args.empty()) return std::nullopt;
    const std::string name(call.getSubroutineName());
    const ast::Type& et = args[0]->type->getCanonicalType();
    std::vector<const ast::EnumValueSymbol*> members;
    for (const ast::EnumValueSymbol& m : et.as<ast::EnumType>().values())
        members.push_back(&m);
    if (members.empty()) return std::nullopt;
    const Type vt = typeOf(et, sl);
    const auto member = [&](std::size_t i) { return constantFrom(members[i]->getValue(), vt, sl); };
    const std::size_t n = members.size();
    if (name == "first" && args.size() == 1) return member(0);
    if (name == "last" && args.size() == 1) return member(n - 1);
    if (name == "num" && args.size() == 1) return constant(typeOf(*call.type, sl), n);
    const bool isName = name == "name" && args.size() == 1;
    if (!isName && !((name == "next" || name == "prev") && args.size() <= 2)) return std::nullopt;
    // next(N) and prev(N) wrap around; N is an int unsigned.
    std::size_t step = 0;
    bool dynamicCount = false;
    if (!isName) {
        std::int64_t count = 1;
        if (args.size() == 2) {
            const std::optional<std::int64_t> c = constInt(*args[1]);
            if (c)
                count = *c;
            else
                dynamicCount = true;
        }
        step = static_cast<std::size_t>(count) % n;
        if (name == "prev") step = (n - step) % n;
    }
    // A value that is not a member gives the empty string for name() and
    // the default initial value of the base type (table 6-7) for next()
    // and prev(). Members are told apart by ===, so a value with x or z
    // bits matches only a member with the same bits.
    const Val v = coerce(lowerExpr(*args[0]), vt, sl);
    if (!isName && !dynamicCount && args.size() == 2) observeRuntimeConstant(*args[1]);
    Val result = isName                 ? stringConstant("")
                 : sir::isFourState(vt) ? allXConstant(vt)
                                        : constant(vt, 0);
    if (dynamicCount) {
        // Read the receiver before the count, once each (3.1). slang binds
        // N as int unsigned; the wider index arithmetic avoids overflow
        // before wrapping in declaration order, even for N == 32'hffffffff.
        const Type indexType = sir::u2(64);
        const Val count = coerce(coerce(lowerExpr(*args[1]), sir::u2(32), sl), indexType, sl);
        const Val size = constant(indexType, n);
        Val delta = emit(Op::Mod, indexType, {count.op, size.op}, sl);
        if (name == "prev") delta = emit(Op::Sub, indexType, {size.op, delta.op}, sl);
        Val index = size;
        for (std::size_t i = n; i-- > 0;) {
            const Val hit = emit(Op::Ceq, sir::u2(1), {v.op, member(i).op}, sl);
            index = emit(Op::Select, indexType, {hit.op, constant(indexType, i).op, index.op}, sl);
        }
        const Val valid = emit(Op::Ne, sir::u2(1), {index.op, size.op}, sl);
        const Val offset = emit(Op::Add, indexType, {index.op, delta.op}, sl);
        const Val target = emit(Op::Mod, indexType, {offset.op, size.op}, sl);
        const Val fallback = result;
        for (std::size_t i = n; i-- > 0;) {
            const Val hit = emit(Op::Ceq, sir::u2(1), {target.op, constant(indexType, i).op}, sl);
            result = emit(Op::Select, vt, {hit.op, member(i).op, result.op}, sl);
        }
        return emit(Op::Select, vt, {valid.op, result.op, fallback.op}, sl);
    }
    for (std::size_t i = n; i-- > 0;) {
        const Val m = member(i);
        const Val hit = emit(Op::Ceq, sir::u2(1), {v.op, m.op}, sl);
        const Val then =
            isName ? stringConstant(std::string(members[i]->name)) : member((i + step) % n);
        result = emit(Op::Select, result.type, {hit.op, then.op, result.op}, sl);
    }
    return result;
}

std::optional<Val> Lowerer::lowerStringCall(const ast::CallExpression& call,
                                            slang::SourceLocation sl) {
    const std::string name(call.getSubroutineName());
    const auto args = call.arguments();
    const Type st{sir::TypeKind::String, 0};
    if (name == "$sformatf" || name == "$psprintf") {
        std::vector<Operand> ops{Operand::str("$sformatf")};
        lowerFormatArgs(args, ops, sl, 'd', name);
        return emit(Op::Syscall, st, std::move(ops), sl);
    }
    if (name == "$stacktrace" && args.empty())
        // The chain of calls as text (SYSTEM_TASKS.md 10.3): the scope of the
        // call, known here; the VM adds the calls that led to it.
        return emit(Op::Syscall, st, {Operand::str(name), Operand::str(b_.scope)}, sl);
    // The string methods (6.16): the string is the first argument.
    if (!isBuiltinMethod(call, ast::SymbolKind::StringType) || args.empty()) return std::nullopt;
    // A method that changes the string gets it as an lvalue argument.
    const ast::Expression* selfExpr = args[0];
    if (selfExpr->kind == ast::ExpressionKind::Assignment &&
        selfExpr->as<ast::AssignmentExpression>().isLValueArg())
        selfExpr = &selfExpr->as<ast::AssignmentExpression>().left();
    // A method that changes the string reads and writes it as one place,
    // its indices evaluated once.
    std::optional<Place> place;
    if (name == "putc" || name == "itoa" || name == "hextoa" || name == "octtoa" ||
        name == "bintoa" || name == "realtoa")
        place = placeOf(*selfExpr, sl);
    const Val self = place ? loadPlace(*place, sl) : lowerExpr(*selfExpr);
    const auto arg = [&](std::size_t i, Type t) { return coerce(lowerExpr(*args[i]), t, sl); };
    const auto method = [&](Type rt, std::vector<Operand> ops) {
        ops.insert(ops.begin(), {Operand::str(name), self.op});
        return emit(Op::Builtin, rt, std::move(ops), sl);
    };
    const Type rt = call.type->isVoid() ? sir::voidType() : typeOf(*call.type, sl);
    if (name == "len" && args.size() == 1) return method(rt, {});
    if (name == "getc" && args.size() == 2)
        return coerce(method(sir::u2(8), {arg(1, sir::s2(32)).op}), rt, sl);
    if ((name == "toupper" || name == "tolower") && args.size() == 1) return method(st, {});
    if ((name == "compare" || name == "icompare") && args.size() == 2)
        return method(rt, {arg(1, st).op});
    if (name == "substr" && args.size() == 3)
        return method(st, {arg(1, sir::s2(32)).op, arg(2, sir::s2(32)).op});
    if ((name == "atoi" || name == "atohex" || name == "atooct" || name == "atobin" ||
         name == "atoreal") &&
        args.size() == 1)
        return method(rt, {});
    // The methods that change the string: the new string goes to it.
    std::optional<Val> changed;
    if (name == "putc" && args.size() == 3)
        changed = method(st, {arg(1, sir::s2(32)).op, arg(2, sir::u2(8)).op});
    if ((name == "itoa" || name == "hextoa" || name == "octtoa" || name == "bintoa") &&
        args.size() == 2) {
        const Val v = lowerExpr(*args[1]);
        if (!sir::isIntegral(v.type)) unsupported(sl, name + " of this type");
        changed = method(st, {v.op});
    }
    if (name == "realtoa" && args.size() == 2)
        changed = method(st, {arg(1, Type{sir::TypeKind::Real, 0}).op});
    if (!changed || !place) unsupported(sl, "string method " + name);
    storePlace(*place, *changed, false, sl);
    return Val{Operand::temp(sir::TempId{0}), sir::voidType()};
}

void Lowerer::lowerFormatArgs(std::span<const ast::Expression* const> args,
                              std::vector<Operand>& ops, slang::SourceLocation sl, char radix,
                              std::string_view formatOf, std::vector<Val>* observed) {
    // A string literal not taken by an earlier specifier is a format
    // (SYSTEM_TASKS.md 3.1); the VM parses it once when it loads the
    // design. The format of $sformat and of $sformatf, then the first of
    // `args`, is a format whatever its form (3.13): a constant one is read
    // as a literal is, and as slang counts the arguments of literal formats
    // only, a wrong count for it is reported here. Runtime formats pass
    // raw arguments and the scope/time metadata to the shared kernel.
    // For constant formats a %t value is scaled from the caller's
    // time unit to ticks of the global precision here, so the VM needs no
    // unit.
    std::deque<char> pending;
    std::string cell;
    for (const ast::Symbol* sy = evalSymbol_; sy != nullptr && cell.empty();
         sy = sy->getParentScope() != nullptr ? &sy->getParentScope()->asSymbol() : nullptr)
        if (sy->kind == ast::SymbolKind::InstanceBody)
            cell = "work." + std::string(sy->as<ast::InstanceBodySymbol>().getDefinition().name);
    const auto takeFormat = [&](const std::string& text, slang::SourceLocation at) {
        std::vector<char> convs;
        std::string bad;
        std::string extra;
        if (!scanFormat(text, convs, bad, extra)) {
            // A % with no conversion is an error of the input; any other
            // specifier or flag left is one not implemented yet
            // (SYSTEM_TASKS.md 3.1, 3.12).
            if (bad == "%")
                error(at, "format has a % without a conversion");
            else
                unsupported(at, bad);
        }
        // A flag the standard does not define: one warning for the format
        // (3.12).
        if (!extra.empty()) warning(at, "format flag " + extra + " is not in IEEE 1800-2023");
        pending.assign(convs.begin(), convs.end());
        // %l: <library>.<cell> of the module the call is in; the library is
        // work until chapter 33's libraries exist (SYSTEM_TASKS.md 3.8).
        // Outside a module a %l has no cell: \x01 marks where it would go.
        const std::string f = withScope(text, b_.scope, cell.empty() ? "\x01" : cell);
        if (cell.empty() && f.find('\x01') != std::string::npos)
            unsupported(at, "%l outside a module");
        ops.push_back(Operand::str(f));
    };
    // Set by a constant format that is not a literal: every later argument
    // fills one of its specifiers (3.13).
    bool counted = false;
    const auto observe = [&](const ast::Expression& expression, std::span<const Val> values) {
        if (observed == nullptr || constantValue(expression)) return;
        if (expression.kind == ast::ExpressionKind::Call &&
            expression.as<ast::CallExpression>().isSystemCall()) {
            const auto name = expression.as<ast::CallExpression>().getSubroutineName();
            if (name == "$time" || name == "$stime" || name == "$realtime") return;
        }
        for (const auto& value : values)
            observed->push_back(snapshotValue(value, sl));
    };
    for (std::size_t k = 0; k < args.size(); ++k) {
        const ast::Expression* arg = args[k];
        if (k == 0 && !formatOf.empty() && arg->kind != ast::ExpressionKind::StringLiteral) {
            const std::optional<slang::ConstantValue> cv = constantValue(*arg);
            std::optional<std::string> text;
            if (cv && cv->isString()) text = cv->str();
            if (cv && cv->isInteger()) text = stringBytes(cv->integer());
            if (!text) {
                const Val format = lowerExpr(*arg);
                if (!isString(format.type) && !sir::isIntegral(format.type))
                    unsupported(arg->sourceRange.start(), "format of this type");
                ops[0] = Operand::str("$sformatf.runtime.strength");
                ops.push_back(Operand::str(b_.scope));
                ops.push_back(Operand::str(cell));
                const int shift = timeScale().unit - precision();
                if (shift < 0 || shift > 19) unsupported(sl, "%t with this time unit");
                std::uint64_t scale = 1;
                for (int i = 0; i < shift; ++i)
                    scale *= 10;
                ops.push_back(constant(sir::u2(64), scale).op);
                ops.push_back(format.op);
                bool hasStrength = false;
                for (const ast::Expression* value : args.subspan(1)) {
                    if (value->kind == ast::ExpressionKind::EmptyArgument) {
                        for (int i = 0; i < 5; ++i)
                            ops.push_back(Operand::str(""));
                        continue;
                    }
                    const auto argument = capturePatternArgument(*value, true);
                    hasStrength |= argument.strength.has_value();
                    const auto named =
                        formatPatternArgument(argument, false, value->sourceRange.start());
                    const auto positional =
                        formatPatternArgument(argument, true, value->sourceRange.start());
                    // The quartet describes the unconverted value. Its
                    // pre-rendered strings may use a string presentation
                    // for a literal-derived integral expression.
                    ops.push_back(
                        Operand::pattern(patternLayout(*value->type, value->sourceRange.start())));
                    ops.push_back(argument.scalar ? argument.scalar->op : Operand::str(""));
                    ops.push_back(named.op);
                    ops.push_back(positional.op);
                    ops.push_back(argument.strength ? argument.strength->op : Operand::str(""));
                }
                // Preserve the existing quartet ABI when no argument carries
                // net provenance. Runtime %v still has its Strong fallback.
                if (!hasStrength) {
                    ops[0] = Operand::str("$sformatf.runtime.pattern");
                    std::vector<Operand> compact(ops.begin(), ops.begin() + 5);
                    for (std::size_t first = 5; first < ops.size(); first += 5)
                        compact.insert(compact.end(), ops.begin() + first, ops.begin() + first + 4);
                    ops = std::move(compact);
                }
                return;
            }
            observeRuntimeConstant(*arg);
            takeFormat(*text, arg->sourceRange.start());
            counted = true;
            continue;
        }
        if (counted && pending.empty())
            error(arg->sourceRange.start(), "too many arguments for the format");
        if (arg->kind == ast::ExpressionKind::EmptyArgument) {
            ops.push_back(Operand::str(" "));
            continue;
        }
        if (arg->kind == ast::ExpressionKind::StringLiteral && pending.empty()) {
            takeFormat(std::string(arg->as<ast::StringLiteral>().getValue()),
                       arg->sourceRange.start());
            continue;
        }
        // A value without a specifier prints in the radix of the task's
        // form, like that specifier (SYSTEM_TASKS.md 3.1).
        const bool explicitSpec = !pending.empty();
        const bool hasSpec = explicitSpec || radix != 'd';
        const char conv = explicitSpec ? pending.front() : radix;
        if (!pending.empty()) pending.pop_front();
        if (explicitSpec && conv == 'v') {
            const auto captured = captureStrengthArgument(*arg);
            const auto rendered = strengthPresentation(captured, arg->sourceRange.start());
            ops.push_back(rendered.op);
            observe(*arg, std::span(&rendered, 1));
            continue;
        }
        if (explicitSpec && (conv == 'p' || conv == 'P')) {
            const auto argument = capturePatternArgument(*arg);
            ops.push_back(
                formatPatternArgument(argument, conv == 'P', arg->sourceRange.start()).op);
            if (argument.scalar)
                observe(*arg, std::span(&*argument.scalar, 1));
            else
                observe(*arg, argument.values);
            continue;
        }
        if (hasSpec && conv == 's' && arg->kind == ast::ExpressionKind::StringLiteral &&
            arg->type->getBitWidth() > 64) {
            // Its characters, which %s prints (SYSTEM_TASKS.md 3.5); a literal
            // has no zero bytes in front.
            const std::string text(arg->as<ast::StringLiteral>().getValue());
            if (text.find('\0') == std::string::npos) {
                ops.push_back(stringConstant(text).op);
                continue;
            }
        }
        Val v = lowerExpr(*arg);
        observe(*arg, std::span(&v, 1));
        const bool realFormat = conv == 'e' || conv == 'f' || conv == 'g';
        if (isReal(v.type)) {
            // A real with an integer format goes through a 64-bit signed
            // integer; without a specifier the VM shows it like %g
            // (SYSTEM_TASKS.md 3.6).
            // A real %t value, in the caller's time unit, becomes a real
            // count of ticks of the global precision; the VM rounds it once
            // (SYSTEM_TASKS.md 3.7).
            if (conv == 't') {
                const int shift = timeScale().unit - precision();
                if (shift < 0 || shift > 19) unsupported(sl, "%t with this time unit");
                v = coerce(v, Type{sir::TypeKind::Real, 0}, sl);
                if (shift > 0)
                    v = emit(Op::Mul, v.type,
                             {v.op, realConstant(v.type, std::pow(10.0, shift)).op}, sl);
                ops.push_back(v.op);
                continue;
            }
            if (hasSpec && !realFormat) v = coerce(v, sir::s2(64), sl);
        } else if (isString(v.type)) {
            // A string with %s or no specifier prints its characters, in
            // the b, o and h forms too (SYSTEM_TASKS.md 3.1).
            if (explicitSpec && conv != 's')
                unsupported(arg->sourceRange.start(), "%" + std::string(1, conv) + " of a string");
        } else if (!sir::isIntegral(v.type)) {
            unsupported(arg->sourceRange.start(), "display of a value of this type");
        } else if (realFormat) {
            v = coerce(v, Type{sir::TypeKind::Real, 0}, sl);  // integer as a real (7.4)
        }
        if (conv == 't') {
            const int shift = timeScale().unit - precision();
            if (shift < 0 || shift > 19) unsupported(sl, "%t with this time unit");
            if (shift > 0) {
                std::uint64_t f = 1;
                for (int i = 0; i < shift; ++i)
                    f *= 10;
                // The value as it is, with its sign, at a width that holds
                // the product (SYSTEM_TASKS.md 3.7).
                const Type scaled =
                    integral(sir::isFourState(v.type), sir::isSigned(v.type), v.type.width + 64);
                v = coerce(v, scaled, sl);
                v = emit(Op::Mul, scaled, {v.op, constant(scaled, f).op}, sl);
            }
        }
        ops.push_back(v.op);
    }
    if (counted && !pending.empty())
        error(args[0]->sourceRange.start(),
              std::string("no argument for the format's %") + pending.front());
}

bool Lowerer::readsAutomatic(const ast::Expression& e) const {
    bool automatic = false;
    auto finder = ast::makeVisitor([&](auto&, const ast::NamedValueExpression& n) {
        if (isVariable(n.symbol)) automatic = automatic || !inSlot(n.symbol);
    });
    e.visit(finder);
    return automatic;
}

Val Lowerer::lowerDescriptor(const ast::Expression& e, slang::SourceLocation sl) {
    // slang gives some of these tasks an int argument; the value as
    // written keeps its x and z bits, which name no file (SYSTEM_TASKS.md 6.1).
    const ast::Expression& written = e.unwrapImplicitConversions();
    const ast::Expression& d = written.type->isIntegral() ? written : e;
    return coerce(lowerExpr(d), integral(true, false, 32), sl);
}

void Lowerer::lowerPostponed(const ast::CallExpression& call, bool monitor, bool file, char radix) {
    const slang::SourceLocation sl = call.sourceRange.start();
    // $fstrobe and $fmonitor read their descriptor at the call, and the
    // rest in the Postponed region (SYSTEM_TASKS.md 6.2).
    const auto args = file ? call.arguments().subspan(1) : call.arguments();
    // The process runs apart from the caller, so it cannot read the
    // caller's automatic variables; slang rejects them already (13.3.2).
    for (const ast::Expression* a : args)
        if (readsAutomatic(*a)) unsupported(sl, "$strobe or $monitor of an automatic variable");
    const sir::InstanceId inst = b_.isSub ? design_.subroutine(b_.sub).instance : proc().instance;
    sir::Process p;
    p.name = processName(design_.instance(inst).scopeName,
                         std::string(file ? "$f" : "$") + (monitor ? "monitor" : "strobe"));
    p.kind = sir::ProcessKind::Postponed;
    p.instance = inst;
    p.loc = loc(sl);
    p.blocks.resize(1);
    const auto id = static_cast<sir::ProcessId>(design_.processes.size());
    design_.processes.push_back(std::move(p));
    Body saved = std::move(b_);
    b_ = Body{};
    b_.proc = id;
    b_.scale = saved.scale;
    b_.scope = saved.scope;
    const auto display = [&] {
        std::vector<Operand> ops{
            Operand::str(radix == 'd' ? "$display" : "$display" + std::string(1, radix))};
        lowerFormatArgs(args, ops, sl, radix);
        emitVoid(Op::Syscall, std::move(ops), sl);
    };
    try {
        if (monitor) {
            // It shows the values the first time it runs and whenever one
            // of them is not the one it showed last; the time is not
            // compared (SIR_SPEC.md 7.2, SYSTEM_TASKS.md 4). The arguments
            // are evaluated once, for the comparison and the print.
            lowerMonitor(args, radix, sl);
        } else {
            display();
        }
        terminate(Op::Ret, {}, sl);
    } catch (const Unsupported&) {
        b_ = std::move(saved);
        throw;
    }
    b_ = std::move(saved);
    std::vector<Operand> ops{Operand::process(id)};
    if (file) ops.push_back(lowerDescriptor(*call.arguments()[0], sl).op);
    emitVoid(monitor ? Op::Monitor : Op::Strobe, std::move(ops), sl);
}

void Lowerer::lowerSystemTask(const ast::CallExpression& call) {
    if (lowerForeignCall(call, call.sourceRange.start())) return;
    const slang::SourceLocation sl = call.sourceRange.start();
    const std::string name(call.getSubroutineName());
    const auto args = call.arguments();
    if (name == "$cast" && args.size() == 2 && args[0]->type->isClass()) {
        lowerClassCast(call, true);
        return;
    }
    // The b, o and h forms print values without a specifier in that radix
    // (SYSTEM_TASKS.md 3.1).
    const char radix =
        name.size() > 1 && (name.back() == 'b' || name.back() == 'o' || name.back() == 'h')
            ? name.back()
            : 'd';
    const std::string base = radix == 'd' ? name : name.substr(0, name.size() - 1);
    if (base == "$display" || base == "$write") {
        std::vector<Operand> ops{Operand::str(name)};
        lowerFormatArgs(args, ops, sl, radix);
        emitVoid(Op::Syscall, std::move(ops), sl);
        return;
    }
    if (base == "$strobe" || base == "$monitor" || base == "$fstrobe" || base == "$fmonitor") {
        lowerPostponed(call, base.ends_with("monitor"), base.starts_with("$f"), radix);
        return;
    }
    if ((base == "$fdisplay" || base == "$fwrite") && !args.empty()) {
        // The descriptor, then as $display and $write (SYSTEM_TASKS.md 6.2).
        std::vector<Operand> ops{Operand::str(name), lowerDescriptor(*args[0], sl).op};
        lowerFormatArgs(args.subspan(1), ops, sl, radix);
        emitVoid(Op::Syscall, std::move(ops), sl);
        return;
    }
    if ((name == "$readmemb" || name == "$readmemh" || name == "$writememb" ||
         name == "$writememh") &&
        args.size() >= 2 && args.size() <= 4) {
        // (file, memory[, start[, finish]]): the memory is an operand of
        // the syscall, which the kernel and the VM read or write element
        // by element; $readmem counts as writing all of it (SIR_SPEC.md 8).
        const ast::Expression* m = args[1];
        if (m->kind == ast::ExpressionKind::Assignment &&
            m->as<ast::AssignmentExpression>().isLValueArg())
            m = &m->as<ast::AssignmentExpression>().left();
        const std::optional<ArrayTarget> a = arrayTarget(*m);
        if (!a || !a->indices.empty()) unsupported(sl, name + " of this memory");
        if (!sir::isIntegral(elemTypeOf(baseOf(*a))))
            unsupported(sl, name + " of a memory of this element type");
        std::vector<Operand> ops{
            Operand::str(name), coerce(lowerExpr(*args[0]), Type{sir::TypeKind::String, 0}, sl).op,
            a->home == Place::Home::Slot ? Operand::slot(a->slot) : Operand::frame(a->frameHome())};
        // $readmem of a memory of an enumerated type takes its items' values
        // only (21.4.2): the enumeration's row follows the memory.
        const ast::Type* et = &m->type->getCanonicalType();
        while (et->isUnpackedArray())
            et = &et->getArrayElementType()->getCanonicalType();
        if (name.starts_with("$readmem") && et->isEnum())
            ops.push_back(Operand::typeRow(enumRow(et->as<ast::EnumType>(), sl)));
        for (std::size_t i = 2; i < args.size(); ++i)
            ops.push_back(coerce(lowerExpr(*args[i]), integral(true, true, 64), sl).op);
        if (a->home == Place::Home::Slot) {
            if (name.starts_with("$readmem"))
                noteWrite({a->slot, 0, 0, 0, 0});
            else
                noteRead({a->slot, 0, 0, 0, 0});
        }
        emitVoid(Op::Syscall, std::move(ops), sl);
        return;
    }
    if (name == "$fclose" || name == "$fflush") {
        // $fflush with no argument flushes every file (SYSTEM_TASKS.md 6.2).
        std::vector<Operand> ops{Operand::str(name)};
        if (!args.empty()) ops.push_back(lowerDescriptor(*args[0], sl).op);
        emitVoid(Op::Syscall, std::move(ops), sl);
        return;
    }
    if (name == "$fopen" || name == "$fgets" || name == "$fgetc" || name == "$ungetc" ||
        name == "$fread" || name == "$ftell" || name == "$fseek" || name == "$rewind" ||
        name == "$feof" || name == "$ferror" || name == "$random" || name.starts_with("$dist_") ||
        name == "$urandom" || name == "$urandom_range") {
        (void)lowerExpr(call);  // its value unused
        return;
    }
    if (name == "$monitoron" || name == "$monitoroff") {
        emitVoid(Op::Syscall, {Operand::str(name)}, sl);
        return;
    }
    if (name == "$stacktrace" && args.empty()) {
        // As a task it prints the chain of calls (SYSTEM_TASKS.md 10.3).
        emitVoid(Op::Syscall, {Operand::str(name), Operand::str(b_.scope)}, sl);
        return;
    }
    if (name == "$timeformat" && (args.empty() || args.size() == 4)) {
        // The unit, digits, suffix and minimum width of %t (SYSTEM_TASKS.md
        // 5.4); no arguments restore the defaults.
        if (!args.empty()) {
            const auto unit = constInt(*args[0]);
            if (unit && (*unit < -15 || *unit > 2))
                error(sl, "$timeformat unit must be between -15 and 2");
        }
        std::vector<Operand> ops{Operand::str(name)};
        for (std::size_t i = 0; i < args.size(); ++i) {
            const Val v = lowerExpr(*args[i]);
            ops.push_back((i == 2 ? coerce(v, Type{sir::TypeKind::String, 0}, sl)
                                  : coerce(v, sir::s2(32), sl))
                              .op);
        }
        emitVoid(Op::Syscall, std::move(ops), sl);
        return;
    }
    if (name == "$printtimescale") {
        // One line with the unit and precision of the caller's design
        // element, or of the instance named (SYSTEM_TASKS.md 5.3, 20.4.2).
        const ast::Symbol* at = nullptr;
        if (args.size() == 1 && args[0]->kind == ast::ExpressionKind::ArbitrarySymbol) {
            at = args[0]->as<ast::ArbitrarySymbolExpression>().symbol.get();
        } else if (args.empty()) {
            for (const ast::Symbol* s = evalSymbol_; s != nullptr && at == nullptr;
                 s = s->getParentScope() != nullptr ? &s->getParentScope()->asSymbol() : nullptr)
                if (s->kind == ast::SymbolKind::InstanceBody)
                    at = s->as<ast::InstanceBodySymbol>().parentInstance;
        }
        if (at == nullptr || at->kind != ast::SymbolKind::Instance)
            unsupported(sl, "$printtimescale here or of this argument");
        const sir::TimeScale ts = timeScaleOf(at->as<ast::InstanceSymbol>().body);
        const std::string line = "Time scale of (" + formatLiteral(at->getHierarchicalPath()) +
                                 ") is " + sir::timeExponentText(ts.unit) + " / " +
                                 sir::timeExponentText(ts.precision) + "\n";
        emitVoid(Op::Syscall, {Operand::str("$write"), Operand::str(line)}, sl);
        return;
    }
    if ((name == "$sformat" || base == "$swrite") && !args.empty()) {
        // $sformat(dest, format, ...) and $swrite(dest, ...): $sformatf
        // into dest (21.3.3); $swrite reads every string literal argument
        // not taken by a specifier as a format, as lowerFormatArgs does.
        // $swriteb, $swriteo and $swriteh keep their names for the radix.
        std::vector<Operand> ops{
            Operand::str(radix == 'd' || name == "$sformat" ? "$sformatf" : name)};
        if (name == "$sformat")
            lowerFormatArgs(args.subspan(1), ops, sl, 'd', name);
        else
            lowerFormatArgs(args.subspan(1), ops, sl, radix);
        const ast::Expression* dest = args[0];
        if (dest->kind == ast::ExpressionKind::Assignment &&
            dest->as<ast::AssignmentExpression>().isLValueArg())
            dest = &dest->as<ast::AssignmentExpression>().left();
        // Format into the destination's type: integral destinations retain
        // raw zero bytes, while SV strings cannot contain them (6.16).
        const Type target = typeOf(*dest->type, sl);
        const Val v = emit(Op::Syscall, target, std::move(ops), sl);
        lowerStore(*dest, v, false, sl);
        return;
    }
    if (name == "$sscanf" || name == "$fscanf" || name == "$value$plusargs") {
        (void)lowerExpr(call);  // its value unused
        return;
    }
    if (const std::optional<Val> v = lowerStringCall(call, sl)) return;  // putc and the like
    if (name == "$fatal" || name == "$error" || name == "$warning" || name == "$info") {
        // The message names the calling scope (SYSTEM_TASKS.md 10.1);
        // $fatal's first argument is the level of $finish (20.10).
        std::vector<Operand> ops{Operand::str(name), Operand::str(b_.scope)};
        std::span<const ast::Expression* const> message = args;
        if (name == "$fatal") {
            std::uint64_t n = 1;
            if (!args.empty()) {
                const std::optional<std::int64_t> v = constInt(*args[0]);
                if (!v || *v < 0 || *v > 2) unsupported(sl, name + " with this first argument");
                observeRuntimeConstant(*args[0]);
                n = static_cast<std::uint64_t>(*v);
                message = args.subspan(1);
            }
            ops.push_back(constant(sir::u2(32), n).op);
        }
        lowerFormatArgs(message, ops, sl);
        emitVoid(Op::Syscall, std::move(ops), sl);
        return;
    }
    if (name == "$dumpfile" && args.size() <= 1) {
        // The call as written, for the $version of the VCD, then the name
        // (WAVEFORM.md 3, 4).
        std::vector<Operand> ops{Operand::str(name), Operand::str(sourceText(call.sourceRange))};
        if (!args.empty())
            ops.push_back(coerce(lowerExpr(*args[0]), Type{sir::TypeKind::String, 0}, sl).op);
        emitVoid(Op::Syscall, std::move(ops), sl);
        return;
    }
    if (name == "$dumpvars") {
        // The levels, then each scope or variable as a row of the dump
        // table: "scope" or "var" and a constant that gets the row once
        // the table is made (SIR_SPEC.md 2). A variable of an unpacked
        // structure is a scope (21.7.5).
        std::vector<Operand> ops{Operand::str(name)};
        if (!args.empty()) ops.push_back(coerce(lowerExpr(*args[0]), sir::u2(32), sl).op);
        for (std::size_t i = 1; i < args.size(); ++i) {
            const ast::Expression& e = *args[i];
            const slang::SourceLocation at = e.sourceRange.start();
            const ast::Symbol* s = e.kind == ast::ExpressionKind::ArbitrarySymbol
                                       ? e.as<ast::ArbitrarySymbolExpression>().symbol.get()
                                       : namedSymbol(e);
            if (s == nullptr) unsupported(at, "$dumpvars of this argument");
            // WAVEFORM 7 does not yet define class scopes or their variables.
            if (s->kind == ast::SymbolKind::ClassProperty || s->kind == ast::SymbolKind::ClassType)
                unsupported(at, "$dumpvars of a class scope or property");
            const bool value = s->kind == ast::SymbolKind::Variable ||
                               s->kind == ast::SymbolKind::Net ||
                               s->kind == ast::SymbolKind::FormalArgument;
            const bool scope =
                !value || unpackedStruct(s->as<ast::ValueSymbol>().getType()) != nullptr;
            const Val row = constant(sir::u2(32), 0);
            dumpRefs_.push_back({row.op.asConst(), s, scope});
            ops.push_back(Operand::str(scope ? "scope" : "var"));
            ops.push_back(row.op);
        }
        emitVoid(Op::Syscall, std::move(ops), sl);
        return;
    }
    if ((name == "$dumpoff" || name == "$dumpon" || name == "$dumpall" || name == "$dumpflush") &&
        args.empty()) {
        emitVoid(Op::Syscall, {Operand::str(name)}, sl);
        return;
    }
    if (name == "$dumplimit" && args.size() == 1) {
        emitVoid(Op::Syscall, {Operand::str(name), coerce(lowerExpr(*args[0]), sir::u2(64), sl).op},
                 sl);
        return;
    }
    if (name == "$exit") {
        emitVoid(Op::Syscall, {Operand::str(name)}, sl);
        return;
    }
    if (name == "$finish" || name == "$stop") {
        std::uint64_t n = 1;  // the default argument (20.2)
        if (!args.empty()) {
            const std::optional<std::int64_t> v = constInt(*args[0]);
            if (!v || *v < 0 || *v > 2) unsupported(sl, name + " with this argument");
            observeRuntimeConstant(*args[0]);
            n = static_cast<std::uint64_t>(*v);
        }
        emitVoid(Op::Syscall, {Operand::str(name), constant(sir::u2(32), n).op}, sl);
        return;
    }
    unsupported(sl, "system task " + name);
}

}  // namespace kirimasim::front
