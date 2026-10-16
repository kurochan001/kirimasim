#include "kirimasim/sir/types.h"

#include <cstdint>

#include "kirimasim/sir/design.h"

namespace kirimasim::sir {

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

Domain domainOf(Type t) {
    switch (t.kind) {
        case TypeKind::U2:
        case TypeKind::S2: return Domain::Two;
        case TypeKind::U4:
        case TypeKind::S4: return Domain::Four;
        case TypeKind::Real:
        case TypeKind::ShortReal: return Domain::Real;
        default: return Domain::Handle;
    }
}

std::uint64_t elementCount(const Slot& s) {
    std::uint64_t n = 1;
    for (const ArrayDim& d : s.dims) {
        const std::int64_t lo = d.left < d.right ? d.left : d.right;
        const std::int64_t hi = d.left < d.right ? d.right : d.left;
        n *= static_cast<std::uint64_t>(hi - lo) + 1;
    }
    return n;
}

const char* opName(Op op) {
    switch (op) {
        case Op::Load: return "load";
        case Op::Store: return "store";
        case Op::Nba: return "nba";
        case Op::NbaDelay: return "nba.delay";
        case Op::LoadElem: return "load.elem";
        case Op::StoreElem: return "store.elem";
        case Op::NbaElem: return "nba.elem";
        case Op::StoreBits: return "store.bits";
        case Op::NbaBits: return "nba.bits";
        case Op::Fload: return "fload";
        case Op::Fstore: return "fstore";
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
        case Op::MatchMask: return "match.mask";
        case Op::Lnot: return "lnot";
        case Op::Land: return "land";
        case Op::Lor: return "lor";
        case Op::Limp: return "limp";
        case Op::Leqv: return "leqv";
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
        case Op::Bitsel: return "bitsel";
        case Op::Partsel: return "partsel";
        case Op::IdxPartsel: return "idxpartsel";
        case Op::Concat: return "concat";
        case Op::Repl: return "repl";
        case Op::Zext: return "zext";
        case Op::Sext: return "sext";
        case Op::Trunc: return "trunc";
        case Op::To2: return "to2";
        case Op::To4: return "to4";
        case Op::Itor: return "itor";
        case Op::Rtoi: return "rtoi";
        case Op::Rconv: return "rconv";
        case Op::Itos: return "itos";
        case Op::Stoi: return "stoi";
        case Op::StrGet: return "str.get";
        case Op::Trigger: return "trigger";
        case Op::TriggerNb: return "trigger.nb";
        case Op::Disable: return "disable";
        case Op::DisableFork: return "disable.fork";
        case Op::Call: return "call";
        case Op::Syscall: return "syscall";
        case Op::Builtin: return "builtin";
        case Op::ReportDefer: return "report.defer";
        case Op::Br: return "br";
        case Op::BrIf: return "br.if";
        case Op::Ret: return "ret";
        case Op::WaitDelay: return "wait.delay";
        case Op::WaitEvent: return "wait.event";
        case Op::WaitCond: return "wait.cond";
        case Op::WaitNamed: return "wait.named";
        case Op::Fork: return "fork";
        case Op::WaitFork: return "wait.fork";
        case Op::CallTask: return "call.task";
    }
    return "?";
}

bool isTerminator(Op op) {
    switch (op) {
        case Op::Br:
        case Op::BrIf:
        case Op::Ret:
        case Op::WaitDelay:
        case Op::WaitEvent:
        case Op::WaitCond:
        case Op::WaitNamed:
        case Op::Fork:
        case Op::WaitFork:
        case Op::CallTask: return true;
        default: return false;
    }
}

bool isWait(const Instr& in) {
    switch (in.op) {
        case Op::WaitDelay:
        case Op::WaitEvent:
        case Op::WaitCond:
        case Op::WaitNamed:
        case Op::WaitFork:
        case Op::CallTask: return true;
        case Op::Fork:
            // fork @P..., "<join kind>", bbN
            return in.operands.size() >= 2 &&
                   in.operands[in.operands.size() - 2].text != "join_none";
        default: return false;
    }
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
        case ProcessKind::StaticInit: return "static-init";
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

const char* kindName(SubroutineKind k) {
    switch (k) {
        case SubroutineKind::Function: return "function";
        case SubroutineKind::Task: return "task";
        case SubroutineKind::Guard: return "guard";
    }
    return "?";
}

const char* kindName(TypeEntryKind k) {
    switch (k) {
        case TypeEntryKind::Enum: return "enum";
        case TypeEntryKind::PackedStruct: return "struct.packed";
        case TypeEntryKind::PackedUnion: return "union.packed";
        case TypeEntryKind::UnpackedStruct: return "struct";
        case TypeEntryKind::Array: return "array";
    }
    return "?";
}

const char* directionName(Direction d) {
    switch (d) {
        case Direction::Input: return "input";
        case Direction::Output: return "output";
        case Direction::Inout: return "inout";
        case Direction::Ref: return "ref";
        case Direction::ConstRef: return "const.ref";
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

}  // namespace kirimasim::sir
