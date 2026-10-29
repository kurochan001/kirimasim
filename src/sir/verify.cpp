// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/verify.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "kirimasim/sir/nba.h"
#include "kirimasim/sir/pattern.h"
#include "kirimasim/sir/strength.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/timescale.h"
#include "kirimasim/sir/types.h"

namespace kirimasim::sir {

namespace {

/// Instructions that produce no value.
bool hasNoResult(Op op) {
    switch (op) {
        case Op::ContainerAssign:
        case Op::ContainerWrite:
        case Op::ContainerClear:
        case Op::ContainerSwapValues:
        case Op::QueueInsert:
        case Op::QueuePushFront:
        case Op::QueuePushBack:
        case Op::QueueErase:
        case Op::Store:
        case Op::Nba:
        case Op::NbaDelay:
        case Op::NbaEvent:
        case Op::DriveDelay:
        case Op::StrengthEdge:
        case Op::StrengthEdgeDelay:
        case Op::StrengthDrive:
        case Op::StrengthDriveDelay:
        case Op::StoreElem:
        case Op::NbaElem:
        case Op::NbaElemDelay:
        case Op::StoreBits:
        case Op::NbaBits:
        case Op::NbaBitsDelay:
        case Op::Fstore:
        case Op::Trigger:
        case Op::TriggerNb:
        case Op::Disable:
        case Op::DisableFork:
        case Op::ReportDefer:
        case Op::ReportCall:
        case Op::OverrideStart:
        case Op::OverrideStop:
        case Op::StoreOverride:
        case Op::Strobe:
        case Op::Monitor:
        case Op::ScopeEnter:
        case Op::ScopeLeave:
        case Op::ActivationEnter:
        case Op::ActivationLeave:
        case Op::DisableScope:
        case Op::DisableTask:
        case Op::StoreRef:
        case Op::NbaRef:
        case Op::StoreRefElem:
        case Op::StoreRefBits: return true;
        default: return isTerminator(op);
    }
}

/// Instructions whose result is optional (a call of a void function, a
/// system task, a method without a value).
bool resultOptional(Op op) { return op == Op::Call || op == Op::Syscall || op == Op::Builtin; }

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
        case ProcessKind::StaticInit:
        case ProcessKind::Override:
        case ProcessKind::Postponed: return true;
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

/// The writers that rule 6 tells apart.
enum class Writer : std::uint8_t { Exclusive, Cont, Other };
constexpr std::size_t kWriters = 3;

/// The writer of a write range of p: a cont process writes as a continuous
/// assignment only what it drives; what the functions it calls write are
/// procedural writes (SIR_SPEC.md 3.4).
Writer writerOf(const Process& p, const SlotRange& r) {
    if (exclusiveWriter(p.kind)) return Writer::Exclusive;
    return p.kind == ProcessKind::Cont && std::ranges::find(p.drives, r) != p.drives.end()
               ? Writer::Cont
               : Writer::Other;
}

/// Whether rule 6 forbids two writers to write the same bits of a slot.
bool clash(Writer a, Writer b, SlotKind k) {
    return a == Writer::Exclusive || b == Writer::Exclusive || k == SlotKind::Uwire ||
           (a == Writer::Cont && b == Writer::Cont && k == SlotKind::Var);
}

/// A write range of a process, with its elements and bits as half-open
/// intervals; a count or width of 0 means all, which overlaps everything.
struct WriteExtent {
    SlotId slot{0};
    std::uint64_t elemLo = 0, elemHi = 0, bitLo = 0, bitHi = 0;
    ProcessId process{0};
    std::uint32_t index = 0;  ///< Its place in the process's writes.
    Writer writer = Writer::Other;
};

std::pair<std::uint64_t, std::uint64_t> extent(std::uint32_t first, std::uint32_t count) {
    constexpr std::uint64_t kAll = std::uint64_t{1} << 33;  // past every first + count
    if (count == 0) return {0, kAll};
    return {first, std::uint64_t{first} + count};
}

/// Two writes that rule 6 forbids: processes first < second, and the places
/// of the ranges in their writes.
struct Clash {
    ProcessId first{0}, second{0};
    std::uint32_t firstIndex = 0, secondIndex = 0;
    SlotId slot{0};
};

/// Adds to `found` the pairs of writes whose bits overlap and that rule 6
/// forbids: one of `a` and one of `b`, or two of `a` when `b` is empty.
/// Each list is sorted by bitLo, and the elements of all overlap. The
/// writes still open are kept by writer, so a write looks only at those it
/// may clash with, and the time grows with the writes and the pairs found.
void sweepBits(std::span<const WriteExtent> a, std::span<const WriteExtent> b, SlotKind kind,
               std::vector<Clash>& found) {
    const bool within = b.empty();
    std::array<std::vector<const WriteExtent*>, 2 * kWriters> open;  // by side, then writer
    std::size_t ia = 0;
    std::size_t ib = 0;
    while (ia < a.size() || ib < b.size()) {
        const bool fromA = ib == b.size() || (ia < a.size() && a[ia].bitLo <= b[ib].bitLo);
        const WriteExtent& w = fromA ? a[ia++] : b[ib++];
        const std::size_t side = fromA ? 0 : 1;
        const std::size_t other = within ? 0 : 1 - side;
        for (std::size_t c = 0; c < kWriters; ++c) {
            if (!clash(w.writer, static_cast<Writer>(c), kind)) continue;
            std::vector<const WriteExtent*>& list = open[other * kWriters + c];
            for (std::size_t n = 0; n < list.size();) {
                const WriteExtent& q = *list[n];
                if (q.bitHi <= w.bitLo) {  // closed for this write and every later one
                    list[n] = list.back();
                    list.pop_back();
                    continue;
                }
                if (q.process != w.process) {
                    const bool qFirst = q.process < w.process;
                    const WriteExtent& x = qFirst ? q : w;
                    const WriteExtent& y = qFirst ? w : q;
                    found.push_back({x.process, y.process, x.index, y.index, w.slot});
                }
                ++n;
            }
        }
        open[side * kWriters + static_cast<std::size_t>(w.writer)].push_back(&w);
    }
}

/// Whether range `outer` covers all of `inner`.
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
        checkClocking();
        checkStrengthNetwork();
        checkNames();
        checkConstants();
        patterns_ = validatePatterns(d_);
        errors_.insert(errors_.end(), patterns_.errors.begin(), patterns_.errors.end());
        const auto nbaErrors = verifyNbaLayouts(d_);
        errors_.insert(errors_.end(), nbaErrors.begin(), nbaErrors.end());
        for (const auto& layout : d_.nbaLayouts)
            for (const auto type : layout.values) {
                checkValueType(type, "nba capture type");
                if (type.kind == TypeKind::Void || type.kind == TypeKind::Strength ||
                    (isIntegral(type) && type.width == 0))
                    error("nba capture type", "needs a nonvoid ordinary value type");
            }
        for (const auto& layout : d_.nbaLayouts)
            for (const auto& item : layout.targets) {
                if (item.valueless_by_exception()) continue;  // Reported by verifyNbaLayouts.
                std::visit(
                    [&](const auto& target) {
                        using T = std::decay_t<decltype(target)>;
                        if constexpr (std::is_same_v<T, NbaRefTarget> ||
                                      std::is_same_v<T, NbaRefBitsTarget>)
                            checkValueType(target.target, "nba reference target type");
                    },
                    item);
            }
        checkDump();
        for (std::size_t i = 0; i < d_.subroutines.size(); ++i)
            checkSubroutine(d_.subroutines[i], static_cast<SubroutineId>(i));
        for (std::size_t i = 0; i < d_.processes.size(); ++i)
            checkProcess(d_.processes[i], static_cast<ProcessId>(i));
        checkWriteConflicts();
        return std::move(errors_);
    }

private:
    PatternValidation patterns_;
    const Design& d_;
    std::vector<VerifyError> errors_;

    void checkClocking() {
        std::set<SlotId> targets, events;
        std::set<SlotId> sources;
        for (const auto& block : d_.clockingBlocks) {
            const auto bad = [&](const char* message) {
                error("clocking block", message, block.loc);
            };
            const auto source = [&](SlotRange r) {
                if (!validSlotRange(d_, r)) return false;
                const auto& s = d_.slot(r.slot);
                return isIntegral(s.type) && s.dims.empty() && s.subroutine == kNone &&
                       r.firstElem == 0 && r.numElems <= 1;
            };
            if (block.instance.index() >= d_.instances.size()) bad("invalid instance");
            if (!source(block.clock))
                bad("invalid static integral clock source");
            else
                sources.insert(physicalSlot(d_, block.clock.slot));
            if (static_cast<unsigned>(block.edge) > static_cast<unsigned>(Edge::Both))
                bad("invalid clock edge");
            if (block.event.index() >= d_.slots.size())
                bad("invalid block event");
            else {
                const auto& s = d_.slot(block.event);
                if (s.type.kind != TypeKind::Event || !s.dims.empty() || s.subroutine != kNone ||
                    s.instance != block.instance || !events.insert(block.event).second)
                    bad("block event must be distinct static event storage owned by the instance");
            }
            for (const auto& in : block.inputs) {
                if (!source(in.source) || in.target.index() >= d_.slots.size()) {
                    bad("invalid clocking input storage");
                    continue;
                }
                sources.insert(physicalSlot(d_, in.source.slot));
                const auto& src = d_.slot(in.source.slot);
                const auto& dst = d_.slot(in.target);
                const auto width = in.source.width == 0 ? src.type.width : in.source.width;
                if (!isIntegral(dst.type) || dst.type.width != width ||
                    isFourState(src.type) != isFourState(dst.type) ||
                    (in.source.width == 0 && src.type != dst.type) ||
                    (in.source.width != 0 && isSigned(dst.type)) || !dst.dims.empty() ||
                    (dst.flags & ~kObservable) != 0 || dst.kind != SlotKind::Var ||
                    dst.viewOf != kNone || dst.driverOf != kNone || dst.subroutine != kNone ||
                    dst.instance != block.instance || !targets.insert(in.target).second)
                    bad("clockvar must be distinct typed variable storage owned by the instance");
            }
        }
        targets.insert(events.begin(), events.end());
        for (const auto target : targets) {
            if (sources.contains(target))
                error("clocking block", "clockvar cannot be a sampling source");
            for (const auto& process : d_.processes)
                for (const auto& write : process.writes)
                    if (write.slot.index() < d_.slots.size() &&
                        physicalSlot(d_, write.slot) == target)
                        error("clocking block", "clockvar is read-only to procedures");
            for (const auto& sub : d_.subroutines)
                for (const auto& block : sub.blocks)
                    for (const auto& in : block.instrs)
                        if (writesSlot(in.op) && !in.operands.empty() &&
                            in.operands.front().kind == Operand::Kind::Slot &&
                            in.operands.front().id < d_.slots.size() &&
                            physicalSlot(d_, SlotId{in.operands.front().id}) == target)
                            error("clocking block", "clockvar is read-only to subroutines");
        }
    }

    void error(std::string where, std::string message, SourceLoc loc = {}) {
        errors_.push_back({std::move(where), std::move(message), loc});
    }

    static std::string processWhere(ProcessId i) { return "process @" + std::to_string(i.index()); }
    static std::string subroutineWhere(SubroutineId i) {
        return "subroutine @" + std::to_string(i.index());
    }

    bool typeRefOk(TypeId t) const { return t == kNone || t.index() < d_.types.size(); }

    Type containerType(TypeId row) const {
        if (row.index() >= d_.types.size()) return voidType();
        const TypeEntryKind kind = d_.types[row.index()].kind;
        if (kind == TypeEntryKind::DynArray) return dynarrayType(row);
        if (kind == TypeEntryKind::Queue) return queueType(row);
        return voidType();
    }
    const Type* containerElement(Type t) const {
        if (!isContainer(t) || t.width != 0 || containerType(t.detail) != t) return nullptr;
        return &d_.types[t.detail.index()].value;
    }
    void checkValueType(Type t, const std::string& where, SourceLoc loc = {},
                        bool strengthSsa = false) {
        if (t.kind == TypeKind::Strength) {
            if (!strengthSsa) error(where, "strength is SSA-only metadata", loc);
            if (t.width == 0 || t.detail != kNone)
                error(where, "strength needs positive width and no descriptor", loc);
            return;
        }
        if (static_cast<unsigned>(t.kind) > static_cast<unsigned>(TypeKind::Strength)) {
            error(where, "unknown value type", loc);
            return;
        }
        if (isContainer(t)) {
            if (!containerElement(t))
                error(where, "container type needs its matching descriptor", loc);
        } else if (t.detail != kNone) {
            if (t.kind != TypeKind::Ref || t.width != 0 ||
                containerType(t.detail).kind == TypeKind::Void)
                error(where, "only container values and refs may name a container descriptor", loc);
        }
    }

