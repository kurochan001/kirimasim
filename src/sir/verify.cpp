#include "haremasim/sir/verify.h"

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "haremasim/sir/text.h"
#include "haremasim/sir/types.h"

namespace haremasim::sir {

namespace {

bool isTerminator(Op op) {
    return op == Op::Br || op == Op::BrIf || op == Op::Ret || op == Op::WaitDelay ||
           op == Op::WaitEvent;
}

bool isWait(Op op) { return op == Op::WaitDelay || op == Op::WaitEvent; }

/// Instructions that produce no value.
bool hasNoResult(Op op) {
    return op == Op::Store || op == Op::Nba || isTerminator(op);
}

/// Process kinds whose body may not contain wait instructions. The leading
/// edge wait of ff and ff-plain is held in the process's triggers, so the
/// body of those kinds has no waits either (SIR_SPEC.md 10, rule 5).
bool waitsForbidden(ProcessKind k) {
    switch (k) {
        case ProcessKind::Cont:
        case ProcessKind::Comb:
        case ProcessKind::Latch:
        case ProcessKind::CombAt:
        case ProcessKind::Ff:
        case ProcessKind::FfPlain:
        case ProcessKind::Final: return true;
        case ProcessKind::Initial:
        case ProcessKind::General: return false;
    }
    return true;
}

Type withWidth(Type t, std::uint32_t w) { return {t.kind, w}; }

/// A part of a slot is unsigned, like a part-select (11.5.1).
Type unsignedOf(Type t) {
    if (t.kind == TypeKind::S2) return {TypeKind::U2, t.width};
    if (t.kind == TypeKind::S4) return {TypeKind::U4, t.width};
    return t;
}

bool sameDomain(Type a, Type b) {
    return isIntegral(a) && isIntegral(b) && isFourState(a) == isFourState(b);
}

class Verifier {
public:
    explicit Verifier(const Design& d) : d_(d) {}

    std::vector<VerifyError> run() {
        checkInstances();
        checkSlots();
        checkNames();
        checkConstants();
        for (std::size_t i = 0; i < d_.processes.size(); ++i)
            checkProcess(d_.processes[i], i);
        return std::move(errors_);
    }

private:
    const Design& d_;
    std::vector<VerifyError> errors_;

    void error(std::string where, std::string message, SourceLoc loc = {}) {
        errors_.push_back({std::move(where), std::move(message), loc});
    }

    void checkInstances() {
        for (std::size_t i = 0; i < d_.instances.size(); ++i) {
            const InstanceId p = d_.instances[i].parent;
            if (p != kNone && p >= i)
                error("instance " + std::to_string(i), "parent must come before the child");
        }
    }

    void checkSlots() {
        std::set<std::string> seen;
        for (std::size_t i = 0; i < d_.slots.size(); ++i) {
            const Slot& s = d_.slots[i];
            const std::string where = "slot %" + s.name;
            if (s.name.empty()) error("slot " + std::to_string(i), "slot has no name");
            if (!seen.insert(s.name).second) error(where, "slot name is not unique");
            if (s.instance >= d_.instances.size()) error(where, "instance out of range");
            if (isIntegral(s.type)) {
                if (s.type.width == 0) error(where, "integral slot has width 0");
                const bool noX = (s.flags & kProvenNoX) != 0;
                const Domain want = isFourState(s.type) && !noX ? Domain::Four : Domain::Two;
                if (s.domain != want) error(where, "domain does not match the type");
            } else if (s.type.kind == TypeKind::Real || s.type.kind == TypeKind::ShortReal) {
                if (s.domain != Domain::Real) error(where, "domain does not match the type");
            } else if (s.type.kind == TypeKind::Void) {
                error(where, "slot has type void");
            } else if (s.domain != Domain::Handle) {
                error(where, "domain does not match the type");
            }
        }
    }

    void checkNames() {
        std::set<std::string> seen;
        for (const NameEntry& n : d_.names) {
            if (n.slot >= d_.slots.size()) error("name " + n.path, "slot out of range");
            if (!seen.insert(n.path).second) error("name " + n.path, "name is not unique");
        }
    }

