// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/model_resources.h"
#include <limits>
#include <new>
namespace kirimasim::kernel {
std::uint64_t ModelResources::nextTicket() const {
    if (issued_ == std::numeric_limits<std::uint64_t>::max()) throw std::bad_alloc{};
    return issued_ + 1;
}
}  // namespace kirimasim::kernel
