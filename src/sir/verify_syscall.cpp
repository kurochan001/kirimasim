// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "verify_syscall.h"

#include <algorithm>
#include <array>

namespace kirimasim::sir {
namespace {
enum class Role {
    End,
    Void,
    VoidOrString,
    String,
    StringOrBits,
    Int,
    Integer,
    Byte,
    Bit,
    U32,
    U64,
    Time,
    Real,
    ShortReal,
    Foreign,
    Integral,
    Descriptor,
    Text,
    FormatString,
    FrameText,
    FrameIntegral,
    FrameScan,
    FrameInt,
    MemoryTarget,
    Level,
    TickUnit,
    Printable,
    Control
};
enum class Tail {
    Fixed,
    Format,
    ScanOutputs,
    Controls,
    AllOrNone,
    MinimumOnly,
    Memory,
    Special,
    DumpRows
};

struct Signature {
    Role result;
    Tail tail;
    std::array<Role, 4> args;
};
constexpr Signature signature(SysTaskKind kind) {
    using enum Role;
    switch (kind) {
#define KIRIMASIM_TASK_SIGNATURE(Kind, Result, Suffix, ...)                                        \
    case SysTaskKind::Kind: return {Result, Tail::Suffix, {__VA_ARGS__}};
#include "kirimasim/sir/task_signatures.def"
#undef KIRIMASIM_TASK_SIGNATURE
    }
    return {};
}

bool textOrBits(Type t) { return t == Type{TypeKind::String} || isIntegral(t); }
bool printable(Type t) {
    return textOrBits(t) || t == Type{TypeKind::Real} || t == Type{TypeKind::ShortReal};
}

bool matches(Type t, Role role) {
    switch (role) {
        case Role::Void: return t == voidType();
        case Role::VoidOrString: return t == voidType() || t == Type{TypeKind::String};
        case Role::Int: return t == s2(32);
        case Role::Integer: return t == s4(32);
        case Role::Byte: return t == u2(8);
        case Role::Bit: return t == u2(1);
        case Role::Control: return t == u4(1);
        case Role::U32: return t == u2(32);
        case Role::U64: return t == u2(64);
        case Role::Time: return t == u4(64) || t == u2(64);
        case Role::Real: return t == Type{TypeKind::Real};
        case Role::ShortReal: return t == Type{TypeKind::ShortReal};
        case Role::String: return t == Type{TypeKind::String};
        case Role::StringOrBits: return textOrBits(t);
        case Role::Integral: return isIntegral(t);
        case Role::Descriptor: return t == u4(32);
        case Role::Foreign: return t == voidType() || isIntegral(t) || t == Type{TypeKind::Real};
        case Role::Printable: return printable(t);
        default: return false;
    }
}

struct Operands {
    const Design& d;
    std::span<const FrameVar> frame;
    std::span<const Type> temps;

