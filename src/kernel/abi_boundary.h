// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <cstdint>
#include <new>
#include <stdexcept>

#include "kirimasim/kernel/value_abi.h"

namespace kirimasim::kernel {
template <auto Function> struct Boundary;
template <class... Args, std::uint32_t (*Function)(Args...)> struct Boundary<Function> {
    static std::uint32_t call(Args... args) noexcept {
        try {
            return Function(args...);
        } catch (const std::bad_alloc&) {
            return KIRIMA_VALUE_RESOURCE;
        } catch (const std::length_error&) {
            return KIRIMA_VALUE_RESOURCE;
        }
    }
};
}  // namespace kirimasim::kernel