    void checkConstants() {
        for (std::size_t i = 0; i < d_.constants.size(); ++i) {
            const Constant& c = d_.constants[i];
            const std::string where = "const #" + std::to_string(i);
            if (!isIntegral(c.type) || c.type.width == 0) {
                error(where, "constant is not of an integral type");
                continue;
            }
            const std::size_t words = (c.type.width + 63) / 64;
            if (c.aWords.size() != words) error(where, "wrong number of words");
            if (isFourState(c.type) ? c.bWords.size() != words : !c.bWords.empty())
                error(where, "wrong number of b words");
            const std::uint32_t used = c.type.width % 64;
            if (used == 0) continue;
            const std::uint64_t spare = ~((std::uint64_t{1} << used) - 1);
            if ((!c.aWords.empty() && (c.aWords.back() & spare) != 0) ||
                (!c.bWords.empty() && (c.bWords.back() & spare) != 0))
                error(where, "bits above the width are not 0");  // rule 8
        }
    }

    bool slotRangeOk(SlotId s, std::uint32_t lsb, std::uint32_t width) const {
        if (s >= d_.slots.size()) return false;
        if (width == 0) return lsb == 0;
        const Type t = d_.slots[s].type;
        return isIntegral(t) && lsb + width <= t.width && lsb + width > lsb;
    }

    Type slotRangeType(SlotId s, std::uint32_t width) const {
        const Type t = d_.slots[s].type;
        return width == 0 ? t : unsignedOf(withWidth(t, width));
    }

    void checkProcess(const Process& p, std::size_t index) {
        const std::string pw = "process @" + (p.name.empty() ? std::to_string(index) : p.name);
        if (p.instance >= d_.instances.size()) error(pw, "instance out of range", p.loc);
        for (const Trigger& t : p.triggers)
            if (!slotRangeOk(t.slot, t.lsb, t.width)) error(pw, "trigger slot range is invalid");
        if (p.kind == ProcessKind::Ff && p.triggers.empty())
            error(pw, "always_ff needs exactly one event control", p.loc);
        if (p.blocks.empty()) {
            error(pw, "process has no blocks", p.loc);
            return;
        }
        const std::size_t nb = p.blocks.size();

        // Rule 3: each block ends with exactly one terminator; targets exist.
        std::vector<std::vector<BlockId>> succ(nb);
        for (std::size_t b = 0; b < nb; ++b) {
            const auto& instrs = p.blocks[b].instrs;
            const std::string bw = pw + " bb" + std::to_string(b);
            if (instrs.empty() || !isTerminator(instrs.back().op)) {
                error(bw, "block does not end with a terminator");
            }
            for (std::size_t k = 0; k < instrs.size(); ++k) {
                const Instr& in = instrs[k];
                if (isTerminator(in.op) && k + 1 != instrs.size())
                    error(bw, std::string(opName(in.op)) + " is not the last instruction", in.loc);
                if (isWait(in.op) && waitsForbidden(p.kind))
                    error(bw, std::string(opName(in.op)) + " in a " + kindName(p.kind) + " process",
                          in.loc);  // rule 5
                for (const Operand& o : in.operands) {
                    if (o.kind != Operand::Kind::Block) continue;
                    if (o.id >= nb) error(bw, "branch target out of range", in.loc);
                    else succ[b].push_back(o.id);
                }
            }
        }
        std::vector<bool> reach(nb, false);
        std::vector<BlockId> work{0};
        reach[0] = true;
        while (!work.empty()) {
            const BlockId b = work.back();
            work.pop_back();
            for (const BlockId s : succ[b])
                if (!reach[s]) reach[s] = true, work.push_back(s);
        }
        for (std::size_t b = 0; b < nb; ++b)
            if (!reach[b]) error(pw + " bb" + std::to_string(b), "block is unreachable");

        // Dominators (iterative; processes are small).
        std::vector<std::vector<BlockId>> pred(nb);
        for (std::size_t b = 0; b < nb; ++b)
            for (const BlockId s : succ[b])
                pred[s].push_back(static_cast<BlockId>(b));
        std::vector<std::vector<bool>> dom(nb, std::vector<bool>(nb, true));
        dom[0].assign(nb, false);
        dom[0][0] = true;
        for (bool changed = true; changed;) {
            changed = false;
            for (std::size_t b = 1; b < nb; ++b) {
                if (!reach[b]) continue;
                std::vector<bool> nd(nb, true);
                bool any = false;
                for (const BlockId q : pred[b]) {
                    if (!reach[q]) continue;
                    any = true;
                    for (std::size_t i = 0; i < nb; ++i)
                        nd[i] = nd[i] && dom[q][i];
                }
                if (!any) nd.assign(nb, false);
                nd[b] = true;
                if (nd != dom[b]) dom[b] = std::move(nd), changed = true;
            }
        }

        // Rule 4 (SSA): one definition per temp, before every use.
        std::vector<int> defBlock(p.numTemps, -1);
        std::vector<std::size_t> defIndex(p.numTemps, 0);
        std::vector<Type> tempType(p.numTemps);
        for (std::size_t b = 0; b < nb; ++b) {
            const auto& instrs = p.blocks[b].instrs;
            for (std::size_t k = 0; k < instrs.size(); ++k) {
                const Instr& in = instrs[k];
                if (in.result == kNone) continue;
                const std::string bw = pw + " bb" + std::to_string(b);
                if (in.result >= p.numTemps) {
                    error(bw, "temp %t" + std::to_string(in.result) + " out of range", in.loc);
                } else if (defBlock[in.result] >= 0) {
                    error(bw, "temp %t" + std::to_string(in.result) + " is defined twice", in.loc);
                } else {
                    defBlock[in.result] = static_cast<int>(b);
                    defIndex[in.result] = k;
                    tempType[in.result] = in.type;
                }
            }
        }
        for (std::size_t b = 0; b < nb; ++b) {
            const auto& instrs = p.blocks[b].instrs;
            const std::string bw = pw + " bb" + std::to_string(b);
            for (std::size_t k = 0; k < instrs.size(); ++k) {
                const Instr& in = instrs[k];
                for (const Operand& o : in.operands) {
                    if (o.kind != Operand::Kind::Temp) continue;
                    if (o.id >= p.numTemps || defBlock[o.id] < 0) {
                        error(bw, "use of undefined temp %t" + std::to_string(o.id), in.loc);
                        continue;
                    }
                    const auto db = static_cast<std::size_t>(defBlock[o.id]);
                    const bool ok = db == b ? defIndex[o.id] < k : (reach[b] && dom[b][db]);
                    if (!ok)
                        error(bw, "temp %t" + std::to_string(o.id) + " is not defined before use",
                              in.loc);
                }
                checkInstr(bw, in, tempType);
            }
        }
    }

