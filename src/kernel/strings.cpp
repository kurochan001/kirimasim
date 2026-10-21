// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/strings.h"

namespace kirimasim::kernel {

std::uint64_t Strings::intern(std::string_view s) {
    if (s.empty()) return 0;
    std::string key(s);
    if (auto it = handles_.find(key); it != handles_.end()) return it->second;
    texts_.push_back(key);
    const auto h = static_cast<std::uint64_t>(texts_.size());
    handles_.emplace(std::move(key), h);
    return h;
}

}  // namespace kirimasim::kernel
