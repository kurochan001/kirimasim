// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/valsem/module_path.h"

#include <algorithm>
#include <array>
#include <limits>

namespace kirimasim::valsem {
bool modulePathEnabled(Logic lsb) {
    switch (lsb) {
        case Logic::Zero: return false;
        case Logic::One:
        case Logic::X:
        case Logic::Z: return true;
    }
    return false;
}
std::optional<std::uint64_t> modulePathDelay(std::span<const std::uint64_t> values, Logic from,
                                             Logic to) {
    const auto n = values.size();
    if (n != 1 && n != 2 && n != 3 && n != 6 && n != 12) return std::nullopt;
    if (from == to) return 0;
    constexpr std::array<std::array<Logic, 2>, 12> endpoints{{{Logic::Zero, Logic::One},
                                                              {Logic::One, Logic::Zero},
                                                              {Logic::Zero, Logic::Z},
                                                              {Logic::Z, Logic::One},
                                                              {Logic::One, Logic::Z},
                                                              {Logic::Z, Logic::Zero},
                                                              {Logic::Zero, Logic::X},
                                                              {Logic::X, Logic::One},
                                                              {Logic::One, Logic::X},
                                                              {Logic::X, Logic::Zero},
                                                              {Logic::X, Logic::Z},
                                                              {Logic::Z, Logic::X}}};
    if (n == 12) {
        for (std::size_t i = 0; i < endpoints.size(); ++i)
            if (endpoints[i][0] == from && endpoints[i][1] == to) return values[i];
        return std::nullopt;
    }
    const auto known = [&](Logic a, Logic b) {
        if (n == 1) return values[0];
        if (n == 2) return values[b == Logic::One || (b == Logic::Z && a == Logic::Zero) ? 0 : 1];
        if (n == 3) return values[b == Logic::One ? 0 : b == Logic::Zero ? 1 : 2];
        for (std::size_t i = 0; i < 6; ++i)
            if (endpoints[i][0] == a && endpoints[i][1] == b) return values[i];
        return std::uint64_t{0};
    };
    if (from != Logic::X && to != Logic::X) return known(from, to);
    std::uint64_t answer = to == Logic::X ? std::numeric_limits<std::uint64_t>::max() : 0;
    for (const auto candidate : {Logic::Zero, Logic::One, Logic::Z}) {
        if (candidate == from || candidate == to) continue;
        const auto delay = to == Logic::X ? known(from, candidate) : known(candidate, to);
        answer = to == Logic::X ? std::min(answer, delay) : std::max(answer, delay);
    }
    return answer;
}
}  // namespace kirimasim::valsem
