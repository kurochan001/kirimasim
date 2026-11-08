// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace kirimasim::sir {

/// The kinds of system task and function that a syscall names (systasks.def).
/// Several names may share a kind.
enum class SysTaskKind : std::uint8_t {
#define KIRIMASIM_SIR_SYSTASK_KIND(Kind) Kind,
#define KIRIMASIM_SIR_SYSTASK(Kind, name, minOperands, formatStart)
#define KIRIMASIM_SIR_METHOD(Kind, name)
#include "kirimasim/sir/systasks.def"
#undef KIRIMASIM_SIR_SYSTASK_KIND
#undef KIRIMASIM_SIR_SYSTASK
#undef KIRIMASIM_SIR_METHOD
};

/// What systasks.def says of one system task name.
struct SysTaskInfo {
    SysTaskKind kind{};
    std::uint32_t minOperands = 1;  ///< With the name: what the handler reads by position.
    std::uint32_t formatStart = 0;  ///< The operand where a format starts; 0 for none.
};

/// The system task of a name, or nothing when SIR has no such name.
[[nodiscard]] std::optional<SysTaskInfo> sysTask(std::string_view name) noexcept;

/// The built-in methods that a builtin names (systasks.def).
enum class BuiltinMethod : std::uint8_t {
#define KIRIMASIM_SIR_SYSTASK_KIND(Kind)
#define KIRIMASIM_SIR_SYSTASK(Kind, name, minOperands, formatStart)
#define KIRIMASIM_SIR_METHOD(Kind, name) Kind,
#include "kirimasim/sir/systasks.def"
#undef KIRIMASIM_SIR_SYSTASK_KIND
#undef KIRIMASIM_SIR_SYSTASK
#undef KIRIMASIM_SIR_METHOD
};

/// The built-in method of a name, or nothing when SIR has no such method.
[[nodiscard]] std::optional<BuiltinMethod> builtinMethod(std::string_view name) noexcept;

}  // namespace kirimasim::sir
