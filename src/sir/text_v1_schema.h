// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <array>
#include <string_view>
#include <type_traits>

#include "kirimasim/sir/design.h"

namespace kirimasim::sir::text_v1 {

template <typename T>
constexpr bool usesKindTag = std::is_same_v<T, Operand> || std::is_same_v<T, TypeEntry>;

template <typename T> constexpr std::string_view recordTag() {
#define KIRIMASIM_SIR_RECORD(Struct, tag)                                                          \
    if constexpr (std::is_same_v<T, Struct>) return tag;
#define KIRIMASIM_SIR_FIELD(Struct, member, mode)
#define KIRIMASIM_SIR_KIND(Struct, kind, tag)
#define KIRIMASIM_SIR_KIND_FIELD(Struct, kind, member)
#include "kirimasim/sir/records.def"
#undef KIRIMASIM_SIR_RECORD
#undef KIRIMASIM_SIR_FIELD
#undef KIRIMASIM_SIR_KIND
#undef KIRIMASIM_SIR_KIND_FIELD
    return {};
}

template <typename T, typename Visit> void visitTags(Visit visit) {
    if constexpr (usesKindTag<T>) {
#define KIRIMASIM_SIR_RECORD(Struct, tag)
#define KIRIMASIM_SIR_FIELD(Struct, member, mode)
#define KIRIMASIM_SIR_KIND(Struct, kind, tag)                                                      \
    if constexpr (std::is_same_v<T, Struct>) visit(tag, kind);
#define KIRIMASIM_SIR_KIND_FIELD(Struct, kind, member)
#include "kirimasim/sir/records.def"
#undef KIRIMASIM_SIR_RECORD
#undef KIRIMASIM_SIR_FIELD
#undef KIRIMASIM_SIR_KIND
#undef KIRIMASIM_SIR_KIND_FIELD
    } else {
        static_assert(!recordTag<T>().empty(), "SIR record needs a text tag and fields");
        visit(recordTag<T>());
    }
}

template <typename T, bool selected = true, typename Visit>
void visitFields(const T& record, Visit visit) {
#define KIRIMASIM_SIR_RECORD(Struct, tag)
#define KIRIMASIM_SIR_FIELD(Struct, member, mode) KIRIMASIM_SIR_VISIT_##mode(Struct, member)
#define KIRIMASIM_SIR_VISIT_value(Struct, member)                                                  \
    if constexpr (std::is_same_v<T, Struct> && (!selected || !usesKindTag<T>))                     \
        visit(std::string_view{#member}, &Struct::member);
#define KIRIMASIM_SIR_VISIT_derived(Struct, member)
#define KIRIMASIM_SIR_KIND(Struct, value, tag)
#define KIRIMASIM_SIR_KIND_FIELD(Struct, value, member)                                            \
    if constexpr (std::is_same_v<T, Struct> && selected) {                                         \
        if (record.kind == (value)) visit(std::string_view{#member}, &Struct::member);             \
    }
#include "kirimasim/sir/records.def"
#undef KIRIMASIM_SIR_RECORD
#undef KIRIMASIM_SIR_FIELD
#undef KIRIMASIM_SIR_VISIT_value
#undef KIRIMASIM_SIR_VISIT_derived
#undef KIRIMASIM_SIR_KIND
#undef KIRIMASIM_SIR_KIND_FIELD
}

template <typename E> struct EnumItemText {
    E value;
    std::string_view name;
};

// Each scalar enum uses its symbolic values. Op's stable names already live in ops.def.
#define ENUM_VALUE(name)                                                                           \
    EnumItemText { E::name, #name }
#define ENUM_ITEMS(Enum, ...)                                                                      \
    constexpr auto enumItems(Enum) {                                                               \
        using E = Enum;                                                                            \
        return std::array{__VA_ARGS__};                                                            \
    }

ENUM_ITEMS(TypeKind, ENUM_VALUE(Void), ENUM_VALUE(U2), ENUM_VALUE(S2), ENUM_VALUE(U4),
           ENUM_VALUE(S4), ENUM_VALUE(Real), ENUM_VALUE(ShortReal), ENUM_VALUE(String),
           ENUM_VALUE(Event), ENUM_VALUE(Chandle), ENUM_VALUE(Ref), ENUM_VALUE(DynArray),
           ENUM_VALUE(Queue), ENUM_VALUE(Strength), ENUM_VALUE(Class), ENUM_VALUE(Assoc),
           ENUM_VALUE(VirtualInterface))
ENUM_ITEMS(SlotKind, ENUM_VALUE(Var), ENUM_VALUE(Wire), ENUM_VALUE(Tri), ENUM_VALUE(Wand),
           ENUM_VALUE(Triand), ENUM_VALUE(Wor), ENUM_VALUE(Trior), ENUM_VALUE(Tri0),
           ENUM_VALUE(Tri1), ENUM_VALUE(Supply0), ENUM_VALUE(Supply1), ENUM_VALUE(Uwire))
ENUM_ITEMS(InstanceKind, ENUM_VALUE(Module), ENUM_VALUE(Package), ENUM_VALUE(Unit),
           ENUM_VALUE(Program))
ENUM_ITEMS(PatternLayout::Kind, ENUM_VALUE(Integral), ENUM_VALUE(Real), ENUM_VALUE(String),
           ENUM_VALUE(Enumeration), ENUM_VALUE(Record), ENUM_VALUE(PackedRecord), ENUM_VALUE(Array),
           ENUM_VALUE(Union), ENUM_VALUE(PackedUnion), ENUM_VALUE(TaggedUnion),
           ENUM_VALUE(Container))
ENUM_ITEMS(FrameHome, ENUM_VALUE(Ordinary), ENUM_VALUE(Scoped), ENUM_VALUE(Capture))
ENUM_ITEMS(ProcessKind, ENUM_VALUE(Cont), ENUM_VALUE(Comb), ENUM_VALUE(Latch), ENUM_VALUE(CombAt),
           ENUM_VALUE(Ff), ENUM_VALUE(FfPlain), ENUM_VALUE(Initial), ENUM_VALUE(Final),
           ENUM_VALUE(General), ENUM_VALUE(StaticInit), ENUM_VALUE(Override), ENUM_VALUE(Postponed))
ENUM_ITEMS(Edge, ENUM_VALUE(Any), ENUM_VALUE(Pos), ENUM_VALUE(Neg), ENUM_VALUE(Both))
ENUM_ITEMS(StrengthOriginKind, ENUM_VALUE(Process), ENUM_VALUE(Pull), ENUM_VALUE(Supply))
ENUM_ITEMS(ModulePathEnable, ENUM_VALUE(Always), ENUM_VALUE(Condition), ENUM_VALUE(IfNone))
ENUM_ITEMS(StrengthEdgeKind, ENUM_VALUE(Identity), ENUM_VALUE(Tran), ENUM_VALUE(Rtran),
           ENUM_VALUE(Tranif0), ENUM_VALUE(Tranif1), ENUM_VALUE(Rtranif0), ENUM_VALUE(Rtranif1))
ENUM_ITEMS(Owner::Kind, ENUM_VALUE(None), ENUM_VALUE(Process), ENUM_VALUE(Subroutine))
ENUM_ITEMS(SubroutineKind, ENUM_VALUE(Function), ENUM_VALUE(Task), ENUM_VALUE(Guard),
           ENUM_VALUE(Watch), ENUM_VALUE(PathCondition))
ENUM_ITEMS(Direction, ENUM_VALUE(Input), ENUM_VALUE(Output), ENUM_VALUE(Inout), ENUM_VALUE(Ref),
           ENUM_VALUE(ConstRef))
ENUM_ITEMS(ClassRole, ENUM_VALUE(None), ENUM_VALUE(Constructor), ENUM_VALUE(Method),
           ENUM_VALUE(StaticMethod))
ENUM_ITEMS(DumpScopeKind, ENUM_VALUE(Module), ENUM_VALUE(Begin), ENUM_VALUE(Fork), ENUM_VALUE(Task),
           ENUM_VALUE(Function))
ENUM_ITEMS(DumpType, ENUM_VALUE(Reg), ENUM_VALUE(Integer), ENUM_VALUE(Time), ENUM_VALUE(Real),
           ENUM_VALUE(Realtime), ENUM_VALUE(Event), ENUM_VALUE(Wire), ENUM_VALUE(Tri),
           ENUM_VALUE(Wand), ENUM_VALUE(Triand), ENUM_VALUE(Wor), ENUM_VALUE(Trior),
           ENUM_VALUE(Tri0), ENUM_VALUE(Tri1), ENUM_VALUE(Supply0), ENUM_VALUE(Supply1))
ENUM_ITEMS(ForeignScopeKind, ENUM_VALUE(None), ENUM_VALUE(Module), ENUM_VALUE(Begin),
           ENUM_VALUE(Fork), ENUM_VALUE(Task), ENUM_VALUE(Function), ENUM_VALUE(Generate))

#undef ENUM_ITEMS
#undef ENUM_VALUE

constexpr auto enumItems(Op) {
    return std::array{
#define KIRIMASIM_SIR_OP(name, text, result, flow, operands, writeTarget)                          \
    EnumItemText{Op::name, std::string_view{text}},
#include "kirimasim/sir/ops.def"
#undef KIRIMASIM_SIR_OP
    };
}

}  // namespace kirimasim::sir::text_v1
