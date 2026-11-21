// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <span>
#include <string_view>

#include "kirimasim/sir/design.h"
#include "kirimasim/sir/systasks.h"

namespace kirimasim::sir {
std::string_view syscallContractError(const Design& design, std::span<const FrameVar> frame,
                                      std::span<const Type> temps, const Instr& in,
                                      SysTaskInfo task);
}
