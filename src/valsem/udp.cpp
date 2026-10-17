#include "kirimasim/valsem/udp.h"

#include <cstddef>
#include <string>
#include <vector>

namespace kirimasim::valsem {

namespace {

Logic known(Logic v) { return v == Logic::Z ? Logic::X : v; }

/// A level symbol of table 29-1: 0, 1 and x themselves, ? any value, b 0
/// or 1.
bool levelMatches(char sym, Logic v) {
    switch (sym) {
        case '0': return v == Logic::Zero;
        case '1': return v == Logic::One;
        case 'x': return v == Logic::X;
        case '?': return true;
        case 'b': return v != Logic::X;
        default: return false;
    }
}

/// The fields of a row's inputs: a symbol, or the two values of "(vw)".
std::vector<std::string> fieldsOf(const std::string& inputs) {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        if (inputs[i] == '(' && i + 3 < inputs.size()) {
            out.push_back(inputs.substr(i + 1, 2));
            i += 3;
        } else {
            out.emplace_back(1, inputs[i]);
        }
    }
    return out;
}

bool isEdge(const std::string& f) {
    return f.size() == 2 || f == "r" || f == "f" || f == "p" || f == "n" || f == "*";
}

/// Whether an edge symbol matches a change from `from` to `to` (table
/// 29-1): (vw) when v matches from and w matches to; r (01), f (10), p
/// (01), (0x), (x1); n (10), (1x), (x0); * any change.
bool edgeMatches(const std::string& f, Logic from, Logic to) {
    using L = Logic;
    if (f.size() == 2) return levelMatches(f[0], from) && levelMatches(f[1], to);
    switch (f[0]) {
        case 'r': return from == L::Zero && to == L::One;
        case 'f': return from == L::One && to == L::Zero;
        case 'p': return (from == L::Zero && to != L::Zero) || (from == L::X && to == L::One);
        case 'n': return (from == L::One && to != L::One) || (from == L::X && to == L::Zero);
        case '*': return true;
        default: return false;
    }
}

Logic outputOf(char out, Logic state) {
    switch (out) {
        case '0': return Logic::Zero;
        case '1': return Logic::One;
        case '-': return state;
        default: return Logic::X;
    }
}

}  // namespace

Logic udpOutput(const std::vector<UdpRow>& rows, const std::vector<Logic>& inputs) {
    for (const UdpRow& row : rows) {
        const std::vector<std::string> fs = fieldsOf(row.inputs);
        bool all = fs.size() == inputs.size();
        for (std::size_t i = 0; all && i < fs.size(); ++i)
            all = !isEdge(fs[i]) && levelMatches(fs[i][0], known(inputs[i]));
        if (all) return outputOf(row.output, Logic::X);
    }
    return Logic::X;
}

Logic udpNext(const std::vector<UdpRow>& rows, Logic state, const std::vector<Logic>& inputs,
              std::size_t k, Logic from) {
    const Logic to = known(inputs[k]);
    bool haveEdge = false;
    Logic edge = Logic::X;
    for (const UdpRow& row : rows) {
        if (!levelMatches(row.state, state)) continue;
        const std::vector<std::string> fs = fieldsOf(row.inputs);
        if (fs.size() != inputs.size()) continue;
        std::size_t edges = 0;
        bool all = true;
        for (std::size_t i = 0; all && i < fs.size(); ++i) {
            if (isEdge(fs[i])) {
                ++edges;
                all = i == k && edgeMatches(fs[i], known(from), to);
            } else {
                all = levelMatches(fs[i][0], known(inputs[i]));
            }
        }
        if (!all) continue;
        if (edges == 0) return outputOf(row.output, state);  // a level row decides
        if (!haveEdge) {
            haveEdge = true;
            edge = outputOf(row.output, state);
        }
    }
    return haveEdge ? edge : Logic::X;
}

Logic udpStep(const std::vector<UdpRow>& rows, Logic state, const std::vector<Logic>& prev,
              const std::vector<Logic>& cur) {
    std::vector<Logic> values;
    values.reserve(prev.size());
    for (const Logic v : prev)
        values.push_back(known(v));
    for (std::size_t k = 0; k < values.size() && k < cur.size(); ++k) {
        const Logic to = known(cur[k]);
        if (to == values[k]) continue;
        const Logic from = values[k];
        values[k] = to;
        state = udpNext(rows, state, values, k, from);
    }
    return state;
}

}  // namespace kirimasim::valsem
