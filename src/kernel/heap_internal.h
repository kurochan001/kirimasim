// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/kernel/heap.h"

namespace kirimasim::kernel::detail {
struct HeapFault {
    HeapError error;
};
}  // namespace kirimasim::kernel::detail
