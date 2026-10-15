#include "haremasim/sir/text.h"

#include <cstdint>
#include <string>
#include <vector>

#include "haremasim/sir/types.h"

namespace haremasim::sir {

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
            default: out += c; break;
        }
    }
    out += '"';
    return out;
}

std::string slotRef(const Design& d, SlotId s, std::uint32_t lsb, std::uint32_t width) {
    std::string out = "%";
    out += s < d.slots.size() ? d.slots[s].name : "?slot" + std::to_string(s);
    if (width != 0) {
        out += "[" + std::to_string(lsb + width - 1) + ":" + std::to_string(lsb) + "]";
    }
    return out;
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
            return out + slotRef(d, o.id, o.lsb, o.width);
        }
        case Operand::Kind::Block: return "bb" + std::to_string(o.id);
        case Operand::Kind::Text: return quoted(o.text);
    }
    return "?";
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
                  {kNeedsStrength, "strength"},
                  {kProvenNoX, "no-x"}};
    std::string out;
    for (const auto& n : kNames) {
        if ((f & n.bit) == 0) continue;
        if (!out.empty()) out += ",";
        out += n.name;
    }
    return out.empty() ? "" : " flags(" + out + ")";
}

const char* domainName(Domain d) {
    switch (d) {
        case Domain::Two: return "two";
        case Domain::Four: return "four";
        case Domain::Real: return "real";
        case Domain::Handle: return "handle";
    }
    return "?";
}

}  // namespace

std::string constantText(const Constant& c) {
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
    for (std::size_t i = 0; i < d.sources.size(); ++i)
        out += "source " + std::to_string(i) + " " + quoted(d.sources[i]) + "\n";
    for (std::size_t i = 0; i < d.instances.size(); ++i) {
        const Instance& inst = d.instances[i];
        out += "instance " + std::to_string(i) + " " + inst.scopeName + " def=" + inst.definition +
               " parent=" + (inst.parent == kNone ? "-" : std::to_string(inst.parent)) + "\n";
    }
    for (const Slot& s : d.slots) {
        out += "slot %" + s.name + " : " + toString(s.type) + " " + kindName(s.kind) +
               " domain=" + domainName(s.domain) + " inst=" + std::to_string(s.instance);
        for (const ArrayDim& dim : s.dims)
            out += " [" + std::to_string(dim.left) + ":" + std::to_string(dim.right) + "]";
        out += slotFlags(s.flags) + "\n";
    }
    for (const NameEntry& n : d.names) {
        if (n.slot < d.slots.size() && n.path == d.slots[n.slot].name) continue;
        out += "alias " + quoted(n.path) + " -> " + slotRef(d, n.slot, 0, 0) + "\n";
    }
    for (const Process& p : d.processes) {
        out += "\nprocess @" + p.name + " kind=" + kindName(p.kind) +
               " inst=" + std::to_string(p.instance);
        if (!p.triggers.empty()) {
            out += " trigger=";
            for (std::size_t i = 0; i < p.triggers.size(); ++i) {
                const Trigger& t = p.triggers[i];
                if (i != 0) out += ",";
                out +=
                    std::string(edgeName(t.edge)) + "(" + slotRef(d, t.slot, t.lsb, t.width) + ")";
            }
        }
        out += " {" + locText(p.loc) + "\n";
        for (std::size_t b = 0; b < p.blocks.size(); ++b) {
            out += "bb" + std::to_string(b) + ":\n";
            for (const Instr& in : p.blocks[b].instrs) {
                std::string line = "  ";
                if (in.result != kNone) line += "%t" + std::to_string(in.result) + " = ";
                line += opName(in.op);
                if (in.type.kind != TypeKind::Void) line += " " + toString(in.type);
                for (std::size_t k = 0; k < in.operands.size(); ++k)
                    line += (k == 0 ? " " : ", ") + operandText(d, in.operands[k]);
                out += line + locText(in.loc) + "\n";
            }
        }
        out += "}\n";
    }
    return out;
}

}  // namespace haremasim::sir
