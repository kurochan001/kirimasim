// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include <cstdint>

namespace kirimasim::kernel {
const void* queryService(std::uint32_t id, std::uint32_t version) noexcept;
const void* valueService() noexcept;
const void* ioService() noexcept;
const void* referenceService() noexcept;
const void* processService() noexcept;
const void* eventService() noexcept;
const void* objectService() noexcept;
const void* containerService() noexcept;
const void* presentationService() noexcept;
const void* scalarTaskService() noexcept;
}  // namespace kirimasim::kernel
