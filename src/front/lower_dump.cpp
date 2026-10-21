// The dump table (SIR_SPEC.md 2, WAVEFORM.md 2): the scopes and variables
// a waveform can name, made from slang's model once the design is lowered,
// and the rows the arguments of $dumpvars name.
//
// Scopes come depth first in the order of elaboration: module instances,
// named begin and fork blocks, named generate blocks (as begin), tasks
// and functions with static variables, and unpacked structure variables
// (as fork). In each, its nets and static variables come first, in the
// order of declaration, then the scopes in it. Unpacked arrays, automatic
// variables, dynamic data and parameters are not in it; nor is a variable
// that was given no slot, as nothing reads or writes it.

#include <cstdint>
#include <optional>
#include <string>

#include "lowerer.h"
#include "slang/ast/symbols/BlockSymbols.h"
#include "slang/ast/symbols/CompilationUnitSymbols.h"
#include "slang/ast/symbols/MemberSymbols.h"
#include "slang/ast/symbols/SubroutineSymbols.h"
#include "slang/ast/types/AllTypes.h"
#include "slang/ast/types/NetType.h"

namespace kirimasim::front {

namespace {

/// A variable's VCD type, size and range (WAVEFORM.md 2, Table 21-11).
struct DumpShape {
    sir::DumpType type = sir::DumpType::Reg;
    std::uint32_t width = 1;
    std::string range;
};

/// The shape of a value of type `type`, or nothing for the types a VCD has
/// none for.
std::optional<DumpShape> dumpShape(const ast::Type& type) {
    const ast::Type& t = type.getCanonicalType();
    if (t.isFloating()) {
        const bool rt = t.as<ast::FloatingType>().floatKind == ast::FloatingType::RealTime;
        return DumpShape{rt ? sir::DumpType::Realtime : sir::DumpType::Real, 64, {}};
    }
    if (t.isEvent()) return DumpShape{sir::DumpType::Event, 1, {}};
    if (!t.isIntegral()) return std::nullopt;
    DumpShape s;
    s.width = static_cast<std::uint32_t>(t.getBitWidth());
    // int and integer are integer, time is time; an enum takes its base
    // type's; byte, shortint, longint and the rest are reg.
    const ast::Type& base = t.isEnum() ? t.as<ast::EnumType>().baseType.getCanonicalType() : t;
    if (base.kind == ast::SymbolKind::PredefinedIntegerType) {
        switch (base.as<ast::PredefinedIntegerType>().integerKind) {
            case ast::PredefinedIntegerType::Int:
            case ast::PredefinedIntegerType::Integer: s.type = sir::DumpType::Integer; break;
            case ast::PredefinedIntegerType::Time: s.type = sir::DumpType::Time; break;
            default: break;
        }
    }
    // One packed range of bits as declared; any other type wider than a
    // bit, [width-1:0].
    if (t.kind == ast::SymbolKind::PackedArrayType) {
        const auto& p = t.as<ast::PackedArrayType>();
        if (p.elementType.getCanonicalType().kind == ast::SymbolKind::ScalarType)
            s.range =
                "[" + std::to_string(p.range.left) + ":" + std::to_string(p.range.right) + "]";
    }
    if (s.range.empty() && s.width > 1) s.range = "[" + std::to_string(s.width - 1) + ":0]";
    return s;
}

/// The VCD type of a net (21.7.2.3: uwire is wire), or nothing for the
/// kinds that have no slot.
std::optional<sir::DumpType> netDumpType(const ast::NetSymbol& n) {
    switch (n.netType.netKind) {
        case ast::NetType::Wire:
        case ast::NetType::UWire: return sir::DumpType::Wire;
        case ast::NetType::Tri: return sir::DumpType::Tri;
        case ast::NetType::WAnd: return sir::DumpType::Wand;
        case ast::NetType::TriAnd: return sir::DumpType::Triand;
        case ast::NetType::WOr: return sir::DumpType::Wor;
        case ast::NetType::TriOr: return sir::DumpType::Trior;
        case ast::NetType::Tri0: return sir::DumpType::Tri0;
        case ast::NetType::Tri1: return sir::DumpType::Tri1;
        case ast::NetType::Supply0: return sir::DumpType::Supply0;
        case ast::NetType::Supply1: return sir::DumpType::Supply1;
        default: return std::nullopt;
    }
}

bool isValueMember(const ast::Symbol& m) {
    return m.kind == ast::SymbolKind::Variable || m.kind == ast::SymbolKind::Net ||
           m.kind == ast::SymbolKind::FormalArgument;
}

}  // namespace

void Lowerer::buildDumpTable() {
    for (const ast::InstanceSymbol* top : root_->topInstances)
        dumpInstance(*top, sir::kNone);
    // The rows the arguments of $dumpvars name (SIR_SPEC.md 2, 8); kNone
    // for what is not dumped, an unpacked array for one (21.7.5), which
    // adds nothing. slang takes only module instances and variables there.
    for (const DumpRef& r : dumpRefs_) {
        const auto& rows = r.scope ? dumpScopeRows_ : dumpVarRows_;
        const auto it = rows.find(r.symbol);
        design_.constant(r.constant).aWords[0] = it == rows.end() ? sir::kNone : it->second;
    }
}

std::uint32_t Lowerer::addDumpScope(const ast::Symbol& s, std::string path, sir::DumpScopeKind kind,
                                    std::uint32_t parent) {
    // Its own name: the end of its path, with the index of an element of
    // an array of instances or generate blocks.
    std::string name(s.name);
    if (parent != sir::kNone) {
        const std::string& outer = design_.dumpScopes[parent].path;
        if (path.size() > outer.size() + 1 && path.starts_with(outer) && path[outer.size()] == '.')
            name = path.substr(outer.size() + 1);
    } else {
        name = path;
    }
    const auto row = static_cast<std::uint32_t>(design_.dumpScopes.size());
    design_.dumpScopes.push_back({std::move(name), std::move(path), kind, parent});
    dumpScopeRows_.emplace(&s, row);
    return row;
}

void Lowerer::dumpInstance(const ast::InstanceSymbol& inst, std::uint32_t parent) {
    // Interfaces and programs come later (WAVEFORM.md 7).
    if (inst.getDefinition().definitionKind != ast::DefinitionKind::Module ||
        !instanceIds_.contains(&inst))
        return;
    const std::uint32_t row =
        addDumpScope(inst, inst.getHierarchicalPath(), sir::DumpScopeKind::Module, parent);
    dumpMembers(inst.body, row, true);
}

void Lowerer::dumpMembers(const ast::Scope& scope, std::uint32_t row, bool named) {
    // The nets and variables first, then the scopes; an unnamed block is
    // no scope, and its variables have no hierarchical name.
    if (named)
        for (const ast::Symbol& m : scope.members())
            if (isValueMember(m) && unpackedStruct(m.as<ast::ValueSymbol>().getType()) == nullptr)
                dumpVariable(m.as<ast::ValueSymbol>(), row);
    const auto instances = [&](const ast::InstanceArraySymbol& a, const auto& self) -> void {
        for (const ast::Symbol* e : a.elements) {
            if (e->kind == ast::SymbolKind::InstanceArray)
                self(e->as<ast::InstanceArraySymbol>(), self);
            else if (e->kind == ast::SymbolKind::Instance)
                dumpInstance(e->as<ast::InstanceSymbol>(), row);
        }
    };
    for (const ast::Symbol& m : scope.members()) {
        switch (m.kind) {
            case ast::SymbolKind::Variable:
                if (named && unpackedStruct(m.as<ast::VariableSymbol>().getType()) != nullptr)
                    dumpStruct(m.as<ast::VariableSymbol>(), row);
                break;
            case ast::SymbolKind::Instance: dumpInstance(m.as<ast::InstanceSymbol>(), row); break;
            case ast::SymbolKind::InstanceArray:
                instances(m.as<ast::InstanceArraySymbol>(), instances);
                break;
            case ast::SymbolKind::GenerateBlock: {
                const auto& g = m.as<ast::GenerateBlockSymbol>();
                if (g.isUninstantiated) break;
                dumpMembers(
                    g, addDumpScope(g, g.getHierarchicalPath(), sir::DumpScopeKind::Begin, row),
                    true);
                break;
            }
            case ast::SymbolKind::GenerateBlockArray:
                for (const ast::GenerateBlockSymbol* g :
                     m.as<ast::GenerateBlockArraySymbol>().entries)
                    if (!g->isUninstantiated)
                        dumpMembers(*g,
                                    addDumpScope(*g, g->getHierarchicalPath(),
                                                 sir::DumpScopeKind::Begin, row),
                                    true);
                break;
            case ast::SymbolKind::StatementBlock: {
                const auto& b = m.as<ast::StatementBlockSymbol>();
                if (b.name.empty()) {
                    dumpMembers(b, row, false);
                    break;
                }
                const auto kind = b.blockKind == ast::StatementBlockKind::Sequential
                                      ? sir::DumpScopeKind::Begin
                                      : sir::DumpScopeKind::Fork;
                dumpMembers(b, addDumpScope(b, b.getHierarchicalPath(), kind, row), true);
                break;
            }
            case ast::SymbolKind::Subroutine: {
                // A task or function is a scope when it holds a static
                // variable (WAVEFORM.md 2).
                const auto& s = m.as<ast::SubroutineSymbol>();
                const std::size_t scopes = design_.dumpScopes.size();
                const std::size_t vars = design_.dumpVars.size();
                const auto kind = s.subroutineKind == ast::SubroutineKind::Task
                                      ? sir::DumpScopeKind::Task
                                      : sir::DumpScopeKind::Function;
                dumpMembers(s, addDumpScope(s, s.getHierarchicalPath(), kind, row), true);
                if (design_.dumpVars.size() != vars) break;
                design_.dumpScopes.resize(scopes);
                std::erase_if(dumpScopeRows_, [&](const auto& e) { return e.second >= scopes; });
                break;
            }
            default: break;
        }
    }
}

void Lowerer::dumpVariable(const ast::ValueSymbol& v, std::uint32_t row) {
    const auto it = slots_.find(&v);
    if (!inSlot(v) || it == slots_.end() || !design_.slot(it->second).dims.empty()) return;
    std::optional<DumpShape> shape = dumpShape(v.getType());
    if (!shape) return;
    if (v.kind == ast::SymbolKind::Net) {
        const std::optional<sir::DumpType> t = netDumpType(v.as<ast::NetSymbol>());
        if (!t) return;
        shape->type = *t;
    }
    dumpVarRows_.emplace(&v, static_cast<std::uint32_t>(design_.dumpVars.size()));
    design_.dumpVars.push_back(
        {row, std::string(v.name), std::move(shape->range), shape->type, shape->width, it->second});
}

void Lowerer::dumpStruct(const ast::VariableSymbol& v, std::uint32_t row) {
    // A fork scope with the members as its variables (21.7.5).
    const auto it = structSlots_.find(&v);
    if (!inSlot(v) || it == structSlots_.end()) return;
    const std::string path = v.getHierarchicalPath();
    dumpFields(*unpackedStruct(v.getType()), it->second,
               addDumpScope(v, path, sir::DumpScopeKind::Fork, row));
}

void Lowerer::dumpFields(const ast::UnpackedStructType& st, sir::SlotId first, std::uint32_t row) {
    for (const ast::FieldSymbol* f : st.fields) {
        const sir::SlotId s{first.index() + leafOffset(st, *f)};
        std::optional<DumpShape> shape = dumpShape(f->getType());
        if (unpackedStruct(f->getType()) != nullptr || !shape || !design_.slot(s).dims.empty())
            continue;
        design_.dumpVars.push_back(
            {row, std::string(f->name), std::move(shape->range), shape->type, shape->width, s});
    }
    for (const ast::FieldSymbol* f : st.fields)
        if (const ast::UnpackedStructType* inner = unpackedStruct(f->getType()))
            dumpFields(*inner, sir::SlotId{first.index() + leafOffset(st, *f)},
                       addDumpScope(*f, design_.dumpScopes[row].path + "." + std::string(f->name),
                                    sir::DumpScopeKind::Fork, row));
}

}  // namespace kirimasim::front
