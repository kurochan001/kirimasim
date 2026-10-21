#include "kirimasim/sir/text.h"

#include <array>
#include <bit>
#include <charconv>
#include <cstdint>
#include <string>
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
    return "%s" + std::to_string(r.slot) + rangeText(r.firstElem, r.numElems, r.lsb, r.width);
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
        case Operand::Kind::Frame: return "%f" + std::to_string(o.id);
        case Operand::Kind::Subroutine:
            // A watch in wait.event has its edge.
            return (o.edge == Edge::Any ? "" : std::string(edgeName(o.edge)) + " ") + "@" +
                   std::to_string(o.id);
        case Operand::Kind::Process: return "@" + std::to_string(o.id);
        case Operand::Kind::Type: return "!" + std::to_string(o.id);
        case Operand::Kind::Scope: return "#" + std::to_string(o.id);
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
        out += "(";
        for (std::uint32_t a = 0; a < o.count && k + 1 < ops.size(); ++a)
            out += (a == 0 ? "" : ", ") + operandText(d, ops[++k]);
        out += ")";
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
        if (!out.empty()) out += ",";
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

std::string typeRef(TypeId t) { return t == kNone ? "-" : "!" + std::to_string(t); }

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
        if (f.type.kind == TypeKind::Ref) out += "<" + toString(f.target) + ">";
        out += dimsText(f.dims);
        if (f.declType != kNone) out += " type=" + typeRef(f.declType);
        out += "\n";
    }
    return out;
}

std::string blocksText(const Design& d, const std::vector<BasicBlock>& blocks) {
    std::string out;
    for (std::size_t b = 0; b < blocks.size(); ++b) {
        out += "bb" + std::to_string(b);
        const auto& params = blocks[b].params;
        if (!params.empty()) {
            out += "(";
            for (std::size_t i = 0; i < params.size(); ++i) {
                out += (i == 0 ? "%t" : ", %t") + std::to_string(params[i].temp) + ": " +
                       toString(params[i].type);
            }
            out += ")";
        }
        out += ":\n";
        for (const Instr& in : blocks[b].instrs) {
            std::string line = "  ";
            if (in.result != kNone) line += "%t" + std::to_string(in.result) + " = ";
            line += opName(in.op);
            if (in.type.kind != TypeKind::Void) line += " " + toString(in.type);
            out += line + operandsText(d, in) + locText(in.loc) + "\n";
        }
    }
    return out;
}

std::string typeEntryText(const Design& d, const TypeEntry& t) {
    std::string out = std::string(kindName(t.kind)) + " " + quoted(t.name);
    if (t.value.kind != TypeKind::Void) out += " : " + toString(t.value);
    if (t.kind == TypeEntryKind::Array) out += " elem=" + typeRef(t.element) + dimsText(t.dims);
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
                   (item.value < d.constants.size() ? constantText(d.constants[item.value]) : "?");
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

std::string toText(const Design& d) {
    std::string out = "; sir format 0\n";
    out += "precision " + timeExponentText(d.timePrecision) + "\n";
    for (std::size_t i = 0; i < d.sources.size(); ++i)
        out += "source " + std::to_string(i) + " " + quoted(d.sources[i]) + "\n";
    for (std::size_t i = 0; i < d.instances.size(); ++i) {
        const Instance& inst = d.instances[i];
        out += "instance " + std::to_string(i) + " " + quoted(inst.scopeName) +
               " def=" + quoted(inst.definition) +
               " parent=" + (inst.parent == kNone ? "-" : std::to_string(inst.parent)) +
               (inst.kind == InstanceKind::Package ? " package"
                : inst.kind == InstanceKind::Unit  ? " unit"
                                                   : "") +
               "\n";
    }
    for (std::size_t i = 0; i < d.types.size(); ++i)
        out += "type !" + std::to_string(i) + " " + typeEntryText(d, d.types[i]) + "\n";
    for (std::size_t i = 0; i < d.slots.size(); ++i) {
        const Slot& s = d.slots[i];
        out += "slot %s" + std::to_string(i) + " " + quoted(s.name) + " : " + toString(s.type) +
               dimsText(s.dims) + " " + kindName(s.kind) + " inst=" + std::to_string(s.instance);
        if (s.subroutine != kNone) out += " sub=@" + std::to_string(s.subroutine.index());
        if (s.declType != kNone) out += " type=" + typeRef(s.declType);
        if (s.driverOf != kNone)
            out += " driver-of=%s" + std::to_string(s.driverOf) + " strength=(" +
                   std::to_string(s.strength1) + "," + std::to_string(s.strength0) + ")";
        out += slotFlags(s.flags) + "\n";
    }
    for (const NameEntry& n : d.names) {
        if (n.slot < d.slots.size() && n.path == d.slots[n.slot].name) continue;
        out += "alias " + quoted(n.path) + " -> %s" + std::to_string(n.slot) + "\n";
    }
    for (std::size_t i = 0; i < d.subroutines.size(); ++i) {
        const Subroutine& sub = d.subroutines[i];
        out += "\nsubroutine @" + std::to_string(i) + " " + quoted(sub.name) + " " +
               kindName(sub.kind) + (sub.automatic ? " automatic" : " static") +
               (sub.mayWait ? " may-wait" : "") + " inst=" + std::to_string(sub.instance) +
               " ret=" + toString(sub.returnType);
        if (sub.frameOf.kind != Owner::Kind::None) out += " frame-of=" + ownerText(sub.frameOf);
        out += " {" + locText(sub.loc) + "\n";
        for (const Param& prm : sub.params) {
            out += "  param " + std::string(directionName(prm.dir)) + " " + quoted(prm.name) +
                   " : " + toString(prm.type) + " -> " +
                   (prm.slot != kNone ? "%s" + std::to_string(prm.slot)
                                      : "%f" + std::to_string(prm.frameVar)) +
                   "\n";
        }
        out += frameText(sub.frame) + blocksText(d, sub.blocks) + "}\n";
    }
    for (std::size_t i = 0; i < d.processes.size(); ++i) {
        const Process& p = d.processes[i];
        out += "\nprocess @" + std::to_string(i) + " " + quoted(p.name) +
               " kind=" + kindName(p.kind) + " inst=" + std::to_string(p.instance);
        if (p.parent.kind != Owner::Kind::None) out += " parent=" + ownerText(p.parent);
        if (!p.triggers.empty()) {
            out += " trigger=";
            for (std::size_t k = 0; k < p.triggers.size(); ++k) {
                const Trigger& t = p.triggers[k];
                if (k != 0) out += ",";
                out += std::string(edgeName(t.edge)) + "(" + slotRef(t.range) + ")";
                if (t.guard != kNone) out += " iff @" + std::to_string(t.guard.index());
            }
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
        out += " {" + locText(p.loc) + "\n" + frameText(p.frame) + blocksText(d, p.blocks) + "}\n";
    }
    return out;
}

}  // namespace kirimasim::sir
