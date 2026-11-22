// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/analysis/values.h"

#include <algorithm>
#include <queue>
#include <sstream>

namespace kirimasim::analysis {
namespace {
using sir::Op;
using sir::Operand;
using Proof = NoXProof;

bool declared(sir::Type type) { return sir::isIntegral(type) && !sir::isFourState(type); }
bool knownConstant(const sir::Constant& c) {
    return sir::isIntegral(c.type) &&
           std::ranges::all_of(c.bWords, [](auto word) { return word == 0; });
}
bool propagates(Op op) {
    switch (op) {
        case Op::Add:
        case Op::Sub:
        case Op::Mul:
        case Op::Neg:
        case Op::Lt:
        case Op::Le:
        case Op::Gt:
        case Op::Ge:
        case Op::Eq:
        case Op::Ne:
        case Op::Weq:
        case Op::Wne:
        case Op::Truth:
        case Op::Lnot:
        case Op::Land:
        case Op::Lor:
        case Op::Limp:
        case Op::Leqv:
        case Op::And:
        case Op::Or:
        case Op::Xor:
        case Op::Xnor:
        case Op::Not:
        case Op::Rand:
        case Op::Rnand:
        case Op::Ror:
        case Op::Rnor:
        case Op::Rxor:
        case Op::Rxnor:
        case Op::Shl:
        case Op::Shr:
        case Op::Ashr:
        case Op::Select:
        case Op::Concat:
        case Op::Repl:
        case Op::Zext:
        case Op::Sext:
        case Op::Trunc:
        case Op::To4: return true;
        default: return false;
    }
}
struct Node {
    sir::Type type;
    bool eligible = false, known = false, parameter = false, incoming = false;
    std::size_t waiting = 0;
    std::vector<std::uint32_t> users;
};
void bodyFacts(const sir::Design& d, const auto& body, sir::Owner owner,
               std::vector<ValueFact>& out) {
    std::vector<Node> nodes(body.numTemps);
    for (const auto& block : body.blocks) {
        for (const auto& param : block.params) {
            auto& node = nodes[param.temp.index()];
            node.type = param.type;
            node.parameter = true;
            node.eligible = true;
        }
        for (const auto& in : block.instrs)
            if (in.result != sir::kNone) nodes[in.result.index()].type = in.type;
    }
    const auto needs = [&](sir::TempId result, const Operand& op) {
        auto& node = nodes[result.index()];
        node.incoming = true;
        if (op.kind == Operand::Kind::Const) {
            if (!knownConstant(d.constant(op.asConst()))) node.eligible = false;
        } else if (op.kind == Operand::Kind::Temp) {
            ++node.waiting;
            nodes[op.id].users.push_back(result.index());
        } else
            node.eligible = false;
    };
    for (const auto& block : body.blocks)
        for (const auto& in : block.instrs) {
            for (std::size_t i = 0; i < in.operands.size(); ++i) {
                const auto& o = in.operands[i];
                if (o.kind == Operand::Kind::Block)
                    for (std::size_t j = 0; j < o.count; ++j)
                        needs(body.blocks[o.id].params[j].temp, in.operands[i + 1 + j]);
            }
            if (in.result == sir::kNone) continue;
            auto& n = nodes[in.result.index()];
            if (in.op == Op::Ceq || in.op == Op::Cne || in.op == Op::To2)
                n.eligible = true;
            else if (in.op == Op::Load && declared(d.slot(in.operands[0].asSlot()).type))
                n.eligible = true;
            else if (propagates(in.op)) {
                n.eligible = true;
                for (const auto& o : in.operands)
                    needs(in.result, o);
            }
        }
    std::queue<std::uint32_t> ready;
    for (std::uint32_t t = 0; t < nodes.size(); ++t) {
        auto& n = nodes[t];
        if (declared(n.type) || (sir::isIntegral(n.type) && n.eligible && n.waiting == 0 &&
                                 (!n.parameter || n.incoming))) {
            n.known = true;
            ready.push(t);
        }
    }
    while (!ready.empty()) {
        const auto t = ready.front();
        ready.pop();
        for (auto id : nodes[t].users) {
            auto& n = nodes[id];
            if (--n.waiting == 0 && !n.known && n.eligible && sir::isIntegral(n.type)) {
                n.known = true;
                ready.push(id);
            }
        }
    }
    const auto add = [&](SiteKind kind, std::uint32_t b, std::uint32_t i, std::uint32_t j,
                         sir::Type type, bool known) {
        if (known)
            out.push_back({{kind, owner.kind, owner.id, b, i, j},
                           type.width,
                           declared(type) ? Proof::DeclaredTwoState : Proof::Dataflow,
                           {}});
    };
    for (std::uint32_t b = 0; b < body.blocks.size(); ++b) {
        const auto& block = body.blocks[b];
        for (std::uint32_t j = 0; j < block.params.size(); ++j) {
            const auto& param = block.params[j];
            add(SiteKind::BlockParameter, b, 0, j, param.type, nodes[param.temp.index()].known);
        }
        for (std::uint32_t i = 0; i < block.instrs.size(); ++i) {
            const auto& in = block.instrs[i];
            if (in.result != sir::kNone)
                add(SiteKind::Result, b, i, 0, in.type, nodes[in.result.index()].known);
            for (std::uint32_t j = 0; j < in.operands.size(); ++j) {
                const auto& arg = in.operands[j];
                if (arg.kind == Operand::Kind::Temp)
                    add(SiteKind::Operand, b, i, j, nodes[arg.id].type, nodes[arg.id].known);
                if (arg.kind == Operand::Kind::Const) {
                    const auto& c = d.constant(arg.asConst());
                    add(SiteKind::Operand, b, i, j, c.type, knownConstant(c));
                }
            }
        }
    }
}
}  // namespace
std::vector<ValueFact> analyzeValues(const sir::VerifiedDesign& verified, bool waveform) {
    const auto& d = verified.design();
    std::vector<bool> dumped(d.slots.size()), foreign(d.slots.size()), forced(d.slots.size());
    if (waveform)
        for (const auto& item : d.dumpVars)
            dumped[sir::physicalSlot(d, item.slot).index()] = true;
    for (const auto& item : d.foreignObjects)
        if (item.slot != sir::kNone) foreign[sir::physicalSlot(d, item.slot).index()] = true;
    for (std::uint32_t i = 0; i < d.slots.size(); ++i)
        if ((d.slots[i].flags & sir::kForceTarget) != 0)
            forced[sir::physicalSlot(d, sir::SlotId{i}).index()] = true;
    for (const auto& p : d.processes)
        if (p.kind == sir::ProcessKind::Override)
            for (const auto& r : p.writes)
                forced[sir::physicalSlot(d, r.slot).index()] = true;
    std::vector<ValueFact> out;
    for (std::uint32_t i = 0; i < d.slots.size(); ++i) {
        const auto& s = d.slots[i];
        const auto physical = sir::physicalSlot(d, sir::SlotId{i}).index();
        ValueFact fact{{SiteKind::Slot, sir::Owner::Kind::None, i, 0, 0, 0},
                       s.type.width,
                       declared(s.type) ? Proof::DeclaredTwoState : Proof::Unknown,
                       {}};
        if (dumped[physical]) fact.preserve.push_back(PreserveReason::Waveform);
        if (foreign[physical]) fact.preserve.push_back(PreserveReason::Foreign);
        if (forced[physical]) fact.preserve.push_back(PreserveReason::Force);
        if (s.flags & sir::kObservable) fact.preserve.push_back(PreserveReason::Explicit);
        if (fact.noX != Proof::Unknown || !fact.preserve.empty()) out.push_back(std::move(fact));
    }
    for (std::uint32_t p = 0; p < d.processes.size(); ++p)
        bodyFacts(d, d.processes[p], sir::Owner::of(sir::ProcessId{p}), out);
    for (std::uint32_t s = 0; s < d.subroutines.size(); ++s)
        bodyFacts(d, d.subroutines[s], sir::Owner::of(sir::SubroutineId{s}), out);
    std::ranges::sort(out, {}, &ValueFact::site);
    return out;
}
std::string stateStorageReport(const sir::VerifiedDesign& verified, bool waveform) {
    const auto facts = analyzeValues(verified, waveform);
    const auto& d = verified.design();
    auto fact = facts.begin();
    std::ostringstream out;
    for (std::uint32_t i = 0; i < d.slots.size(); ++i) {
        const auto& slot = d.slots[i];
        if (!sir::isIntegral(slot.type)) continue;
        // Only slot rows precede the SSA rows in the sorted value table.
        while (fact != facts.end() && fact->site.kind == SiteKind::Slot && fact->site.row < i)
            ++fact;
        out << "state slot " << i << ' ' << slot.name << " storage="
            << (sir::isFourState(slot.type) ? "4 reason=declared-four-state"
                                            : "2 reason=declared-two-state");
        if (slot.viewOf != sir::kNone) out << " alias-of=" << slot.viewOf.index();
        if (fact != facts.end() && fact->site.kind == SiteKind::Slot && fact->site.row == i)
            for (auto reason : fact->preserve) {
                constexpr std::string_view names[]{"waveform", "foreign", "force", "explicit"};
                out << " preserve=" << names[static_cast<unsigned>(reason)];
            }
        out << '\n';
    }
    return out.str();
}
}  // namespace kirimasim::analysis
