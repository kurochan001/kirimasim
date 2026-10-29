// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/text.h"
#include "kirimasim/sir/pattern.h"
#include "kirimasim/sir/strength.h"

#include <array>
#include <bit>
#include <charconv>
#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "kirimasim/sir/timescale.h"
#include "kirimasim/sir/types.h"

namespace kirimasim::sir {

namespace {

bool bitOf(const std::vector<std::uint64_t>& words, std::uint32_t i) {
    const std::size_t w = i / 64;
    return w < words.size() && ((words[w] >> (i % 64)) & 1U) != 0;
}

std::string quoted(const std::string& s) {
    std::string out = "\"";
    for (const char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) {
                    static constexpr char kHex[] = "0123456789abcdef";
                    const auto u = static_cast<unsigned char>(c);
                    out += "\\x";
                    out += kHex[u >> 4];
                    out += kHex[u & 15U];
                } else {
                    out += c;
                }
                break;
        }
    }
    out += '"';
    return out;
}

std::string rangeText(std::uint32_t firstElem, std::uint32_t numElems, std::uint32_t lsb,
                      std::uint32_t width) {
    std::string out;
    if (numElems == 1) {
        out += "{" + std::to_string(firstElem) + "}";
    } else if (numElems != 0) {
        out +=
            "{" + std::to_string(firstElem) + ".." + std::to_string(firstElem + numElems - 1) + "}";
    }
    if (width != 0) {
        out += "[" + std::to_string(lsb + width - 1) + ":" + std::to_string(lsb) + "]";
    }
    return out;
}

std::string slotRef(const SlotRange& r) {
    return "%s" + std::to_string(r.slot.index()) +
           rangeText(r.firstElem, r.numElems, r.lsb, r.width);
}

std::string operandText(const Design& d, const Operand& o) {
    switch (o.kind) {
        case Operand::Kind::Temp: return "%t" + std::to_string(o.id);
        case Operand::Kind::Const:
            if (o.id < d.constants.size()) {
                const Constant& c = d.constants[o.id];
                return "const " + toString(c.type) + " " + constantText(c);
            }
            return "const ?" + std::to_string(o.id);
        case Operand::Kind::Slot: {
            std::string out = o.edge == Edge::Any ? "" : std::string(edgeName(o.edge)) + " ";
            return out + "%s" + std::to_string(o.id) +
                   rangeText(o.firstElem, o.numElems, o.lsb, o.width);
        }
        case Operand::Kind::Block: return "bb" + std::to_string(o.id);
        case Operand::Kind::Text: return quoted(o.text);
        case Operand::Kind::Frame:
            return "%f" + std::to_string(o.id) + rangeText(0, 0, o.lsb, o.width);
        case Operand::Kind::Subroutine:
            // A watch in wait.event has its edge.
            return (o.edge == Edge::Any ? "" : std::string(edgeName(o.edge)) + " ") + "@" +
                   std::to_string(o.id);
        case Operand::Kind::Process: return "@" + std::to_string(o.id);
        case Operand::Kind::Pattern: return "!p" + std::to_string(o.id);
        case Operand::Kind::NbaLayout: return "!n" + std::to_string(o.id);
        case Operand::Kind::Type: return "!" + std::to_string(o.id);
        case Operand::Kind::Scope: return "#" + std::to_string(o.id);
        case Operand::Kind::StrengthOrigin: return "!d" + std::to_string(o.id);
        case Operand::Kind::StrengthEdge: return "!e" + std::to_string(o.id);
    }
    return "?";
}

/// The operands of an instruction: block arguments go in parentheses after
/// their block, and an iff guard of wait.event follows its event item.
std::string operandsText(const Design& d, const Instr& in) {
    std::string out;
    const auto& ops = in.operands;
    for (std::size_t k = 0; k < ops.size(); ++k) {
        const Operand& o = ops[k];
        if (in.op == Op::WaitEvent && o.kind == Operand::Kind::Subroutine &&
            o.id < d.subroutines.size() && d.subroutines[o.id].kind == SubroutineKind::Guard) {
            out += " iff " + operandText(d, o);
            continue;
        }
        out += (k == 0 ? " " : ", ") + operandText(d, o);
        if (o.kind != Operand::Kind::Block || o.count == 0) continue;
        out += '(';
        for (std::uint32_t a = 0; a < o.count && k + 1 < ops.size(); ++a)
            out += (a == 0 ? "" : ", ") + operandText(d, ops[++k]);
        out += ')';
    }
    return out;
}

