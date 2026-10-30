// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/types.h"

#include <algorithm>
#include <cstddef>
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
        case TypeKind::Ref:
            return t.detail == kNone ? "ref" : "ref<!" + std::to_string(t.detail.index()) + ">";
        case TypeKind::DynArray: return "dynarray<!" + std::to_string(t.detail.index()) + ">";
        case TypeKind::Queue: return "queue<!" + std::to_string(t.detail.index()) + ">";
        case TypeKind::Strength: return sized("strength");
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

InitValue initialValue(SlotKind k) {
    switch (k) {
        case SlotKind::Var: return InitValue::X;
        case SlotKind::Tri0:
        case SlotKind::Supply0: return InitValue::Zero;
        case SlotKind::Tri1:
        case SlotKind::Supply1: return InitValue::One;
        default: return InitValue::Z;
    }
}

std::vector<SlotRange> unknownAtStart(const Design& d) {
    std::vector<SlotRange> out;
    for (const Process& p : d.processes) {
        if (p.kind != ProcessKind::Cont) continue;
        for (const SlotRange& w : p.writes)
            if (d.slot(w.slot).kind != SlotKind::Var && isFourState(d.slot(w.slot).type))
                out.push_back(w);
    }
    return out;
}

namespace {
std::uint64_t checkedElements(const std::vector<ArrayDim>& dims) {
    std::uint64_t n = 1;
    for (const ArrayDim& d : dims) {
        const auto lo = static_cast<std::uint64_t>(std::min(d.left, d.right));
        const auto hi = static_cast<std::uint64_t>(std::max(d.left, d.right));
        const std::uint64_t span = hi - lo;
        if (span == UINT64_MAX || n > UINT64_MAX / (span + 1)) return 0;
        n *= span + 1;
    }
    return n;
}
}  // namespace

std::uint64_t elementCount(const Slot& s) { return checkedElements(s.dims); }
std::uint64_t elementCount(const FrameVar& f) { return checkedElements(f.dims); }

SlotId physicalSlot(const Design& d, SlotId s) {
    return d.slot(s).viewOf == kNone ? s : d.slot(s).viewOf;
}
SlotRange physicalRange(const Design& d, SlotRange r) {
    r.slot = physicalSlot(d, r.slot);
    return r;
}

