// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/valsem/mos.h"

#include <algorithm>
#include <array>

namespace kirimasim::valsem {
namespace {
bool valid(Signal input) { return input.lo >= -7 && input.hi <= 7 && input.lo <= input.hi; }
std::optional<int> point(StrengthTransfer kind, int p) {
    const int rank = p < 0 ? -p : p;
    int result = 0;
    switch (kind) {
        case StrengthTransfer::Identity: result = rank; break;
        case StrengthTransfer::Nonresistive: result = rank == 7 ? 6 : rank; break;
        case StrengthTransfer::Resistive: {
            constexpr std::array<int, 8> table{0, 1, 1, 2, 2, 3, 5, 5};
            result = table[static_cast<unsigned>(rank)];
            break;
        }
        default: return std::nullopt;
    }
    return p < 0 ? -result : result;
}
bool admitted(Logic control, int choice) {
    return control == Logic::Zero ? choice == 0 : control == Logic::One ? choice == 1 : true;
}
bool conducts(MosKind kind, int first, int second) {
    switch (kind) {
        case MosKind::Nmos:
        case MosKind::Rnmos: return first == 1;
        case MosKind::Pmos:
        case MosKind::Rpmos: return first == 0;
        case MosKind::Cmos:
        case MosKind::Rcmos: return first == 1 || second == 0;
    }
    return false;
}
}  // namespace

std::optional<Signal> transferStrength(StrengthTransfer kind, Signal input) {
    if (!valid(input)) return std::nullopt;
    Signal output{8, -8};
    for (int p = input.lo; p <= input.hi; ++p) {
        const auto transformed = point(kind, p);
        if (!transformed) return std::nullopt;
        output.lo = std::min(output.lo, *transformed);
        output.hi = std::max(output.hi, *transformed);
    }
    return output;
}
std::optional<Signal> mosStrength(MosKind kind, Signal input, Logic first, Logic second) {
    if (!valid(input)) return std::nullopt;
    bool dual = false;
    StrengthTransfer transfer = StrengthTransfer::Nonresistive;
    switch (kind) {
        case MosKind::Nmos:
        case MosKind::Pmos: break;
        case MosKind::Cmos: dual = true; break;
        case MosKind::Rnmos:
        case MosKind::Rpmos: transfer = StrengthTransfer::Resistive; break;
        case MosKind::Rcmos:
            transfer = StrengthTransfer::Resistive;
            dual = true;
            break;
        default: return std::nullopt;
    }
    Signal output{8, -8};
    for (int a = 0; a <= 1; ++a) {
        if (!admitted(first, a)) continue;
        for (int b = 0; b <= (dual ? 1 : 0); ++b) {
            if (dual && !admitted(second, b)) continue;
            for (int p = input.lo; p <= input.hi; ++p) {
                const auto transformed = point(transfer, p);
                if (!transformed) return std::nullopt;
                const int contribution = conducts(kind, a, b) ? *transformed : 0;
                output.lo = std::min(output.lo, contribution);
                output.hi = std::max(output.hi, contribution);
            }
        }
    }
    return output;
}
}  // namespace kirimasim::valsem