    const FrameVar* field(const Operand& o) const {
        return o.kind == Operand::Kind::Frame && o.id < frame.size() ? &frame[o.id] : nullptr;
    }
    Type memoryElement(const Operand& o) const {
        if (const auto* f = field(o); f && !f->dims.empty())
            return f->type.kind == TypeKind::Ref ? f->target : f->type;
        if (o.kind == Operand::Kind::Slot && o.id < d.slots.size() && !d.slots[o.id].dims.empty())
            return d.slots[o.id].type;
        return voidType();
    }
    Type value(const Operand& o) const {
        if (o.kind == Operand::Kind::Temp && o.id < temps.size()) return temps[o.id];
        if (o.kind == Operand::Kind::Const && o.id < d.constants.size())
            return d.constants[o.id].type;
        return voidType();
    }
    bool arg(const Operand& o, Role role) const {
        switch (role) {
            case Role::Text: return o.kind == Operand::Kind::Text;
            case Role::FormatString: return o.kind == Operand::Kind::Text || textOrBits(value(o));
            case Role::Printable: return o.kind == Operand::Kind::Text || printable(value(o));
            case Role::MemoryTarget: return isIntegral(memoryElement(o));
            case Role::FrameIntegral:
            case Role::FrameText:
            case Role::FrameScan:
            case Role::FrameInt: {
                const auto* f = field(o);
                if (!f || !f->dims.empty()) return false;
                if (role == Role::FrameIntegral) return isIntegral(f->type);
                if (role == Role::FrameText) return textOrBits(f->type);
                if (role == Role::FrameScan) return printable(f->type);
                return f->type == s2(32);
            }
            case Role::Level:
            case Role::TickUnit: {
                if (o.kind != Operand::Kind::Const || o.id >= d.constants.size()) return false;
                const auto& c = d.constants[o.id];
                const auto nonzero = [](auto word) { return word != 0; };
                if (!isIntegral(c.type) || c.aWords.empty() ||
                    std::ranges::any_of(std::span(c.aWords).subspan(1), nonzero) ||
                    std::ranges::any_of(c.bWords, nonzero))
                    return false;
                return role == Role::Level ? c.aWords.front() <= 2 : c.aWords.front() != 0;
            }
            default: return matches(value(o), role);
        }
    }
};

bool memoryTail(const Operands& operands, std::span<const Operand> args) {
    if (args.size() < 2) return false;
    const auto element = operands.memoryElement(args[1]);
    auto tail = args.subspan(2);
    if (!tail.empty() && tail.front().kind == Operand::Kind::Type) {
        const auto id = tail.front().id;
        if (id >= operands.d.types.size()) return false;
        const auto& row = operands.d.types[id];
        if (row.kind != TypeEntryKind::Enum || row.value != element) return false;
        tail = tail.subspan(1);
    }
    return tail.size() <= 2 &&
           std::ranges::all_of(tail, [&](const auto& o) { return operands.value(o) == s4(64); });
}
}  // namespace

std::string_view syscallContractError(const Design& design, std::span<const FrameVar> frame,
                                      std::span<const Type> temps, const Instr& in,
                                      SysTaskInfo task) {
    const auto spec = signature(task.kind);
    if (!matches(in.type, spec.result)) return "result differs from its system task signature";
    if (in.operands.size() < task.minOperands)
        return "has fewer operands than its system task reads";
    const Operands values{design, frame, temps};
    const auto args = std::span(in.operands).subspan(1);
    const auto prefix =
        static_cast<std::size_t>(std::ranges::find(spec.args, Role::End) - spec.args.begin());
    for (std::size_t i = 0; i < std::min(prefix, args.size()); ++i)
        if (!values.arg(args[i], spec.args[i]))
            return "operand differs from its system task signature";
    switch (spec.tail) {
        case Tail::Special:
            break;  // VPI and patterned/strength format records have dedicated checks.
        case Tail::Memory:
            if (!memoryTail(values, args))
                return "invalid memory array, enum row or address operands";
            break;
        case Tail::DumpRows:
            if (!args.empty() && !values.arg(args.front(), Role::U32)) return "invalid dump level";
            for (std::size_t i = 2; i < args.size(); i += 2)
                if (args[i].kind != Operand::Kind::Const || !values.arg(args[i], Role::U32) ||
                    std::ranges::any_of(design.constants[args[i].id].bWords,
                                        [](auto w) { return w != 0; }))
                    return "invalid dump-table row operand";
            break;
        case Tail::Fixed:
            if (args.size() > prefix) return "has extra system task operands";
            break;
        case Tail::AllOrNone:
            if (!args.empty() && args.size() != prefix)
                return "system task needs all or none of its arguments";
            break;
        case Tail::MinimumOnly:
            if (in.operands.size() != task.minOperands) return "has extra distribution arguments";
            break;
        case Tail::Format:
        case Tail::ScanOutputs:
        case Tail::Controls: {
            const auto role = spec.tail == Tail::Format        ? Role::Printable
                              : spec.tail == Tail::ScanOutputs ? Role::FrameScan
                                                               : Role::Control;
            if (args.size() < prefix || (spec.tail == Tail::Controls && args.size() == prefix))
                return "system task is missing a required argument";
            for (const auto& o : args.subspan(prefix))
                if (!values.arg(o, role)) return "invalid variadic system task operand";
            break;
        }
    }
    return {};
}
}  // namespace kirimasim::sir
