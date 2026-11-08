// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/systasks.h"

#include <utility>

namespace kirimasim::sir {
namespace {

struct NamedTask {
    std::string_view name;
    SysTaskInfo info;
};

constexpr NamedTask kTasks[] = {
#define KIRIMASIM_SIR_SYSTASK_KIND(Kind)
#define KIRIMASIM_SIR_SYSTASK(Kind, name, minOperands, formatStart)                                \
    {name, {SysTaskKind::Kind, minOperands, formatStart}},
#define KIRIMASIM_SIR_METHOD(Kind, name)
#include "kirimasim/sir/systasks.def"
#undef KIRIMASIM_SIR_SYSTASK_KIND
#undef KIRIMASIM_SIR_SYSTASK
#undef KIRIMASIM_SIR_METHOD
};

constexpr std::pair<std::string_view, BuiltinMethod> kMethods[] = {
#define KIRIMASIM_SIR_SYSTASK_KIND(Kind)
#define KIRIMASIM_SIR_SYSTASK(Kind, name, minOperands, formatStart)
#define KIRIMASIM_SIR_METHOD(Kind, name) {name, BuiltinMethod::Kind},
#include "kirimasim/sir/systasks.def"
#undef KIRIMASIM_SIR_SYSTASK_KIND
#undef KIRIMASIM_SIR_SYSTASK
#undef KIRIMASIM_SIR_METHOD
};

}  // namespace

std::optional<SysTaskInfo> sysTask(std::string_view name) noexcept {
    for (const NamedTask& task : kTasks)
        if (task.name == name) return task.info;
    return std::nullopt;
}

std::optional<BuiltinMethod> builtinMethod(std::string_view name) noexcept {
    for (const auto& [text, method] : kMethods)
        if (text == name) return method;
    return std::nullopt;
}

}  // namespace kirimasim::sir
