// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Scalar module paths: approved specify contract sections 1–8.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include "kirimasim/kernel/store.h"
#include "kirimasim/kernel/w4.h"

namespace kirimasim::kernel {

/// Internal order: 01,10,0z,z1,1z,z0,0x,x1,1x,x0,xz,zx.
/// No value represents a normalized delay beyond UINT64_MAX ticks.
using ModulePathDelays = std::array<std::optional<std::uint64_t>, 12>;
/// Input delays have already been selected, clamped, rounded and scaled.
std::optional<ModulePathDelays> modulePathDelays(std::span<const std::uint64_t> values) noexcept;
/// Logic encoding is 0,1,Z=2,X=3. Equal logic values have zero delay.
std::optional<std::uint64_t> modulePathDelay(const ModulePathDelays& delays, unsigned from,
                                             unsigned to) noexcept;

enum class ModulePathEdge : std::uint8_t { Any, Pos, Neg };
enum class ModulePathEnable : std::uint8_t { Always, Condition, IfNone };
inline constexpr std::uint32_t kNoModulePathCondition = UINT32_MAX;
/// A static read-only evaluator; false propagates an existing execution error.
struct ModulePathEvaluator {
    bool (*read)(void*, std::uint32_t, W4&) noexcept = nullptr;
    void* user = nullptr;
    std::uint32_t count = 0;
};
/// Fast implementation of the independent valsem path-enable rule.
inline bool modulePathEnabled(W4 value) noexcept { return ((value.a | value.b) & 1U) != 0; }
struct ModulePathSpec {
    SlotBits source;           ///< A scalar selection in a four-state slot.
    std::uint32_t origin = 0;  ///< The original single physical destination driver.
    ModulePathEdge edge = ModulePathEdge::Any;
    ModulePathDelays delays{};
    ModulePathEnable enable = ModulePathEnable::Always;
    std::uint32_t condition = kNoModulePathCondition;
    std::uint32_t group = kNoModulePathCondition;
    /// Logical declarations remain distinct when ports share physical storage.
    std::uint32_t sourceIdentity = kNoModulePathCondition;
    std::uint32_t destinationIdentity = kNoModulePathCondition;
};
}  // namespace kirimasim::kernel