std::string locText(const SourceLoc& loc) {
    if (loc.line == 0) return "";
    return "  ; " + std::to_string(loc.file) + ":" + std::to_string(loc.line) + ":" +
           std::to_string(loc.column);
}

std::string slotFlags(std::uint32_t f) {
    static constexpr struct {
        std::uint32_t bit;
        const char* name;
    } kNames[] = {{kObservable, "observable"},
                  {kForceTarget, "force"},
                  {kMultiDriver, "multi-driver"},
                  {kNeedsStrength, "strength"}};
    std::string out;
    for (const auto& n : kNames) {
        if ((f & n.bit) == 0) continue;
        if (!out.empty()) out += ',';
        out += n.name;
    }
    return out.empty() ? "" : " flags(" + out + ")";
}

std::string ownerText(const Owner& o) {
    switch (o.kind) {
        case Owner::Kind::None: return "-";
        case Owner::Kind::Process: return "process @" + std::to_string(o.id);
        case Owner::Kind::Subroutine: return "subroutine @" + std::to_string(o.id);
    }
    return "?";
}

std::string typeRef(TypeId t) { return t == kNone ? "-" : "!" + std::to_string(t.index()); }

std::string dimsText(const std::vector<ArrayDim>& dims) {
    std::string out;
    for (const ArrayDim& dim : dims)
        out += " [" + std::to_string(dim.left) + ":" + std::to_string(dim.right) + "]";
    return out;
}

std::string frameText(const std::vector<FrameVar>& frame) {
    std::string out;
    for (std::size_t i = 0; i < frame.size(); ++i) {
        const FrameVar& f = frame[i];
        out += "  frame %f" + std::to_string(i) + " " + quoted(f.name) + " : " + toString(f.type);
        if (f.type.kind == TypeKind::Ref && f.type.detail == kNone)
            out += "<" + toString(f.target) + ">";
        out += dimsText(f.dims);
        if (f.declType != kNone) out += " type=" + typeRef(f.declType);
        if (f.home == FrameHome::Scoped)
            out += " home=activation#" + std::to_string(f.activationScope.index());
        if (f.home == FrameHome::Capture)
            out += " home=capture%f" + std::to_string(f.captureSource.index());
        if (f.readonly) out += " readonly";
        if (f.refStatic) out += " ref-static";
        out += '\n';
    }
    return out;
}

std::string activationText(const ActivationInfo& info) {
    std::string out;
    for (std::size_t i = 0; i < info.scopes.size(); ++i) {
        const auto& scope = info.scopes[i];
        out += "  activation #" + std::to_string(i);
        if (info.root.index() == i) out += " root";
        if (scope.parent != kNone) out += " parent=#" + std::to_string(scope.parent.index());
        out += " fields=(";
        for (std::size_t f = 0; f < scope.fields.size(); ++f) {
            if (f != 0) out += ", ";
            out += "%f" + std::to_string(scope.fields[f].index());
        }
        out += ")\n";
    }
    if (!info.extents.empty() || info.inheritCaptures) {
        out += "  capture.extents";
        for (auto scope : info.extents)
            out += " #" + std::to_string(scope.index());
        if (info.inheritCaptures) out += " inherited";
        out += '\n';
    }
    return out;
}

std::string blocksText(const Design& d, const std::vector<BasicBlock>& blocks) {
    std::string out;
    for (std::size_t b = 0; b < blocks.size(); ++b) {
        out += "bb" + std::to_string(b);
        const auto& params = blocks[b].params;
        if (!params.empty()) {
            out += '(';
            for (std::size_t i = 0; i < params.size(); ++i) {
                out += (i == 0 ? "%t" : ", %t") + std::to_string(params[i].temp.index()) + ": " +
                       toString(params[i].type);
            }
            out += ')';
        }
        out += ":\n";
        for (const Instr& in : blocks[b].instrs) {
            std::string line = "  ";
            if (in.result != kNone) line += "%t" + std::to_string(in.result.index()) + " = ";
            line += opName(in.op);
            if (in.type.kind != TypeKind::Void) line += " " + toString(in.type);
            out += line + operandsText(d, in) + locText(in.loc) + "\n";
        }
    }
    return out;
}

