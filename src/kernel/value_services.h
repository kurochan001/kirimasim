// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include <cstdint>

namespace kirimasim::kernel {
const void* queryIoService(std::uint32_t id, std::uint32_t version) noexcept;
const void* queryService(std::uint32_t id, std::uint32_t version) noexcept;
}  // namespace kirimasim::kernel
