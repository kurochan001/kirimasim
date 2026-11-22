// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/process_abi.h"
namespace kirimasim::kernel::abi {
inline bool valid(kirima_context* c, kirima_process h) {
    return c && h.owner == c && !h.reserved &&
           fromC(c).scheduler().processSerial(ProcessId{h.index}) == h.serial;
}
}  // namespace kirimasim::kernel::abi
