#include "kirimasim/sir/verify.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "kirimasim/sir/text.h"
#include "kirimasim/sir/timescale.h"
#include "kirimasim/sir/types.h"

namespace kirimasim::sir {

namespace {

/// Instructions that produce no value.
bool hasNoResult(Op op) {
    switch (op) {
        case Op::Store:
        case Op::Nba:
        case Op::NbaDelay:
        case Op::StoreElem:
        case Op::NbaElem:
        case Op::StoreBits:
        case Op::NbaBits:
        case Op::Fstore:
        case Op::Trigger:
        case Op::TriggerNb:
        case Op::Disable:
        case Op::DisableFork:
        case Op::ReportDefer: return true;
        default: return isTerminator(op);
    }
}

/// Instructions whose result is optional (a call of a void function, a
/// system task, a method without a value).
bool resultOptional(Op op) { return op == Op::Call || op == Op::Syscall || op == Op::Builtin; }

/// Instructions that write a slot (SIR_SPEC.md 3.4).
bool writesSlot(Op op) {
    switch (op) {
        case Op::Store:
        case Op::Nba:
        case Op::NbaDelay:
        case Op::StoreElem:
        case Op::NbaElem:
        case Op::StoreBits:
        case Op::NbaBits: return true;
        default: return false;
    }
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
        case ProcessKind::Final:
        case ProcessKind::StaticInit: return true;
        case ProcessKind::Initial:
        case ProcessKind::General: return false;
    }
    return true;
}

/// Process kinds whose writes no other process may write (SIR_SPEC.md 10,
/// rule 6; SCHEDULER_SPEC.md 6.4).
bool exclusiveWriter(ProcessKind k) {
    return k == ProcessKind::Comb || k == ProcessKind::Latch || k == ProcessKind::Ff;
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

bool isReal(Type t) { return t.kind == TypeKind::Real || t.kind == TypeKind::ShortReal; }
bool isString(Type t) { return t.kind == TypeKind::String; }
bool width1(Type t) { return isIntegral(t) && t.width == 1; }

/// Whether a value of type `v` may be written to a place of type `p`: the
/// same type up to signedness (SIR_SPEC.md 5.1; conversions are explicit).
bool fits(Type p, Type v) { return v.kind != TypeKind::Void && unsignedOf(p) == unsignedOf(v); }

bool overlaps(std::uint32_t a, std::uint32_t na, std::uint32_t b, std::uint32_t nb) {
    if (na == 0 || nb == 0) return true;  // 0 means "all"
    return a < b + nb && b < a + na;
}

bool overlaps(const SlotRange& x, const SlotRange& y) {
    return x.slot == y.slot && overlaps(x.firstElem, x.numElems, y.firstElem, y.numElems) &&
           overlaps(x.lsb, x.width, y.lsb, y.width);
}

/// Whether range `outer` covers all of `inner`.
bool covers(const SlotRange& outer, const SlotRange& inner) {
    const auto within = [](std::uint32_t o, std::uint32_t no, std::uint32_t i, std::uint32_t ni) {
        if (no == 0) return true;
        if (ni == 0) return false;
        return o <= i && i + ni <= o + no;
    };
    return outer.slot == inner.slot &&
           within(outer.firstElem, outer.numElems, inner.firstElem, inner.numElems) &&
           within(outer.lsb, outer.width, inner.lsb, inner.width);
}

/// What the verifier needs to know about a process or subroutine body.
struct Body {
    std::string where;
    Owner owner;
    const std::vector<BasicBlock>* blocks = nullptr;
    const std::vector<FrameVar>* frame = nullptr;  ///< A guard's is its owner's.
    std::uint32_t numTemps = 0;
    bool waitsAllowed = false;
    std::string kindText;
    const Subroutine* subroutine = nullptr;
    const Process* process = nullptr;
};

class Verifier {
public:
    explicit Verifier(const Design& d) : d_(d) {}

    std::vector<VerifyError> run() {
        if (d_.timePrecision < kMinTimeExponent || d_.timePrecision > kMaxTimeExponent)
            error("design", "time precision must be between 1fs and 100s");
        checkInstances();
        checkTypes();
        checkSlots();
        checkNames();
        checkConstants();
        for (std::size_t i = 0; i < d_.subroutines.size(); ++i)
            checkSubroutine(d_.subroutines[i], static_cast<SubroutineId>(i));
        for (std::size_t i = 0; i < d_.processes.size(); ++i)
            checkProcess(d_.processes[i], static_cast<ProcessId>(i));
        checkWriteConflicts();
        return std::move(errors_);
    }

private:
    const Design& d_;
    std::vector<VerifyError> errors_;

    void error(std::string where, std::string message, SourceLoc loc = {}) {
        errors_.push_back({std::move(where), std::move(message), loc});
    }

    static std::string processWhere(ProcessId i) { return "process @" + std::to_string(i); }
    static std::string subroutineWhere(SubroutineId i) {
        return "subroutine @" + std::to_string(i);
    }

    bool typeRefOk(TypeId t) const { return t == kNone || t < d_.types.size(); }

    void checkInstances() {
        for (std::size_t i = 0; i < d_.instances.size(); ++i) {
            const InstanceId p = d_.instances[i].parent;
            if (p != kNone && p >= i)
                error("instance " + std::to_string(i), "parent must come before the child");
        }
    }

    // Rule 10: the type table.
    void checkTypes() {
        for (std::size_t i = 0; i < d_.types.size(); ++i) {
            const TypeEntry& t = d_.types[i];
            const std::string where = "type !" + std::to_string(i);
            for (const Field& f : t.fields)
                if (!typeRefOk(f.type)) error(where, "field type out of range");
            switch (t.kind) {
                case TypeEntryKind::Enum: {
                    if (!isIntegral(t.value)) error(where, "enum base type is not integral");
                    if (t.items.empty()) error(where, "enum has no items");
                    std::set<std::string> names;
                    for (const EnumItem& item : t.items) {
                        if (!names.insert(item.name).second)
                            error(where, "enum item name is not unique");
                        if (item.value >= d_.constants.size() ||
                            d_.constants[item.value].type != t.value)
                            error(where, "enum item value does not have the base type");
                    }
                    break;
                }
                case TypeEntryKind::PackedStruct:
                case TypeEntryKind::PackedUnion:
                    if (!isIntegral(t.value)) error(where, "packed type is not integral");
                    for (const Field& f : t.fields) {
                        if (f.width == 0 || f.lsb + f.width > t.value.width ||
                            f.lsb + f.width < f.lsb)
                            error(where, "field is outside the vector");
                        if (t.kind == TypeEntryKind::PackedUnion && f.lsb != 0)
                            error(where, "union field does not start at bit 0");
                    }
                    break;
                case TypeEntryKind::UnpackedStruct:
                    if (t.fields.empty()) error(where, "struct has no members");
                    break;
                case TypeEntryKind::Array:
                    if (t.dims.empty()) error(where, "array has no dimensions");
                    if (!typeRefOk(t.element)) error(where, "element type out of range");
                    break;
            }
        }
    }

    void checkSlots() {
        std::set<std::string> seen;
        for (std::size_t i = 0; i < d_.slots.size(); ++i) {
            const Slot& s = d_.slots[i];
            const std::string where = "slot %s" + std::to_string(i);
            if (s.name.empty()) error(where, "slot has no name");
            if (!seen.insert(s.name).second) error(where, "slot name is not unique");
            if (s.instance >= d_.instances.size()) error(where, "instance out of range");
            if (s.subroutine != kNone && s.subroutine >= d_.subroutines.size())
                error(where, "subroutine out of range");
            if (!typeRefOk(s.declType)) error(where, "type out of range");
            if (s.type.kind == TypeKind::Void) error(where, "slot has type void");
            if (isIntegral(s.type) && s.type.width == 0) error(where, "integral slot has width 0");
            if (!s.dims.empty()) {
                std::uint64_t n = 1;
                for (const ArrayDim& dim : s.dims) {
                    const std::int64_t span =
                        dim.left < dim.right ? dim.right - dim.left : dim.left - dim.right;
                    n *= static_cast<std::uint64_t>(span) + 1;
                    if (span < 0 || n > 0xffffffffULL) {
                        error(where, "array is too large");
                        break;
                    }
                }
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
            if (isString(c.type)) {
                if (!c.aWords.empty() || !c.bWords.empty())
                    error(where, "string constant has words");
                continue;
            }
            if (!c.bytes.empty()) error(where, "only string constants have bytes");
            if (isReal(c.type)) {
                if (c.aWords.size() != 1 || !c.bWords.empty())
                    error(where, "real constant needs exactly one word");
                else if (c.type.kind == TypeKind::ShortReal && (c.aWords[0] >> 32) != 0)
                    error(where, "shortreal constant has more than 32 bits");
                continue;
            }
            if (!isIntegral(c.type) || c.type.width == 0) {
                error(where, "constant has no value type");
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

    // Rule 2: a slot range lies inside its slot.
    bool rangeOk(SlotId s, std::uint32_t firstElem, std::uint32_t numElems, std::uint32_t lsb,
                 std::uint32_t width) const {
        if (s >= d_.slots.size()) return false;
        const Slot& slot = d_.slots[s];
        if (slot.dims.empty()) {
            if (firstElem != 0 || numElems != 0) return false;
        } else if (numElems != 0) {
            const std::uint64_t end = std::uint64_t{firstElem} + numElems;
            if (end > elementCount(slot)) return false;
        } else if (firstElem != 0) {
            return false;
        }
        if (width == 0) return lsb == 0;
        return isIntegral(slot.type) && lsb + width <= slot.type.width && lsb + width > lsb;
    }
    bool rangeOk(const SlotRange& r) const {
        return rangeOk(r.slot, r.firstElem, r.numElems, r.lsb, r.width);
    }
    bool rangeOk(const Operand& o) const {
        return rangeOk(o.id, o.firstElem, o.numElems, o.lsb, o.width);
    }

    /// The value type of a slot operand: one element (or the whole slot if
    /// it is not an array), or a part of it. Void for a range that is not
    /// one value (several elements of an array).
    Type slotValueType(const Operand& o) const {
        if (!rangeOk(o)) return voidType();
        const Slot& s = d_.slots[o.id];
        if (!s.dims.empty() && o.numElems != 1) return voidType();
        return o.width == 0 ? s.type : unsignedOf(withWidth(s.type, o.width));
    }

    void checkSubroutine(const Subroutine& s, SubroutineId id) {
        const std::string where = subroutineWhere(id) + " " + s.name;
        if (s.instance >= d_.instances.size()) error(where, "instance out of range", s.loc);
        Body body;
        body.where = where;
        body.owner = {Owner::Kind::Subroutine, id};
        body.blocks = &s.blocks;
        body.frame = &s.frame;
        body.numTemps = s.numTemps;
        body.subroutine = &s;
        body.kindText = kindName(s.kind);
        body.waitsAllowed = s.kind == SubroutineKind::Task && s.mayWait;
        if (s.mayWait && s.kind != SubroutineKind::Task) error(where, "only tasks may wait", s.loc);
        if (s.kind == SubroutineKind::Task && s.returnType.kind != TypeKind::Void)
            error(where, "a task returns no value", s.loc);
        for (const FrameVar& f : s.frame)
            if (!typeRefOk(f.declType)) error(where, "frame variable type out of range");
        if (s.kind == SubroutineKind::Guard) {
            if (!s.params.empty()) error(where, "a guard has no arguments", s.loc);
            if (!width1(s.returnType)) error(where, "a guard returns a 1-bit value", s.loc);
            const Owner& o = s.frameOf;
            if (o.kind == Owner::Kind::Process && o.id < d_.processes.size()) {
                body.frame = &d_.processes[o.id].frame;
            } else if (o.kind == Owner::Kind::Subroutine && o.id < d_.subroutines.size() &&
                       o.id != id) {
                body.frame = &d_.subroutines[o.id].frame;
            } else {
                error(where, "a guard needs the process or subroutine whose frame it reads", s.loc);
            }
        } else if (s.frameOf.kind != Owner::Kind::None) {
            error(where, "only guards read another frame", s.loc);
        }
        for (const Param& p : s.params) {
            const bool inSlot = p.slot != kNone;
            const bool inFrame = p.frameVar != kNone;
            if (inSlot == inFrame) {
                error(where, "argument '" + p.name + "' needs exactly one place", s.loc);
            } else if (inSlot != !s.automatic) {
                error(where,
                      "argument '" + p.name + "' of a " + (s.automatic ? "automatic" : "static") +
                          " subroutine is in the " + (inSlot ? "slot table" : "frame"),
                      s.loc);
            } else if (inSlot) {
                if (p.slot >= d_.slots.size() || !fits(d_.slots[p.slot].type, p.type))
                    error(where, "argument '" + p.name + "' slot is invalid", s.loc);
            } else if (p.frameVar >= s.frame.size() || !fits(s.frame[p.frameVar].type, p.type)) {
                error(where, "argument '" + p.name + "' frame variable is invalid", s.loc);
            }
        }
        checkBody(body);
    }

    void checkProcess(const Process& p, ProcessId id) {
        const std::string where = processWhere(id) + " " + p.name;
        if (p.instance >= d_.instances.size()) error(where, "instance out of range", p.loc);
        for (const Trigger& t : p.triggers)
            if (!rangeOk(t.range)) error(where, "trigger slot range is invalid");
        for (const SlotRange& w : p.writes)
            if (!rangeOk(w)) error(where, "write range is invalid");
        if (p.kind == ProcessKind::Ff && p.triggers.empty())
            error(where, "always_ff needs exactly one event control", p.loc);
        for (const FrameVar& f : p.frame)
            if (!typeRefOk(f.declType)) error(where, "frame variable type out of range");
        switch (p.parent.kind) {
            case Owner::Kind::None: break;
            case Owner::Kind::Process:
            case Owner::Kind::Subroutine: {
                const std::size_t n = p.parent.kind == Owner::Kind::Process ? d_.processes.size()
                                                                            : d_.subroutines.size();
                if (p.parent.id >= n) error(where, "parent out of range", p.loc);
                if (p.kind != ProcessKind::General)
                    error(where, "a forked process must be of kind general", p.loc);
                break;
            }
        }
        Body body;
        body.where = where;
        body.owner = {Owner::Kind::Process, id};
        body.blocks = &p.blocks;
        body.frame = &p.frame;
        body.numTemps = p.numTemps;
        body.process = &p;
        body.kindText = std::string(kindName(p.kind)) + " process";
        body.waitsAllowed = !waitsForbidden(p.kind);
        checkBody(body);
    }

    void checkBody(const Body& body) {
        const auto& blocks = *body.blocks;
        if (blocks.empty()) {
            error(body.where, "has no blocks");
            return;
        }
        const std::size_t nb = blocks.size();
        const auto bw = [&](std::size_t b) { return body.where + " bb" + std::to_string(b); };
        if (!blocks[0].params.empty()) error(bw(0), "the entry block has parameters");

        // Rule 3: each block ends with exactly one terminator; targets exist.
        // waitEdge[b] is set when b ends with a wait, so every edge out of
        // b crosses a wait.
        std::vector<std::vector<BlockId>> succ(nb);
        std::vector<bool> waitEdge(nb, false);
        for (std::size_t b = 0; b < nb; ++b) {
            const auto& instrs = blocks[b].instrs;
            if (instrs.empty() || !isTerminator(instrs.back().op))
                error(bw(b), "block does not end with a terminator");
            for (std::size_t k = 0; k < instrs.size(); ++k) {
                const Instr& in = instrs[k];
                if (isTerminator(in.op) && k + 1 != instrs.size())
                    error(bw(b), std::string(opName(in.op)) + " is not the last instruction",
                          in.loc);
                if (isWait(in)) {
                    waitEdge[b] = true;
                    if (!body.waitsAllowed)  // rule 5
                        error(bw(b), std::string(opName(in.op)) + " in a " + body.kindText, in.loc);
                }
                for (const Operand& o : in.operands) {
                    if (o.kind != Operand::Kind::Block) continue;
                    if (o.id >= nb)
                        error(bw(b), "branch target out of range", in.loc);
                    else
                        succ[b].push_back(o.id);
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
            if (!reach[b]) error(bw(b), "block is unreachable");

        // Dominators (iterative; bodies are small).
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

        // Rule 4 (SSA): one definition per temp (a block parameter or an
        // instruction result), before every use. defIndex -1 is a parameter.
        const std::uint32_t nt = body.numTemps;
        std::vector<int> defBlock(nt, -1);
        std::vector<long> defIndex(nt, 0);
        std::vector<Type> tempType(nt);
        const auto define = [&](TempId t, std::size_t b, long k, Type ty, SourceLoc loc) {
            if (t >= nt) {
                error(bw(b), "temp %t" + std::to_string(t) + " out of range", loc);
            } else if (defBlock[t] >= 0) {
                error(bw(b), "temp %t" + std::to_string(t) + " is defined twice", loc);
            } else {
                defBlock[t] = static_cast<int>(b);
                defIndex[t] = k;
                tempType[t] = ty;
            }
        };
        for (std::size_t b = 0; b < nb; ++b) {
            for (const BlockParam& prm : blocks[b].params) {
                if (prm.type.kind == TypeKind::Void) error(bw(b), "parameter has type void");
                define(prm.temp, b, -1, prm.type, {});
            }
            const auto& instrs = blocks[b].instrs;
            for (std::size_t k = 0; k < instrs.size(); ++k)
                if (instrs[k].result != kNone)
                    define(instrs[k].result, b, static_cast<long>(k), instrs[k].type,
                           instrs[k].loc);
        }

        // crossed[d][u]: some path from the end of block d reaches block u
        // after crossing a wait, without passing through d again. A temp
        // defined in d may not be used in such a u (SIR_SPEC.md 5.1).
        std::vector<std::vector<bool>> crossed(nb);
        const auto crossesWait = [&](std::size_t d, std::size_t u) {
            if (crossed[d].empty()) {
                crossed[d].assign(nb, false);
                std::vector<std::vector<bool>> seen(2, std::vector<bool>(nb, false));
                std::vector<std::pair<BlockId, bool>> todo;
                for (const BlockId s : succ[d])
                    todo.emplace_back(s, waitEdge[d]);
                while (!todo.empty()) {
                    const auto [b, waited] = todo.back();
                    todo.pop_back();
                    if (b == d || seen[waited ? 1 : 0][b]) continue;
                    seen[waited ? 1 : 0][b] = true;
                    if (waited) crossed[d][b] = true;
                    for (const BlockId s : succ[b])
                        todo.emplace_back(s, waited || waitEdge[b]);
                }
            }
            return crossed[d][u];
        };

        for (std::size_t b = 0; b < nb; ++b) {
            const auto& instrs = blocks[b].instrs;
            for (std::size_t k = 0; k < instrs.size(); ++k) {
                const Instr& in = instrs[k];
                for (const Operand& o : in.operands) {
                    if (o.kind != Operand::Kind::Temp) continue;
                    const std::string t = "temp %t" + std::to_string(o.id);
                    if (o.id >= nt || defBlock[o.id] < 0) {
                        error(bw(b), "use of undefined " + t, in.loc);
                        continue;
                    }
                    const auto db = static_cast<std::size_t>(defBlock[o.id]);
                    const bool ok =
                        db == b ? defIndex[o.id] < static_cast<long>(k) : (reach[b] && dom[b][db]);
                    if (!ok) {
                        error(bw(b), t + " is not defined before use", in.loc);
                    } else if (db != b && crossesWait(db, b)) {
                        error(bw(b), t + " is used after a wait", in.loc);
                    }
                }
                checkInstr(body, bw(b), in, tempType);
            }
        }
    }

    /// The type of a value operand, or void if it has none or is invalid.
    Type operandType(const Body& body, const Operand& o, const std::vector<Type>& tempType) const {
        switch (o.kind) {
            case Operand::Kind::Temp: return o.id < tempType.size() ? tempType[o.id] : voidType();
            case Operand::Kind::Const:
                return o.id < d_.constants.size() ? d_.constants[o.id].type : voidType();
            case Operand::Kind::Slot: return slotValueType(o);
            case Operand::Kind::Frame:
                return o.id < body.frame->size() ? (*body.frame)[o.id].type : voidType();
            default: return voidType();
        }
    }

    /// The value of a constant operand used as an index or a position, or
    /// nullopt if it is not a constant without x or z bits.
    std::optional<std::int64_t> constIndex(const Operand& o) const {
        if (o.kind != Operand::Kind::Const || o.id >= d_.constants.size()) return std::nullopt;
        const Constant& c = d_.constants[o.id];
        if (!isIntegral(c.type) || c.type.width > 64 || c.aWords.empty()) return std::nullopt;
        for (const std::uint64_t b : c.bWords)
            if (b != 0) return std::nullopt;
        std::uint64_t v = c.aWords[0];
        const std::uint32_t w = c.type.width;
        if (isSigned(c.type) && w < 64 && ((v >> (w - 1)) & 1U) != 0) v |= ~std::uint64_t{0} << w;
        return static_cast<std::int64_t>(v);
    }

    /// What an element or bit write (store.elem, store.bits and their NBA
    /// forms) writes, as the write set records it (SIR_SPEC.md 3.4): the
    /// element and bits when the indices and the position are constants,
    /// the element when only the indices are, else the whole slot.
    SlotRange elementTarget(const Body& body, const Instr& in,
                            const std::vector<Type>& tempType) const {
        const auto& ops = in.operands;
        const SlotRange whole{ops[0].id, 0, 0, 0, 0};
        if (ops[0].id >= d_.slots.size()) return whole;
        const Slot& slot = d_.slots[ops[0].id];
        const bool bits = in.op == Op::StoreBits || in.op == Op::NbaBits;
        // slot, indices..., [position,] value
        const std::size_t nIdx = ops.size() < (bits ? 3U : 2U) ? 0 : ops.size() - (bits ? 3 : 2);
        if (nIdx != slot.dims.size()) return whole;
        std::uint64_t elem = 0;
        for (std::size_t d = 0; d < nIdx; ++d) {
            const std::optional<std::int64_t> c = constIndex(ops[1 + d]);
            if (!c) return whole;
            const std::int64_t l = slot.dims[d].left;
            const std::int64_t r = slot.dims[d].right;
            const auto n = static_cast<std::uint64_t>((l < r ? r - l : l - r) + 1);
            const std::int64_t off = l <= r ? *c - l : l - *c;
            if (off < 0 || static_cast<std::uint64_t>(off) >= n) return whole;
            elem = elem * n + static_cast<std::uint64_t>(off);
        }
        SlotRange t{ops[0].id, static_cast<std::uint32_t>(elem), nIdx == 0 ? 0U : 1U, 0, 0};
        if (bits) {
            const std::optional<std::int64_t> pos = constIndex(ops[1 + nIdx]);
            const std::uint32_t w = operandType(body, ops.back(), tempType).width;
            if (pos && *pos >= 0 && w != 0 && *pos + w <= slot.type.width) {
                t.lsb = static_cast<std::uint32_t>(*pos);
                t.width = w;
            }
        }
        return t;
    }

    void checkInstr(const Body& body, const std::string& bw, const Instr& in,
                    const std::vector<Type>& tempType) {
        const auto bad = [&](const std::string& why) {
            error(bw, std::string(opName(in.op)) + ": " + why, in.loc);
        };
        // Rule 9: every instruction has a source location.
        if (in.loc.line == 0)
            bad("no source location");
        else if (in.loc.file >= d_.sources.size())
            bad("source file out of range");

        if (hasNoResult(in.op)) {
            if (in.result != kNone || in.type.kind != TypeKind::Void) bad("must not have a result");
        } else if ((in.result == kNone) != (in.type.kind == TypeKind::Void)) {
            bad("result and result type must come together");
        } else if (in.result == kNone && !resultOptional(in.op)) {
            bad("needs a result");
        }

        const auto& ops = in.operands;
        const auto ty = [&](std::size_t i) { return operandType(body, ops[i], tempType); };
        const auto isValue = [&](std::size_t i) {
            const auto k = ops[i].kind;
            return k == Operand::Kind::Temp || k == Operand::Kind::Const;
        };
        const auto nOps = [&](std::size_t n) {
            if (ops.size() != n) bad("expects " + std::to_string(n) + " operands");
            return ops.size() == n;
        };
        const Type rt = in.type;

        // Rule 2: slot operands refer to valid slots and ranges; frame,
        // subroutine and process operands are in their tables.
        for (const Operand& o : ops) {
            switch (o.kind) {
                case Operand::Kind::Slot:
                    if (!rangeOk(o)) bad("slot range is invalid");
                    break;
                case Operand::Kind::Frame:
                    if (o.id >= body.frame->size()) bad("frame variable out of range");
                    break;
                case Operand::Kind::Subroutine:
                    if (o.id >= d_.subroutines.size()) bad("subroutine out of range");
                    break;
                case Operand::Kind::Process:
                    if (o.id >= d_.processes.size()) bad("process out of range");
                    break;
                case Operand::Kind::Const:
                    if (o.id >= d_.constants.size()) bad("constant out of range");
                    break;
                default: break;
            }
        }

        // Rule 6, first half: a process declares every slot it writes.
        if (writesSlot(in.op) && body.process != nullptr && !ops.empty() &&
            ops[0].kind == Operand::Kind::Slot) {
            const Operand& o = ops[0];
            SlotRange target{o.id, o.firstElem, o.numElems, o.lsb, o.width};
            if (in.op != Op::Store && in.op != Op::Nba && in.op != Op::NbaDelay)
                target = elementTarget(body, in, tempType);
            bool covered = false;
            for (const SlotRange& w : body.process->writes)
                covered = covered || covers(w, target);
            if (!covered) bad("writes %s" + std::to_string(o.id) + " outside the write set");
        }

        // Element operations: the slot is an array and there is one index
        // per dimension (rule 11). Returns the number of indices.
        const auto elemSlot = [&]() -> const Slot* {
            if (ops.empty() || ops[0].kind != Operand::Kind::Slot || ops[0].id >= d_.slots.size())
                return nullptr;
            const Operand& o = ops[0];
            if (o.lsb != 0 || o.width != 0 || o.firstElem != 0 || o.numElems != 0) return nullptr;
            return &d_.slots[o.id];
        };
        const auto indicesOk = [&](std::size_t first, std::size_t n) {
            for (std::size_t i = first; i < first + n; ++i)
                if (!isIntegral(ty(i))) return false;
            return true;
        };

        // A block operand at position i with its arguments: the target's
        // parameters must match (rule 3). Returns the position after them.
        const auto blockArgs = [&](std::size_t i, bool continuation) -> std::size_t {
            if (i >= ops.size() || ops[i].kind != Operand::Kind::Block) {
                bad("needs a block");
                return ops.size();
            }
            const Operand& o = ops[i];
            const std::size_t end = i + 1 + o.count;
            if (end > ops.size()) {
                bad("block arguments are missing");
                return ops.size();
            }
            if (o.id >= body.blocks->size()) return end;
            const auto& params = (*body.blocks)[o.id].params;
            if (continuation && (o.count != 0 || !params.empty())) {
                bad("the block after a wait takes no arguments");
                return end;
            }
            if (params.size() != o.count) {
                bad("bb" + std::to_string(o.id) + " takes " + std::to_string(params.size()) +
                    " arguments");
                return end;
            }
            for (std::size_t a = 0; a < o.count; ++a)
                if (ty(i + 1 + a) != params[a].type)
                    bad("argument " + std::to_string(a) + " of bb" + std::to_string(o.id) +
                        " has the wrong type");
            return end;
        };
        // The last operand is the block after a wait.
        const auto continuation = [&]() {
            if (ops.empty()) {
                bad("needs a block");
                return;
            }
            blockArgs(ops.size() - 1, true);
        };
        const auto checkArgs = [&](const Subroutine& callee, std::size_t first, std::size_t end) {
            if (end - first != callee.params.size()) {
                bad("expects " + std::to_string(callee.params.size()) + " arguments");
                return;
            }
            for (std::size_t a = 0; a < callee.params.size(); ++a) {
                const Param& prm = callee.params[a];
                const std::size_t i = first + a;
                const bool place =
                    ops[i].kind == Operand::Kind::Slot || ops[i].kind == Operand::Kind::Frame;
                const bool want = prm.dir != Direction::Input;
                if (want ? !place : !isValue(i))
                    bad("argument '" + prm.name + "' must be " +
                        (want ? "a slot or frame variable" : "a value"));
                else if (!fits(prm.type, ty(i)))
                    bad("argument '" + prm.name + "' has the wrong type");
            }
        };

        // Rule 1: operand and result types.
        switch (in.op) {
            case Op::Load:
                if (nOps(1) && ops[0].kind == Operand::Kind::Slot && ty(0) != rt)
                    bad("result type differs from the slot range " + toString(ty(0)));
                break;
            case Op::Store:
            case Op::Nba:
                if (nOps(2)) {
                    if (ops[0].kind != Operand::Kind::Slot || !isValue(1))
                        bad("bad operands");
                    else if (!fits(ty(0), ty(1)))
                        bad("value type " + toString(ty(1)) + " does not fit " + toString(ty(0)));
                }
                break;
            case Op::NbaDelay:
                if (nOps(3)) {
                    if (ops[0].kind != Operand::Kind::Slot || !isValue(1) || !isValue(2))
                        bad("bad operands");
                    else if (!fits(ty(0), ty(1)))
                        bad("value type " + toString(ty(1)) + " does not fit " + toString(ty(0)));
                    else if (!isIntegral(ty(2)))
                        bad("the delay must be integral");
                }
                break;
            case Op::LoadElem:
            case Op::StoreElem:
            case Op::NbaElem: {
                const Slot* s = elemSlot();
                if (s == nullptr || s->dims.empty()) {
                    bad("needs a whole unpacked array slot");
                    break;
                }
                const std::size_t n = s->dims.size();
                const std::size_t want = 1 + n + (in.op == Op::LoadElem ? 0 : 1);
                if (ops.size() != want) {
                    bad("needs one index per dimension");  // rule 11
                    break;
                }
                if (!indicesOk(1, n)) bad("indices must be integral");
                if (in.op == Op::LoadElem) {
                    if (rt != s->type) bad("result must have the element type");
                } else if (!isValue(want - 1) || !fits(s->type, ty(want - 1))) {
                    bad("value does not fit the element type");
                }
                break;
            }
            case Op::StoreBits:
            case Op::NbaBits: {
                const Slot* s = elemSlot();
                if (s == nullptr || !isIntegral(s->type)) {
                    bad("needs a whole integral slot");
                    break;
                }
                const std::size_t n = s->dims.size();
                if (ops.size() != n + 3) {
                    bad("needs one index per dimension, a position and a value");  // rule 11
                    break;
                }
                if (!indicesOk(1, n + 1)) bad("indices and the position must be integral");
                const Type v = ty(n + 2);
                if (!isValue(n + 2) || !sameDomain(v, s->type) || v.width > s->type.width)
                    bad("value must be integral, in the slot's domain and not wider");
                break;
            }
            case Op::Fload:
                if (nOps(1) && (ops[0].kind != Operand::Kind::Frame || ty(0) != rt))
                    bad("needs a frame variable of the result type");
                break;
            case Op::Fstore:
                if (nOps(2) &&
                    (ops[0].kind != Operand::Kind::Frame || !isValue(1) || !fits(ty(0), ty(1))))
                    bad("needs a frame variable and a value of its type");
                break;
            case Op::Add:
            case Op::Sub:
            case Op::Mul:
            case Op::Div:
            case Op::Pow:
                if (nOps(2) && (!(isIntegral(rt) || isReal(rt)) || ty(0) != rt || ty(1) != rt))
                    bad("operands and result must have the same integral type (or the same real "
                        "type)");
                break;
            case Op::Mod:
            case Op::And:
            case Op::Or:
            case Op::Xor:
            case Op::Xnor:
                if (nOps(2) && (!isIntegral(rt) || ty(0) != rt || ty(1) != rt))
                    bad("operands and result must have the same integral type");
                break;
            case Op::Neg:
                if (nOps(1) && (!(isIntegral(rt) || isReal(rt)) || ty(0) != rt))
                    bad("operand and result must have the same integral type (or the same real "
                        "type)");
                break;
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
                if (nOps(2)) {
                    const Type t = ty(0);
                    if (!(isIntegral(t) || isReal(t) || isString(t)) || t != ty(1))
                        bad("operands must have one integral, real or string type");
                    else if (!width1(rt) || isFourState(rt) != isFourState(t))
                        bad("result must be 1 bit in the operands' domain");
                }
                break;
            case Op::Weq:
            case Op::Wne:
                if (nOps(2)) {
                    if (!isIntegral(ty(0)) || ty(0) != ty(1))
                        bad("operands must have one type");
                    else if (!width1(rt) || isFourState(rt) != isFourState(ty(0)))
                        bad("result must be 1 bit in the operands' domain");
                }
                break;
            case Op::MatchMask:
                if (nOps(3)) {
                    if (!isIntegral(ty(0)) || ty(0) != ty(1))
                        bad("operands must have one type");
                    else if (ops[2].kind != Operand::Kind::Text ||
                             (ops[2].text != "casez" && ops[2].text != "casex"))
                        bad("the kind must be casez or casex");
                    else if (!width1(rt) || isFourState(rt))
                        bad("result must be i2<1>");
                }
                break;
            case Op::Ceq:
            case Op::Cne:
                if (nOps(2)) {
                    if (!isIntegral(ty(0)) || ty(0) != ty(1))
                        bad("operands must have one type");
                    else if (!width1(rt) || isFourState(rt))
                        bad("result must be i2<1>");
                }
                break;
            case Op::Lnot:
            case Op::Truth:
                if (nOps(1) && (!(isIntegral(ty(0)) || isReal(ty(0))) || !width1(rt) ||
                                isFourState(rt) != isFourState(ty(0))))
                    bad("needs an integral or real operand and a 1-bit result in its domain");
                break;
            case Op::Rand:
            case Op::Rnand:
            case Op::Ror:
            case Op::Rnor:
            case Op::Rxor:
            case Op::Rxnor:
                if (nOps(1) &&
                    (!isIntegral(ty(0)) || !width1(rt) || isFourState(rt) != isFourState(ty(0))))
                    bad("needs an integral operand and a 1-bit result in its domain");
                break;
            case Op::Land:
            case Op::Lor:
            case Op::Limp:
            case Op::Leqv:
                if (nOps(2)) {
                    const Type a = ty(0);
                    const Type b = ty(1);
                    if (!(isIntegral(a) || isReal(a)) || !(isIntegral(b) || isReal(b)) ||
                        !width1(rt) || isFourState(rt) != (isFourState(a) || isFourState(b)))
                        bad("needs integral or real operands and a 1-bit result");
                }
                break;
            case Op::Shl:
            case Op::Shr:
            case Op::Ashr:
                if (nOps(2) && (!isIntegral(rt) || ty(0) != rt || !isIntegral(ty(1))))
                    bad("value must have the result type; the amount must be integral");
                break;
            case Op::Select:
                if (nOps(3) &&
                    (!width1(ty(0)) || rt.kind == TypeKind::Void || ty(1) != rt || ty(2) != rt))
                    bad("needs a 1-bit condition and two values of the result type");
                break;
            case Op::Bitsel:
                if (nOps(2) && (!isIntegral(ty(0)) || !isIntegral(ty(1)) || !width1(rt) ||
                                isSigned(rt) || isFourState(rt) != isFourState(ty(0))))
                    bad("needs an integral value and index and an unsigned 1-bit result");
                break;
            case Op::Partsel:
                if (nOps(2)) {
                    const Type v = ty(0);
                    if (!isIntegral(v) || ops[1].kind != Operand::Kind::Const ||
                        !isIntegral(ty(1)) || !isIntegral(rt) || isSigned(rt) ||
                        isFourState(rt) != isFourState(v) || rt.width > v.width) {
                        bad("needs an integral value, a constant position and a narrower "
                            "unsigned result");
                    } else {
                        const Constant& c = d_.constants[ops[1].id];
                        const bool known = c.bWords.empty() || c.bWords[0] == 0;
                        const std::uint64_t lsb = c.aWords.empty() ? 0 : c.aWords[0];
                        if (!known || c.type.width > 64 || lsb + rt.width > v.width)
                            bad("part is outside the value");
                    }
                }
                break;
            case Op::IdxPartsel:
                if (nOps(2) && (!isIntegral(ty(0)) || !isIntegral(ty(1)) || !isIntegral(rt) ||
                                isSigned(rt) || isFourState(rt) != isFourState(ty(0))))
                    bad("needs an integral value and position and an unsigned result");
                break;
            case Op::Concat: {
                if (ops.empty()) {
                    bad("needs at least one operand");
                    break;
                }
                if (isString(rt)) {
                    for (std::size_t i = 0; i < ops.size(); ++i)
                        if (!isValue(i) || !isString(ty(i))) bad("operands must be strings");
                    break;
                }
                std::uint64_t width = 0;
                bool four = false;
                bool ok = isIntegral(rt) && !isSigned(rt);
                for (std::size_t i = 0; i < ops.size() && ok; ++i) {
                    const Type t = ty(i);
                    ok = isValue(i) && isIntegral(t);
                    width += t.width;
                    four = four || isFourState(t);
                }
                if (!ok || width != rt.width || four != isFourState(rt))
                    bad("result must be unsigned, as wide as the operands together, and "
                        "four-state if any operand is");
                break;
            }
            case Op::Repl:
                if (!nOps(2)) break;
                if (isString(rt)) {
                    if (!isIntegral(ty(0)) || !isString(ty(1)))
                        bad("needs an integral count and a string");
                } else if (ops[0].kind != Operand::Kind::Const || !isIntegral(ty(0)) ||
                           !isIntegral(ty(1)) || !isIntegral(rt) || isSigned(rt) ||
                           isFourState(rt) != isFourState(ty(1))) {
                    bad("needs a constant count and an integral value");
                } else {
                    const Constant& c = d_.constants[ops[0].id];
                    const std::uint64_t n = c.aWords.empty() ? 0 : c.aWords[0];
                    if (n == 0 || (!c.bWords.empty() && c.bWords[0] != 0) ||
                        n * ty(1).width != rt.width)
                        bad("result width must be count times the value's width");
                }
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
                if (nOps(1) &&
                    (!isIntegral(ty(0)) || isFourState(ty(0)) || otherDomain(ty(0)) != rt))
                    bad("must convert a two-state value to the same four-state type");
                break;
            case Op::Itor:
                if (nOps(1) && (!isIntegral(ty(0)) || !isReal(rt)))
                    bad("must convert an integral value to a real");
                break;
            case Op::Rtoi:
                if (nOps(1) && (!isReal(ty(0)) || !isIntegral(rt)))
                    bad("must convert a real to an integral value");
                break;
            case Op::Rconv:
                if (nOps(1) && (!isReal(ty(0)) || !isReal(rt) || ty(0) == rt))
                    bad("must convert between real and shortreal");
                break;
            case Op::Itos:
                if (nOps(1) && (!isIntegral(ty(0)) || !isString(rt)))
                    bad("must convert an integral value to a string");
                break;
            case Op::Stoi:
                if (nOps(1) && (!isString(ty(0)) || !isIntegral(rt)))
                    bad("must convert a string to an integral value");
                break;
            case Op::StrGet:
                if (nOps(2) && (!isString(ty(0)) || !isIntegral(ty(1)) || !isIntegral(rt) ||
                                isFourState(rt) || rt.width != 8))
                    bad("needs a string and an index, and gives a two-state byte");
                break;
            case Op::Trigger:
                if (nOps(1) && (ops[0].kind != Operand::Kind::Slot ||
                                slotValueType(ops[0]).kind != TypeKind::Event))
                    bad("needs an event slot");
                break;
            case Op::TriggerNb:
                if (ops.empty() || ops.size() > 2 || ops[0].kind != Operand::Kind::Slot ||
                    slotValueType(ops[0]).kind != TypeKind::Event ||
                    (ops.size() == 2 && !isIntegral(ty(1))))
                    bad("needs an event slot and an optional integral delay");
                break;
            case Op::Disable:
                if (nOps(1) && ops[0].kind != Operand::Kind::Process &&
                    ops[0].kind != Operand::Kind::Subroutine && ops[0].kind != Operand::Kind::Text)
                    bad("needs a process, a subroutine or a block name");
                break;
            case Op::DisableFork: nOps(0); break;
            case Op::ReportDefer: {
                static constexpr std::string_view kConstructs[] = {"unique case",   "unique0 case",
                                                                   "priority case", "unique if",
                                                                   "unique0 if",    "priority if"};
                static constexpr std::string_view kContents[] = {
                    "no item matches", "more than one item matches", "no condition is true",
                    "more than one condition is true"};
                const auto among = [](std::string_view s, const auto& set) {
                    for (const std::string_view t : set)
                        if (s == t) return true;
                    return false;
                };
                if (nOps(2) &&
                    (ops[0].kind != Operand::Kind::Text || ops[1].kind != Operand::Kind::Text ||
                     !among(ops[0].text, kConstructs) || !among(ops[1].text, kContents)))
                    bad("needs a construct and a content of SYSTEM_TASKS.md 10.4");
                break;
            }
            case Op::Call: {
                if (ops.empty() || ops[0].kind != Operand::Kind::Subroutine ||
                    ops[0].id >= d_.subroutines.size()) {
                    bad("first operand must be a subroutine");
                    break;
                }
                const Subroutine& callee = d_.subroutines[ops[0].id];
                if (callee.kind == SubroutineKind::Guard) bad("a guard is not called");
                if (callee.mayWait) bad("a task that may wait is called with call.task");
                if (rt != callee.returnType) bad("result type differs from the return type");
                checkArgs(callee, 1, ops.size());
                break;
            }
            case Op::Syscall:
                if (ops.empty() || ops[0].kind != Operand::Kind::Text || ops[0].text.empty() ||
                    ops[0].text[0] != '$') {
                    bad("first operand must be the system task name");
                } else if (ops[0].text == "$monitor" || ops[0].text == "$dumpvars") {
                    // Rule 7: these may not follow automatic variables (13.3.2).
                    for (const Operand& o : ops)
                        if (o.kind == Operand::Kind::Frame)
                            bad("may not refer to an automatic variable");
                }
                break;
            case Op::Builtin:
                if (ops.empty() || ops[0].kind != Operand::Kind::Text || ops[0].text.empty())
                    bad("first operand must be the method name");
                break;
            case Op::Br:
                if (blockArgs(0, false) != ops.size()) bad("has extra operands");
                break;
            case Op::BrIf:
                if (ops.empty() || !width1(ty(0))) {
                    bad("needs a 1-bit condition");
                    break;
                }
                if (blockArgs(blockArgs(1, false), false) != ops.size()) bad("has extra operands");
                break;
            case Op::Ret: {
                const Type want =
                    body.subroutine != nullptr ? body.subroutine->returnType : voidType();
                if (want.kind == TypeKind::Void)
                    nOps(0);
                else if (nOps(1) && (!isValue(0) || !fits(want, ty(0))))
                    bad("needs a value of the return type " + toString(want));
                break;
            }
            case Op::WaitDelay:
                if (nOps(2) && !isIntegral(ty(0))) bad("needs an integral delay");
                continuation();
                break;
            case Op::WaitEvent: {
                if (ops.size() < 2) {
                    bad("needs at least one event and a block");
                    break;
                }
                for (std::size_t i = 0; i + 1 < ops.size(); ++i) {
                    const Operand& o = ops[i];
                    if (o.kind == Operand::Kind::Slot) continue;
                    if (o.kind == Operand::Kind::Subroutine && i > 0 &&
                        ops[i - 1].kind == Operand::Kind::Slot) {
                        checkGuard(body, o, bad);
                        continue;
                    }
                    bad("events must be slot ranges, each with an optional iff guard");
                }
                continuation();
                break;
            }
            case Op::WaitCond:
                if (nOps(2)) {
                    if (ops[0].kind != Operand::Kind::Subroutine)
                        bad("needs a guard");
                    else
                        checkGuard(body, ops[0], bad);
                }
                continuation();
                break;
            case Op::WaitNamed:
                if (nOps(2) && (ops[0].kind != Operand::Kind::Slot ||
                                slotValueType(ops[0]).kind != TypeKind::Event))
                    bad("needs an event slot");
                continuation();
                break;
            case Op::Fork: {
                if (ops.size() < 3) {
                    bad("needs processes, a join kind and a block");
                    break;
                }
                const Operand& join = ops[ops.size() - 2];
                if (join.kind != Operand::Kind::Text ||
                    (join.text != "join" && join.text != "join_any" && join.text != "join_none"))
                    bad("join kind must be join, join_any or join_none");
                for (std::size_t i = 0; i + 2 < ops.size(); ++i) {
                    const Operand& o = ops[i];
                    if (o.kind != Operand::Kind::Process || o.id >= d_.processes.size()) {
                        bad("children must be processes");
                        continue;
                    }
                    const Owner& parent = d_.processes[o.id].parent;
                    if (parent.kind != body.owner.kind || parent.id != body.owner.id)
                        bad("child @" + std::to_string(o.id) + " has another parent");
                }
                continuation();
                break;
            }
            case Op::WaitFork:
                nOps(1);
                continuation();
                break;
            case Op::CallTask: {
                if (ops.size() < 2 || ops[0].kind != Operand::Kind::Subroutine ||
                    ops[0].id >= d_.subroutines.size()) {
                    bad("needs a task and a block");
                    break;
                }
                const Subroutine& callee = d_.subroutines[ops[0].id];
                if (callee.kind != SubroutineKind::Task || !callee.mayWait)
                    bad("calls only tasks that may wait");
                checkArgs(callee, 1, ops.size() - 1);
                continuation();
                break;
            }
        }
    }

    template <typename Bad>
    void checkGuard(const Body& body, const Operand& o, const Bad& bad) const {
        if (o.id >= d_.subroutines.size()) return;
        const Subroutine& g = d_.subroutines[o.id];
        if (g.kind != SubroutineKind::Guard)
            bad("@" + std::to_string(o.id) + " is not a guard");
        else if (g.frameOf.kind != body.owner.kind || g.frameOf.id != body.owner.id)
            bad("guard @" + std::to_string(o.id) + " reads another frame");
    }

    // Rule 6, second half: what an always_comb, always_latch or always_ff
    // writes, no other process writes; a uwire has one driver; a variable
    // has at most one continuous assignment.
    void checkWriteConflicts() {
        const auto& ps = d_.processes;
        for (std::size_t i = 0; i < ps.size(); ++i) {
            for (std::size_t j = i + 1; j < ps.size(); ++j) {
                for (const SlotRange& a : ps[i].writes) {
                    for (const SlotRange& b : ps[j].writes) {
                        if (!overlaps(a, b) || a.slot >= d_.slots.size()) continue;
                        // Initial values of declarations are written by no
                        // process (6.8).
                        if (ps[i].kind == ProcessKind::StaticInit ||
                            ps[j].kind == ProcessKind::StaticInit)
                            continue;
                        const Slot& s = d_.slots[a.slot];
                        const bool exclusive =
                            exclusiveWriter(ps[i].kind) || exclusiveWriter(ps[j].kind);
                        const bool bothCont =
                            ps[i].kind == ProcessKind::Cont && ps[j].kind == ProcessKind::Cont;
                        const bool conflict = exclusive || s.kind == SlotKind::Uwire ||
                                              (bothCont && s.kind == SlotKind::Var);
                        if (conflict)
                            error(processWhere(static_cast<ProcessId>(j)) + " " + ps[j].name,
                                  "writes %s" + std::to_string(a.slot) + " (" + s.name +
                                      "), which " + processWhere(static_cast<ProcessId>(i)) +
                                      " also writes",
                                  ps[j].loc);
                    }
                }
            }
        }
    }
};

}  // namespace

std::vector<VerifyError> verify(const Design& design) { return Verifier(design).run(); }

}  // namespace kirimasim::sir