    /// The type of an operand, or void if it has none or is invalid.
    Type operandType(const Operand& o, const std::vector<Type>& tempType) const {
        switch (o.kind) {
            case Operand::Kind::Temp: return o.id < tempType.size() ? tempType[o.id] : voidType();
            case Operand::Kind::Const:
                return o.id < d_.constants.size() ? d_.constants[o.id].type : voidType();
            case Operand::Kind::Slot:
                return slotRangeOk(o.id, o.lsb, o.width) ? slotRangeType(o.id, o.width)
                                                         : voidType();
            default: return voidType();
        }
    }

    void checkInstr(const std::string& bw, const Instr& in, const std::vector<Type>& tempType) {
        const auto bad = [&](const std::string& why) {
            error(bw, std::string(opName(in.op)) + ": " + why, in.loc);
        };
        // Rule 9: every instruction has a source location.
        if (in.loc.line == 0) bad("no source location");
        else if (in.loc.file >= d_.sources.size()) bad("source file out of range");

        if (hasNoResult(in.op)) {
            if (in.result != kNone || in.type.kind != TypeKind::Void) bad("must not have a result");
        } else if (in.op != Op::Syscall && (in.result == kNone || in.type.kind == TypeKind::Void)) {
            bad("needs a result");
        }

        const auto& ops = in.operands;
        const auto nOps = [&](std::size_t n) {
            if (ops.size() != n) bad("expects " + std::to_string(n) + " operands");
            return ops.size() == n;
        };
        const auto ty = [&](std::size_t i) { return operandType(ops[i], tempType); };
        const auto isValue = [&](std::size_t i) {
            const auto k = ops[i].kind;
            return k == Operand::Kind::Temp || k == Operand::Kind::Const;
        };
        const Type rt = in.type;
        const auto width1 = [&](Type t) { return isIntegral(t) && t.width == 1; };

        // Rule 2: slot operands refer to valid slots and bit ranges.
        for (const Operand& o : ops)
            if (o.kind == Operand::Kind::Slot && !slotRangeOk(o.id, o.lsb, o.width))
                bad("slot range is invalid");

        // Rule 1: operand and result types.
        switch (in.op) {
            case Op::Load:
                if (nOps(1) && ops[0].kind == Operand::Kind::Slot && ty(0) != rt)
                    bad("result type differs from the slot range " + toString(ty(0)));
                break;
            case Op::Store:
            case Op::Nba:
                if (nOps(2)) {
                    if (ops[0].kind != Operand::Kind::Slot || !isValue(1)) bad("bad operands");
                    else if (unsignedOf(ty(0)) != unsignedOf(ty(1)))
                        bad("value type " + toString(ty(1)) + " does not fit " + toString(ty(0)));
                }
                break;
            case Op::Add:
            case Op::Sub:
            case Op::Mul:
            case Op::Div:
            case Op::Mod:
            case Op::Pow:
            case Op::And:
            case Op::Or:
            case Op::Xor:
            case Op::Xnor:
                if (nOps(2) && (!isIntegral(rt) || ty(0) != rt || ty(1) != rt))
                    bad("operands and result must have the same integral type");
                break;
            case Op::Neg:
            case Op::Not:
                if (nOps(1) && (!isIntegral(rt) || ty(0) != rt))
                    bad("operand and result must have the same integral type");
                break;
            case Op::Lt:
            case Op::Le:
            case Op::Gt:
            case Op::Ge:
            case Op::Eq:
            case Op::Ne:
            case Op::Weq:
            case Op::Wne:
                if (nOps(2)) {
                    if (!isIntegral(ty(0)) || ty(0) != ty(1)) bad("operands must have one type");
                    else if (!width1(rt) || isFourState(rt) != isFourState(ty(0)))
                        bad("result must be 1 bit in the operands' domain");
                }
                break;
            case Op::Ceq:
            case Op::Cne:
                if (nOps(2)) {
                    if (!isIntegral(ty(0)) || ty(0) != ty(1)) bad("operands must have one type");
                    else if (!width1(rt) || isFourState(rt)) bad("result must be i2<1>");
                }
                break;
            case Op::Lnot:
            case Op::Truth:
            case Op::Rand:
            case Op::Rnand:
            case Op::Ror:
            case Op::Rnor:
            case Op::Rxor:
            case Op::Rxnor:
                if (nOps(1) && (!isIntegral(ty(0)) || !width1(rt) ||
                                isFourState(rt) != isFourState(ty(0))))
                    bad("needs an integral operand and a 1-bit result in its domain");
                break;
            case Op::Land:
            case Op::Lor:
                if (nOps(2) && (!isIntegral(ty(0)) || !isIntegral(ty(1)) || !width1(rt) ||
                                isFourState(rt) != (isFourState(ty(0)) || isFourState(ty(1)))))
                    bad("needs integral operands and a 1-bit result");
                break;
            case Op::Shl:
            case Op::Shr:
            case Op::Ashr:
                if (nOps(2) && (!isIntegral(rt) || ty(0) != rt || !isIntegral(ty(1))))
                    bad("value must have the result type; the amount must be integral");
                break;
            case Op::Select:
                if (nOps(3) && (!width1(ty(0)) || ty(1) != rt || ty(2) != rt))
                    bad("needs a 1-bit condition and two values of the result type");
                break;
            case Op::Zext:
            case Op::Sext:
                if (nOps(1) && (!sameDomain(ty(0), rt) || rt.width < ty(0).width))
                    bad("must widen within one domain");
                break;
            case Op::Trunc:
                if (nOps(1) && (!sameDomain(ty(0), rt) || rt.width > ty(0).width))
                    bad("must narrow within one domain");
                break;
            case Op::To2:
                if (nOps(1) && (!isFourState(ty(0)) || otherDomain(ty(0)) != rt))
                    bad("must convert a four-state value to the same two-state type");
                break;
            case Op::To4:
                if (nOps(1) && (!isIntegral(ty(0)) || isFourState(ty(0)) || otherDomain(ty(0)) != rt))
                    bad("must convert a two-state value to the same four-state type");
                break;
            case Op::Syscall:
                if (ops.empty() || ops[0].kind != Operand::Kind::Text || ops[0].text.empty() ||
                    ops[0].text[0] != '$')
                    bad("first operand must be the system task name");
                break;
            case Op::Br:
                if (nOps(1) && ops[0].kind != Operand::Kind::Block) bad("needs a block");
                break;
            case Op::BrIf:
                if (nOps(3) && (!width1(ty(0)) || ops[1].kind != Operand::Kind::Block ||
                                ops[2].kind != Operand::Kind::Block))
                    bad("needs a 1-bit condition and two blocks");
                break;
            case Op::Ret: nOps(0); break;
            case Op::WaitDelay:
                if (nOps(2) && (!isIntegral(ty(0)) || ops[1].kind != Operand::Kind::Block))
                    bad("needs an integral delay and a block");
                break;
            case Op::WaitEvent:
                if (ops.size() < 2 || ops[0].kind != Operand::Kind::Block) {
                    bad("needs a block and at least one event");
                } else {
                    for (std::size_t i = 1; i < ops.size(); ++i)
                        if (ops[i].kind != Operand::Kind::Slot) bad("events must be slot ranges");
                }
                break;
        }
    }
};

}  // namespace

std::vector<VerifyError> verify(const Design& design) { return Verifier(design).run(); }

}  // namespace haremasim::sir
