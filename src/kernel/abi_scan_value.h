// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/io_abi.h"
#include "kirimasim/kernel/scalar_value.h"
#include "kirimasim/kernel/scan.h"

namespace kirimasim::kernel::abi {
inline ScanTarget targetKind(kirima_scan_target t) {
    switch (t.domain) {
        case KIRIMA_SCAN_TWO:
        case KIRIMA_SCAN_FOUR:
            if (t.width) return ScanTarget::Integral;
            break;
        case KIRIMA_SCAN_REAL:
        case KIRIMA_SCAN_SHORTREAL:
            if (t.width == (t.domain == KIRIMA_SCAN_REAL ? 64U : 32U)) return ScanTarget::Real;
            break;
        case KIRIMA_SCAN_STRING:
            if (!t.width) return ScanTarget::String;
            break;
        default: break;
    }
    return ScanTarget::Other;
}
// Caller has validated the target with targetKind.
inline void scanCells(Context& ctx, kirima_scan_target target, ScanTarget kind, const ScanValue& v,
                      std::vector<W4>& cells) {
    cells.resize(kind == ScanTarget::Integral ? wordsFor(target.width) : 1);
    switch (kind) {
        case ScanTarget::Integral:
            assignScannedIntegral(cells.data(), target.width, target.domain == KIRIMA_SCAN_FOUR, v);
            break;
        case ScanTarget::Real:
            cells[0] = writeReal(v.real, target.domain == KIRIMA_SCAN_SHORTREAL);
            break;
        case ScanTarget::String: cells[0] = {ctx.strings().internRead(v.text), 0}; break;
        case ScanTarget::Other: return;
    }
}
}  // namespace kirimasim::kernel::abi