std::string nbaLayoutText(const NbaLayout& layout) {
    const auto value = [](NbaValueId id) { return "$" + std::to_string(id.index()); };
    const auto call = [](SubroutineId id) { return "@" + std::to_string(id.index()); };
    std::string out = " captures=(";
    for (std::size_t i = 0; i < layout.values.size(); ++i)
        out += (i == 0 ? "" : ", ") + toString(layout.values[i]);
    out += ") {\n";
    for (const auto& item : layout.events)
        std::visit(
            [&](const auto& event) {
                using T = std::decay_t<decltype(event)>;
                if constexpr (std::is_same_v<T, NbaSlotEvent>)
                    out += "  event.slot " + slotRef(event.part);
                else if constexpr (std::is_same_v<T, NbaNamedEvent>)
                    out += "  event.named " + value(event.value);
                else
                    out += "  event.watch " + call(event.watch);
                if constexpr (!std::is_same_v<T, NbaNamedEvent>)
                    out += " edge=" + std::string(edgeName(event.edge));
                if (event.guard != kNone) out += " iff=" + call(event.guard);
                out += '\n';
            },
            item);
    for (const auto& item : layout.targets)
        std::visit(
            [&](const auto& target) {
                using T = std::decay_t<decltype(target)>;
                if constexpr (std::is_same_v<T, NbaSlotTarget>)
                    out += "  write.slot " + slotRef(target.part);
                else if constexpr (std::is_same_v<T, NbaElementTarget>)
                    out += "  write.elem %s" + std::to_string(target.slot.index());
                else if constexpr (std::is_same_v<T, NbaBitsTarget>)
                    out += "  write.bits " + slotRef(target.parent);
                else if constexpr (std::is_same_v<T, NbaRefTarget>)
                    out += "  write.ref " + value(target.reference);
                else
                    out += "  write.ref.bits " + value(target.reference);
                if constexpr (std::is_same_v<T, NbaElementTarget> ||
                              std::is_same_v<T, NbaBitsTarget>) {
                    out += " indices=(";
                    for (std::size_t i = 0; i < target.indices.size(); ++i)
                        out += (i == 0 ? "" : ", ") + value(target.indices[i]);
                    out += ')';
                }
                if constexpr (std::is_same_v<T, NbaBitsTarget> ||
                              std::is_same_v<T, NbaRefBitsTarget>)
                    out += " position=" + value(target.position);
                if constexpr (std::is_same_v<T, NbaRefTarget> ||
                              std::is_same_v<T, NbaRefBitsTarget>)
                    out +=
                        " leaf=" + std::to_string(target.leaf) + " type=" + toString(target.target);
                out += " value=" + value(target.value) + "\n";
            },
            item);
    return out + "}\n";
}

std::string typeEntryText(const Design& d, const TypeEntry& t) {
    std::string out = std::string(kindName(t.kind)) + " " + quoted(t.name);
    if (t.value.kind != TypeKind::Void) out += " : " + toString(t.value);
    if (t.kind == TypeEntryKind::Array) out += " elem=" + typeRef(t.element) + dimsText(t.dims);
    if (t.kind == TypeEntryKind::Queue) out += " capacity=" + std::to_string(t.maxElements);
    if (t.kind == TypeEntryKind::Udp) {
        out += " inputs=" + std::to_string(t.inputs);
        if (t.sequential) out += " seq init=" + std::string(1, t.initial);
        out += " {";
        for (std::size_t i = 0; i < t.rows.size(); ++i) {
            const UdpRow& r = t.rows[i];
            std::string row = r.inputs;
            if (r.state != 0) row += std::string(" : ") + r.state;
            row += std::string(" : ") + r.output;
            out += (i == 0 ? " " : ", ") + quoted(row);
        }
        out += " }";
    }
    if (t.kind == TypeEntryKind::Enum) {
        out += " {";
        for (std::size_t i = 0; i < t.items.size(); ++i) {
            const EnumItem& item = t.items[i];
            out += (i == 0 ? " " : ", ") + quoted(item.name) + " = " +
                   (item.value.index() < d.constants.size() ? constantText(d.constant(item.value))
                                                            : "?");
        }
        out += " }";
    } else if (!t.fields.empty()) {
        const bool packed = t.kind != TypeEntryKind::UnpackedStruct;
        out += " {";
        for (std::size_t i = 0; i < t.fields.size(); ++i) {
            const Field& f = t.fields[i];
            out += (i == 0 ? " " : ", ") + quoted(f.name);
            if (packed)
                out +=
                    " [" + std::to_string(f.lsb + f.width - 1) + ":" + std::to_string(f.lsb) + "]";
            if (f.type != kNone) out += " " + typeRef(f.type);
        }
        out += " }";
    }
    return out;
}

}  // namespace

