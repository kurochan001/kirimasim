// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "lowerer.h"
#include "slang/ast/expressions/SelectExpressions.h"
#include "slang/ast/symbols/InstanceSymbols.h"
#include "slang/ast/types/AllTypes.h"
#include <algorithm>

namespace kirimasim::front {
sir::Type Lowerer::interfaceType(const ast::Type& type, slang::SourceLocation sl) {
    const auto& vi = type.getCanonicalType().as<ast::VirtualInterfaceType>();
    if (vi.modport) unsupported(sl, "modport-qualified virtual interface handle");
    const auto& body = vi.iface.body;
    for (std::uint32_t i = 0; i < interfaceBodies_.size(); ++i)
        if (body.hasSameType(*interfaceBodies_[i]))
            return {sir::TypeKind::VirtualInterface, 0, sir::TypeId{i}};
    const auto id = sir::TypeId{static_cast<std::uint32_t>(interfaceBodies_.size())};
    interfaceBodies_.push_back(&body);
    design_.interfaces.push_back({std::string(body.getDefinition().name), type.toString()});
    if (instancesDeclared_) bindInterfaceInstances();
    return {sir::TypeKind::VirtualInterface, 0, id};
}

void Lowerer::bindInterfaceInstances() {
    for (std::size_t i = 0; i < interfaceBodies_.size(); ++i) {
        auto& ids = design_.interfaces[i].instances;
        ids.clear();
        for (std::uint32_t j = 0; j < instanceSyms_.size(); ++j)
            if (const auto* inst = instanceSyms_[j]->as_if<ast::InstanceSymbol>();
                inst && inst->body.hasSameType(*interfaceBodies_[i]))
                ids.push_back(sir::InstanceId{j});
    }
}

Lowerer::Place Lowerer::interfaceMember(const ast::MemberAccessExpression& member, bool writable,
                                        slang::SourceLocation sl) {
    if (!member.type->isIntegral() || (member.member.kind != ast::SymbolKind::Variable &&
                                       member.member.kind != ast::SymbolKind::Net))
        unsupported(sl, "virtual interface member of this type");
    const auto receiver = lowerExpr(member.value());
    const auto type = typeOf(*member.type, sl);
    auto& iface = design_.interfaces.at(receiver.type.detail.index());
    auto it = std::ranges::find(iface.members, member.member.name, &sir::InterfaceMember::name);
    if (it == iface.members.end()) {
        sir::InterfaceMember m{std::string(member.member.name), type, {}};
        for (const auto id : iface.instances) {
            const auto& body = instanceSyms_[id.index()]->as<ast::InstanceSymbol>().body;
            const auto* decl = body.find(m.name);
            if (!decl || !decl->isValue()) unsupported(sl, "unbound virtual interface member");
            m.slots.push_back(slotOf(decl->as<ast::ValueSymbol>(), sl));
        }
        iface.members.push_back(std::move(m));
        it = std::prev(iface.members.end());
    }
    Place p;
    p.kind = Place::Kind::Ref;
    p.home = Place::Home::Ref;
    p.frame = newFrameVar(sir::refType());
    p.type = type;
    p.virtualMember = true;
    frameVar(p.frame).target = type;
    const auto join = newBlock();
    for (std::size_t i = 0; i < it->slots.size(); ++i) {
        const auto slot = it->slots[i];
        const auto concrete =
            constant(receiver.type, std::uint64_t{iface.instances[i].index()} + 1);
        const auto matches = emit(sir::Op::Eq, sir::u2(1), {receiver.op, concrete.op}, sl);
        const auto take = newBlock(), next = newBlock();
        terminate(sir::Op::BrIf, {matches.op, sir::Operand::block(take), sir::Operand::block(next)},
                  sl);
        setBlock(take);
        const auto ref = referencePlace(placeOfSlot(slot), !writable, sl);
        emitVoid(sir::Op::Fstore, {sir::Operand::frame(p.frame), ref.op}, sl);
        jump(join, sl);
        setBlock(next);
    }
    const auto failure = newBlock();
    jump(failure, sl);
    setBlock(failure);
    emitVoid(sir::Op::Syscall,
             {sir::Operand::str("$fatal"), sir::Operand::str(b_.scope), constant(sir::u2(32), 1).op,
              sir::Operand::str("null virtual interface")},
             sl);
    jump(failure, sl);
    setBlock(join);
    return p;
}
}  // namespace kirimasim::front
