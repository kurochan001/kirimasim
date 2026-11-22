// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/io_abi.h"
#include "kirimasim/kernel/wide.h"
#include <string>
#include <string_view>

namespace kirimasim::kernel::abi {
inline bool valid(kirima_bytes data) {
    return (data.data || data.size == 0) && data.size <= std::string{}.max_size();
}
inline std::string_view view(kirima_bytes data) {
    return {data.data ? data.data : "", static_cast<std::size_t>(data.size)};
}
inline kirima_bytes bytesOf(std::string_view text) { return {text.data(), text.size()}; }
inline bool input(kirima_value_in v) {
    if (!v.words || !v.width || v.count != wide::chunks(v.width)) return false;
    const auto used = v.width % 64;
    const auto last = v.words[v.count - 1];
    return !used || ((last.a | last.b) >> used) == 0;
}
}  // namespace kirimasim::kernel::abi
