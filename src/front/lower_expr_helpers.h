// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Helpers shared by the lower_*.cpp files that were one file, lower_expr.cpp.
// Private to src/front/.

#pragma once

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

using sir::Op;
using sir::Operand;
using sir::Type;

/// An integral type of the given domain, signedness and width.
inline Type integral(bool four, bool isSigned, std::uint32_t w) {
    if (four) return isSigned ? sir::s4(w) : sir::u4(w);
    return isSigned ? sir::s2(w) : sir::u2(w);
}
inline constexpr std::string_view kRealMath2[] = {"$pow", "$atan2", "$hypot"};

inline bool among(std::string_view n, std::span<const std::string_view> set) {
    return std::find(set.begin(), set.end(), n) != set.end();
}

inline bool isString(Type t) { return t.kind == sir::TypeKind::String; }

/// The bytes of a known integral constant from the most significant one,
/// without the zero bytes, as a string (6.16); nothing if it has x or z.
inline std::optional<std::string> stringBytes(const slang::SVInt& v) {
    if (v.hasUnknown()) return std::nullopt;
    const std::uint32_t n = sir::integralBytes(v.getBitWidth());
    const slang::SVInt x = v.getBitWidth() < n * 8 ? v.zext(n * 8) : v;
    std::string out;
    for (std::uint32_t k = n; k-- > 0;) {
        const std::optional<std::uint64_t> byte =
            x.slice(static_cast<std::int32_t>(8 * k + 7), static_cast<std::int32_t>(8 * k))
                .as<std::uint64_t>();
        if (byte && *byte != 0) out.push_back(static_cast<char>(*byte));
    }
    return out;
}

inline bool isReal(Type t) {
    return t.kind == sir::TypeKind::Real || t.kind == sir::TypeKind::ShortReal;
}

/// The position from the least significant bit of declared index `idx` in
/// a range, or nothing if it is outside.
inline std::optional<std::int64_t> positionOf(std::int64_t idx, const slang::ConstantRange& r) {
    const std::int64_t pos = r.left >= r.right ? idx - r.right : r.right - idx;
    const std::int64_t width =
        std::int64_t{r.left >= r.right ? r.left - r.right : r.right - r.left} + 1;
    if (pos < 0 || pos >= width) return std::nullopt;
    return pos;
}

/// The specifiers of a $display format, in order, with %x as h. Returns
/// false and what is wrong in `bad`: "%" for a % with no conversion after
/// it, else a specifier or a flag that is not supported yet. `extra` gets
/// the first flag that the standard does not define, a - or a 0 in a
/// format other than %e, %f and %g, as "'-' with %d" (SYSTEM_TASKS.md
/// 3.12).
inline bool scanFormat(std::string_view f, std::vector<char>& convs, std::string& bad,
                       std::string& extra) {
    const auto digit = [&](std::size_t i) { return i < f.size() && f[i] >= '0' && f[i] <= '9'; };
    for (std::size_t i = 0; i < f.size(); ++i) {
        if (f[i] != '%') continue;
        if (++i >= f.size()) {
            bad = "%";
            return false;
        }
        if (f[i] == '%') continue;
        std::string flags;
        while (i < f.size() && std::string_view("-+ #").find(f[i]) != std::string_view::npos)
            flags += f[i++];
        // A width that starts with 0 and is not 0 is the 0 flag; %0d is the
        // width 0 (3.3).
        const bool leadingZero = i < f.size() && f[i] == '0';
        bool zeroFlag = false;
        bool nonzeroWidth = false;
        for (; digit(i); ++i) {
            zeroFlag = zeroFlag || (leadingZero && f[i] != '0');
            nonzeroWidth = nonzeroWidth || f[i] != '0';
        }
        const bool precision = i < f.size() && f[i] == '.';
        if (precision)
            for (++i; digit(i); ++i) {
            }
        if (i >= f.size()) {
            bad = "%";
            return false;
        }
        auto conv = static_cast<char>(std::tolower(static_cast<unsigned char>(f[i])));
        if (conv == 'x') conv = 'h';
        if (conv == 'm' || conv == 'l') continue;  // take no argument (withScope)
        if (std::string_view("bohdcstefguzpv").find(conv) == std::string_view::npos) {
            bad = std::string("format specifier %") + f[i];
            return false;
        }
        if (conv == 'p') {
            if (!flags.empty() || precision || nonzeroWidth) {
                bad = "assignment-pattern format other than %p or %0p";
                return false;
            }
            // Internal P records positional mode, independent of the
            // source conversion letter's case.
            convs.push_back(leadingZero ? 'P' : 'p');
            continue;
        }
        // The real formats take C's flags and a precision (3.6).
        if (std::string_view("efg").find(conv) == std::string_view::npos) {
            const std::string with = std::string(" with %") + f[i];
            for (const char c : flags)
                if (c != '-') {
                    bad = std::string("format flag '") + c + "'" + with;
                    return false;
                }
            if (precision) {
                bad = "format precision" + with;
                return false;
            }
            if (extra.empty() && flags.find('-') != std::string::npos) extra = "'-'" + with;
            if (extra.empty() && zeroFlag) extra = "'0'" + with;
        }
        convs.push_back(conv);
    }
    return true;
}