std::string constantText(const Constant& c) {
    if (isContainer(c.type)) return "empty";
    if (c.type.kind == TypeKind::Chandle || c.type.kind == TypeKind::Event) return "null";
    if (c.type.kind == TypeKind::String) return quoted(c.bytes);
    if (c.type.kind == TypeKind::Real || c.type.kind == TypeKind::ShortReal) {
        const std::uint64_t bits = c.aWords.empty() ? 0 : c.aWords[0];
        std::array<char, 64> buf{};
        std::to_chars_result r{};
        if (c.type.kind == TypeKind::Real) {
            r = std::to_chars(buf.data(), buf.data() + buf.size(), std::bit_cast<double>(bits));
        } else {
            r = std::to_chars(buf.data(), buf.data() + buf.size(),
                              std::bit_cast<float>(static_cast<std::uint32_t>(bits)));
        }
        return std::string(buf.data(), r.ptr);
    }
    if (!isIntegral(c.type)) return "?";
    const std::uint32_t w = c.type.width;
    bool hasXz = false;
    for (std::uint32_t i = 0; i < w && !hasXz; ++i)
        hasXz = bitOf(c.bWords, i);
    std::string digits;
    if (!hasXz) {
        static constexpr char kHex[] = "0123456789abcdef";
        for (std::uint32_t i = 0; i < w; i += 4) {
            unsigned v = 0;
            for (std::uint32_t k = 0; k < 4 && i + k < w; ++k)
                v |= (bitOf(c.aWords, i + k) ? 1U : 0U) << k;
            digits.insert(digits.begin(), kHex[v]);
        }
        return std::to_string(w) + "'h" + digits;
    }
    for (std::uint32_t i = 0; i < w; ++i) {
        const bool a = bitOf(c.aWords, i);
        const bool b = bitOf(c.bWords, i);
        digits.insert(digits.begin(), b ? (a ? 'x' : 'z') : (a ? '1' : '0'));
    }
    return std::to_string(w) + "'b" + digits;
}

std::string patternLayoutText(const Design& d, const PatternLayout& p) {
    using Kind = PatternLayout::Kind;
    std::string out;
    switch (p.kind) {
        case Kind::Integral: out = "integral"; break;
        case Kind::Real: out = "real"; break;
        case Kind::String: out = "string"; break;
        case Kind::Enumeration: out = "enum"; break;
        case Kind::Record: out = "record"; break;
        case Kind::PackedRecord: out = "packed-record"; break;
        case Kind::Array: out = "array"; break;
        case Kind::Union: out = "union"; break;
        case Kind::PackedUnion: out = "packed-union"; break;
        case Kind::TaggedUnion: out = "tagged-union"; break;
        case Kind::Container: out = "container"; break;
    }
    if (p.value != voidType()) out += " " + toString(p.value);
    if (p.element != kNone) out += " !p" + std::to_string(p.element.index());
    if (p.kind == Kind::Array) out += " count=" + std::to_string(p.count);
    if (!p.tag.empty()) out += " tag=" + quoted(p.tag) + (p.tagEscaped ? " escaped" : "");
    for (const auto& e : p.enumerators) {
        out += " enum " + quoted(e.name) + (e.escaped ? " escaped=" : "=");
        out +=
            e.value.index() < d.constants.size() ? constantText(d.constants[e.value.index()]) : "?";
    }
    for (const auto& m : p.members)
        out += " member " + quoted(m.name) + (m.escaped ? " escaped:" : ":") + "!p" +
               std::to_string(m.layout.index()) + "@" + std::to_string(m.lsb);
    return out;
}

