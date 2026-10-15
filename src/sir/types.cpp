#include "haremasim/sir/types.h"

#include "haremasim/sir/design.h"

namespace haremasim::sir {

Type otherDomain(Type t) {
    switch (t.kind) {
        case TypeKind::U2: return {TypeKind::U4, t.width};
        case TypeKind::S2: return {TypeKind::S4, t.width};
        case TypeKind::U4: return {TypeKind::U2, t.width};
        case TypeKind::S4: return {TypeKind::S2, t.width};
        default: return t;
    }
}

std::string toString(Type t) {
    const auto sized = [&](const char* prefix) {
        return std::string(prefix) + "<" + std::to_string(t.width) + ">";
    };
    switch (t.kind) {
        case TypeKind::Void: return "void";
        case TypeKind::U2: return sized("i2");
        case TypeKind::S2: return sized("s2");
        case TypeKind::U4: return sized("i4");
        case TypeKind::S4: return sized("s4");
        case TypeKind::Real: return "real";
        case TypeKind::ShortReal: return "shortreal";
        case TypeKind::String: return "string";
        case TypeKind::Event: return "event";
        case TypeKind::Chandle: return "chandle";
    }
    return "?";
}

const char* opName(Op op) {
    switch (op) {
        case Op::Load: return "load";
        case Op::Store: return "store";
        case Op::Nba: return "nba";
        case Op::Add: return "add";
        case Op::Sub: return "sub";
        case Op::Mul: return "mul";
        case Op::Div: return "div";
        case Op::Mod: return "mod";
        case Op::Pow: return "pow";
        case Op::Neg: return "neg";
        case Op::Lt: return "lt";
        case Op::Le: return "le";
        case Op::Gt: return "gt";
        case Op::Ge: return "ge";
        case Op::Eq: return "eq";
        case Op::Ne: return "ne";
        case Op::Ceq: return "ceq";
        case Op::Cne: return "cne";
        case Op::Weq: return "weq";
        case Op::Wne: return "wne";
        case Op::Lnot: return "lnot";
        case Op::Land: return "land";
        case Op::Lor: return "lor";
        case Op::Truth: return "truth";
        case Op::And: return "and";
        case Op::Or: return "or";
        case Op::Xor: return "xor";
        case Op::Xnor: return "xnor";
        case Op::Not: return "not";
        case Op::Rand: return "rand";
        case Op::Rnand: return "rnand";
        case Op::Ror: return "ror";
        case Op::Rnor: return "rnor";
        case Op::Rxor: return "rxor";
        case Op::Rxnor: return "rxnor";
        case Op::Shl: return "shl";
        case Op::Shr: return "shr";
        case Op::Ashr: return "ashr";
        case Op::Select: return "select";
        case Op::Zext: return "zext";
        case Op::Sext: return "sext";
        case Op::Trunc: return "trunc";
        case Op::To2: return "to2";
        case Op::To4: return "to4";
        case Op::Syscall: return "syscall";
        case Op::Br: return "br";
        case Op::BrIf: return "br.if";
        case Op::Ret: return "ret";
        case Op::WaitDelay: return "wait.delay";
        case Op::WaitEvent: return "wait.event";
    }
    return "?";
}

const char* kindName(ProcessKind k) {
    switch (k) {
        case ProcessKind::Cont: return "cont";
        case ProcessKind::Comb: return "comb";
        case ProcessKind::Latch: return "latch";
        case ProcessKind::CombAt: return "comb-at";
        case ProcessKind::Ff: return "ff";
        case ProcessKind::FfPlain: return "ff-plain";
        case ProcessKind::Initial: return "initial";
        case ProcessKind::Final: return "final";
        case ProcessKind::General: return "general";
    }
    return "?";
}

const char* kindName(SlotKind k) {
    switch (k) {
        case SlotKind::Var: return "var";
        case SlotKind::Wire: return "wire";
        case SlotKind::Tri: return "tri";
        case SlotKind::Wand: return "wand";
        case SlotKind::Triand: return "triand";
        case SlotKind::Wor: return "wor";
        case SlotKind::Trior: return "trior";
        case SlotKind::Tri0: return "tri0";
        case SlotKind::Tri1: return "tri1";
        case SlotKind::Supply0: return "supply0";
        case SlotKind::Supply1: return "supply1";
        case SlotKind::Uwire: return "uwire";
    }
    return "?";
}

const char* edgeName(Edge e) {
    switch (e) {
        case Edge::Any: return "any";
        case Edge::Pos: return "posedge";
        case Edge::Neg: return "negedge";
        case Edge::Both: return "edge";
    }
    return "?";
}

}  // namespace haremasim::sir
