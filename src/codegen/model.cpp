// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/codegen/model.h"
#include "kirimasim/analysis/islands.h"
#include "kirimasim/analysis/values.h"
#include "kirimasim/sir/constant_index.h"

#include <algorithm>
#include <sstream>
#include <tuple>

namespace kirimasim::codegen {
namespace {
using sir::Op;
using sir::Operand;

bool scalar(sir::Type type) { return sir::isIntegral(type) && type.width <= 64; }

class Body {
public:
    Body(const sir::Design& design, const sir::Process& process,
         const std::vector<std::uint32_t>& slots, bool bounds, const std::vector<bool>& known,
         std::vector<sir::SlotRange>* guard)
        : d_(design), p_(process), slots_(slots), bounds_(bounds), types_(p_.numTemps),
          known_(known), guard_(guard) {
        for (const auto& block : p_.blocks) {
            for (const auto& param : block.params)
                types_[param.temp.index()] = param.type;
            for (const auto& in : block.instrs)
                if (in.result != sir::kNone) types_[in.result.index()] = in.type;
        }
    }

    std::string emit(sir::ProcessId id, std::uint64_t maxSteps) {
        if (!p_.frame.empty() || p_.blocks.empty() || std::ranges::any_of(types_, [](auto t) {
                return t.kind != sir::TypeKind::Void && !scalar(t);
            }))
            return {};
        std::uint64_t steps = 0;
        for (const auto& block : p_.blocks)
            steps += block.instrs.size();
        if (maxSteps && steps > maxSteps) return {};
        // Bound the compiler's per-function optimization work on deep but
        // valid straight-line expressions. Each part consumes the same SSA
        // array and writes through the same kernel table in program order.
        constexpr std::size_t partSize = 192;
        const auto name = [&](std::size_t b, std::size_t i) {
            return "part" + std::to_string(id.index()) + "_" + std::to_string(b) + "_" +
                   std::to_string(i);
        };
        for (std::size_t b = 0; b < p_.blocks.size(); ++b) {
            const auto& ins = p_.blocks[b].instrs;
            if (ins.size() <= partSize) continue;
            for (std::size_t first = 0; first < ins.size() - 1; first += partSize) {
                out_ << "static void " << name(b, first)
                     << "(kirima_context* ctx,kirima_model_activation& f,W4* t) {\n";
                for (auto i = first; i < std::min(first + partSize, ins.size() - 1); ++i)
                    if (!instruction(ins[i], b)) return {};
                out_ << "}\n";
            }
        }
        out_ << "static kirima_status body" << id.index()
             << "(kirima_context* ctx, void* raw) {\n  auto& f = "
                "*static_cast<kirima_model_activation*>(raw);\n";
        if (!types_.empty()) out_ << "  W4 t[" << types_.size() << "];\n";
        for (std::size_t b = 0; b < p_.blocks.size(); ++b) {
            const auto& ins = p_.blocks[b].instrs;
            out_ << "b" << b << ":\n";
            if (ins.size() > partSize) {
                for (std::size_t first = 0; first < ins.size() - 1; first += partSize)
                    out_ << "  " << name(b, first) << "(ctx,f,"
                         << (types_.empty() ? "nullptr" : "t") << ");\n";
                if (!instruction(ins.back(), b)) return {};
            } else {
                for (const auto& in : ins)
                    if (!instruction(in, b)) return {};
            }
        }
        out_ << "}\n";
        return out_.str();
    }

private:
    const sir::Design& d_;
    const sir::Process& p_;
    const std::vector<std::uint32_t>& slots_;
    bool bounds_;
    std::vector<sir::Type> types_;
    const std::vector<bool>& known_;
    std::vector<sir::SlotRange>* guard_;
    std::ostringstream out_;