std::string toText(const Design& d) {
    std::string out = "; sir format 0\n";
    out += "precision " + timeExponentText(d.timePrecision) + "\n";
    for (std::size_t i = 0; i < d.sources.size(); ++i)
        out += "source " + std::to_string(i) + " " + quoted(d.sources[i]) + "\n";
    for (std::size_t i = 0; i < d.instances.size(); ++i) {
        const Instance& inst = d.instances[i];
        out += "instance " + std::to_string(i) + " " + quoted(inst.scopeName) +
               " def=" + quoted(inst.definition) +
               " parent=" + (inst.parent == kNone ? "-" : std::to_string(inst.parent.index())) +
               (inst.kind == InstanceKind::Package   ? " package"
                : inst.kind == InstanceKind::Unit    ? " unit"
                : inst.kind == InstanceKind::Program ? " program"
                                                     : "") +
               "\n";
    }
    for (std::size_t i = 0; i < d.types.size(); ++i)
        out += "type !" + std::to_string(i) + " " + typeEntryText(d, d.types[i]) + "\n";
    for (std::size_t i = 0; i < d.patterns.size(); ++i)
        out += "pattern !p" + std::to_string(i) + " " + patternLayoutText(d, d.patterns[i]) + "\n";
    for (std::size_t i = 0; i < d.nbaLayouts.size(); ++i)
        out += "nba.layout !n" + std::to_string(i) + nbaLayoutText(d.nbaLayouts[i]);
    for (std::size_t i = 0; i < d.slots.size(); ++i) {
        const Slot& s = d.slots[i];
        out += "slot %s" + std::to_string(i) + " " + quoted(s.name) + " : " + toString(s.type) +
               dimsText(s.dims) + " " + kindName(s.kind) +
               " inst=" + std::to_string(s.instance.index());
        if (s.subroutine != kNone) out += " sub=@" + std::to_string(s.subroutine.index());
        if (s.declType != kNone) out += " type=" + typeRef(s.declType);
        if (s.driverOf != kNone)
            out += " driver-of=%s" + std::to_string(s.driverOf.index()) + " strength=(" +
                   std::to_string(s.strength1) + "," + std::to_string(s.strength0) + ")";
        if (s.viewOf != kNone) out += " view-of=%s" + std::to_string(s.viewOf.index());
        if (!s.driverElements.empty()) {
            out += " driver-elements=[";
            for (std::size_t k = 0; k < s.driverElements.size(); ++k) {
                if (k != 0) out += ',';
                out += std::to_string(s.driverElements[k]);
            }
            out += ']';
        }
        out += slotFlags(s.flags) + "\n";
    }
    for (std::size_t i = 0; i < d.strengthNodes.size(); ++i) {
        const auto& node = d.strengthNodes[i];
        out += "strength-node !n" + std::to_string(i) + " " + slotRef(node.target) +
               " kind=" + kindName(node.kind) + " component=" + std::to_string(node.component) +
               " initial=(" + std::to_string(node.initial.lo) + "," +
               std::to_string(node.initial.hi) + ")" + locText(node.loc) + "\n";
    }
    for (std::size_t i = 0; i < d.strengthOrigins.size(); ++i) {
        const auto& origin = d.strengthOrigins[i];
        const char* kind = origin.kind == StrengthOriginKind::Process ? "process"
                           : origin.kind == StrengthOriginKind::Pull  ? "pull"
                                                                      : "supply";
        out += "strength-origin !d" + std::to_string(i) + " node=!n" + std::to_string(origin.node) +
               " kind=" + kind + " owner=" +
               (origin.owner == kNone ? "-" : "@" + std::to_string(origin.owner.index())) +
               " initial=(" + std::to_string(origin.initial.lo) + "," +
               std::to_string(origin.initial.hi) + ")" + locText(origin.loc) + "\n";
    }
    for (std::size_t i = 0; i < d.strengthEdges.size(); ++i) {
        const auto& edge = d.strengthEdges[i];
        out += "strength-edge !e" + std::to_string(i) + " nodes=(!n" + std::to_string(edge.first) +
               ",!n" + std::to_string(edge.second) + ") kind=" + strengthEdgeName(edge.kind) +
               " owner=" + (edge.owner == kNone ? "-" : "@" + std::to_string(edge.owner.index())) +
               (edge.delayed ? " delayed" : "") + locText(edge.loc) + "\n";
    }
    for (const NameEntry& n : d.names) {
        if (n.slot.index() < d.slots.size() && n.path == d.slot(n.slot).name) continue;
        out += "alias " + quoted(n.path) + " -> %s" + std::to_string(n.slot.index()) + "\n";
    }
    for (std::size_t i = 0; i < d.subroutines.size(); ++i) {
        const Subroutine& sub = d.subroutines[i];
        out += "\nsubroutine @" + std::to_string(i) + " " + quoted(sub.name) + " " +
               kindName(sub.kind) + (sub.automatic ? " automatic" : " static") +
               (sub.mayWait ? " may-wait" : "") + " inst=" + std::to_string(sub.instance.index()) +
               " ret=" + toString(sub.returnType);
        if (sub.frameOf.kind != Owner::Kind::None) out += " frame-of=" + ownerText(sub.frameOf);
        out += " {" + locText(sub.loc) + "\n";
        for (const Param& prm : sub.params) {
            out += "  param " + std::string(directionName(prm.dir)) + " " + quoted(prm.name) +
                   " : " + toString(prm.type) + " -> " +
                   (prm.slot != kNone ? "%s" + std::to_string(prm.slot.index())
                                      : "%f" + std::to_string(prm.frameVar.index())) +
                   (prm.refStatic ? " ref-static" : "") + "\n";
        }
        out += activationText(sub.activation) + frameText(sub.frame) + blocksText(d, sub.blocks) +
               "}\n";
    }
    for (std::size_t i = 0; i < d.processes.size(); ++i) {
        const Process& p = d.processes[i];
        out += "\nprocess @" + std::to_string(i) + " " + quoted(p.name) +
               " kind=" + kindName(p.kind) + " inst=" + std::to_string(p.instance.index());
        if (p.parent.kind != Owner::Kind::None) out += " parent=" + ownerText(p.parent);
        if (p.armBeforeStart) out += " arm-before-start";
        if (!p.triggers.empty()) {
            out += " trigger=";
            for (std::size_t k = 0; k < p.triggers.size(); ++k) {
                const Trigger& t = p.triggers[k];
                if (k != 0) out += ',';
                out += std::string(edgeName(t.edge)) + "(" + slotRef(t.range) + ")";
                if (t.guard != kNone) out += " iff @" + std::to_string(t.guard.index());
            }
        }
        if (!p.strengthTriggers.empty()) {
            out += " strength-trigger=";
            for (std::size_t k = 0; k < p.strengthTriggers.size(); ++k)
                out += (k == 0 ? "" : ",") + slotRef(p.strengthTriggers[k]);
        }
        if (!p.strengthWrites.empty()) {
            out += " strength-writes=";
            for (std::size_t k = 0; k < p.strengthWrites.size(); ++k)
                out += (k == 0 ? "!d" : ",!d") + std::to_string(p.strengthWrites[k].index());
        }
        if (!p.strengthEdgeWrites.empty()) {
            out += " strength-edge-write=";
            for (std::size_t k = 0; k < p.strengthEdgeWrites.size(); ++k)
                out += (k == 0 ? "!e" : ",!e") + std::to_string(p.strengthEdgeWrites[k].index());
        }
        if (!p.writes.empty()) {
            out += " writes=";
            for (std::size_t k = 0; k < p.writes.size(); ++k)
                out += (k == 0 ? "" : ",") + slotRef(p.writes[k]);
        }
        // What a cont process drives, when it is not all it writes (SIR_SPEC.md
        // 9); no other process drives.
        if (p.kind == ProcessKind::Cont ? p.drives != p.writes : !p.drives.empty()) {
            out += " drives=";
            for (std::size_t k = 0; k < p.drives.size(); ++k)
                out += (k == 0 ? "" : ",") + slotRef(p.drives[k]);
        }
        out += " {" + locText(p.loc) + "\n" + activationText(p.activation) + frameText(p.frame) +
               blocksText(d, p.blocks) + "}\n";
    }
    return out;
}

}  // namespace kirimasim::sir