    void checkInstances() {
        for (std::size_t i = 0; i < d_.instances.size(); ++i) {
            const InstanceKind kind = d_.instances[i].kind;
            if (kind != InstanceKind::Module && kind != InstanceKind::Package &&
                kind != InstanceKind::Unit && kind != InstanceKind::Program)
                error("instance " + std::to_string(i), "invalid instance kind");
            const InstanceId p = d_.instances[i].parent;
            if (p != kNone && p.index() >= i)
                error("instance " + std::to_string(i), "parent must come before the child");
        }
    }

    // Rule 10: the type table.
    void checkTypes() {
        for (std::size_t i = 0; i < d_.types.size(); ++i) {
            const TypeEntry& t = d_.types[i];
            const std::string where = "type !" + std::to_string(i);
            checkValueType(t.value, where);
            if (t.kind != TypeEntryKind::Queue && t.maxElements != 0)
                error(where, "only a queue descriptor has a capacity");
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
                        if (item.value.index() >= d_.constants.size() ||
                            d_.constant(item.value).type != t.value)
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
                case TypeEntryKind::Udp: checkUdp(t, where); break;
                case TypeEntryKind::DynArray:
                case TypeEntryKind::Queue: {
                    const Type e = t.value;
                    const bool scalar = (isIntegral(e) && e.width != 0) || isReal(e) ||
                                        e.kind == TypeKind::String || e.kind == TypeKind::Chandle;
                    if ((!scalar && !containerElement(e)) || (!isIntegral(e) && e.width != 0))
                        error(where, "container element must be an implemented value type");
                    if (isContainer(e) && e.detail.index() >= i)
                        error(where, "nested container descriptor must precede its parent");
                    if (!t.dims.empty() || !t.fields.empty() || !t.items.empty() ||
                        !t.rows.empty() || t.element != kNone)
                        error(where, "container descriptor has unrelated aggregate data");
                    break;
                }
            }
        }
    }

    /// A UDP table (SIR_SPEC.md 4.3): rows of one symbol of table 29-1 per
    /// input, at most one edge, a state symbol exactly when sequential.
    void checkUdp(const TypeEntry& t, const std::string& where) {
        if (t.inputs == 0) error(where, "a UDP needs at least one input");
        if (t.initial != '0' && t.initial != '1' && t.initial != 'x')
            error(where, "initial state must be 0, 1 or x");
        const auto level = [](char c) {
            return c == '0' || c == '1' || c == 'x' || c == '?' || c == 'b';
        };
        for (const UdpRow& r : t.rows) {
            std::uint32_t fields = 0;
            std::uint32_t edges = 0;
            bool ok = true;
            for (std::size_t i = 0; ok && i < r.inputs.size(); ++i, ++fields) {
                const char c = r.inputs[i];
                if (c == '(') {
                    ok = i + 3 < r.inputs.size() && level(r.inputs[i + 1]) &&
                         level(r.inputs[i + 2]) && r.inputs[i + 3] == ')';
                    i += 3;
                    ++edges;
                } else if (c == 'r' || c == 'f' || c == 'p' || c == 'n' || c == '*') {
                    ++edges;
                } else {
                    ok = level(c);
                }
            }
            const bool stateOk = t.sequential ? level(r.state) : r.state == 0;
            const bool outOk = r.output == '0' || r.output == '1' || r.output == 'x' ||
                               (t.sequential && r.output == '-');
            if (!ok || fields != t.inputs || !stateOk || !outOk || edges > (t.sequential ? 1U : 0U))
                error(where, "row \"" + r.inputs + "\" is not a row of this UDP");
        }
    }

    void checkSlots() {
        std::set<std::string> seen;
        for (std::size_t i = 0; i < d_.slots.size(); ++i) {
            const Slot& s = d_.slots[i];
            const std::string where = "slot %s" + std::to_string(i);
            if (s.name.empty()) error(where, "slot has no name");
            if (!seen.insert(s.name).second) error(where, "slot name is not unique");
            if (s.instance.index() >= d_.instances.size()) error(where, "instance out of range");
            if (s.subroutine != kNone && s.subroutine.index() >= d_.subroutines.size())
                error(where, "subroutine out of range");
            if (!typeRefOk(s.declType)) error(where, "type out of range");
            checkValueType(s.type, where);
            if (isContainer(s.type) && s.kind != SlotKind::Var)
                error(where, "a container slot must be a variable");
            if (s.type.kind == TypeKind::Void) error(where, "slot has type void");
            if (isIntegral(s.type) && s.type.width == 0) error(where, "integral slot has width 0");
            if (s.viewOf != kNone) {
                bool ok = s.viewOf.index() < i && s.kind != SlotKind::Var && s.driverOf == kNone &&
                          !s.dims.empty() &&
                          (isIntegral(s.type) || s.type.kind == TypeKind::Real ||
                           s.type.kind == TypeKind::ShortReal || s.type.kind == TypeKind::String ||
                           s.type.kind == TypeKind::Chandle);
                if (ok) {
                    const Slot& root = d_.slot(s.viewOf);
                    ok = root.viewOf == kNone && root.driverOf == kNone &&
                         root.kind != SlotKind::Var && root.type == s.type &&
                         root.dims.size() == s.dims.size();
                    for (std::size_t k = 0; ok && k < s.dims.size(); ++k) {
                        Slot a, b;
                        a.dims = {s.dims[k]};
                        b.dims = {root.dims[k]};
                        ok = elementCount(a) == elementCount(b);
                    }
                }
                if (!ok) error(where, "a net view must match an earlier physical net's shape");
            }
            if (s.driverOf != kNone) {
                bool ok = s.driverOf.index() < d_.slots.size() && s.viewOf == kNone;
                if (ok) {
                    const Slot& net = d_.slot(s.driverOf);
                    const bool scalar = s.dims.empty() && net.dims.empty() &&
                                        s.driverElements.empty() && s.type == net.type;
                    bool array =
                        !net.dims.empty() && s.dims.size() == 1 && s.dims[0].left == 0 &&
                        elementCount(s) == s.driverElements.size() && !s.driverElements.empty() &&
                        isIntegral(net.type) &&
                        s.type == (isFourState(net.type) ? net.type : otherDomain(net.type));
                    for (std::size_t k = 0; array && k < s.driverElements.size(); ++k)
                        array = s.driverElements[k] < elementCount(net) &&
                                (k == 0 || s.driverElements[k - 1] < s.driverElements[k]);
                    ok = net.kind != SlotKind::Var && net.viewOf == kNone &&
                         net.driverOf == kNone && (scalar || array);
                }
                if (!ok)
                    error(where, "a driver slot drives a net of its own type and valid coverage");
                if (s.strength1 > 7 || s.strength0 > 7) error(where, "strength out of range");
            } else if (!s.driverElements.empty()) {
                error(where, "driver elements require a driver net");
            }
            if (elementCount(s) == 0 || elementCount(s) > 0xffffffffULL)
                error(where, "array is too large");
        }
    }

    void checkStrengthNetwork() {
        const auto range = [](StrengthRange value) {
            return value.lo >= -7 && value.hi <= 7 && value.lo <= value.hi;
        };
        const auto coreKind = [](SlotKind kind) {
            return kind == SlotKind::Wire || kind == SlotKind::Tri || kind == SlotKind::Wand ||
                   kind == SlotKind::Triand || kind == SlotKind::Wor || kind == SlotKind::Trior ||
                   kind == SlotKind::Uwire;
        };
        std::set<std::tuple<SlotId, std::uint32_t, std::uint32_t>> targets;
        std::vector<std::uint64_t> covered(d_.slots.size());
        std::vector<std::uint32_t> roots(d_.strengthNodes.size());
        std::vector<bool> components(d_.strengthNodes.size());
        std::uint32_t maxComponent = 0;
        for (std::size_t i = 0; i < d_.strengthNodes.size(); ++i) {
            const auto& node = d_.strengthNodes[i];
            const auto& at = node.target;
            const auto where = "strength node !n" + std::to_string(i);
            if (node.loc.line == 0 || node.loc.column == 0 || node.loc.file >= d_.sources.size())
                error(where, "invalid source location", node.loc);
            if (!range(node.initial)) error(where, "invalid initial strength range", node.loc);
            if (!coreKind(node.kind))
                error(where, "implicit roots must be explicit origins", node.loc);
            if (node.component >= components.size())
                error(where, "component ID out of range", node.loc);
            else {
                components[node.component] = true;
                maxComponent = std::max(maxComponent, node.component);
            }
            if (at.slot.index() >= d_.slots.size()) {
                error(where, "net slot out of range", node.loc);
                continue;
            }
            const auto& slot = d_.slot(at.slot);
            if (at.firstElem >= elementCount(slot) || at.lsb >= slot.type.width ||
                at.numElems != 1 || at.width != 1 || slot.viewOf != kNone ||
                slot.driverOf != kNone || slot.kind == SlotKind::Var ||
                static_cast<unsigned>(slot.kind) > static_cast<unsigned>(SlotKind::Uwire) ||
                !isFourState(slot.type) || !isIntegral(slot.type) ||
                (slot.flags & kNeedsStrength) == 0 || (slot.flags & kForceTarget) != 0)
                error(where, "needs an eligible normalized physical net bit", node.loc);
            if (!targets.emplace(at.slot, at.firstElem, at.lsb).second)
                error(where, "duplicate physical node target", node.loc);
            ++covered[at.slot.index()];
        }
        if (!d_.strengthNodes.empty())
            for (std::uint32_t c = 0; c <= maxComponent; ++c)
                if (!components[c]) error("strength network", "component IDs must be dense");
        for (std::size_t i = 0; i < d_.slots.size(); ++i) {
            const auto& slot = d_.slots[i];
            if ((slot.flags & kForceTarget) != 0 &&
                isStrengthNet(d_, SlotId{static_cast<std::uint32_t>(i)}))
                error("slot %s" + std::to_string(i), "strength net holds are not connected yet");
            if ((slot.flags & kNeedsStrength) == 0) continue;
            const auto expected = std::uint64_t{slot.type.width} * elementCount(slot);
            if (slot.viewOf != kNone || slot.driverOf != kNone || slot.kind == SlotKind::Var ||
                !isFourState(slot.type) || !isIntegral(slot.type) || expected == 0 ||
                covered[i] != expected || (slot.flags & kForceTarget) != 0)
                error("slot %s" + std::to_string(i),
                      "strength nodes must cover the eligible physical net completely");
        }
        for (std::size_t i = 0; i < d_.strengthOrigins.size(); ++i) {
            const auto& origin = d_.strengthOrigins[i];
            const auto where = "strength origin !d" + std::to_string(i);
            if (origin.loc.line == 0 || origin.loc.column == 0 ||
                origin.loc.file >= d_.sources.size())
                error(where, "invalid source location", origin.loc);
            if (!range(origin.initial)) error(where, "invalid initial strength range", origin.loc);
            if (origin.node >= d_.strengthNodes.size())
                error(where, "node out of range", origin.loc);
            else if (++roots[origin.node] > 1 &&
                     d_.strengthNodes[origin.node].kind == SlotKind::Uwire)
                error(where, "uwire has multiple physical origins", origin.loc);
            if (origin.kind == StrengthOriginKind::Process) {
                if (origin.owner.index() >= d_.processes.size()) {
                    error(where, "process owner out of range", origin.loc);
                    continue;
                }
                const auto& owner = d_.process(origin.owner);
                if (owner.kind != ProcessKind::Cont || owner.parent.kind != Owner::Kind::None ||
                    std::ranges::find(owner.strengthWrites,
                                      StrengthOriginId{static_cast<std::uint32_t>(i)}) ==
                        owner.strengthWrites.end())
                    error(where, "origin needs its static cont owner's declared write", origin.loc);
            } else {
                const int rank = origin.kind == StrengthOriginKind::Pull     ? 5
                                 : origin.kind == StrengthOriginKind::Supply ? 7
                                                                             : 0;
                if (rank == 0 || origin.owner != kNone || origin.initial.lo != origin.initial.hi ||
                    (origin.initial.lo != rank && origin.initial.lo != -rank))
                    error(where, "implicit origin needs no owner and a fixed pull/supply point",
                          origin.loc);
            }
        }
        std::vector<std::uint32_t> parent(d_.strengthNodes.size());
        std::iota(parent.begin(), parent.end(), 0U);
        const auto find = [&](std::uint32_t node) {
            auto root = node;
            while (parent[root] != root)
                root = parent[root];
            while (parent[node] != node) {
                const auto next = parent[node];
                parent[node] = root;
                node = next;
            }
            return root;
        };
        for (std::size_t i = 0; i < d_.strengthEdges.size(); ++i) {
            const auto& edge = d_.strengthEdges[i];
            const auto where = "strength edge !e" + std::to_string(i);
            if (edge.loc.line == 0 || edge.loc.column == 0 || edge.loc.file >= d_.sources.size())
                error(where, "invalid source location", edge.loc);
            if (static_cast<unsigned>(edge.kind) >
                static_cast<unsigned>(StrengthEdgeKind::Rtranif1))
                error(where, "invalid passive edge kind", edge.loc);
            if (edge.first >= d_.strengthNodes.size() || edge.second >= d_.strengthNodes.size())
                error(where, "passive endpoint out of range", edge.loc);
            else {
                if (d_.strengthNodes[edge.first].component !=
                    d_.strengthNodes[edge.second].component)
                    error(where, "passive endpoints need the same publication component", edge.loc);
                parent[find(edge.first)] = find(edge.second);
            }
            if (conditionalStrengthEdge(edge.kind)) {
                if (edge.owner.index() >= d_.processes.size())
                    error(where, "conditional edge needs a static cont owner", edge.loc);
                else {
                    const auto& owner = d_.process(edge.owner);
                    if (owner.kind != ProcessKind::Cont || owner.parent.kind != Owner::Kind::None ||
                        std::ranges::find(owner.strengthEdgeWrites,
                                          StrengthEdgeId{static_cast<std::uint32_t>(i)}) ==
                            owner.strengthEdgeWrites.end())
                        error(where,
                              "conditional edge needs its static cont owner's declared write",
                              edge.loc);
                }
            } else if (edge.owner != kNone || edge.delayed)
                error(where, "unconditional edge has neither control owner nor delay", edge.loc);
        }
        if (!d_.strengthEdges.empty()) {
            std::vector<std::uint64_t> possible(d_.strengthNodes.size());
            for (const auto& origin : d_.strengthOrigins)
                if (origin.node < parent.size()) ++possible[find(origin.node)];
            for (std::uint32_t n = 0; n < d_.strengthNodes.size(); ++n)
                if (d_.strengthNodes[n].kind == SlotKind::Uwire && possible[find(n)] > 1)
                    error("strength node !n" + std::to_string(n),
                          "uwire has multiple potential physical origins", d_.strengthNodes[n].loc);
        }
    }

    void checkNames() {
        std::set<std::string> seen;
        for (const NameEntry& n : d_.names) {
            if (n.slot.index() >= d_.slots.size()) error("name " + n.path, "slot out of range");
            if (!seen.insert(n.path).second) error("name " + n.path, "name is not unique");
        }
    }

    // Rule 10: the dump table (SIR_SPEC.md 2, WAVEFORM.md 2).
    void checkDump() {
        for (std::size_t i = 0; i < d_.dumpScopes.size(); ++i) {
            const DumpScope& s = d_.dumpScopes[i];
            const std::string where = "dump scope " + std::to_string(i);
            if (s.name.empty() || s.path.empty()) error(where, "scope has no name");
            if (s.parent != kNone && s.parent >= i)
                error(where, "the scope around it must come before it");
        }
        for (std::size_t i = 0; i < d_.dumpVars.size(); ++i) {
            const DumpVar& v = d_.dumpVars[i];
            const std::string where = "dump variable " + std::to_string(i);
            if (v.scope >= d_.dumpScopes.size()) error(where, "scope out of range");
            if (i > 0 && v.scope < d_.dumpVars[i - 1].scope)
                error(where, "variables must come in the order of their scopes");
            if (v.name.empty()) error(where, "variable has no name");
            if (v.slot.index() >= d_.slots.size()) {
                error(where, "slot out of range");
                continue;
            }
            const Slot& s = d_.slot(v.slot);
            if (!s.dims.empty()) error(where, "an unpacked array is not dumped");
            const bool real = v.type == DumpType::Real || v.type == DumpType::Realtime;
            const bool realSlot =
                s.type.kind == TypeKind::Real || s.type.kind == TypeKind::ShortReal;
            if (real                        ? !realSlot
                : v.type == DumpType::Event ? s.type.kind != TypeKind::Event
                                            : !isIntegral(s.type) || v.width != s.type.width)
                error(where, "the VCD type or size does not fit the slot");
        }
    }

    void checkConstants() {
        for (std::size_t i = 0; i < d_.constants.size(); ++i) {
            const Constant& c = d_.constants[i];
            const std::string where = "const #" + std::to_string(i);
            checkValueType(c.type, where);
            if (isContainer(c.type)) {
                if (c.aWords.size() != 1 || c.aWords[0] != 0 || !c.bWords.empty() ||
                    !c.bytes.empty())
                    error(where, "a container constant must be empty");
                continue;
            }
            if (isString(c.type)) {
                if (!c.aWords.empty() || !c.bWords.empty())
                    error(where, "string constant has words");
                continue;
            }
            if (!c.bytes.empty()) error(where, "only string constants have bytes");
            if (c.type.kind == TypeKind::Event) {
                // Event identities are created by event.new or loaded from
                // typed storage; a constant cannot forge a registry token.
                if (c.aWords.size() != 1 || c.aWords[0] != 0 || !c.bWords.empty())
                    error(where, "an event constant must be null");
                continue;
            }
            if (c.type.kind == TypeKind::Chandle) {
                // Only null (VALUE_SEMANTICS.md 7.8).
                if (c.aWords.size() != 1 || c.aWords[0] != 0 || !c.bWords.empty())
                    error(where, "a chandle constant is null");
                continue;
            }
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
            const std::size_t words = integralWords(c.type.width);
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
        return validSlotRange(d_, {s, firstElem, numElems, lsb, width});
    }
    bool rangeOk(const SlotRange& r) const {
        return rangeOk(r.slot, r.firstElem, r.numElems, r.lsb, r.width);
    }
    bool rangeOk(const Operand& o) const {
        return rangeOk(o.asSlot(), o.firstElem, o.numElems, o.lsb, o.width);
    }

    /// "assign" or "force", the levels of SIR_SPEC.md 5.6.
    static bool levelOk(const Operand& o) {
        return o.kind == Operand::Kind::Text && (o.text == "assign" || o.text == "force");
    }
    /// A target of assign or force: bits of an integral slot marked as a
    /// force target, in one element of an array (SIR_SPEC.md 5.6).
    bool heldOk(const Operand& o) const {
        if (!rangeOk(o)) return false;
        const Slot& s = d_.slots[o.id];
        return isIntegral(s.type) && (s.flags & kForceTarget) != 0 &&
               (s.dims.empty() || o.numElems == 1);
    }
    /// The bits of one element a slot operand covers.
    std::uint32_t rangeWidth(const Operand& o) const {
        return o.width == 0 ? d_.slots[o.id].type.width : o.width;
    }

    /// The number of elements of an array argument, or 0 for one that is
    /// not an array.
    std::uint64_t paramElements(const Subroutine& s, const Param& p) const {
        if (p.slot != kNone && p.slot.index() < d_.slots.size())
            return d_.slot(p.slot).dims.empty() ? 0 : elementCount(d_.slot(p.slot));
        if (p.frameVar.index() < s.frame.size())
            return s.frame[p.frameVar.index()].dims.empty()
                       ? 0
                       : elementCount(s.frame[p.frameVar.index()]);
        return 0;
    }
    /// The number of elements an array actual argument covers: a whole
    /// array slot, a run of its elements, or an array frame variable.
    std::uint64_t argElements(const Body& body, const Operand& o) const {
        if (o.kind == Operand::Kind::Slot) {
            if (!rangeOk(o) || d_.slots[o.id].dims.empty() || o.lsb != 0 || o.width != 0) return 0;
            return o.numElems != 0 ? o.numElems : elementCount(d_.slots[o.id]);
        }
        if (o.kind == Operand::Kind::Frame && o.id < body.frame->size())
            return (*body.frame)[o.id].dims.empty() ? 0 : elementCount((*body.frame)[o.id]);
        return 0;
    }
    /// The element type of an array slot or frame variable operand.
    Type elemType(const Body& body, const Operand& o) const {
        if (o.kind == Operand::Kind::Slot && o.id < d_.slots.size()) return d_.slots[o.id].type;
        if (o.kind == Operand::Kind::Frame && o.id < body.frame->size())
            return (*body.frame)[o.id].type;
        return voidType();
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
        if (s.instance.index() >= d_.instances.size()) error(where, "instance out of range", s.loc);
        Body body;
        body.where = where;
        body.owner = Owner::of(id);
        body.blocks = &s.blocks;
        body.frame = &s.frame;
        body.numTemps = s.numTemps;
        body.subroutine = &s;
        body.kindText = kindName(s.kind);
        body.waitsAllowed = s.kind == SubroutineKind::Task && s.mayWait;
        checkValueType(s.returnType, where, s.loc);
        if (s.mayWait && s.kind != SubroutineKind::Task) error(where, "only tasks may wait", s.loc);
        if (s.kind == SubroutineKind::Task && s.returnType.kind != TypeKind::Void)
            error(where, "a task returns no value", s.loc);
        for (const FrameVar& f : s.frame) {
            if (!typeRefOk(f.declType)) error(where, "frame variable type out of range");
            if (isIntegral(f.type) && f.type.width == 0)
                error(where, "integral frame variable has width 0");
        }
        if (readsWaiterFrame(s.kind)) {
            const std::string a = std::string("a ") + kindName(s.kind);
            if (!s.params.empty()) error(where, a + " has no arguments", s.loc);
            if (s.kind == SubroutineKind::Guard && !width1(s.returnType))
                error(where, "a guard returns a 1-bit value", s.loc);
            // A watch returns the value of an event expression (SIR_SPEC.md 7.2).
            if (s.kind == SubroutineKind::Watch && !isIntegral(s.returnType))
                error(where, "a watch returns an integral value", s.loc);
            const Owner& o = s.frameOf;
            if (o.kind == Owner::Kind::Process && o.id < d_.processes.size()) {
                body.frame = &d_.processes[o.id].frame;
            } else if (o.kind == Owner::Kind::Subroutine && o.id < d_.subroutines.size() &&
                       o.id != id.index()) {
                body.frame = &d_.subroutines[o.id].frame;
            } else {
                error(where, a + " needs the process or subroutine whose frame it reads", s.loc);
            }
        } else if (s.frameOf.kind != Owner::Kind::None) {
            error(where, "only guards read another frame", s.loc);
        }
        for (const Param& p : s.params) {
            checkValueType(p.type, where, s.loc);
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
                if (p.slot.index() >= d_.slots.size() || !fits(d_.slot(p.slot).type, p.type) ||
                    p.dir == Direction::Ref || p.dir == Direction::ConstRef)
                    error(where, "argument '" + p.name + "' slot is invalid", s.loc);
            } else if (p.frameVar.index() >= s.frame.size()) {
                error(where, "argument '" + p.name + "' frame variable is invalid", s.loc);
            } else {
                const FrameVar& f = s.frame[p.frameVar.index()];
                const bool ref = p.dir == Direction::Ref || p.dir == Direction::ConstRef;
                const bool badRef =
                    f.type.kind != TypeKind::Ref ||
                    (f.type.detail != kNone && f.target != p.type) ||
                    (isContainer(p.type) &&
                     f.type != (f.dims.empty() ? refType(p.type.detail) : refType()));
                if (ref ? badRef : !fits(f.type, p.type))
                    error(where, "argument '" + p.name + "' frame variable is invalid", s.loc);
            }
        }
        checkBody(body);
    }

    void checkProcess(const Process& p, ProcessId id) {
        const std::string where = processWhere(id) + " " + p.name;
        if (p.instance.index() >= d_.instances.size()) error(where, "instance out of range", p.loc);
        if (p.netResolver && (p.kind != ProcessKind::Cont || p.parent.kind != Owner::Kind::None))
            error(where, "a net resolver must be a top-level cont process", p.loc);
        if (p.outermostScope) {
            if (*p.outermostScope >= d_.numScopes)
                error(where, "outermost scope out of range", p.loc);
            if (p.kind == ProcessKind::Cont || p.kind == ProcessKind::StaticInit ||
                p.kind == ProcessKind::Override || p.kind == ProcessKind::Postponed)
                error(where, "outermost scope needs a procedure or fork process", p.loc);
            bool entered = false;
            for (const auto& block : p.blocks)
                for (const auto& in : block.instrs)
                    entered = entered || (in.op == Op::ScopeEnter && !in.operands.empty() &&
                                          in.operands[0].kind == Operand::Kind::Scope &&
                                          in.operands[0].id == *p.outermostScope);
            if (!entered) error(where, "outermost scope has no entry in the process", p.loc);
        }
        for (const Trigger& t : p.triggers) {
            if (!rangeOk(t.range)) error(where, "trigger slot range is invalid");
            if (t.guard == kNone) continue;
            // An iff guard reads the frame of the process (SIR_SPEC.md 7.2).
            const std::string g = "trigger iff @" + std::to_string(t.guard.index());
            if (t.guard.index() >= d_.subroutines.size() ||
                d_.subroutine(t.guard).kind != SubroutineKind::Guard)
                error(where, g + " is not a guard", p.loc);
            else if (const Owner& o = d_.subroutine(t.guard).frameOf;
                     o.kind != Owner::Kind::Process || o.asProcess() != id)
                error(where, g + " reads another frame", p.loc);
        }
        for (const SlotRange& w : p.writes) {
            if (!rangeOk(w)) error(where, "write range is invalid");
            if (isStrengthNet(d_, w.slot))
                error(where, "only the component publisher writes a strength net", p.loc);
        }
        for (const auto& trigger : p.strengthTriggers)
            if (p.kind != ProcessKind::Cont || !rangeOk(trigger) ||
                !isStrengthNet(d_, trigger.slot))
                error(where, "strength sensitivity needs a cont process and registered net range",
                      p.loc);
        std::set<StrengthOriginId> origins;
        for (const auto origin : p.strengthWrites) {
            if (!origins.insert(origin).second) error(where, "duplicate strength write", p.loc);
            if (origin.index() >= d_.strengthOrigins.size() ||
                d_.strengthOrigins[origin.index()].kind != StrengthOriginKind::Process ||
                d_.strengthOrigins[origin.index()].owner != id)
                error(where, "strength write does not name an owned process origin", p.loc);
        }
        std::set<StrengthEdgeId> edges;
        for (const auto edge : p.strengthEdgeWrites) {
            if (!edges.insert(edge).second) error(where, "duplicate strength edge write", p.loc);
            if (edge.index() >= d_.strengthEdges.size() ||
                !conditionalStrengthEdge(d_.strengthEdges[edge.index()].kind) ||
                d_.strengthEdges[edge.index()].owner != id)
                error(where, "strength edge write does not name an owned conditional edge", p.loc);
        }
        // Only a cont process drives, and only what it writes (SIR_SPEC.md
        // 3.4, 10).
        for (const SlotRange& w : p.drives)
            if (p.kind != ProcessKind::Cont)
                error(where, "only a cont process drives", p.loc);
            else if (std::ranges::find(p.writes, w) == p.writes.end())
                error(where, "drives a range it does not write", p.loc);
        if (p.kind == ProcessKind::Ff && p.triggers.empty())
            error(where, "always_ff needs exactly one event control", p.loc);
        // A postponed process runs when $strobe or $monitor asks; a write
        // it makes is the design's run-time error, which the run reports
        // (SCHEDULER_SPEC.md 2.2).
        if (p.kind == ProcessKind::Postponed && !p.triggers.empty())
            error(where, "a postponed process has no triggers", p.loc);
        for (const FrameVar& f : p.frame) {
            if (!typeRefOk(f.declType)) error(where, "frame variable type out of range");
            if (isIntegral(f.type) && f.type.width == 0)
                error(where, "integral frame variable has width 0");
        }
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
        body.owner = Owner::of(id);
        body.blocks = &p.blocks;
        body.frame = &p.frame;
        body.numTemps = p.numTemps;
        body.process = &p;
        body.kindText = std::string(kindName(p.kind)) + " process";
        body.waitsAllowed = !waitsForbidden(p.kind);
        checkBody(body);
    }

    void checkBody(const Body& body) {
        for (const FrameVar& f : *body.frame) {
            checkValueType(f.type, body.where);
            checkValueType(f.target, body.where);
            if (f.type.kind == TypeKind::Ref && (f.type.detail != kNone || isContainer(f.target)) &&
                ((f.type.detail != kNone && f.target != containerType(f.type.detail)) ||
                 f.type != (isContainer(f.target) && f.dims.empty() ? refType(f.target.detail)
                                                                    : refType())))
                error(body.where, "container ref target differs from its descriptor");
        }
        const auto& blocks = *body.blocks;
        if (blocks.empty()) {
            error(body.where, "has no blocks");
            return;
        }
        const std::size_t nb = blocks.size();
        const auto bw = [&](std::size_t b) { return body.where + " bb" + std::to_string(b); };
        if (!blocks[0].params.empty()) error(bw(0), "the entry block has parameters");

        // The graph below numbers the blocks as plain indices into its own
        // vectors; a BlockId of an operand becomes one only here.
        using Block = std::uint32_t;

        // Rule 3: each block ends with exactly one terminator; targets exist.
        // waitEdge[b] is set when b ends with a wait, so every edge out of
        // b crosses a wait.
        std::vector<std::vector<Block>> succ(nb);
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
        std::vector<Block> work{0};
        reach[0] = true;
        while (!work.empty()) {
            const Block b = work.back();
            work.pop_back();
            for (const Block s : succ[b])
                if (!reach[s]) reach[s] = true, work.push_back(s);
        }
        for (std::size_t b = 0; b < nb; ++b)
            if (!reach[b]) error(bw(b), "block is unreachable");

        // Dominators (iterative; bodies are small).
        std::vector<std::vector<Block>> pred(nb);
        for (std::size_t b = 0; b < nb; ++b)
            for (const Block s : succ[b])
                pred[s].push_back(static_cast<Block>(b));
        std::vector<std::vector<bool>> dom(nb, std::vector<bool>(nb, true));
        dom[0].assign(nb, false);
        dom[0][0] = true;
        for (bool changed = true; changed;) {
            changed = false;
            for (std::size_t b = 1; b < nb; ++b) {
                if (!reach[b]) continue;
                std::vector<bool> nd(nb, true);
                bool any = false;
                for (const Block q : pred[b]) {
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
            if (t.index() >= nt) {
                error(bw(b), "temp %t" + std::to_string(t.index()) + " out of range", loc);
            } else if (defBlock[t.index()] >= 0) {
                error(bw(b), "temp %t" + std::to_string(t.index()) + " is defined twice", loc);
            } else {
                defBlock[t.index()] = static_cast<int>(b);
                defIndex[t.index()] = k;
                tempType[t.index()] = ty;
            }
        };
        for (std::size_t b = 0; b < nb; ++b) {
            for (const BlockParam& prm : blocks[b].params) {
                if (prm.type.kind == TypeKind::Void) error(bw(b), "parameter has type void");
                checkValueType(prm.type, bw(b), {}, true);
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
                std::vector<std::pair<Block, bool>> todo;
                for (const Block s : succ[d])
                    todo.emplace_back(s, waitEdge[d]);
                while (!todo.empty()) {
                    const auto [b, waited] = todo.back();
                    todo.pop_back();
                    if (b == d || seen[waited ? 1 : 0][b]) continue;
                    seen[waited ? 1 : 0][b] = true;
                    if (waited) crossed[d][b] = true;
                    for (const Block s : succ[b])
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
        const SlotRange whole{ops[0].asSlot(), 0, 0, 0, 0};
        if (ops[0].id >= d_.slots.size()) return whole;
        const Slot& slot = d_.slots[ops[0].id];
        const bool bits =
            in.op == Op::StoreBits || in.op == Op::NbaBits || in.op == Op::NbaBitsDelay;
        const bool delayed = in.op == Op::NbaElemDelay || in.op == Op::NbaBitsDelay;
        // slot, indices..., [position,] value
        const std::size_t extra = (bits ? 3U : 2U) + (delayed ? 1U : 0U);
        if (ops.size() < extra) return whole;
        const std::size_t nIdx = ops.size() - extra;
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
        SlotRange t{ops[0].asSlot(), static_cast<std::uint32_t>(elem), nIdx == 0 ? 0U : 1U,
                    bits ? ops[0].lsb : 0, bits ? ops[0].width : 0};
        if (bits) {
            const std::optional<std::int64_t> pos = constIndex(ops[1 + nIdx]);
            const std::uint32_t w =
                operandType(body, ops[ops.size() - (delayed ? 2 : 1)], tempType).width;
            const std::uint32_t limit = ops[0].width ? ops[0].width : slot.type.width;
            if (pos && *pos >= 0 && w != 0 && *pos <= limit && w <= limit - *pos) {
                t.lsb = ops[0].lsb + static_cast<std::uint32_t>(*pos);
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

        checkValueType(in.type, bw, in.loc, true);
        if (hasNoResult(in.op)) {
            if (in.result != kNone || in.type.kind != TypeKind::Void) bad("must not have a result");
        } else if ((in.result == kNone) != (in.type.kind == TypeKind::Void)) {
            bad("result and result type must come together");
        } else if (in.result == kNone && !resultOptional(in.op)) {
            bad("needs a result");
        } else if (in.result != kNone && isIntegral(in.type) && in.type.width == 0) {
            bad("the result has width 0");
        }

        const auto& ops = in.operands;
        const auto ty = [&](std::size_t i) { return operandType(body, ops[i], tempType); };
        const auto isValue = [&](std::size_t i) {
            const auto k = ops[i].kind;
            return k == Operand::Kind::Temp || k == Operand::Kind::Const;
        };
        const auto rawValue = [&](std::size_t i) {
            const auto& o = ops[i];
            return isValue(i) && o.edge == Edge::Any && o.firstElem == 0 && o.numElems == 0 &&
                   o.lsb == 0 && o.width == 0 && o.count == 0 && o.text.empty();
        };
        const auto nOps = [&](std::size_t n) {
            if (ops.size() != n) bad("expects " + std::to_string(n) + " operands");
            return ops.size() == n;
        };
        const Type rt = in.type;
        const auto eventTarget = [&](std::size_t i) {
            if (ty(i) != Type{TypeKind::Event, 0}) return false;
            if (ops[i].kind == Operand::Kind::Slot) return true;
            const auto& o = ops[i];
            return isValue(i) && o.edge == Edge::Any && o.firstElem == 0 && o.numElems == 0 &&
                   o.lsb == 0 && o.width == 0 && o.count == 0;
        };
        // A delay counts ticks, an integral value of any width; 2^64 or more
        // takes the time past the largest time when it runs (SIR_SPEC.md 1).
        const auto delayOk = [&](std::size_t i) { return isIntegral(ty(i)); };

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
                case Operand::Kind::NbaLayout:
                    if (o.id >= d_.nbaLayouts.size()) bad("NBA layout out of range");
                    break;
                case Operand::Kind::Pattern:
                    if (o.id >= d_.patterns.size()) bad("pattern layout out of range");
                    break;
                case Operand::Kind::Type:
                    if (o.id >= d_.types.size()) bad("type out of range");
                    break;
                case Operand::Kind::Const:
                    if (o.id >= d_.constants.size()) bad("constant out of range");
                    break;
                default: break;
            }
        }

        // Complete strength values are owned SSA metadata, not ordinary
        // operands, storage, handles or a new arithmetic state domain.
        if (rt.kind == TypeKind::Strength && in.op != Op::StrengthMake &&
            in.op != Op::StrengthMos && in.op != Op::StrengthLoad)
            bad("only strength.make/mos/load produce strength metadata");
        for (std::size_t i = 0; i < ops.size(); ++i) {
            if (ty(i).kind != TypeKind::Strength) continue;
            const auto& o = ops[i];
            if (o.kind != Operand::Kind::Temp || o.edge != Edge::Any || o.firstElem != 0 ||
                o.numElems != 0 || o.lsb != 0 || o.width != 0 || o.count != 0)
                bad("strength operands must be whole SSA values");
            if (in.op != Op::StrengthValue && in.op != Op::StrengthMos &&
                in.op != Op::StrengthDrive && in.op != Op::StrengthDriveDelay && in.op != Op::Br &&
                in.op != Op::BrIf)
                bad("strength metadata is not an ordinary operand");
        }

        for (std::size_t i = 0; i < ops.size(); ++i) {
            const auto& operand = ops[i];
            if (operand.kind == Operand::Kind::NbaLayout && (in.op != Op::NbaEvent || i != 0))
                bad("NBA layout IDs are only the first operand of nba.event");
            if (operand.kind == Operand::Kind::StrengthOrigin &&
                ((in.op != Op::StrengthDrive && in.op != Op::StrengthDriveDelay) || i != 0))
                bad("physical origin IDs are not ordinary operands");
            if (operand.kind == Operand::Kind::StrengthEdge &&
                ((in.op != Op::StrengthEdge && in.op != Op::StrengthEdgeDelay) || i != 0))
                bad("passive edge IDs are not ordinary operands");
            if (operand.kind != Operand::Kind::Slot || !isStrengthNet(d_, operand.asSlot()))
                continue;
            bool readOnly = in.op == Op::Load || in.op == Op::LoadElem ||
                            in.op == Op::StrengthLoad || in.op == Op::WaitEvent;
            if ((in.op == Op::Call || in.op == Op::CallTask) && !ops.empty() &&
                ops[0].kind == Operand::Kind::Subroutine && ops[0].id < d_.subroutines.size() &&
                i > 0 && i <= d_.subroutines[ops[0].id].params.size()) {
                const auto dir = d_.subroutines[ops[0].id].params[i - 1].dir;
                readOnly = dir == Direction::Input || dir == Direction::ConstRef;
            }
            if (in.op == Op::Syscall && !ops.empty() && ops[0].kind == Operand::Kind::Text) {
                const auto& name = ops[0].text;
                static constexpr std::string_view calls[]{
                    "$display",   "$displayb",  "$displayo", "$displayh", "$write",
                    "$writeb",    "$writeo",    "$writeh",   "$fdisplay", "$fdisplayb",
                    "$fdisplayo", "$fdisplayh", "$fwrite",   "$fwriteb",  "$fwriteo",
                    "$fwriteh",   "$sformatf",  "$dumpvars", "$dumpports"};
                readOnly = std::ranges::find(calls, name) != std::end(calls);
            }
            if (!readOnly)
                bad("a strength net operand must be read-only; only its component publishes it");
        }

        // Rule 6, first half: a process declares every slot it writes.
        if (writesSlot(in.op) && body.process != nullptr && !ops.empty() &&
            ops[0].kind == Operand::Kind::Slot) {
            const Operand& o = ops[0];
            SlotRange target{o.asSlot(), o.firstElem, o.numElems, o.lsb, o.width};
            if (in.op != Op::Store && in.op != Op::Nba && in.op != Op::NbaDelay &&
                in.op != Op::DriveDelay)
                target = elementTarget(body, in, tempType);
            bool covered = false;
            for (const SlotRange& w : body.process->writes)
                covered = covered || rangeCovered(d_.slots[o.id], w, target);
            if (!covered) bad("writes %s" + std::to_string(o.id) + " outside the write set");
        }

        // Element operations: the slot is an array and there is one index
        // per dimension (rule 11). Returns the number of indices.
        const auto elemSlot = [&]() -> const Slot* {
            if (ops.empty() || ops[0].kind != Operand::Kind::Slot || ops[0].id >= d_.slots.size())
                return nullptr;
            const Operand& o = ops[0];
            const bool bits =
                in.op == Op::StoreBits || in.op == Op::NbaBits || in.op == Op::NbaBitsDelay;
            if ((!bits && (o.lsb != 0 || o.width != 0)) || o.firstElem != 0 || o.numElems != 0)
                return nullptr;
            return &d_.slots[o.id];
        };
        // The element type and dimensions of the first operand of an
        // element or bit operation: a whole slot, or a frame variable
        // (SIR_SPEC.md 5.3, 7.4). A ref argument gives the dimensions of
        // what it refers to and no type.
        struct Base {
            Type type;
            const std::vector<ArrayDim>* dims = nullptr;
            bool frame = false;
        };
        const auto elemBase = [&]() -> std::optional<Base> {
            if (const Slot* s = elemSlot()) return Base{s->type, &s->dims, false};
            if (ops.empty() || ops[0].kind != Operand::Kind::Frame ||
                ops[0].id >= body.frame->size())
                return std::nullopt;
            const FrameVar& f = (*body.frame)[ops[0].id];
            return Base{f.type, &f.dims, true};
        };
        const auto indicesOk = [&](std::size_t first, std::size_t n) {
            for (std::size_t i = first; i < first + n; ++i)
                if (!isIntegral(ty(i))) return false;
            return true;
        };
        const auto windowOk = [&](Type t) {
            const Operand& o = ops[0];
            const std::uint32_t width = o.width ? o.width : t.width;
            return o.firstElem == 0 && o.numElems == 0 && o.lsb <= t.width &&
                   width <= t.width - o.lsb;
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
                if (prm.dir == Direction::Ref || prm.dir == Direction::ConstRef) {
                    // A ref made by ref.make (SIR_SPEC.md 8).
                    if (!isValue(i) || ty(i).kind != TypeKind::Ref ||
                        (isContainer(prm.type) && paramElements(callee, prm) == 0 &&
                         !equivalentValueTypes(d_, ty(i), refType(prm.type.detail))))
                        bad("argument '" + prm.name + "' must be a ref");
                    continue;
                }
                if (paramElements(callee, prm) != 0) {
                    // A fixed-size unpacked array: a whole array slot, a
                    // run of its elements, or an array frame variable, with
                    // as many elements of the formal's type.
                    if (!place || argElements(body, ops[i]) != paramElements(callee, prm) ||
                        !fits(prm.type, elemType(body, ops[i])))
                        bad("argument '" + prm.name + "' must be an array of its shape");
                    continue;
                }
                const bool want = prm.dir != Direction::Input;
                if (want ? !place : !isValue(i))
                    bad("argument '" + prm.name + "' must be " +
                        (want ? "a slot or frame variable" : "a value"));
                else if (!fits(prm.type, ty(i)))
                    bad("argument '" + prm.name + "' has the wrong type");
            }
        };

        // Rule 1: the operands of value instructions are values, temps or
        // constants (SIR_SPEC.md 5.1); a slot is read with load. The tiers
        // read these operands as values only.
        switch (in.op) {
            case Op::Add:
            case Op::Sub:
            case Op::Mul:
            case Op::Div:
            case Op::Pow:
            case Op::Mod:
            case Op::And:
            case Op::Or:
            case Op::Xor:
            case Op::Xnor:
            case Op::Neg:
            case Op::Not:
            case Op::Lt:
            case Op::Le:
            case Op::Gt:
            case Op::Ge:
            case Op::Eq:
            case Op::Ne:
            case Op::Weq:
            case Op::Wne:
            case Op::Ceq:
            case Op::Cne:
            case Op::Lnot:
            case Op::Truth:
            case Op::Rand:
            case Op::Rnand:
            case Op::Ror:
            case Op::Rnor:
            case Op::Rxor:
            case Op::Rxnor:
            case Op::Land:
            case Op::Lor:
            case Op::Limp:
            case Op::Leqv:
            case Op::Shl:
            case Op::Shr:
            case Op::Ashr:
            case Op::Select:
            case Op::Bitsel:
            case Op::Partsel:
            case Op::IdxPartsel:
            case Op::Repl:
            case Op::Zext:
            case Op::Sext:
            case Op::Trunc:
            case Op::To2:
            case Op::To4:
            case Op::Itor:
            case Op::Rtoi:
            case Op::Rconv:
                for (std::size_t i = 0; i < ops.size(); ++i)
                    if (!isValue(i)) {
                        bad("operands must be values");
                        break;
                    }
                break;
            default: break;
        }

        // Rule 1: operand and result types.
        switch (in.op) {
            case Op::FormatPattern: {
                if (rt != Type{TypeKind::String, 0}) bad("pattern result must be string");
                if (ops.size() < 2 || ops[0].kind != Operand::Kind::Pattern ||
                    ops[0].id >= d_.patterns.size() || ops[1].kind != Operand::Kind::Text ||
                    (ops[1].text != "named" && ops[1].text != "positional")) {
                    bad("pattern needs a layout and named/positional mode");
                    break;
                }
                std::vector<Type> actual;
                for (std::size_t i = 2; i < ops.size(); ++i) {
                    if (!isValue(i)) bad("pattern payload must be a value");
                    actual.push_back(ty(i));
                }
                if (patterns_.errors.empty() &&
                    !patternArguments(d_, PatternId{ops[0].id}, patterns_.shapes, actual))
                    bad("pattern payload does not match the layout");
                break;
            }
            case Op::ContainerNew:
            case Op::ContainerCopy:
            case Op::ContainerSnapshot:
            case Op::ContainerPack: {
                const Type* element = containerElement(rt);
                if (!element) {
                    bad("result needs a container descriptor");
                    break;
                }
                if (in.op == Op::ContainerPack) {
                    for (std::size_t i = 0; i < ops.size(); ++i)
                        if (!isValue(i) || !equivalentValueTypes(d_, ty(i), *element))
                            bad("packed value must have the container element type");
                    break;
                }
                const bool make = in.op == Op::ContainerNew;
                if (!nOps(make ? 2 : 1)) break;
                const std::size_t source = make ? 1 : 0;
                const Type* from = containerElement(ty(source));
                if (!isValue(source) || !from || !equivalentValueTypes(d_, *from, *element))
                    bad("source must be a container with the same element type");
                if (in.op == Op::ContainerSnapshot && rt != ty(0))
                    bad("snapshot must preserve its source value type");
                if (make && (rt.kind != TypeKind::DynArray || !isValue(0) || ty(0) != s4(64)))
                    bad("construction needs a dynamic array and a signed four-state 64-bit size");
                break;
            }
            case Op::ContainerSize:
            case Op::ContainerRead: {
                const bool read = in.op == Op::ContainerRead;
                if (!nOps(read ? 2 : 1)) break;
                const Type* element = containerElement(ty(0));
                if (!isValue(0) || !element) {
                    bad("needs a container value");
                    break;
                }
                if (rt != (read ? *element : u2(64))) bad("incorrect container result type");
                if (read && (!isValue(1) || !isIntegral(ty(1)) || ty(1).width == 0))
                    bad("element index must be integral");
                break;
            }
            case Op::ContainerCell:
            case Op::ContainerAssign:
            case Op::ContainerWrite:
            case Op::ContainerClear:
            case Op::ContainerSwapValues:
            case Op::QueueInsert:
            case Op::QueuePushFront:
            case Op::QueuePushBack:
            case Op::QueuePopFront:
            case Op::QueuePopBack:
            case Op::QueueErase: {
                const bool indexed = in.op == Op::ContainerCell || in.op == Op::ContainerWrite ||
                                     in.op == Op::QueueInsert || in.op == Op::QueueErase;
                const bool writes = in.op == Op::ContainerWrite || in.op == Op::QueueInsert ||
                                    in.op == Op::QueuePushFront || in.op == Op::QueuePushBack;
                const bool assign = in.op == Op::ContainerAssign;
                const bool swap = in.op == Op::ContainerSwapValues;
                const std::size_t n = swap ? 4 : 2 + (indexed ? 1 : 0) + (writes || assign ? 1 : 0);
                if (!nOps(n)) break;
                if (ops[0].kind != Operand::Kind::Type) {
                    bad("needs a container type row");
                    break;
                }
                const Type receiver = containerType(ops[0].asType());
                const Type* element = containerElement(receiver);
                if (!element) {
                    bad("needs a container type row");
                    break;
                }
                if (!isValue(1) || ty(1) != refType(receiver.detail))
                    bad("receiver must be a captured ref to this container type");
                const bool queue = in.op != Op::ContainerCell && in.op != Op::ContainerWrite &&
                                   in.op != Op::ContainerClear && !assign && !swap;
                if (queue && receiver.kind != TypeKind::Queue) bad("method needs a queue receiver");
                if (indexed && (!isValue(2) || !isIntegral(ty(2)) || ty(2).width == 0))
                    bad("element index must be integral");
                if (swap)
                    for (std::size_t i = 2; i < 4; ++i)
                        if (!isValue(i) || !isIntegral(ty(i)) || ty(i).width == 0)
                            bad("swap index must be integral");
                if (assign) {
                    const Type* source = containerElement(ty(2));
                    if (!isValue(2) || !source || !equivalentValueTypes(d_, *source, *element))
                        bad("assignment source must have an equivalent element type");
                }
                if (writes && (!isValue(n - 1) || !equivalentValueTypes(d_, ty(n - 1), *element)))
                    bad("written value must have the container element type");
                if (in.op == Op::ContainerCell) {
                    const Type want =
                        refType(isContainer(*element) ? element->detail : TypeId{kNone});
                    if (rt != want) bad("cell ref result must describe the element target");
                } else if ((in.op == Op::QueuePopFront || in.op == Op::QueuePopBack) &&
                           rt != *element) {
                    bad("pop result must have the container element type");
                }
                break;
            }
            case Op::Load:
                if (!nOps(1)) break;
                if (ops[0].kind != Operand::Kind::Slot)
                    bad("bad operands");
                else if (ty(0) != rt)
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
            case Op::NbaEvent: {
                if (ops.size() < 2 || ops[0].kind != Operand::Kind::NbaLayout ||
                    ops[0].id >= d_.nbaLayouts.size()) {
                    bad("needs a valid NBA layout and a typed repeat count");
                    break;
                }
                const auto& layout = d_.nbaLayouts[ops[0].id];
                if (!rawValue(1) || !isIntegral(ty(1)) || ty(1).width == 0)
                    bad("repeat count must be a captured integral value");
                if (ops.size() != layout.values.size() + 2) {
                    bad("captured operand count differs from layout");
                    break;
                }
                for (std::size_t i = 0; i < layout.values.size(); ++i)
                    if (!rawValue(i + 2) || ty(i + 2) != layout.values[i])
                        bad("captured value type differs from layout");
                if (body.process != nullptr)
                    for (const auto& target : nbaWrites(layout)) {
                        if (target.slot.index() >= d_.slots.size()) continue;
                        bool covered = false;
                        for (const auto& write : body.process->writes)
                            covered = covered || rangeCovered(d_.slot(target.slot), write, target);
                        if (!covered) bad("NBA target is outside the write set");
                    }
                break;
            }
            case Op::NbaDelay:
                if (nOps(3)) {
                    if (ops[0].kind != Operand::Kind::Slot || !isValue(1) || !isValue(2))
                        bad("bad operands");
                    else if (!fits(ty(0), ty(1)))
                        bad("value type " + toString(ty(1)) + " does not fit " + toString(ty(0)));
                    else if (!delayOk(2))
                        bad("the delay must be integral");
                }
                break;
            case Op::DriveDelay: {
                if (body.process == nullptr || body.process->kind != ProcessKind::Cont)
                    bad("only a cont process writes with it");
                if (ops.size() < 5 || (ops.size() - 5) % 2 != 0) {
                    bad("expects five operands followed by complete side target/value pairs");
                    break;
                }
                if (ops[0].kind != Operand::Kind::Slot || !isValue(1) || !fits(ty(0), ty(1)) ||
                    !isIntegral(ty(0)) || !delayOk(2) || !delayOk(3) || !delayOk(4))
                    bad("needs an integral slot range, a value of its type and three integral "
                        "delays");
                for (std::size_t i = 5; i + 1 < ops.size(); i += 2) {
                    const auto& o = ops[i];
                    if (o.kind != Operand::Kind::Slot || o.id >= d_.slots.size()) {
                        bad("side target must be a scalar two-state slot bit");
                        continue;
                    }
                    const Slot& slot = d_.slots[o.id];
                    if (!slot.dims.empty() || !isIntegral(slot.type) || isFourState(slot.type) ||
                        o.width != 1 || o.firstElem != 0 || o.numElems > 1 || !isValue(i + 1) ||
                        !fits(ty(i), ty(i + 1)))
                        bad("side target/value must be a fitting scalar two-state bit");
                    const SlotRange side{o.asSlot(), o.firstElem, o.numElems, o.lsb, o.width};
                    for (std::size_t j = 0; j < i; j = j == 0 ? 5 : j + 2) {
                        const auto& other = ops[j];
                        if (other.kind != Operand::Kind::Slot || other.id != o.id) continue;
                        if (overlaps(slot, side,
                                     {other.asSlot(), other.firstElem, other.numElems, other.lsb,
                                      other.width}))
                            bad("side target overlaps the primary or another side target");
                    }
                    bool covered = false;
                    if (body.process)
                        for (const SlotRange& w : body.process->writes)
                            covered = covered || rangeCovered(slot, w, side);
                    if (!covered) bad("side target writes outside the write set");
                }
                break;
            }
            case Op::LoadElem:
            case Op::StoreElem:
            case Op::NbaElem:
            case Op::NbaElemDelay: {
                const bool delayed = in.op == Op::NbaElemDelay;
                // An automatic variable takes no NBA (10.4.2).
                const std::optional<Base> s = elemBase();
                if (!s || s->dims->empty() || s->type.kind == TypeKind::Ref ||
                    (s->frame && in.op != Op::StoreElem && in.op != Op::LoadElem)) {
                    bad("needs a whole unpacked array slot or frame variable");
                    break;
                }
                const std::size_t n = s->dims->size();
                const std::size_t want =
                    1 + n + (in.op == Op::LoadElem ? 0 : 1) + (delayed ? 1 : 0);
                if (ops.size() != want) {
                    bad("needs one index per dimension");  // rule 11
                    break;
                }
                if (!indicesOk(1, n)) bad("indices must be integral");
                if (in.op == Op::LoadElem) {
                    if (rt != s->type) bad("result must have the element type");
                } else if (!isValue(n + 1) || !fits(s->type, ty(n + 1))) {
                    bad("value does not fit the element type");
                }
                if (delayed && !delayOk(n + 2)) bad("the delay must be integral");
                break;
            }
            case Op::StoreBits:
            case Op::NbaBits:
            case Op::NbaBitsDelay: {
                const bool delayed = in.op == Op::NbaBitsDelay;
                const std::optional<Base> s = elemBase();
                if (!s || !isIntegral(s->type) || (s->frame && in.op != Op::StoreBits)) {
                    bad("needs a whole integral slot or frame variable");
                    break;
                }
                if (!windowOk(s->type)) bad("invalid packed parent range");
                const std::size_t n = s->dims->size();
                if (ops.size() != n + 3 + (delayed ? 1 : 0)) {
                    bad("needs one index per dimension, a position and a value");  // rule 11
                    break;
                }
                if (!indicesOk(1, n + 1)) bad("indices and the position must be integral");
                const Type v = ty(n + 2);
                if (!isValue(n + 2) || !sameDomain(v, s->type) || v.width > s->type.width)
                    bad("value must be integral, in the slot's domain and not wider");
                if (delayed && !delayOk(n + 3)) bad("the delay must be integral");
                break;
            }
            case Op::Fload:
                if (nOps(1) &&
                    (ops[0].kind != Operand::Kind::Frame || ty(0) != rt ||
                     (!(*body.frame)[ops[0].id].dims.empty() && rt.kind != TypeKind::Ref)))
                    bad("needs a frame variable of the result type that is not an array");
                break;
            case Op::Fstore:
                if (nOps(2) &&
                    (ops[0].kind != Operand::Kind::Frame || !isValue(1) || !fits(ty(0), ty(1)) ||
                     (!(*body.frame)[ops[0].id].dims.empty() && ty(0).kind != TypeKind::Ref)))
                    bad("needs a frame variable that is not an array and a value of its type");
                break;
            case Op::RefMake: {
                // A slot or a frame variable, and an index per dimension
                // of it when it selects an element (SIR_SPEC.md 8).
                if (ops.empty() || rt.kind != TypeKind::Ref) {
                    bad("needs a slot or frame variable and a ref result");
                    break;
                }
                const std::optional<Base> s = elemBase();
                if (!s) {
                    bad("needs a whole slot or frame variable");
                    break;
                }
                const std::size_t n = ops.size() - 1;
                if ((n != 0 && n != s->dims->size()) || !indicesOk(1, n))
                    bad("needs no index or one per dimension");
                Type target = s->type;
                if (s->frame && target.kind == TypeKind::Ref)
                    target = (*body.frame)[ops[0].id].target;
                const bool single = s->dims->empty() || n == s->dims->size();
                const Type want =
                    isContainer(target) && single ? refType(target.detail) : refType();
                if (rt != want) bad("ref result does not describe its target");
                break;
            }
            case Op::LoadRef:
            case Op::StoreRef:
            case Op::NbaRef: {
                // The ref, a constant leaf number when it is not 0, and the
                // value written.
                const bool load = in.op == Op::LoadRef;
                const std::size_t n = ops.size() - (load ? 0 : 1);
                const bool ok = !ops.empty() && (n == 1 || n == 2) && ty(0).kind == TypeKind::Ref &&
                                (n == 1 || constIndex(ops[1])) &&
                                (load ? rt.kind != TypeKind::Void && rt.kind != TypeKind::Ref
                                      : isValue(ops.size() - 1));
                if (!ok)
                    bad("needs a ref, an optional constant leaf and a value");
                else if (ty(0).detail != kNone && (n != 1 || (load ? rt : ty(ops.size() - 1)) !=
                                                                 containerType(ty(0).detail)))
                    bad("container ref access must use its target type without a leaf");
                break;
            }
            case Op::LoadRefElem:
            case Op::StoreRefElem:
            case Op::StoreRefBits: {
                // A ref argument (a frame variable that knows the shape of
                // what it refers to), its indices, a position for bits, and
                // the value written.
                const std::optional<Base> s = elemBase();
                if (!s || !s->frame || s->type.kind != TypeKind::Ref) {
                    bad("needs a ref argument");
                    break;
                }
                const std::size_t n = s->dims->size();
                const std::size_t want = 1 + n +
                                         (in.op == Op::LoadRefElem    ? 0
                                          : in.op == Op::StoreRefElem ? 1
                                                                      : 2);
                if (ops.size() != want || (in.op != Op::StoreRefBits && n == 0)) {
                    bad("needs one index per dimension of what it refers to");
                    break;
                }
                if (!indicesOk(1, in.op == Op::StoreRefBits ? n + 1 : n))
                    bad("indices and the position must be integral");
                const Type target = (*body.frame)[ops[0].id].target;
                if (in.op == Op::StoreRefBits && !windowOk(target))
                    bad("invalid packed parent range");
                if (in.op != Op::LoadRefElem && !isValue(want - 1)) bad("needs a value");
                if (in.op == Op::LoadRefElem && rt != target)
                    bad("result must have the type of what the ref refers to");
                if (in.op == Op::StoreRefBits && !isIntegral(target))
                    bad("needs a ref to an integral value");
                break;
            }
            case Op::Add:
            case Op::Sub:
            case Op::Mul:
            case Op::Div:
                if (nOps(2) && (!(isIntegral(rt) || isReal(rt)) || ty(0) != rt || ty(1) != rt))
                    bad("operands and result must have the same integral type (or the same real "
                        "type)");
                break;
            case Op::Pow:
                // The exponent of an integral power is self-determined and
                // may have any integral type (VALUE_SEMANTICS.md 3.2).
                if (nOps(2) && (!(isIntegral(rt) || isReal(rt)) || ty(0) != rt ||
                                (isReal(rt) ? ty(1) != rt : !isIntegral(ty(1)))))
                    bad("base and result must have the same integral type with an integral "
                        "exponent (or the same real type)");
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
                    if (!(isIntegral(t) || isReal(t) || isString(t) ||
                          t.kind == TypeKind::Chandle) ||
                        t != ty(1))
                        bad("operands must have one integral, real, string or chandle type");
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
                if (nOps(1) &&
                    (!(isIntegral(ty(0)) || isReal(ty(0)) || ty(0).kind == TypeKind::Chandle) ||
                     !width1(rt) || isFourState(rt) != isFourState(ty(0))))
                    bad("needs an integral, real or chandle operand and a 1-bit result in its "
                        "domain");
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
            case Op::StrengthLoad: {
                const auto shape = strengthLoadShape(d_, in);
                if (!shape) {
                    bad("needs a registered net window, strength result and captured selectors");
                    break;
                }
                for (std::size_t i = 1; i < ops.size(); ++i)
                    if (!rawValue(i) || !isIntegral(ty(i)) || ty(i).width == 0)
                        bad("strength selectors must be captured integral values");
                break;
            }
            case Op::StrengthEdge:
            case Op::StrengthEdgeDelay: {
                const bool delayed = in.op == Op::StrengthEdgeDelay;
                if (!nOps(delayed ? 4U : 2U)) break;
                const auto& target = ops[0];
                bool valid = target.kind == Operand::Kind::StrengthEdge &&
                             target.id < d_.strengthEdges.size() && target.edge == Edge::Any &&
                             target.firstElem == 0 && target.numElems == 0 && target.lsb == 0 &&
                             target.width == 0 && target.count == 0 && target.text.empty() &&
                             body.process != nullptr && body.process->kind == ProcessKind::Cont &&
                             body.owner.kind == Owner::Kind::Process && rawValue(1) &&
                             isIntegral(ty(1)) && ty(1).width == 1;
                if (valid) {
                    const auto& edge = d_.strengthEdges[target.id];
                    valid = conditionalStrengthEdge(edge.kind) && edge.delayed == delayed &&
                            edge.owner == body.owner.asProcess() &&
                            std::ranges::find(body.process->strengthEdgeWrites,
                                              StrengthEdgeId{target.id}) !=
                                body.process->strengthEdgeWrites.end();
                }
                for (std::size_t i = 2; i < ops.size(); ++i)
                    valid = valid && rawValue(i) && delayOk(i);
                if (!valid)
                    bad("needs an owned declared conditional edge, captured scalar control and "
                        "matching normalized delays");
                break;
            }
            case Op::StrengthDrive:
            case Op::StrengthDriveDelay: {
                const auto count = in.op == Op::StrengthDrive ? 2U : 5U;
                if (!nOps(count)) break;
                const auto& target = ops[0];
                bool valid = target.kind == Operand::Kind::StrengthOrigin &&
                             target.id < d_.strengthOrigins.size() && target.edge == Edge::Any &&
                             target.firstElem == 0 && target.numElems == 0 && target.lsb == 0 &&
                             target.width == 0 && target.count == 0 && target.text.empty() &&
                             body.process != nullptr && body.process->kind == ProcessKind::Cont &&
                             body.owner.kind == Owner::Kind::Process && rawValue(1) &&
                             ty(1) == strengthType(1);
                if (valid) {
                    const auto& origin = d_.strengthOrigins[target.id];
                    valid = origin.kind == StrengthOriginKind::Process &&
                            origin.owner == body.owner.asProcess() &&
                            std::ranges::find(body.process->strengthWrites,
                                              StrengthOriginId{target.id}) !=
                                body.process->strengthWrites.end();
                }
                for (std::size_t i = 2; i < ops.size(); ++i)
                    valid = valid && rawValue(i) && delayOk(i);
                if (!valid)
                    bad("needs an owned declared physical origin, captured strength<1> and "
                        "normalized delays");
                break;
            }
            case Op::StrengthMake: {
                bool ok = (ops.size() == 3 || ops.size() == 4) && rt.kind == TypeKind::Strength;
                if (ok) ok = isValue(0) && isFourState(ty(0)) && ty(0).width == rt.width;
                for (std::size_t i = 1; ok && i <= 2; ++i) {
                    ok = ops[i].kind == Operand::Kind::Const && isIntegral(ty(i)) &&
                         ty(i).width != 0;
                    if (ok) {
                        const auto& rank = d_.constants[ops[i].id];
                        ok = !rank.aWords.empty() && rank.aWords[0] <= 7 &&
                             std::all_of(rank.aWords.begin() + 1, rank.aWords.end(),
                                         [](auto word) { return word == 0; }) &&
                             std::all_of(rank.bWords.begin(), rank.bWords.end(),
                                         [](auto word) { return word == 0; });
                        if (ok && isSigned(rank.type) && rank.type.width <= 3)
                            ok = ((rank.aWords[0] >> (rank.type.width - 1)) & 1U) == 0;
                    }
                }
                if (ok && ops.size() == 4)
                    ok = isValue(3) && isIntegral(ty(3)) && !isFourState(ty(3)) &&
                         std::uint64_t{ty(3).width} == std::uint64_t{rt.width} * 2;
                if (!ok)
                    bad("needs strength<w>, a captured 4-state value of width w, two static "
                        "ranks 0..7 and an optional 2-state side of width 2w");
                break;
            }
            case Op::StrengthValue:
                if (nOps(1) &&
                    (!isValue(0) || ty(0).kind != TypeKind::Strength || rt != u4(ty(0).width)))
                    bad("needs strength<w> and returns i4<w>");
                break;
            case Op::StrengthMos: {
                static constexpr std::string_view kinds[] = {"nmos",  "pmos", "rnmos",
                                                             "rpmos", "cmos", "rcmos"};
                bool ok =
                    !ops.empty() && ops[0].kind == Operand::Kind::Text &&
                    std::find(std::begin(kinds), std::end(kinds), ops[0].text) != std::end(kinds);
                if (ok) {
                    const bool parallel = ops[0].text == "cmos" || ops[0].text == "rcmos";
                    ok = ops.size() == (parallel ? 4U : 3U) && rt.kind == TypeKind::Strength;
                }
                if (ok) ok = isValue(1) && ty(1) == rt;
                for (std::size_t i = 2; ok && i < ops.size(); ++i)
                    ok = isValue(i) && width1(ty(i));
                if (!ok)
                    bad("needs a legal MOS kind, strength<w> input/result and scalar controls");
                break;
            }
            case Op::Resolve: {
                // A net kind, then for each driver a value of the result
                // type, two constant strengths from 0 to 7 and, for a
                // driver with a side slot, a 2-state value of twice the
                // width (SIR_SPEC.md 3.5).
                static constexpr std::string_view kKinds[] = {
                    "wire", "tri",  "wand",    "triand",  "wor",  "trior",
                    "tri0", "tri1", "supply0", "supply1", "uwire"};
                bool ok = !ops.empty() && ops[0].kind == Operand::Kind::Text &&
                          std::find(std::begin(kKinds), std::end(kKinds), ops[0].text) !=
                              std::end(kKinds) &&
                          isIntegral(rt) && isFourState(rt);
                for (std::size_t i = 1; ok && i < ops.size();) {
                    ok = i + 2 < ops.size() && ty(i) == rt;
                    for (std::size_t j = i + 1; ok && j < i + 3; ++j) {
                        ok = ops[j].kind == Operand::Kind::Const && isIntegral(ty(j));
                        if (ok) {
                            const Constant& c = d_.constants[ops[j].id];
                            ok = (c.bWords.empty() || c.bWords[0] == 0) &&
                                 (c.aWords.empty() || c.aWords[0] <= 7);
                        }
                    }
                    i += 3;
                    if (ok && i < ops.size() && !isFourState(ty(i))) {
                        ok = isIntegral(ty(i)) && ty(i).width == 2 * rt.width;
                        ++i;
                    }
                }
                if (!ok)
                    bad("needs a net kind, then a 4-state value of the result type, two "
                        "constant strengths from 0 to 7 and an optional 2-state side of twice "
                        "the width for each driver");
                break;
            }
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
                        if (!known || c.type.width > 64 || lsb > v.width ||
                            rt.width > v.width - lsb)
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
                    // The count is known and fits 64 bits; the product is
                    // compared by division, which cannot wrap.
                    const Constant& c = d_.constants[ops[0].id];
                    const std::uint64_t n = c.aWords.empty() ? 0 : c.aWords[0];
                    const std::uint32_t w = ty(1).width;
                    const auto zero = [](const std::vector<std::uint64_t>& v, std::size_t from) {
                        for (std::size_t i = from; i < v.size(); ++i)
                            if (v[i] != 0) return false;
                        return true;
                    };
                    if (n == 0 || !zero(c.aWords, 1) || !zero(c.bWords, 0) || w == 0 ||
                        rt.width % w != 0 || n != rt.width / w)
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
            case Op::EventNew:
                if (nOps(0) && rt != Type{TypeKind::Event, 0}) bad("must return an event value");
                break;
            case Op::Trigger:
                if (nOps(1) && !eventTarget(0)) bad("needs an event slot");
                break;
            case Op::TriggerNb:
                if (ops.empty() || ops.size() > 2 || !eventTarget(0) ||
                    (ops.size() == 2 && !delayOk(1)))
                    bad("needs an event slot and an optional integral delay");
                break;
            case Op::Disable:
                if (nOps(1) && ops[0].kind != Operand::Kind::Process &&
                    ops[0].kind != Operand::Kind::Subroutine && ops[0].kind != Operand::Kind::Text)
                    bad("needs a process, a subroutine or a block name");
                break;
            case Op::DisableFork: nOps(0); break;
            case Op::ScopeEnter:
                if (nOps(2) && (ops[0].kind != Operand::Kind::Scope || ops[0].id >= d_.numScopes ||
                                ops[1].kind != Operand::Kind::Block || ops[1].count != 0))
                    bad("needs a scope and its exit block");
                break;
            case Op::ActivationEnter:
            case Op::ActivationLeave:
                if (nOps(1) && ops[0].kind != Operand::Kind::Scope)
                    bad("needs a body-local automatic scope");
                break;
            case Op::ScopeLeave:
            case Op::DisableScope:
                if (nOps(1) && (ops[0].kind != Operand::Kind::Scope || ops[0].id >= d_.numScopes))
                    bad("needs a scope");
                break;
            case Op::DisableTask:
                if (nOps(1) && (ops[0].kind != Operand::Kind::Subroutine ||
                                ops[0].id >= d_.subroutines.size() ||
                                d_.subroutines[ops[0].id].kind != SubroutineKind::Task))
                    bad("needs a task");
                break;
            case Op::Strobe:
            case Op::Monitor:
                // With a descriptor for $fstrobe and $fmonitor (SIR_SPEC.md 7.1).
                if (ops.size() != 1 && ops.size() != 2) {
                    bad("expects 1 or 2 operands");
                } else if (ops[0].kind != Operand::Kind::Process ||
                           ops[0].id >= d_.processes.size() ||
                           d_.processes[ops[0].id].kind != ProcessKind::Postponed) {
                    bad("needs a postponed process");
                } else if (ops.size() == 2 && (!isValue(1) || !isIntegral(ty(1)))) {
                    bad("the descriptor must be an integral value");
                }
                break;
            case Op::Udp:
            case Op::UdpSeq: {
                // A UDP table, then the state and the previous inputs of a
                // sequential one, then the inputs: 1-bit values. The result
                // is a 4-state bit (SIR_SPEC.md 5.5).
                const bool seq = in.op == Op::UdpSeq;
                const bool table = !ops.empty() && ops[0].kind == Operand::Kind::Type &&
                                   ops[0].id < d_.types.size() &&
                                   d_.types[ops[0].id].kind == TypeEntryKind::Udp &&
                                   d_.types[ops[0].id].sequential == seq;
                if (!table) {
                    bad(std::string("needs a ") + (seq ? "sequential" : "combinational") +
                        " UDP table");
                    break;
                }
                const std::uint64_t n = d_.types[ops[0].id].inputs;
                if (ops.size() != 1 + (seq ? 1 + 2 * n : n)) {
                    bad("needs one value per input" +
                        std::string(seq ? ", twice, and the state" : ""));
                    break;
                }
                for (std::size_t i = 1; i < ops.size(); ++i)
                    if (!isValue(i) || !width1(ty(i))) bad("values must be 1-bit");
                if (rt != u4(1)) bad("result must be i4<1>");
                break;
            }
            case Op::OverrideStart:
            case Op::OverrideStop: {
                // A level, the override process (start), then the targets:
                // whole slots, elements or constant bit ranges.
                const std::size_t first = in.op == Op::OverrideStart ? 2 : 1;
                bool ok = ops.size() > first && levelOk(ops[0]);
                if (ok && in.op == Op::OverrideStart)
                    ok = ops[1].kind == Operand::Kind::Process && ops[1].id < d_.processes.size() &&
                         d_.processes[ops[1].id].kind == ProcessKind::Override;
                for (std::size_t i = first; ok && i < ops.size(); ++i)
                    ok = ops[i].kind == Operand::Kind::Slot && heldOk(ops[i]);
                if (!ok)
                    bad("needs \"assign\" or \"force\", " +
                        std::string(in.op == Op::OverrideStart ? "an override process, " : "") +
                        "and integral slot ranges marked as force targets");
                break;
            }
            case Op::StoreOverride: {
                if (body.process == nullptr || body.process->kind != ProcessKind::Override) {
                    bad("only an override process writes with it");
                    break;
                }
                if (!nOps(3)) break;
                if (!levelOk(ops[0]) || ops[1].kind != Operand::Kind::Slot || !heldOk(ops[1]) ||
                    !isIntegral(ty(2)) || ty(2).width != rangeWidth(ops[1])) {
                    bad("needs \"assign\" or \"force\", a slot range marked as a force target "
                        "and a value of its width");
                    break;
                }
                const Operand& o = ops[1];
                const SlotRange target{o.asSlot(), o.firstElem, o.numElems, o.lsb, o.width};
                bool covered = false;
                for (const SlotRange& w : body.process->writes)
                    covered = covered || rangeCovered(d_.slots[o.id], w, target);
                if (!covered) bad("writes %s" + std::to_string(o.id) + " outside the write set");
                break;
            }
            case Op::ReportCall: {
                // A function of input arguments, called with values of
                // their types (SIR_SPEC.md 5.7).
                if (ops.size() < 2 || ops[0].kind != Operand::Kind::Text ||
                    (ops[0].text != "#0" && ops[0].text != "final") ||
                    ops[1].kind != Operand::Kind::Subroutine ||
                    ops[1].id >= d_.subroutines.size()) {
                    bad("needs \"#0\" or \"final\" and a function");
                    break;
                }
                const Subroutine& s = d_.subroutines[ops[1].id];
                if (s.kind != SubroutineKind::Function || s.params.size() != ops.size() - 2) {
                    bad("needs a function and one value per argument");
                    break;
                }
                for (std::size_t i = 0; i < s.params.size(); ++i)
                    if (s.params[i].dir != Direction::Input || !isValue(i + 2) ||
                        !fits(s.params[i].type, ty(i + 2)))
                        bad("argument " + std::to_string(i) + " needs a value of its input's type");
                break;
            }
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
                if (nOps(3) &&
                    (ops[0].kind != Operand::Kind::Text || ops[1].kind != Operand::Kind::Text ||
                     ops[2].kind != Operand::Kind::Text || !among(ops[0].text, kConstructs) ||
                     !among(ops[1].text, kContents) || ops[2].text.empty()))
                    bad("needs a construct and a content of SYSTEM_TASKS.md 10.4 and a scope");
                break;
            }
            case Op::Call: {
                if (ops.empty() || ops[0].kind != Operand::Kind::Subroutine ||
                    ops[0].id >= d_.subroutines.size()) {
                    bad("first operand must be a subroutine");
                    break;
                }
                const Subroutine& callee = d_.subroutines[ops[0].id];
                if (readsWaiterFrame(callee.kind))
                    bad(std::string("a ") + kindName(callee.kind) + " is not called");
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
                    // The levels, then what to dump as rows of the dump
                    // table (SIR_SPEC.md 2): "scope" or "var" and the row,
                    // kNone for something that is not dumped.
                    for (std::size_t i = 2; ops[0].text == "$dumpvars" && i < ops.size(); i += 2) {
                        const bool scope =
                            ops[i].kind == Operand::Kind::Text && ops[i].text == "scope";
                        const bool var = ops[i].kind == Operand::Kind::Text && ops[i].text == "var";
                        const std::uint64_t n = scope ? d_.dumpScopes.size() : d_.dumpVars.size();
                        if ((!scope && !var) || i + 1 >= ops.size() ||
                            ops[i + 1].kind != Operand::Kind::Const ||
                            ops[i + 1].id >= d_.constants.size() ||
                            d_.constants[ops[i + 1].id].aWords.empty() ||
                            (d_.constants[ops[i + 1].id].aWords[0] >= n &&
                             d_.constants[ops[i + 1].id].aWords[0] != kNone)) {
                            bad("names a scope or variable that is no row of the dump table");
                            break;
                        }
                    }
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
                if (nOps(2) && !delayOk(0)) bad("needs an integral delay");
                continuation();
                break;
            case Op::WaitEvent: {
                if (ops.size() < 2) {
                    bad("needs at least one event and a block");
                    break;
                }
                // Slot ranges, watches and loaded event values each have an
                // optional iff guard (SIR_SPEC 7.2; capture contract 5/7).
                bool afterItem = false;
                for (std::size_t i = 0; i + 1 < ops.size(); ++i) {
                    const Operand& o = ops[i];
                    const bool watch = o.kind == Operand::Kind::Subroutine &&
                                       o.id < d_.subroutines.size() &&
                                       d_.subroutines[o.id].kind == SubroutineKind::Watch;
                    const bool eventValue = isValue(i) && ty(i) == Type{TypeKind::Event, 0};
                    if (o.kind == Operand::Kind::Slot || watch || eventValue) {
                        if (watch) checkWaiter(body, o, SubroutineKind::Watch, bad);
                        if (o.kind == Operand::Kind::Slot && ty(i).kind == TypeKind::Event &&
                            (o.edge != Edge::Any || o.lsb != 0 || o.width != 0))
                            bad("named event items have no edge or packed range");
                        if (eventValue && !eventTarget(i))
                            bad("named event values have no edge, range or argument count");
                        afterItem = true;
                        continue;
                    }
                    if (o.kind == Operand::Kind::Subroutine && afterItem) {
                        checkGuard(body, o, bad);
                        afterItem = false;
                        continue;
                    }
                    bad("events must be slot ranges or watches, each with an optional iff guard");
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
                if (nOps(2) && !eventTarget(0)) bad("needs an event slot");
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
        checkWaiter(body, o, SubroutineKind::Guard, bad);
    }

    /// o names a guard or a watch (kind) of the frame of the body that waits.
    template <typename Bad>
    void checkWaiter(const Body& body, const Operand& o, SubroutineKind kind,
                     const Bad& bad) const {
        if (o.id >= d_.subroutines.size()) return;
        const Subroutine& g = d_.subroutines[o.id];
        if (g.kind != kind)
            bad("@" + std::to_string(o.id) + " is not a " + kindName(kind));
        else if (g.frameOf.kind != body.owner.kind || g.frameOf.id != body.owner.id)
            bad(std::string(kindName(kind)) + " @" + std::to_string(o.id) + " reads another frame");
    }

    bool rangeCovered(const Slot& shape, SlotRange outer, SlotRange inner) const {
        const auto canonical = [&](SlotId id) {
            if (id.index() >= d_.slots.size()) return id;
            const SlotId view = d_.slot(id).viewOf;
            return view != kNone && view.index() < d_.slots.size() ? view : id;
        };
        outer.slot = canonical(outer.slot);
        inner.slot = canonical(inner.slot);
        return covers(shape, outer, inner);
    }

    // Rule 6, second half: what an always_comb, always_latch or always_ff
    // writes, no other process writes; a uwire has one driver; a variable
    // has at most one continuous assignment. The writes of each slot are
    // sorted by place and swept, rather than every pair of processes
    // compared; the errors come in the order of the processes and of their
    // writes, first by the earlier process.
    void checkWriteConflicts() {
        const auto& ps = d_.processes;
        std::vector<WriteExtent> writes;
        for (std::size_t i = 0; i < ps.size(); ++i) {
            // Initial values of declarations are written by no process
            // (6.8). assign and force hold a target over its other writers
            // (SCHEDULER_SPEC.md 14).
            if (ps[i].kind == ProcessKind::StaticInit || ps[i].kind == ProcessKind::Override)
                continue;
            for (std::size_t k = 0; k < ps[i].writes.size(); ++k) {
                const SlotRange& r = ps[i].writes[k];
                // A static subroutine's variables and arguments: a call
                // copies into them, and slang reports the assignments
                // inside the function (SIR_SPEC.md 10).
                if (r.slot.index() >= d_.slots.size() || d_.slot(r.slot).subroutine != kNone)
                    continue;
                const auto [elemLo, elemHi] = extent(r.firstElem, r.numElems);
                const auto [bitLo, bitHi] = extent(r.lsb, r.width);
                const SlotId view = d_.slot(r.slot).viewOf;
                const SlotId physical =
                    view != kNone && view.index() < d_.slots.size() ? view : r.slot;
                writes.push_back({physical, elemLo, elemHi, bitLo, bitHi, static_cast<ProcessId>(i),
                                  static_cast<std::uint32_t>(k), writerOf(ps[i], r)});
            }
        }
        std::ranges::sort(writes, [](const WriteExtent& x, const WriteExtent& y) {
            return std::tie(x.slot, x.elemLo, x.elemHi, x.bitLo) <
                   std::tie(y.slot, y.elemLo, y.elemHi, y.bitLo);
        });
        std::vector<Clash> found;
        for (std::size_t b = 0; b < writes.size();) {
            std::size_t e = b;
            while (e < writes.size() && writes[e].slot == writes[b].slot)
                ++e;
            sweepSlot(std::span(writes).subspan(b, e - b), found);
            b = e;
        }
        std::ranges::sort(found, [](const Clash& x, const Clash& y) {
            return std::tie(x.first, x.second, x.firstIndex, x.secondIndex) <
                   std::tie(y.first, y.second, y.firstIndex, y.secondIndex);
        });
        for (const Clash& c : found)
            error(processWhere(c.second) + " " + d_.process(c.second).name,
                  "writes %s" + std::to_string(c.slot.index()) + " (" + d_.slot(c.slot).name +
                      "), which " + processWhere(c.first) + " also writes",
                  d_.process(c.second).loc);
    }

    /// The writes of one slot, sorted by elements and then bits. Writes
    /// with the same elements form a group; each group is swept on its own
    /// and with each earlier group whose elements overlap its own.
    void sweepSlot(std::span<const WriteExtent> slotWrites, std::vector<Clash>& found) const {
        const SlotKind kind = d_.slot(slotWrites.front().slot).kind;
        struct Group {
            std::span<const WriteExtent> writes;
            std::array<bool, kWriters> has{};
        };
        std::vector<Group> open;
        for (std::size_t b = 0; b < slotWrites.size();) {
            std::size_t e = b;
            while (e < slotWrites.size() && slotWrites[e].elemLo == slotWrites[b].elemLo &&
                   slotWrites[e].elemHi == slotWrites[b].elemHi)
                ++e;
            Group g{slotWrites.subspan(b, e - b), {}};
            for (const WriteExtent& w : g.writes)
                g.has[static_cast<std::size_t>(w.writer)] = true;
            const std::uint64_t elemLo = g.writes.front().elemLo;
            std::erase_if(open, [&](const Group& h) { return h.writes.front().elemHi <= elemLo; });
            for (const Group& h : open) {
                bool may = false;
                for (std::size_t x = 0; x < kWriters; ++x)
                    for (std::size_t y = 0; y < kWriters; ++y)
                        may = may || (h.has[x] && g.has[y] &&
                                      clash(static_cast<Writer>(x), static_cast<Writer>(y), kind));
                if (may) sweepBits(h.writes, g.writes, kind, found);
            }
            sweepBits(g.writes, {}, kind, found);
            open.push_back(g);
            b = e;
        }
    }
};

}  // namespace

std::vector<VerifyError> verify(const Design& design) {
    auto errors = Verifier(design).run();
    auto activation = verifyActivationScopes(design);
    errors.insert(errors.end(), std::make_move_iterator(activation.begin()),
                  std::make_move_iterator(activation.end()));
    return errors;
}

}  // namespace kirimasim::sir