    sir::Type type(const Operand& o) const {
        if (o.kind == Operand::Kind::Temp) return types_[o.id];
        if (o.kind == Operand::Kind::Const) return d_.constant(o.asConst()).type;
        return {};
    }
    std::string value(const Operand& o) const {
        if (o.kind == Operand::Kind::Temp) return "t[" + std::to_string(o.id) + "]";
        const auto& c = d_.constant(o.asConst());
        return "W4{" + std::to_string(c.aWords.empty() ? 0 : c.aWords[0]) + "ULL," +
               std::to_string(c.bWords.empty() ? 0 : c.bWords[0]) + "ULL}";
    }
    bool jump(const sir::Instr& in, std::size_t at, std::size_t block) {
        const auto& edge = in.operands[at];
        if (edge.id <= block) return false;
        const auto& params = p_.blocks[edge.id].params;
        out_ << "{\n";
        for (std::size_t i = 0; i < params.size(); ++i)
            out_ << "    const W4 a" << i << " = " << value(in.operands[at + 1 + i]) << ";\n";
        for (std::size_t i = 0; i < params.size(); ++i)
            out_ << "    t[" << params[i].temp.index() << "] = a" << i << ";\n";
        out_ << "    goto b" << edge.id << ";\n  }\n";
        return true;
    }
    std::optional<std::uint32_t> element(const sir::Instr& in, const sir::Slot& slot) const {
        if (in.op == Op::Store || in.op == Op::Load || in.op == Op::Nba)
            return in.operands[0].firstElem;
        return sir::constantElement(d_, slot.dims,
                                    std::span{in.operands}.subspan(1, slot.dims.size()));
    }
    bool storage(const sir::Instr& in) {
        const auto& target = in.operands[0];
        if (target.kind != Operand::Kind::Slot) return false;
        const auto& slot = d_.slot(target.asSlot());
        if (!scalar(slot.type)) return false;
        const auto elem = element(in, slot);
        if (!elem) return false;
        const auto width = target.width ? target.width : slot.type.width;
        const std::string where = std::to_string(slots_[target.id]) + "," + std::to_string(*elem);
        if (in.op == Op::Load || in.op == Op::LoadElem) {
            if (guard_ && sir::isFourState(slot.type)) {
                const auto range =
                    sir::physicalRange(d_, {target.asSlot(), *elem, 1, target.lsb, width});
                guard_->push_back(range);
            }
            out_ << "  t[" << in.result.index() << "] = load(ctx,f," << where << "," << target.lsb
                 << "," << width << "," << (!guard_ && sir::isFourState(slot.type)) << ");\n";
            if (!sir::isFourState(in.type) && sir::isFourState(slot.type))
                out_ << "  t[" << in.result.index() << "]=to2(t[" << in.result.index() << "]);\n";
            return true;
        }
        const bool nba = in.op == Op::Nba || in.op == Op::NbaElem;
        const auto& rhs = in.operands.back();
        const auto word = value(rhs);
        out_ << "  { const W4 v=" << (sir::isFourState(slot.type) ? word : "to2(" + word + ")")
             << "; const uint64_t words[]={v.a,v.b}; f.kernel->"
             << (nba ? "schedule_nba(ctx,f.process," : "write(ctx,") << where << "," << target.lsb
             << "," << target.width << ",words" << (nba ? ",0" : "") << "); }\n";
        return true;
    }
    std::string knownExpression(const sir::Instr& in) const {
        // A two-state destination alone does not justify integer arithmetic:
        // both inputs must have no X/Z before the operation.
        std::string_view op;
        switch (in.op) {
            case Op::Add: op = "+"; break;
            case Op::Sub: op = "-"; break;
            case Op::Mul: op = "*"; break;
            case Op::And: op = "&"; break;
            case Op::Or: op = "|"; break;
            case Op::Xor: op = "^"; break;
            default: return {};
        }
        const auto known = [&](const Operand& o) {
            if (guard_) return true;
            if (o.kind == Operand::Kind::Temp) return static_cast<bool>(known_[o.id]);
            const auto& c = d_.constant(o.asConst());
            return std::ranges::all_of(c.bWords, [](auto w) { return w == 0; });
        };
        if (!std::ranges::all_of(in.operands, known)) return {};
        std::ostringstream expr;
        expr << "W4{(" << value(in.operands[0]) << ".a " << op << ' ' << value(in.operands[1])
             << ".a) & mask(" << in.type.width << "),0}";
        return expr.str();
    }
    std::string expression(const sir::Instr& in) const {
        const auto& ops = in.operands;
        const auto v = [&](std::size_t i) { return value(ops[i]); };
        const auto w = std::to_string(in.type.width);
        const auto inputWidth = std::to_string(type(ops[0]).width);
        const auto signedInput = sir::isSigned(type(ops[0])) ? "true" : "false";
        const auto two = [&](const std::string& name) {
            return name + "(" + v(0) + "," + v(1) + ")";
        };
        const auto arithmetic = [&](const std::string& name) {
            return name + "(" + v(0) + "," + v(1) + "," + w + ")";
        };
        const auto compare = [&](const std::string& name) {
            return name + "(" + v(0) + "," + v(1) + "," + inputWidth + "," + signedInput + ")";
        };
        switch (in.op) {
            case Op::Add: return arithmetic("add4");
            case Op::Sub: return arithmetic("sub4");
            case Op::Mul: return arithmetic("mul4");
            case Op::Div: return compare("div4");
            case Op::Mod: return compare("mod4");
            case Op::Gt: return compare("gt4");
            case Op::Ge: return compare("ge4");
            case Op::Lt: return compare("lt4");
            case Op::Le: return compare("le4");
            case Op::Eq: return two("eq4");
            case Op::Ne: return two("ne4");
            case Op::Ceq: return two("ceq4");
            case Op::Cne: return two("cne4");
            case Op::Weq: return two("weq4");
            case Op::Wne: return two("wne4");
            case Op::And: return two("and4");
            case Op::Or: return two("or4");
            case Op::Xor: return two("xor4");
            case Op::Xnor: return arithmetic("xnor4");
            case Op::Shl: return arithmetic("shl4");
            case Op::Shr: return arithmetic("shr4");
            case Op::Ashr: return compare("ashr4");
            case Op::Lor: return two("lor4");
            case Op::Land: return two("land4");
            case Op::Neg: return "neg4(" + v(0) + "," + w + ")";
            case Op::Not: return "not4(" + v(0) + "," + w + ")";
            case Op::Lnot: return "lnot4(" + v(0) + ")";
            case Op::Truth: return "truth4(" + v(0) + ")";
            case Op::Sext: return "sext4(" + v(0) + "," + inputWidth + "," + w + ")";
            case Op::Trunc: return "trunc4(" + v(0) + "," + w + ")";
            case Op::Zext:
            case Op::To2:
            case Op::To4: return v(0);
            case Op::Select: return "select4(" + v(0) + "," + v(1) + "," + v(2) + "," + w + ")";
            case Op::Concat: {
                std::ostringstream result;
                for (std::size_t i = 0; i < ops.size(); ++i)
                    result << "concat4(";
                result << "W4{0,0}";
                for (const auto& op : ops)
                    result << ',' << value(op) << ',' << type(op).width << ')';
                return result.str();
            }
            case Op::Repl:
                return "repl4(" + v(1) + "," + std::to_string(type(ops[1]).width) + "," + v(0) +
                       ".a)";
            case Op::Partsel:
            case Op::Bitsel:
            case Op::IdxPartsel:
                if (bounds_) return {};
                return "select_bits(" + v(0) + "," + inputWidth + "," + v(1) + "," +
                       std::to_string(type(ops[1]).width) + "," +
                       (sir::isSigned(type(ops[1])) ? "true" : "false") + "," + w + ")";
            default: return {};
        }
    }
    bool closedOverKnownInputs(const sir::Instr& in) const {
        for (const auto& o : in.operands)
            if (o.kind == Operand::Kind::Const &&
                std::ranges::any_of(d_.constant(o.asConst()).bWords,
                                    [](auto word) { return word != 0; }))
                return false;
        if (in.op == Op::Div || in.op == Op::Mod) {
            const auto& divisor = in.operands[1];
            return divisor.kind == Operand::Kind::Const &&
                   std::ranges::any_of(d_.constant(divisor.asConst()).aWords,
                                       [](auto word) { return word != 0; });
        }
        if (in.op == Op::Bitsel || in.op == Op::IdxPartsel || in.op == Op::Partsel) {
            const auto& selector = in.operands[1];
            if (selector.kind != Operand::Kind::Const) return false;
            const auto pos = sir::constantIndex(d_.constant(selector.asConst()));
            const auto width = type(in.operands[0]).width;
            return pos && *pos >= 0 && in.type.width <= width && *pos <= width - in.type.width;
        }
        return true;
    }
    bool instruction(const sir::Instr& in, std::size_t block) {
        if (guard_ && !closedOverKnownInputs(in)) return false;
        for (const auto& o : in.operands)
            if ((o.kind == Operand::Kind::Temp || o.kind == Operand::Kind::Const) &&
                !scalar(type(o)))
                return false;
        switch (in.op) {
            case Op::Ret: out_ << "  return KIRIMA_DONE;\n"; return true;
            case Op::Br: return jump(in, 0, block);
            case Op::BrIf:
                out_ << "  if ((" << value(in.operands[0]) << ".a & ~" << value(in.operands[0])
                     << ".b & 1u) != 0) ";
                if (!jump(in, 1, block)) return false;
                return jump(in, 2 + in.operands[1].count, block);
            case Op::Load:
            case Op::LoadElem:
            case Op::Store:
            case Op::StoreElem:
            case Op::Nba:
            case Op::NbaElem: return storage(in);
            default: break;
        }
        if (in.result == sir::kNone || in.operands.empty() ||
            !std::ranges::all_of(in.operands, [](const auto& o) {
                return o.kind == Operand::Kind::Temp || o.kind == Operand::Kind::Const;
            }))
            return false;
        auto rhs = knownExpression(in);
        if (rhs.empty()) rhs = expression(in);
        if (rhs.empty()) return false;
        out_ << "  t[" << in.result.index()
             << "] = " << (sir::isFourState(in.type) ? rhs : "to2(" + rhs + ")") << ";\n";
        return true;
    }
};
Model makeModel(const sir::VerifiedDesign& verified, const analysis::TableData& table,
                std::uint64_t maxSteps, bool checkBounds, std::vector<KnownIsland>* guards) {
    Model result;
    if (table.members.empty() || !analysis::matchesIslandAnalysis(verified, table)) return result;
    const auto& d = verified.design();
    std::vector<std::uint32_t> slots;
    slots.reserve(d.slots.size());
    std::uint32_t count = 0;
    for (const auto& s : d.slots)
        slots.push_back(s.viewOf == sir::kNone ? count++ : slots[s.viewOf.index()]);
    std::vector<std::vector<bool>> known(d.processes.size());
    for (const auto& row : table.members)
        known[row.process.index()].resize(d.process(row.process).numTemps);
    if (!guards)
        for (const auto& fact : analysis::analyzeValues(verified, false)) {
            const auto& site = fact.site;
            if (site.owner != sir::Owner::Kind::Process || known[site.row].empty() ||
                fact.noX == analysis::NoXProof::Unknown)
                continue;
            const auto& block = d.processes[site.row].blocks[site.block];
            if (site.kind == analysis::SiteKind::Result)
                known[site.row][block.instrs[site.instruction].result.index()] = true;
            else if (site.kind == analysis::SiteKind::BlockParameter)
                known[site.row][block.params[site.index].temp.index()] = true;
        }
    std::vector<std::string> bodies;
    bodies.reserve(table.members.size());
    std::vector<bool> supported;
    for (const auto& row : table.members) {
        if (supported.size() <= row.island) supported.resize(row.island + 1, true);
        if (guards && guards->size() <= row.island) guards->resize(row.island + 1);
        auto* guard = guards ? &(*guards)[row.island] : nullptr;
        if (guard) guard->island = row.island;
        bodies.push_back(Body(d, d.process(row.process), slots, checkBounds,
                              known[row.process.index()], guard ? &guard->inputs : nullptr)
                             .emit(row.process, maxSteps));
        if (bodies.back().empty()) supported[row.island] = false;
    }
    for (std::uint32_t i = 0; i < supported.size(); ++i)
        if (!supported[i]) result.unsupportedIslands.push_back(i);
    if (guards) {
        std::erase_if(*guards, [&](const auto& guard) { return !supported[guard.island]; });
        for (auto& guard : *guards) {
            const auto key = [](const sir::SlotRange& r) {
                return std::tie(r.slot, r.firstElem, r.lsb, r.width);
            };
            std::ranges::sort(guard.inputs, {}, key);
            guard.inputs.erase(std::unique(guard.inputs.begin(), guard.inputs.end()),
                               guard.inputs.end());
        }
    }
    std::ostringstream out;
    out << R"(#include "kirimasim/kernel/model_abi.h"
#include "kirimasim/kernel/w4ops.h"
namespace {
using namespace kirimasim::kernel;
W4 load(kirima_context* ctx, kirima_model_activation& f, unsigned slot, unsigned elem,
        unsigned lsb, unsigned width, bool four) {
    const auto* p = f.kernel->slot_words(ctx,slot,elem);
    return {(p[0] >> lsb) & mask(width),four ? (p[1] >> lsb) & mask(width) : 0};
}
W4 select_bits(W4 v,unsigned width,W4 index,unsigned indexWidth,bool sign,unsigned n) {
    const auto pos = position4(index,indexWidth,sign);
    return pos ? bits4(v,width,*pos,n) : allX(n);
}
)";
    for (std::size_t i = 0; i < table.members.size(); ++i)
        if (supported[table.members[i].island]) {
            result.bodies.push_back(table.members[i].process);
            out << bodies[i];
        }
    if (result.bodies.empty()) return result;
    out << "constexpr kirima_model_body bodies[] = {\n";
    for (auto id : result.bodies)
        out << "  {{KIRIMA_MODEL_PROCESS," << id.index() << ",0,8,0,0,nullptr},body" << id.index()
            << ",nullptr},\n";
    out << "};\nconstexpr kirima_module_table "
           "model{KIRIMA_MODULE_DESCRIPTOR,sizeof(kirima_module_table),"
           "KIRIMA_ABI_MAJOR,KIRIMA_ABI_MINOR,{1,KIRIMA_MODEL_LAYOUT_64_V1,{";
    for (auto byte : table.key.design)
        out << static_cast<unsigned>(byte) << ",";
    out << "}}," << result.bodies.size()
        << ",0,bodies,nullptr};\n}\n"
           "extern \"C\" KIRIMA_MODULE_EXPORT const kirima_module_table* "
           "kirima_get_module_table(uint32_t major,uint32_t minor) {\n"
           "  return major == model.abi_major && minor >= model.minimum_minor ? &model : "
           "nullptr;\n}\n";
    result.source = out.str();
    return result;
}
}  // namespace

Model emitModel(const sir::VerifiedDesign& verified, const analysis::TableData& table,
                std::uint64_t maxSteps, bool checkBounds) {
    return makeModel(verified, table, maxSteps, checkBounds, nullptr);
}
KnownModel emitKnownModel(const sir::VerifiedDesign& verified, const analysis::TableData& table,
                          std::uint64_t maxSteps, bool checkBounds) {
    const auto& d = verified.design();
    // Foreign calls and synchronous guard/watch/path callbacks may write a
    // value between two ordinary instructions in the same island evaluation.
    if (d.foreignReflection || !d.foreignObjects.empty() || !d.strengthNodes.empty() ||
        !d.modulePaths.empty() ||
        std::ranges::any_of(d.processes, [](const auto& p) { return p.netResolver; }) ||
        std::ranges::any_of(d.subroutines, [](const auto& sub) {
            return sir::readsWaiterFrame(sub.kind) ||
                   sub.kind == sir::SubroutineKind::PathCondition;
        }))
        return {};
    KnownModel result;
    result.model = makeModel(verified, table, maxSteps, checkBounds, &result.guards);
    return result;
}
}  // namespace kirimasim::codegen
