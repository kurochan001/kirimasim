// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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

/// A compiled row. For each input, the values it matches (bit v); for its
/// edge input, if it has one, the changes it matches (bit from * 3 + to).
/// The states it matches (bit v), and its output: 0, 1, kUdpX or kUdpKeep.
/// A row with more than one edge never matches (edge -2).
struct UdpRow {
    std::vector<std::uint16_t> masks;
    std::int64_t edge = -1;
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

template <class Read> inline bool udpLevel(const UdpRow& r, Read read, unsigned n, unsigned skip) {
    for (unsigned i = 0; i < n; ++i)
        if (i != skip && ((r.masks[i] >> read(i)) & 1U) == 0) return false;
    return true;
}

/// read, state and from are already normalized to 0, 1 or x.
template <class Read>
inline std::uint8_t udpNext(const UdpTable& t, std::uint8_t state, Read read, unsigned k,
                            std::uint8_t from) {
    int edge = -1;
    const unsigned change = from * 3U + read(k);
    for (const UdpRow& r : t.rows) {
        if (((r.states >> state) & 1U) == 0) continue;
        if (r.edge == -1) {
            if (udpLevel(r, read, t.inputs, t.inputs))
                return r.output == kUdpKeep ? state : r.output;
            continue;
        }
        if (edge >= 0 || r.edge != static_cast<std::int64_t>(k)) continue;
        if (((r.masks[k] >> change) & 1U) == 0 || !udpLevel(r, read, t.inputs, k)) continue;
        edge = r.output == kUdpKeep ? state : r.output;
    }
    return edge >= 0 ? static_cast<std::uint8_t>(edge) : kUdpX;
}

}  // namespace detail

/// Getter transport avoids a fixed input buffer. Getters must return stable
/// values through the call and must not mutate state or throw.
template <class Read> inline std::uint8_t udpOutputRead(const UdpTable& t, Read read) {
    const auto known = [&](unsigned i) { return detail::udpKnown(read(i)); };
    for (const UdpRow& r : t.rows)
        if (r.edge == -1 && detail::udpLevel(r, known, t.inputs, t.inputs))
            return r.output == kUdpKeep ? kUdpX : r.output;
    return kUdpX;
}

/// A combinational UDP: the output of the first level row that matches,
/// x if none does.
inline std::uint8_t udpOutput(const UdpTable& t, const std::uint8_t* in) {
    return udpOutputRead(t, [in](unsigned i) { return in[i]; });
}

/// A sequential UDP: the next state when input k changed from `from` to
/// v[k]. The values and state are normalized to 0, 1 or x.
inline std::uint8_t udpNext(const UdpTable& t, std::uint8_t state, const std::uint8_t* v,
                            unsigned k, std::uint8_t from) {
    return detail::udpNext(t, state, [v](unsigned i) { return v[i]; }, k, from);
}

/// A sequential UDP whose inputs changed from `prev` to `cur`: each changed
/// input in port order, each seeing the changes before it (VALUE_SEMANTICS.md
/// 5.9). The prefix view supplies the already processed current inputs and
/// the remaining previous inputs without allocating intermediate storage.
template <class Previous, class Current>
inline std::uint8_t udpStepRead(const UdpTable& t, std::uint8_t state, Previous prev, Current cur) {
    state = detail::udpKnown(state);
    for (unsigned k = 0; k < t.inputs; ++k) {
        const std::uint8_t to = detail::udpKnown(cur(k));
        const std::uint8_t from = detail::udpKnown(prev(k));
        if (to == from) continue;
        const auto at = [&](unsigned i) { return detail::udpKnown(i <= k ? cur(i) : prev(i)); };
        state = detail::udpNext(t, state, at, k, from);
    }
    return state;
}

inline std::uint8_t udpStep(const UdpTable& t, std::uint8_t state, const std::uint8_t* prev,
                            const std::uint8_t* cur) {
    return udpStepRead(
        t, state, [prev](unsigned i) { return prev[i]; }, [cur](unsigned i) { return cur[i]; });
}

}  // namespace kirimasim::kernel
