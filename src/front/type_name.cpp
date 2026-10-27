// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The strings of $typename (SYSTEM_TASKS.md 8, 20.6.1), built from slang's
// types.

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "lowerer.h"
#include "slang/ast/ASTVisitor.h"
#include "slang/ast/symbols/CompilationUnitSymbols.h"
#include "slang/ast/types/AllTypes.h"

namespace kirimasim::front {

namespace {

bool isUserDefined(const ast::Type& t) {
    switch (t.kind) {
        case ast::SymbolKind::EnumType:
        case ast::SymbolKind::PackedStructType:
        case ast::SymbolKind::UnpackedStructType:
        case ast::SymbolKind::PackedUnionType:
        case ast::SymbolKind::UnpackedUnionType: return true;
        default: return false;
    }
}

/// The namespace an anonymous type is declared in: the first scope around
/// it that is not a type.
const ast::Scope* namespaceOf(const ast::Type& t) {
    const ast::Scope* s = t.getParentScope();
    while (s != nullptr && s->asSymbol().isType())
        s = s->asSymbol().getParentScope();
    return s;
}

/// What a user-defined type's name is prefixed with: its package or
/// compilation unit, or the hierarchical name of its scope.
std::string prefixOf(const ast::Scope& ns) {
    const ast::Symbol& s = ns.asSymbol();
    if (s.kind == ast::SymbolKind::Package) return std::string(s.name) + "::";
    if (s.kind == ast::SymbolKind::CompilationUnit) return "$unit::";
    if (s.kind == ast::SymbolKind::InstanceBody) {
        const ast::InstanceSymbol* inst = s.as<ast::InstanceBodySymbol>().parentInstance;
        if (inst != nullptr) return inst->getHierarchicalPath() + ".";
    }
    return s.getHierarchicalPath() + ".";
}

/// An enumeration constant's value as a sized decimal, or binary with x
/// or z bits: 32'sd0.
std::string enumValueText(const slang::SVInt& v) {
    std::string text = std::to_string(v.getBitWidth()) + "'";
    if (v.isSigned()) text += 's';
    if (v.hasUnknown()) return text + "b" + v.toString(slang::LiteralBase::Binary, false);
    return text + "d" + v.toString(slang::LiteralBase::Decimal, false);
}

}  // namespace

const ast::TypeAliasType* Lowerer::aliasDeclaring(const ast::Type& t) {
    if (!aliasesScanned_) {
        // The typedefs whose type is a structure, union or enumeration
        // written there: they name it (20.6.1).
        aliasesScanned_ = true;
        auto visitor = ast::makeVisitor([&](auto& v, const ast::TypeAliasType& a) {
            const ast::Type& target = a.targetType.getType();
            if (!target.isAlias() && isUserDefined(target)) typeAliases_.emplace(&target, &a);
            v.visitDefault(a);
        });
        root_->visit(visitor);
    }
    const auto it = typeAliases_.find(&t);
    return it == typeAliases_.end() ? nullptr : it->second;
}

std::string Lowerer::anonymousName(const ast::Type& t) {
    // e$1, s$1 and u$1: by kind, in the order the anonymous types appear in
    // their namespace (SYSTEM_TASKS.md 8).
    const ast::Scope* ns = namespaceOf(t);
    if (ns == nullptr) return "$";
    auto [it, fresh] = anonymous_.try_emplace(ns);
    std::vector<const ast::Type*>& list = it->second;
    if (fresh) {
        const auto add = [&](const ast::Type& type, auto& self) -> void {
            if (type.isAlias()) return;  // its typedef lists what is inside it
            const ast::Type& c = type.getCanonicalType();
            if (isUserDefined(c) && aliasDeclaring(c) == nullptr &&
                std::find(list.begin(), list.end(), &c) == list.end())
                list.push_back(&c);
            if (c.isStruct() || c.isUnion()) {
                for (const ast::Symbol& m : c.as<ast::Scope>().members())
                    if (m.kind == ast::SymbolKind::Field)
                        self(m.as<ast::FieldSymbol>().getType(), self);
            } else if (c.isArray()) {
                if (const ast::Type* e = c.getArrayElementType()) self(*e, self);
            }
        };
        for (const ast::Symbol& m : ns->members()) {
            if (m.kind == ast::SymbolKind::TypeAlias) {
                const ast::Type& target = m.as<ast::TypeAliasType>().targetType.getType();
                if (!target.isAlias() && (target.isStruct() || target.isUnion())) {
                    for (const ast::Symbol& f : target.as<ast::Scope>().members())
                        if (f.kind == ast::SymbolKind::Field)
                            add(f.as<ast::FieldSymbol>().getType(), add);
                } else {
                    add(target, add);
                }
            } else if (m.isValue()) {
                add(m.as<ast::ValueSymbol>().getType(), add);
            }
        }
    }
    const char letter = t.kind == ast::SymbolKind::EnumType ? 'e' : t.isStruct() ? 's' : 'u';
    std::uint32_t n = 0;
    for (const ast::Type* a : list) {
        const char k = a->kind == ast::SymbolKind::EnumType ? 'e' : a->isStruct() ? 's' : 'u';
        if (k == letter) ++n;
        if (a == &t) return prefixOf(*ns) + letter + "$" + std::to_string(n);
    }
    return prefixOf(*ns) + letter + "$?";
}

std::string Lowerer::typeName(const ast::Type& type, slang::SourceLocation sl) {
    // a) A typedef resolves to the type it names; a structure, union or
    // enumeration keeps the name of the typedef that declares it.
    const ast::Type& t = type.getCanonicalType();
    if (isUserDefined(t)) {
        std::string body;
        if (t.kind == ast::SymbolKind::EnumType) {
            const auto& e = t.as<ast::EnumType>();
            body = "enum";
            // The base type, unless it is the default int.
            const ast::Type& base = e.baseType.getCanonicalType();
            if (!(base.kind == ast::SymbolKind::PredefinedIntegerType &&
                  base.as<ast::PredefinedIntegerType>().integerKind ==
                      ast::PredefinedIntegerType::Int &&
                  base.isSigned()))
                body += " " + typeName(e.baseType, sl);
            body += '{';
            bool first = true;
            for (const ast::EnumValueSymbol& v : e.values()) {
                if (!first) body += ',';
                first = false;
                body += std::string(v.name) + "=" + enumValueText(v.getValue().integer());
            }
            body += '}';
        } else {
            body = t.isStruct() ? "struct" : "union";
            if (t.kind == ast::SymbolKind::PackedUnionType && t.as<ast::PackedUnionType>().isTagged)
                body += " tagged";
            if (t.kind == ast::SymbolKind::UnpackedUnionType &&
                t.as<ast::UnpackedUnionType>().isTagged)
                body += " tagged";
            if (t.isIntegral()) body += t.isSigned() ? " packed signed" : " packed";
            body += '{';
            for (const ast::Symbol& m : t.as<ast::Scope>().members())
                if (m.kind == ast::SymbolKind::Field)
                    body += typeName(m.as<ast::FieldSymbol>().getType(), sl) + " " +
                            std::string(m.name) + ";";
            body += '}';
        }
        if (const ast::TypeAliasType* a = aliasDeclaring(t))
            return body + prefixOf(*a->getParentScope()) + std::string(a->name);
        return body + anonymousName(t);
    }
    switch (t.kind) {
        case ast::SymbolKind::ScalarType: {
            const auto& s = t.as<ast::ScalarType>();
            const char* k = s.scalarKind == ast::ScalarType::Bit     ? "bit"
                            : s.scalarKind == ast::ScalarType::Logic ? "logic"
                                                                     : "reg";
            return std::string(k) + (s.isSigned ? " signed" : "");
        }
        case ast::SymbolKind::PredefinedIntegerType: {
            // b) The default signing is left out.
            const auto& p = t.as<ast::PredefinedIntegerType>();
            static constexpr const char* kNames[] = {"shortint", "int",     "longint",
                                                     "byte",     "integer", "time"};
            std::string name = kNames[p.integerKind];
            if (p.isSigned != ast::PredefinedIntegerType::isDefaultSigned(p.integerKind))
                name += p.isSigned ? " signed" : " unsigned";
            return name;
        }
        case ast::SymbolKind::FloatingType: {
            const auto& f = t.as<ast::FloatingType>();
            return f.floatKind == ast::FloatingType::Real        ? "real"
                   : f.floatKind == ast::FloatingType::ShortReal ? "shortreal"
                                                                 : "realtime";
        }
        case ast::SymbolKind::StringType: return "string";
        case ast::SymbolKind::CHandleType: return "chandle";
        case ast::SymbolKind::EventType: return "event";
        case ast::SymbolKind::VoidType: return "void";
        case ast::SymbolKind::PackedArrayType: {
            // g) Ranges in unsized decimal; one space after the element's
            // name (SYSTEM_TASKS.md 8). The signing is the array's.
            std::string ranges;
            const ast::Type* e = &t;
            while (e->kind == ast::SymbolKind::PackedArrayType) {
                const auto& a = e->as<ast::PackedArrayType>();
                ranges +=
                    "[" + std::to_string(a.range.left) + ":" + std::to_string(a.range.right) + "]";
                e = &a.elementType.getCanonicalType();
            }
            std::string elem;
            if (e->kind == ast::SymbolKind::ScalarType) {
                const auto& s = e->as<ast::ScalarType>();
                elem = s.scalarKind == ast::ScalarType::Bit     ? "bit"
                       : s.scalarKind == ast::ScalarType::Logic ? "logic"
                                                                : "reg";
                if (t.isSigned()) elem += " signed";
            } else {
                elem = typeName(*e, sl);
            }
            return elem + " " + ranges;
        }
        case ast::SymbolKind::FixedSizeUnpackedArrayType:
        case ast::SymbolKind::DynamicArrayType:
        case ast::SymbolKind::QueueType:
        case ast::SymbolKind::AssociativeArrayType: {
            // d) "$" stands for the array's name, then its dimensions.
            std::string dims;
            const ast::Type* e = &t;
            for (bool more = true; more;) {
                switch (e->kind) {
                    case ast::SymbolKind::FixedSizeUnpackedArrayType: {
                        const auto& a = e->as<ast::FixedSizeUnpackedArrayType>();
                        dims += "[" + std::to_string(a.range.left) + ":" +
                                std::to_string(a.range.right) + "]";
                        e = &a.elementType.getCanonicalType();
                        break;
                    }
                    case ast::SymbolKind::DynamicArrayType:
                        dims += "[]";
                        e = &e->as<ast::DynamicArrayType>().elementType.getCanonicalType();
                        break;
                    case ast::SymbolKind::QueueType: {
                        const auto& q = e->as<ast::QueueType>();
                        dims += q.maxBound == 0 ? "[$]" : "[$:" + std::to_string(q.maxBound) + "]";
                        e = &q.elementType.getCanonicalType();
                        break;
                    }
                    case ast::SymbolKind::AssociativeArrayType: {
                        const auto& a = e->as<ast::AssociativeArrayType>();
                        dims += "[" +
                                (a.indexType != nullptr ? typeName(*a.indexType, sl)
                                                        : std::string("*")) +
                                "]";
                        e = &a.elementType.getCanonicalType();
                        break;
                    }
                    default: more = false; break;
                }
            }
            return typeName(*e, sl) + "$" + dims;
        }
        default: unsupported(sl, "$typename of type '" + type.toString() + "'");
    }
}

}  // namespace kirimasim::front
