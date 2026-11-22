// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/udp.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace kirimasim::kernel {

namespace {

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
            m = changes(detail::udpLevelMask(inputs[i + 1]), detail::udpLevelMask(inputs[i + 2]));
            i += 3;
        } else {
            switch (inputs[i]) {
                case 'r': m = changes(1, 2); break;
                case 'f': m = changes(2, 1); break;
                case 'p': m = changes(1, 6) | changes(4, 2); break;
                case 'n': m = changes(2, 5) | changes(4, 1); break;
                case '*': m = changes(7, 7); break;
                default:
                    m = detail::udpLevelMask(inputs[i]);
                    edge = false;
                    break;
            }
        }
        if (edge) r.edge = r.edge == -1 ? static_cast<std::int64_t>(r.masks.size()) : -2;
        r.masks.push_back(m);
    }
    r.states = state == 0 ? 7 : static_cast<std::uint8_t>(detail::udpLevelMask(state));
    r.output = detail::udpOutputSymbol(output);
    return r;
}

}  // namespace kirimasim::kernel