inline bool usesPatternFormat(std::span<const ast::Expression* const> args) {
    std::deque<char> pending;
    for (const auto* argument : args) {
        if (argument->kind == ast::ExpressionKind::StringLiteral && pending.empty()) {
            std::vector<char> conversions;
            std::string bad, extra;
            if (scanFormat(argument->as<ast::StringLiteral>().getValue(), conversions, bad,
                           extra)) {
                for (const char conversion : conversions)
                    if (conversion == 'p' || conversion == 'P' || conversion == 'v') return true;
                pending.assign(conversions.begin(), conversions.end());
            }
        } else if (!pending.empty() && argument->kind != ast::ExpressionKind::EmptyArgument) {
            pending.pop_front();
        }
    }
    return false;
}

/// A format with each %m replaced by the hierarchical name of the scope
/// that calls the task, and each %l by the library and cell of its design
/// element (SYSTEM_TASKS.md 3.8).
inline std::string withScope(std::string_view f, std::string_view scope, std::string_view cell) {
    std::string out;
    for (std::size_t i = 0; i < f.size(); ++i) {
        if (f[i] != '%' || i + 1 >= f.size()) {
            out.push_back(f[i]);
            continue;
        }
        std::size_t j = i + 1;
        while (j < f.size() &&
               std::string_view("-+ #0123456789.").find(f[j]) != std::string_view::npos)
            ++j;
        if (j < f.size() && (f[j] == 'm' || f[j] == 'M')) {
            out += scope;
            i = j;
        } else if (j < f.size() && (f[j] == 'l' || f[j] == 'L')) {
            out += cell;
            i = j;
        } else if (j < f.size() && (f[j] == 'p' || f[j] == 'P')) {
            // The matching operand is rendered from typed metadata at
            // argument evaluation; the ordinary output path writes it.
            out.append(f.substr(i, j - i));
            out += 's';
            i = j;
        } else {
            // %% and any other specifier stay as they are.
            const std::size_t end = j < f.size() ? j : f.size() - 1;
            out.append(f.substr(i, end - i + 1));
            i = end;
        }
    }
    return out;
}

/// Whether a call is the built-in method of a type of kind k (the string
/// methods of 6.16, the enumeration methods of 6.19.5), and not a system
/// task or function, whatever its first argument: slang gives the object
/// of a method as the first argument, and a system function may take a
/// string or an enumeration there too.
inline bool isBuiltinMethod(const ast::CallExpression& call, ast::SymbolKind k) {
    if (!call.isSystemCall()) return false;
    const auto& info = std::get<ast::CallExpression::SystemCallInfo>(call.subroutine);
    return info.subroutine.get() ==
           info.scope->getCompilation().getSystemMethod(k, call.getSubroutineName());
}

}  // namespace kirimasim::front
