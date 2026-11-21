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
#define KIRIMASIM_SIR_METHOD(Kind, name, result, receiver, arg1, arg2)
#include "kirimasim/sir/systasks.def"
#undef KIRIMASIM_SIR_SYSTASK_KIND
#undef KIRIMASIM_SIR_SYSTASK
#undef KIRIMASIM_SIR_METHOD
};

constexpr std::pair<std::string_view, MethodSignature> kMethods[] = {
#define KIRIMASIM_SIR_SYSTASK_KIND(Kind)
#define KIRIMASIM_SIR_SYSTASK(Kind, name, minOperands, formatStart)
#define KIRIMASIM_SIR_METHOD(Kind, name, result, receiver, arg1, arg2)                             \
    {name,                                                                                         \
     {BuiltinMethod::Kind,                                                                         \
      MethodValue::result,                                                                         \
      {MethodValue::receiver, MethodValue::arg1, MethodValue::arg2}}},
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

const MethodSignature* methodSignature(std::string_view name) noexcept {
    for (const auto& [text, signature] : kMethods)
        if (text == name) return &signature;
    return nullptr;
}

std::optional<BuiltinMethod> builtinMethod(std::string_view name) noexcept {
    if (const auto* signature = methodSignature(name)) return signature->kind;
    return std::nullopt;
}

bool matchesMethodValue(Type type, MethodValue role) noexcept {
    switch (role) {
        case MethodValue::None: return type == voidType();
        case MethodValue::String: return type == Type{TypeKind::String};
        case MethodValue::Int: return type == s2(32);
        case MethodValue::Integer: return type == s4(32);
        case MethodValue::Integral: return isIntegral(type);
        case MethodValue::Byte: return type == u2(8);
        case MethodValue::Bit: return type == u2(1);
        case MethodValue::Event: return type == Type{TypeKind::Event};
        case MethodValue::Real: return type == Type{TypeKind::Real};
    }
    return false;
}

}  // namespace kirimasim::sir
