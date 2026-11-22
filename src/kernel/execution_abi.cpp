// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/execution_abi.h"
#include "abi_boundary.h"
#include "abi_views.h"
#include "kirimasim/kernel/execution.h"
#include "service_tables.h"
namespace kirimasim::kernel {
namespace {
using U32 = std::uint32_t;
constexpr U32 ok = KIRIMA_VALUE_OK, invalid = KIRIMA_VALUE_INVALID;
U32 mathName(kirima_bytes name, U32* row, U32* arity) {
    if (!abi::valid(name) || !row || !arity) return invalid;
    const auto functions = mathFunctions();
    for (U32 i = 0; i < functions.size(); ++i) {
        if (functions[i].name != abi::view(name)) continue;
        *row = i;
        *arity = functions[i].fn1 ? 1 : 2;
        return ok;
    }
    return invalid;
}
U32 math(U32 row, const double* args, U32 n, double* out) {
    const auto functions = mathFunctions();
    if (!out || !args || row >= functions.size()) return invalid;
    const auto& fn = functions[row];
    if (n == 1 && fn.fn1)
        *out = fn.fn1(args[0]);
    else if (n == 2 && fn.fn2)
        *out = fn.fn2(args[0], args[1]);
    else
        return invalid;
    return ok;
}
U32 logical(U32 op, W4 x, W4 y, W4* out) {
    if (!out || op > KIRIMA_EQUIVALENT) return invalid;
    *out = logicalValue(static_cast<Logical>(op), x, y);
    return ok;
}
U32 select(W4 cond, W4 yes, W4 no, U32 single, W4* out) {
    if (!out || single > 1) return invalid;
    *out = selectReal(cond, yes, no, single != 0);
    return ok;
}
U32 descriptor(W4 d, U32 input, kirima_byte_sink sink, void* frame) {
    if (input > 1 || !sink) return invalid;
    const auto text = input ? notReadableText(d) : notOpenText(d);
    sink(frame, abi::bytesOf(text));
    return ok;
}
}  // namespace
const void* executionService() noexcept {
    static constexpr kirima_execution_api table{mathName, math, logical, select,
                                                Boundary<descriptor>::call};
    return &table;
}
}  // namespace kirimasim::kernel
