// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/udp.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace kirimasim::kernel {

namespace {

/// The values a level symbol matches (table 29-1), as bits 0 to 2.
std::uint16_t levelMask(char c) {
    switch (c) {
        case '0': return 1;
        case '1': return 2;
        case 'x': return 4;
        case '?': return 7;
        case 'b': return 3;
        default: return 0;
    }
}

/// The changes (from, to) whose two values the masks match.
std::uint16_t changes(std::uint16_t from, std::uint16_t to) {
    std::uint16_t m = 0;
    for (unsigned f = 0; f < 3; ++f)
        for (unsigned t = 0; t < 3; ++t)
            if (f != t && ((from >> f) & 1U) != 0 && ((to >> t) & 1U) != 0)
                m = static_cast<std::uint16_t>(m | (1U << (f * 3 + t)));
    return m;
}

}  // namespace

UdpRow compileUdpRow(std::string_view inputs, char state, char output) {
    UdpRow r;
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        std::uint16_t m = 0;
        bool edge = true;
        if (inputs[i] == '(' && i + 3 < inputs.size()) {
            m = changes(levelMask(inputs[i + 1]), levelMask(inputs[i + 2]));
            i += 3;
        } else {
            switch (inputs[i]) {
                case 'r': m = changes(1, 2); break;
                case 'f': m = changes(2, 1); break;
                case 'p': m = changes(1, 6) | changes(4, 2); break;
                case 'n': m = changes(2, 5) | changes(4, 1); break;
                case '*': m = changes(7, 7); break;
                default:
                    m = levelMask(inputs[i]);
                    edge = false;
                    break;
            }
        }
        if (edge) r.edge = r.edge == -1 ? static_cast<std::int64_t>(r.masks.size()) : -2;
        r.masks.push_back(m);
    }
    r.states = state == 0 ? 7 : static_cast<std::uint8_t>(levelMask(state));
    switch (output) {
        case '0': r.output = 0; break;
        case '1': r.output = 1; break;
        case '-': r.output = kUdpKeep; break;
        default: r.output = kUdpX; break;
    }
    return r;
}

}  // namespace kirimasim::kernel
