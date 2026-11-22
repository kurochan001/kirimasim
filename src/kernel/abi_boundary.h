// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <cstdint>
#include <new>
#include <stdexcept>

#include "kirimasim/kernel/value_abi.h"

namespace kirimasim::kernel {
template <auto Function, std::uint32_t Resource = KIRIMA_VALUE_RESOURCE> struct Boundary;
template <std::uint32_t Resource, class... Args, std::uint32_t (*Function)(Args...)>
struct Boundary<Function, Resource> {
    static std::uint32_t call(Args... args) noexcept {
        try {
            return Function(args...);
        } catch (const std::bad_alloc&) {
            return Resource;
        } catch (const std::length_error&) {
            return Resource;
        }
    }
};
}  // namespace kirimasim::kernel
