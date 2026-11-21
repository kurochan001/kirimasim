// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <array>
#include <cstddef>

#include "kirimasim/sir/design.h"

namespace kirimasim::sir {

enum class OpResult { None, Required, Optional };
enum class OpFlow { Next, End, Wait, Fork };

struct OpInfo {
    const char* text;
    OpResult result;
    OpFlow flow;
};

inline constexpr auto kOpInfo = std::to_array<OpInfo>({
#define KIRIMASIM_SIR_OP(name, text, result, flow) {text, OpResult::result, OpFlow::flow},
#include "kirimasim/sir/ops.def"
#undef KIRIMASIM_SIR_OP
});

constexpr const OpInfo* opInfo(Op op) {
    const auto index = static_cast<std::size_t>(op);
    return index < kOpInfo.size() ? &kOpInfo[index] : nullptr;
}

}  // namespace kirimasim::sir