const char* opName(Op op) {
    switch (op) {
        case Op::ContainerNew: return "container.new";
        case Op::ContainerCopy: return "container.copy";
        case Op::ContainerSnapshot: return "container.snapshot";
        case Op::ContainerAssign: return "container.assign";
        case Op::ContainerPack: return "container.pack";
        case Op::ContainerSize: return "container.size";
        case Op::ContainerRead: return "container.read";
        case Op::ContainerCell: return "container.cell";
        case Op::ContainerWrite: return "container.write";
        case Op::ContainerClear: return "container.clear";
        case Op::ContainerSwapValues: return "container.swap.values";
        case Op::QueueInsert: return "queue.insert";
        case Op::QueuePushFront: return "queue.push.front";
        case Op::QueuePushBack: return "queue.push.back";
        case Op::QueuePopFront: return "queue.pop.front";
        case Op::QueuePopBack: return "queue.pop.back";
        case Op::QueueErase: return "queue.erase";
        case Op::Load: return "load";
        case Op::Store: return "store";
        case Op::Nba: return "nba";
        case Op::NbaDelay: return "nba.delay";
        case Op::NbaEvent: return "nba.event";
        case Op::DriveDelay: return "drive.delay";
        case Op::LoadElem: return "load.elem";
        case Op::StoreElem: return "store.elem";
        case Op::NbaElem: return "nba.elem";
        case Op::NbaElemDelay: return "nba.elem.delay";
        case Op::StoreBits: return "store.bits";
        case Op::RefMake: return "ref.make";
        case Op::LoadRef: return "load.ref";
        case Op::StoreRef: return "store.ref";
        case Op::NbaRef: return "nba.ref";
        case Op::NbaRefDelay: return "nba.ref.delay";
        case Op::LoadRefElem: return "load.ref.elem";
        case Op::StoreRefElem: return "store.ref.elem";
        case Op::StoreRefBits: return "store.ref.bits";
        case Op::NbaBits: return "nba.bits";
        case Op::NbaBitsDelay: return "nba.bits.delay";
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
        case Op::EventNew: return "event.new";
        case Op::Trigger: return "trigger";
        case Op::TriggerNb: return "trigger.nb";
        case Op::Disable: return "disable";
        case Op::DisableFork: return "disable.fork";
        case Op::Call: return "call";
        case Op::Syscall: return "syscall";
        case Op::FormatPattern: return "pattern.format";
        case Op::Builtin: return "builtin";
        case Op::ReportDefer: return "report.defer";
        case Op::ReportCall: return "report.call";
        case Op::CondX: return "cond.x";
        case Op::DefaultInit: return "default.init";
        case Op::Udp: return "udp";
        case Op::UdpSeq: return "udp.seq";
        case Op::OverrideStart: return "override.start";
        case Op::OverrideStop: return "override.stop";
        case Op::StoreOverride: return "store.override";
        case Op::Strobe: return "strobe";
        case Op::Monitor: return "monitor";
        case Op::ScopeEnter: return "scope.enter";
        case Op::ScopeLeave: return "scope.leave";
        case Op::ActivationEnter: return "activation.enter";
        case Op::ActivationLeave: return "activation.leave";
        case Op::DisableScope: return "disable.scope";
        case Op::DisableTask: return "disable.task";
        case Op::Resolve: return "resolve";
        case Op::StrengthLoad: return "strength.load";
        case Op::StrengthEdge: return "strength.edge";
        case Op::StrengthEdgeDelay: return "strength.edge.delay";
        case Op::StrengthDrive: return "strength.drive";
        case Op::StrengthDriveDelay: return "strength.drive.delay";
        case Op::StrengthMake: return "strength.make";
        case Op::StrengthValue: return "strength.value";
        case Op::StrengthMos: return "strength.mos";
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

bool equivalentValueTypes(const Design& d, Type a, Type b) {
    const auto descriptor = [&](TypeId id) {
        if (id.index() >= d.types.size()) return voidType();
        const auto kind = d.types[id.index()].kind;
        if (kind == TypeEntryKind::DynArray) return dynarrayType(id);
        if (kind == TypeEntryKind::Queue) return queueType(id);
        return voidType();
    };
    if (a.kind == TypeKind::Ref && b.kind == TypeKind::Ref) {
        if (a.detail == kNone || b.detail == kNone) return a == b;
        if (a.width != 0 || b.width != 0) return false;
        a = descriptor(a.detail);
        b = descriptor(b.detail);
        if (!isContainer(a) || !isContainer(b)) return false;
    }
    for (std::size_t remaining = d.types.size() + 1; remaining != 0; --remaining) {
        if (a.kind != b.kind || a.width != b.width) return false;
        if (!isContainer(a)) return a == b;
        if (a.width != 0 || descriptor(a.detail) != a || descriptor(b.detail) != b) return false;
        a = d.types[a.detail.index()].value;
        b = d.types[b.detail.index()].value;
    }
    return false;
}

bool isContainerOp(Op op) { return op >= Op::ContainerNew && op <= Op::QueueErase; }

bool isContainerMutation(Op op) {
    switch (op) {
        case Op::ContainerAssign:
        case Op::ContainerWrite:
        case Op::ContainerClear:
        case Op::ContainerSwapValues:
        case Op::QueueInsert:
        case Op::QueuePushFront:
        case Op::QueuePushBack:
        case Op::QueuePopFront:
        case Op::QueuePopBack:
        case Op::QueueErase: return true;
        default: return false;
    }
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
        case ProcessKind::Override: return "override";
        case ProcessKind::Postponed: return "postponed";
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
        case SubroutineKind::Watch: return "watch";
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
        case TypeEntryKind::Udp: return "udp";
        case TypeEntryKind::DynArray: return "dynarray";
        case TypeEntryKind::Queue: return "queue";
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

bool validSlotRange(const Design& d, SlotRange range) {
    const auto [s, firstElem, numElems, lsb, width] = range;
    if (s.index() >= d.slots.size()) return false;
    const Slot& slot = d.slot(s);
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

bool writesSlot(Op op) {
    switch (op) {
        case Op::Store:
        case Op::Nba:
        case Op::NbaDelay:
        case Op::DriveDelay:
        case Op::StoreElem:
        case Op::NbaElem:
        case Op::NbaElemDelay:
        case Op::StoreBits:
        case Op::NbaBits:
        case Op::NbaBitsDelay: return true;
        default: return false;
    }
}

namespace {

/// The elements and bits of a range: first, count, lsb, width, with 0
/// counts and widths spelled out.
struct Extent {
    std::uint64_t firstElem, numElems, lsb, width;
};

Extent extentOf(const Slot& s, const SlotRange& r) {
    const std::uint64_t elems = elementCount(s);
    return {r.firstElem, r.numElems == 0 ? elems : r.numElems, r.lsb,
            r.width == 0 ? s.type.width : r.width};
}

}  // namespace

bool covers(const Slot& s, const SlotRange& outer, const SlotRange& inner) {
    if (outer.slot != inner.slot) return false;
    const Extent o = extentOf(s, outer);
    const Extent i = extentOf(s, inner);
    return o.firstElem <= i.firstElem && i.firstElem + i.numElems <= o.firstElem + o.numElems &&
           o.lsb <= i.lsb && i.lsb + i.width <= o.lsb + o.width;
}

bool overlaps(const Slot& s, const SlotRange& a, const SlotRange& b) {
    if (a.slot != b.slot) return false;
    const Extent x = extentOf(s, a);
    const Extent y = extentOf(s, b);
    return x.firstElem < y.firstElem + y.numElems && y.firstElem < x.firstElem + x.numElems &&
           x.lsb < y.lsb + y.width && y.lsb < x.lsb + x.width;
}

}  // namespace kirimasim::sir
