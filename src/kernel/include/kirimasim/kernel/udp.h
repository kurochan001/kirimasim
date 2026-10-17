// User-defined primitives (VALUE_SEMANTICS.md 5.9), with each table row
// compiled to bit masks.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.
//
// The reference definition, which matches the symbols of table 29-1 one
// by one, lives in valsem; tests/unit compares the two.

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace kirimasim::kernel {

/// A value of a UDP is 0, 1 or 2 (x); 3 (z) reads as x.
inline constexpr std::uint8_t kUdpX = 2;
/// The output of a row that keeps the state.
inline constexpr std::uint8_t kUdpKeep = 3;
/// The most inputs a UDP has here (29.3.2 allows at most 10 and 9).
inline constexpr unsigned kUdpMaxInputs = 32;

/// A compiled row. For each input, the values it matches (bit v); for its
/// edge input, if it has one, the changes it matches (bit from * 3 + to).
/// The states it matches (bit v), and its output: 0, 1, kUdpX or kUdpKeep.
/// A row with more than one edge never matches (edge -2).
struct UdpRow {
    std::vector<std::uint16_t> masks;
    std::int32_t edge = -1;
    std::uint8_t states = 7;
    std::uint8_t output = kUdpX;
};

struct UdpTable {
    unsigned inputs = 0;
    std::vector<UdpRow> rows;
};

/// Compiles a row in slang's form: one symbol per input, an edge written
/// "(vw)" or as r, f, p, n or *; the state symbol (0 for none); the output
/// symbol. Defined in udp.cpp.
UdpRow compileUdpRow(std::string_view inputs, char state, char output);

namespace detail {

inline std::uint8_t udpKnown(std::uint8_t v) { return v >= kUdpX ? kUdpX : v; }

inline bool udpLevel(const UdpRow& r, const std::uint8_t* v, unsigned n, unsigned skip) {
    for (unsigned i = 0; i < n; ++i)
        if (i != skip && ((r.masks[i] >> v[i]) & 1U) == 0) return false;
    return true;
}

}  // namespace detail

/// A combinational UDP: the output of the first level row that matches,
/// x if none does.
inline std::uint8_t udpOutput(const UdpTable& t, const std::uint8_t* in) {
    std::uint8_t v[kUdpMaxInputs];
    for (unsigned i = 0; i < t.inputs; ++i)
        v[i] = detail::udpKnown(in[i]);
    for (const UdpRow& r : t.rows)
        if (r.edge == -1 && detail::udpLevel(r, v, t.inputs, t.inputs))
            return r.output == kUdpKeep ? kUdpX : r.output;
    return kUdpX;
}

/// A sequential UDP: the next state when input k changed from `from` to
/// v[k]. A level row that matches decides, else an edge row on input k,
/// else x.
inline std::uint8_t udpNext(const UdpTable& t, std::uint8_t state, const std::uint8_t* v,
                            unsigned k, std::uint8_t from) {
    int edge = -1;
    const unsigned change = from * 3U + v[k];
    for (const UdpRow& r : t.rows) {
        if (((r.states >> state) & 1U) == 0) continue;
        if (r.edge == -1) {
            if (detail::udpLevel(r, v, t.inputs, t.inputs))
                return r.output == kUdpKeep ? state : r.output;
            continue;
        }
        if (edge >= 0 || r.edge != static_cast<std::int32_t>(k)) continue;
        if (((r.masks[k] >> change) & 1U) == 0 || !detail::udpLevel(r, v, t.inputs, k)) continue;
        edge = r.output == kUdpKeep ? state : r.output;
    }
    return edge >= 0 ? static_cast<std::uint8_t>(edge) : kUdpX;
}

/// A sequential UDP whose inputs changed from `prev` to `cur`: each changed
/// input in port order, each seeing the changes before it (VALUE_SEMANTICS.md
/// 5.9).
inline std::uint8_t udpStep(const UdpTable& t, std::uint8_t state, const std::uint8_t* prev,
                            const std::uint8_t* cur) {
    std::uint8_t v[kUdpMaxInputs];
    for (unsigned i = 0; i < t.inputs; ++i)
        v[i] = detail::udpKnown(prev[i]);
    state = detail::udpKnown(state);
    for (unsigned k = 0; k < t.inputs; ++k) {
        const std::uint8_t to = detail::udpKnown(cur[k]);
        if (to == v[k]) continue;
        const std::uint8_t from = v[k];
        v[k] = to;
        state = udpNext(t, state, v, k, from);
    }
    return state;
}

}  // namespace kirimasim::kernel
