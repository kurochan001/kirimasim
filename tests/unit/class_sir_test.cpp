// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original producer-independent class SIR matrices. Grounds: the approved
// class contract 1, 3-8 and 10; IEEE 1800-2023 clauses 8 and 13.5.2.
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "kirimasim/sir/classes.h"
#include "kirimasim/sir/text.h"
#include "gtest/gtest.h"

namespace {
using namespace kirimasim::sir;

std::string errors(const Design& design) {
    std::string text;
    for (const auto& error : verify(design))
        text += error.where + ": " + error.message + "\n";
    return text;
}
Instr instruction(Op op, Type type = voidType(), TempId result = kNone,
                  std::vector<Operand> operands = {}) {
    return {op, type, result, std::move(operands), {0, 1, 1}};
}
Instr effect(Op op, std::vector<Operand> operands = {}) {
    return instruction(op, voidType(), kNone, std::move(operands));
}
struct Fixture {
    Design d;
    Type scalar;
    explicit Fixture(Type type = s4(32)) : scalar(type) {
        d.sources = {"class.sv"};
        d.instances = {{"top", "top", kNone}};
        d.classes.resize(5);
        d.classes[0].name = "Derived";
        d.classes[0].base = ClassId{1};
        d.classes[0].interfaces = {ClassId{2}};
        d.classes[0].fields = {{"value", classType(ClassId{1})}};
        d.classes[1].name = "Base";
        d.classes[1].fields = {
            {"value", scalar}, {"next", classType(ClassId{0})}, {"array", scalar, {{2, 0}}}};
        d.classes[2].name = "Interface";
        d.classes[2].interfaceClass = true;
        d.classes[3].name = "WeakBase";
        d.classes[3].weakTarget = ClassId{1};
        d.classes[4].name = "Other";
        d.virtualSlots = {{"value", ClassId{1}, SubroutineKind::Function, u2(64)}};
        for (std::uint32_t i = 0; i < d.classes.size(); ++i) {
            d.classes[i].virtualMethods = {SubroutineId{kNone}};
            if (i == 2) continue;
            d.classes[i].constructor = constructor(ClassId{i});
        }
        const auto baseMethod = method(ClassId{1});
        const auto derivedMethod = method(ClassId{0});
        d.classes[1].virtualMethods[0] = baseMethod;
        d.classes[0].virtualMethods[0] = derivedMethod;
        d.slots = {{"top.object", classType(ClassId{1})}, {"top.integer", u2(64)}};
        Process process;
        process.name = "top.initial";
        process.loc = {0, 1, 1};
        process.blocks.resize(1);
        process.blocks[0].instrs = {effect(Op::Ret)};
        process.writes = {{SlotId{0}}, {SlotId{1}}};
        d.processes.push_back(std::move(process));
    }
    Operand constant(Type type, std::uint64_t value = 0) {
        Constant c;
        c.type = type;
        const auto count = isIntegral(type) ? integralWords(type.width) : 1;
        c.aWords.resize(count, 0);
        if (count != 0) c.aWords[0] = value;
        if (isFourState(type)) c.bWords.resize(count, 0);
        const ConstId id{static_cast<std::uint32_t>(d.constants.size())};
        d.constants.push_back(std::move(c));
        return Operand::constant(id);
    }
    Subroutine instance(ClassId owner, ClassRole role) {
        Subroutine sub;
        sub.name = "top.method";
        sub.automatic = true;
        sub.loc = {0, 1, 1};
        sub.classOwner = owner;
        sub.classRole = role;
        sub.params = {{"this", Direction::Input, classType(owner), kNone, FrameVarId{0}}};
        sub.frame = {{"this", classType(owner)}};
        sub.blocks.resize(1);
        return sub;
    }
    SubroutineId constructor(ClassId owner) {
        auto sub = instance(owner, ClassRole::Constructor);
        auto& code = sub.blocks[0].instrs;
        if (d.classes[owner.index()].base != kNone) code.push_back(effect(Op::ObjectBaseInit));
        code.push_back(effect(Op::ObjectStageBegin));
        code.push_back(effect(Op::ObjectStageBody));
        code.push_back(effect(Op::ObjectStageEnd));
        code.push_back(effect(Op::Ret));
        const SubroutineId id{static_cast<std::uint32_t>(d.subroutines.size())};
        d.subroutines.push_back(std::move(sub));
        return id;
    }
    SubroutineId method(ClassId owner) {
        auto sub = instance(owner, ClassRole::Method);
        sub.returnType = u2(64);
        sub.blocks[0].instrs = {effect(Op::Ret, {constant(u2(64), 7)})};
        const SubroutineId id{static_cast<std::uint32_t>(d.subroutines.size())};
        d.subroutines.push_back(std::move(sub));
        return id;
    }
    Subroutine& ctor(unsigned type = 1) {
        return d.subroutines[d.classes[type].constructor.index()];
    }
    Operand emit(Op op, Type type = voidType(), std::vector<Operand> operands = {},
                 unsigned block = 0) {
        auto& p = d.processes[0];
        if (p.blocks.size() <= block) p.blocks.resize(block + 1);
        auto& code = p.blocks[block].instrs;
        if (!code.empty() && code.back().op == Op::Ret) code.pop_back();
        const TempId id = type == voidType() ? TempId{kNone} : TempId{p.numTemps++};
        code.push_back(instruction(op, type, id, std::move(operands)));
        if (!isTerminator(op)) code.push_back(effect(Op::Ret));
        return Operand::temp(id);
    }
    void initialize(unsigned owner, unsigned member, bool constantField = false) {
        auto& field = d.classes[owner].fields[member];
        field.hasInitializer = true;
        field.constant = constantField;
        std::vector<Operand> values{Operand::classField({ClassId{owner}, member})};
        for (std::uint64_t i = 0; i < elementCount(field); ++i)
            values.push_back(constant(field.type));
        auto& code = ctor(owner).blocks[0].instrs;
        const auto at = std::find_if(code.begin(), code.end(),
                                     [](const Instr& in) { return in.op == Op::ObjectStageBody; });
        code.insert(at, effect(Op::ObjectInit, std::move(values)));
    }
    SubroutineId refFunction(Direction direction, std::vector<ArrayDim> dims = {}) {
        Subroutine sub;
        sub.name = "top.reference";
        sub.automatic = true;
        sub.loc = {0, 1, 1};
        const auto type = classType(ClassId{1});
        sub.params = {{"argument", direction, type, kNone, FrameVarId{0}}};
        FrameVar argument;
        argument.name = "argument";
        argument.target = type;
        argument.type = classRefType(ClassId{1}, direction == Direction::ConstRef);
        argument.dims = std::move(dims);
        sub.frame.push_back(std::move(argument));
        sub.blocks.resize(1);
        sub.blocks[0].instrs = {effect(Op::Ret)};
        const SubroutineId id{static_cast<std::uint32_t>(d.subroutines.size())};
        d.subroutines.push_back(std::move(sub));
        return id;
    }
};

TEST(ClassSir, DescriptorsKeepRecursiveFieldsHiddenOwnersAndStaticViewsAcrossWidths) {
    for (const auto width : {1U, 32U, 65U, 129U, 257U}) {
        for (const bool four : {false, true}) {
            Fixture f(four ? u4(width) : u2(width));
            EXPECT_EQ(errors(f.d), "") << width << ' ' << four;
            const auto checked = validateClasses(f.d);
            EXPECT_TRUE(checked.assignable(ClassId{0}, ClassId{1}));
            EXPECT_TRUE(checked.assignable(ClassId{0}, ClassId{2}));
            EXPECT_FALSE(checked.assignable(ClassId{1}, ClassId{0}));
            EXPECT_FALSE(checked.assignable(ClassId{3}, ClassId{1}));
            EXPECT_TRUE(hasClassMetadata(f.d));
        }
    }
    EXPECT_EQ(toString(classType(ClassId{4})), "class<!c4>");
    EXPECT_EQ(toString(classRefType(ClassId{4})), "ref<class<!c4>>");
    EXPECT_EQ(domainOf(classType(ClassId{4})), Domain::Handle);
}

TEST(ClassSir, InheritanceRejectsCyclesKindMismatchesFinalBasesAndDuplicateInterfaces) {
    using Change = void (*)(Fixture&);
    const std::array<Change, 8> changes{
        +[](Fixture& f) { f.d.classes[1].base = ClassId{0}; },
        +[](Fixture& f) { f.d.classes[2].interfaces = {ClassId{2}}; },
        +[](Fixture& f) { f.d.classes[0].base = ClassId{2}; },
        +[](Fixture& f) { f.d.classes[0].base = ClassId{99}; },
        +[](Fixture& f) { f.d.classes[1].final = true; },
        +[](Fixture& f) { f.d.classes[2].final = true; },
        +[](Fixture& f) { f.d.classes[0].interfaces.push_back(ClassId{2}); },
        +[](Fixture& f) { f.d.classes[3].weakTarget = ClassId{99}; }};
    for (std::size_t i = 0; i < changes.size(); ++i) {
        SCOPED_TRACE(i);
        Fixture f;
        ASSERT_EQ(errors(f.d), "");
        changes[i](f);
        EXPECT_FALSE(errors(f.d).empty());
    }
    Design cycle;
    cycle.instances = {{"top", "top", kNone}};
    cycle.classes.resize(2);
    for (unsigned i = 0; i < 2; ++i) {
        cycle.classes[i].name = "Interface" + std::to_string(i);
        cycle.classes[i].interfaceClass = true;
        cycle.classes[i].interfaces = {ClassId{1 - i}};
    }
    EXPECT_NE(errors(cycle).find("inheritance cycle"), std::string::npos);
}

TEST(ClassSir, OwnFieldTypesDimensionsAndDeclaringStaticSlotsAreChecked) {
    using Change = void (*)(Fixture&);
    const std::array<Change, 10> changes{
        +[](Fixture& f) { f.d.classes[1].fields[0].type = voidType(); },
        +[](Fixture& f) { f.d.classes[1].fields[0].type = classType(ClassId{99}); },
        +[](Fixture& f) { f.d.classes[1].fields[0].type.object = ClassId{1}; },
        +[](Fixture& f) { f.d.classes[1].fields[0].type = refType(); },
        +[](Fixture& f) { f.d.classes[1].fields[0].dims = {{INT64_MIN, INT64_MAX}}; },
        +[](Fixture& f) { f.d.classes[1].fields[0].declType = TypeId{99}; },
        +[](Fixture& f) { f.d.classes[1].fields[1].name = "value"; },
        +[](Fixture& f) { f.d.classes[1].statics = {{"static", SlotId{99}}}; },
        +[](Fixture& f) { f.d.classes[2].fields = {{"illegal", u2(1)}}; },
        +[](Fixture& f) { f.d.classes[1].scope = InstanceId{99}; }};
    for (std::size_t i = 0; i < changes.size(); ++i) {
        SCOPED_TRACE(i);
        Fixture f;
        ASSERT_EQ(errors(f.d), "");
        changes[i](f);
        EXPECT_FALSE(errors(f.d).empty());
    }
    Fixture f;
    f.d.classes[1].statics = {{"static", SlotId{1}}};
    EXPECT_EQ(errors(f.d), "");
    f.d.instances.push_back({"other", "other", kNone});
    f.d.slots[1].instance = InstanceId{1};
    EXPECT_FALSE(errors(f.d).empty());
    f.d.slots[1].instance = InstanceId{0};
    f.d.classes[0].statics = {{"duplicate_owner", SlotId{1}}};
    EXPECT_FALSE(errors(f.d).empty());
}

TEST(ClassSir, VirtualSlotsValidateSignaturesOwnershipAndInheritedSelections) {
    using Change = void (*)(Fixture&);
    const std::array<Change, 8> changes{
        +[](Fixture& f) { f.d.classes[0].virtualMethods.clear(); },
        +[](Fixture& f) { f.d.classes[0].virtualMethods[0] = kNone; },
        +[](Fixture& f) { f.d.classes[0].virtualMethods[0] = SubroutineId{99}; },
        +[](Fixture& f) { f.d.classes[3].virtualMethods[0] = f.d.classes[1].virtualMethods[0]; },
        +[](Fixture& f) { f.d.virtualSlots[0].result = u4(64); },
        +[](Fixture& f) { f.d.virtualSlots[0].declaring = ClassId{99}; },
        +[](Fixture& f) { f.d.virtualSlots[0].parameters = {{Direction::Input, u2(8)}}; },
        +[](Fixture& f) {
            f.d.subroutines[f.d.classes[0].virtualMethods[0].index()].classRole =
                ClassRole::StaticMethod;
        }};
    for (std::size_t i = 0; i < changes.size(); ++i) {
        SCOPED_TRACE(i);
        Fixture f;
        ASSERT_EQ(errors(f.d), "");
        changes[i](f);
        EXPECT_FALSE(errors(f.d).empty());
    }
    Fixture abstract;
    abstract.d.classes[1].abstract = true;
    abstract.d.classes[1].virtualMethods[0] = kNone;
    EXPECT_EQ(errors(abstract.d), "");
}

TEST(ClassSir, ConstructorsNeedAutomaticTypedReceiversAndTheirOwnRegisteredRole) {
    using Change = void (*)(Fixture&);
    const std::array<Change, 6> changes{
        +[](Fixture& f) { f.ctor().automatic = false; },
        +[](Fixture& f) { f.ctor().params.clear(); },
        +[](Fixture& f) { f.ctor().frame[0].type = classType(ClassId{0}); },
        +[](Fixture& f) { f.ctor().classRole = ClassRole::Method; },
        +[](Fixture& f) { f.ctor().classOwner = ClassId{99}; },
        +[](Fixture& f) { f.ctor().returnType = classType(ClassId{1}); }};
    for (std::size_t i = 0; i < changes.size(); ++i) {
        SCOPED_TRACE(i);
        Fixture f;
        ASSERT_EQ(errors(f.d), "");
        changes[i](f);
        EXPECT_FALSE(errors(f.d).empty());
    }
}

TEST(ClassSir, ConstructorStagesCannotSkipRepeatOrReorderTheirPhases) {
    using Change = void (*)(Fixture&);
    const std::array<Change, 7> changes{
        +[](Fixture& f) { f.ctor().blocks[0].instrs.erase(f.ctor().blocks[0].instrs.begin()); },
        +[](Fixture& f) { f.ctor().blocks[0].instrs[1] = effect(Op::ObjectStageBegin); },
        +[](Fixture& f) { f.ctor().blocks[0].instrs[2] = effect(Op::Ret); },
        +[](Fixture& f) {
            f.ctor().blocks[0].instrs.insert(f.ctor().blocks[0].instrs.begin(),
                                             effect(Op::ObjectBaseInit));
        },
        +[](Fixture& f) { f.ctor(0).blocks[0].instrs.erase(f.ctor(0).blocks[0].instrs.begin()); },
        +[](Fixture& f) {
            f.ctor(0).blocks[0].instrs.insert(f.ctor(0).blocks[0].instrs.begin() + 1,
                                              effect(Op::ObjectBaseInit));
        },
        +[](Fixture& f) { f.ctor().blocks[0].instrs = {effect(Op::Ret)}; }};
    for (std::size_t i = 0; i < changes.size(); ++i) {
        SCOPED_TRACE(i);
        Fixture f;
        ASSERT_EQ(errors(f.d), "");
        changes[i](f);
        EXPECT_FALSE(errors(f.d).empty());
    }
    Fixture initialized;
    initialized.initialize(1, 0, true);
    initialized.initialize(1, 2);
    EXPECT_EQ(errors(initialized.d), "");
    auto& code = initialized.ctor().blocks[0].instrs;
    std::swap(code[1], code[2]);
    EXPECT_NE(errors(initialized.d).find("field initializers must run in declaration order"),
              std::string::npos);
    std::swap(code[1], code[2]);
    initialized.d.classes[1].fields[2].hasInitializer = false;
    EXPECT_NE(errors(initialized.d).find("declaration initialization needs a source initializer"),
              std::string::npos);
}

TEST(ClassSir, DeclarationInitializersMustDominateBodyEntryOnEveryControlFlowPath) {
    for (unsigned missing = 0; missing < 3; ++missing) {
        Fixture f;
        f.d.classes[1].fields[0].hasInitializer = true;
        const auto condition = f.constant(u2(1), 1), initial = f.constant(f.scalar, 0);
        auto& ctor = f.ctor();
        ctor.blocks.resize(4);
        ctor.blocks[0].instrs = {
            effect(Op::ObjectStageBegin),
            effect(Op::BrIf, {condition, Operand::block(BlockId{1}), Operand::block(BlockId{2})})};
        const auto init = effect(Op::ObjectInit, {Operand::classField({ClassId{1}, 0}), initial});
        if (missing != 1) ctor.blocks[1].instrs.push_back(init);
        ctor.blocks[1].instrs.push_back(effect(Op::Br, {Operand::block(BlockId{3})}));
        if (missing != 2) ctor.blocks[2].instrs.push_back(init);
        ctor.blocks[2].instrs.push_back(effect(Op::Br, {Operand::block(BlockId{3})}));
        ctor.blocks[3].instrs = {effect(Op::ObjectStageBody), effect(Op::ObjectStageEnd),
                                 effect(Op::Ret)};
        EXPECT_EQ(errors(f.d).empty(), missing == 0) << missing << '\n' << errors(f.d);
    }
}

TEST(ClassSir, ClassConstantsCannotForgeTokensOrHideMalformedTypeQualifiers) {
    Fixture f;
    f.constant(classType(ClassId{1}));
    ASSERT_EQ(errors(f.d), "");
    f.d.constants.back().aWords[0] = 1;
    EXPECT_FALSE(errors(f.d).empty());
    f.d.constants.back().aWords[0] = 0;
    f.d.constants.back().bWords = {0};
    EXPECT_FALSE(errors(f.d).empty());
    f.d.constants.back().bWords.clear();
    f.d.constants.back().type.detail = TypeId{0};
    EXPECT_FALSE(errors(f.d).empty());
}

TEST(ClassSir, TypedAllocationViewsCopyCastsPropertiesAndContainerValuesCompose) {
    for (const auto width : {1U, 32U, 65U, 129U, 257U}) {
        Fixture f(u4(width));
        const auto derived = f.emit(Op::ObjectNew, classType(ClassId{0}));
        const auto base = f.emit(Op::ObjectView, classType(ClassId{1}), {derived});
        const auto copy = f.emit(Op::ObjectCopy, classType(ClassId{1}), {base});
        f.emit(Op::ObjectId, u2(64), {copy});
        f.emit(Op::ObjectSeed, voidType(), {derived, f.constant(u2(32), 13)});
        f.emit(Op::ObjectCastCheck, u2(1), {Operand::classRow(ClassId{0}), base});
        f.emit(Op::ObjectCast, classType(ClassId{0}), {base});
        f.emit(Op::ObjectRead, f.scalar, {Operand::classField({ClassId{1}, 0}), derived});
        f.emit(Op::ObjectWrite, voidType(),
               {Operand::classField({ClassId{1}, 0}), base, f.constant(f.scalar, 1)});
        f.emit(Op::ObjectRead, f.scalar,
               {Operand::classField({ClassId{1}, 2}), derived, f.constant(s4(32), 2)});
        f.emit(Op::ObjectWrite, voidType(),
               {Operand::classField({ClassId{1}, 2}), base, f.constant(s4(32), 1),
                f.constant(f.scalar, 1)});
        f.emit(Op::Store, voidType(), {Operand::slot(SlotId{0}), base});
        const auto null = f.constant(classType(ClassId{1}));
        f.emit(Op::Eq, u2(1), {base, null});
        f.emit(Op::Ceq, u2(1), {base, null});
        f.emit(Op::Truth, u2(1), {base});
        f.emit(Op::Select, classType(ClassId{1}), {f.constant(u4(1), 1), base, null});
        TypeEntry queue;
        queue.kind = TypeEntryKind::Queue;
        queue.value = classType(ClassId{1});
        f.d.types.push_back(queue);
        const auto packed = f.emit(Op::ContainerPack, queueType(TypeId{0}), {base, null});
        f.emit(Op::ContainerRead, classType(ClassId{1}), {packed, f.constant(s4(32), 0)});
        EXPECT_EQ(errors(f.d), "") << width;
    }
}

TEST(ClassSir, NullAndTypedNullRemainDistinctCastInputsWithoutAcceptingChandleVariables) {
    for (const auto source : {ClassId{0}, ClassId{1}, ClassId{4}}) {
        Fixture f;
        const auto typed = f.constant(classType(source));
        const auto literal = f.constant({TypeKind::Chandle, 0});
        f.emit(Op::ObjectCastCheck, u2(1), {Operand::classRow(ClassId{0}), typed});
        f.emit(Op::ObjectCastCheck, u2(1), {Operand::classRow(ClassId{0}), literal});
        f.emit(Op::ObjectCast, classType(ClassId{0}), {typed});
        f.emit(Op::ObjectCast, classType(ClassId{0}), {literal});
        ASSERT_EQ(errors(f.d), "");
        f.d.slots.push_back({"top.chandle", {TypeKind::Chandle, 0}});
        const auto value = f.emit(Op::Load, {TypeKind::Chandle, 0}, {Operand::slot(SlotId{2})});
        f.emit(Op::ObjectCast, classType(ClassId{0}), {value});
        EXPECT_FALSE(errors(f.d).empty());
    }
}

TEST(ClassSir, PropertyOperandsCannotLoseTheirOwnerShapeOrWritePermissions) {
    using Change = void (*)(Fixture&);
    const std::array<Change, 8> changes{
        +[](Fixture& f) { f.d.processes[0].blocks[0].instrs[0].operands[0].id = 4; },
        +[](Fixture& f) { f.d.processes[0].blocks[0].instrs[0].operands[0].member = 99; },
        +[](Fixture& f) {
            f.d.processes[0].blocks[0].instrs[0].operands[0].kind = Operand::Kind::Type;
        },
        +[](Fixture& f) { f.d.processes[0].blocks[0].instrs[0].operands[0].width = 1; },
        +[](Fixture& f) { f.d.processes[0].blocks[0].instrs[0].operands.pop_back(); },
        +[](Fixture& f) {
            f.d.processes[0].blocks[0].instrs[0].operands.push_back(f.constant(u2(32)));
        },
        +[](Fixture& f) { f.d.processes[0].blocks[0].instrs[0].type = classType(ClassId{1}); },
        +[](Fixture& f) {
            f.d.processes[0].blocks[0].instrs[0].operands[1] = f.constant(classType(ClassId{4}));
        }};
    for (std::size_t i = 0; i < changes.size(); ++i) {
        Fixture f;
        f.emit(Op::ObjectRead, f.scalar,
               {Operand::classField({ClassId{1}, 2}), f.constant(classType(ClassId{1})),
                f.constant(s4(32), 1)});
        ASSERT_EQ(errors(f.d), "");
        changes[i](f);
        EXPECT_FALSE(errors(f.d).empty()) << i;
    }
    Fixture constant;
    constant.d.classes[1].fields[0].constant = true;
    constant.emit(Op::ObjectWrite, voidType(),
                  {Operand::classField({ClassId{1}, 0}), constant.constant(classType(ClassId{1})),
                   constant.constant(constant.scalar)});
    EXPECT_FALSE(errors(constant.d).empty());
}

TEST(ClassSir, DeclarationAndBodyConstWritesUseDifferentConstructorPhases) {
    Fixture initialized;
    initialized.initialize(1, 0, true);
    ASSERT_EQ(errors(initialized.d), "");
    auto& code = initialized.ctor().blocks[0].instrs;
    std::swap(code[1], code[2]);
    EXPECT_FALSE(errors(initialized.d).empty());
    Fixture body;
    body.d.classes[1].fields[0].constant = true;
    const auto init =
        effect(Op::ObjectInit, {Operand::classField({ClassId{1}, 0}), body.constant(body.scalar)});
    body.ctor().blocks[0].instrs.insert(body.ctor().blocks[0].instrs.begin() + 2, init);
    ASSERT_EQ(errors(body.d), "");
    std::swap(body.ctor().blocks[0].instrs[1], body.ctor().blocks[0].instrs[2]);
    EXPECT_FALSE(errors(body.d).empty());
    Fixture wrongOwner;
    wrongOwner.ctor(0).blocks[0].instrs.insert(
        wrongOwner.ctor(0).blocks[0].instrs.begin() + 2,
        effect(Op::ObjectInit,
               {Operand::classField({ClassId{1}, 0}), wrongOwner.constant(wrongOwner.scalar)}));
    EXPECT_FALSE(errors(wrongOwner.d).empty());
}

TEST(ClassSir, OrdinaryCallsAndReportsCannotInvokeInstanceMethodsOrConstructors) {
    for (const bool constructor : {false, true}) {
        Fixture f;
        const auto callee =
            constructor ? f.d.classes[1].constructor : f.d.classes[1].virtualMethods[0];
        f.emit(Op::Call, constructor ? voidType() : u2(64),
               {Operand::subroutine(callee), f.constant(classType(ClassId{1}))});
        EXPECT_FALSE(errors(f.d).empty());
    }
    Fixture report;
    const auto method = report.d.classes[1].virtualMethods[0];
    report.emit(Op::ReportCall, voidType(),
                {Operand::str("final"), Operand::subroutine(method),
                 report.constant(classType(ClassId{1}))});
    EXPECT_FALSE(errors(report.d).empty());
    Fixture staticCall;
    Subroutine sub;
    sub.name = "top.static";
    sub.classRole = ClassRole::StaticMethod;
    sub.classOwner = ClassId{1};
    sub.automatic = true;
    sub.loc = {0, 1, 1};
    sub.blocks.resize(1);
    sub.blocks[0].instrs = {effect(Op::Ret)};
    const SubroutineId id{static_cast<std::uint32_t>(staticCall.d.subroutines.size())};
    staticCall.d.subroutines.push_back(std::move(sub));
    staticCall.emit(Op::Call, voidType(), {Operand::subroutine(id)});
    EXPECT_EQ(errors(staticCall.d), "");
}

TEST(ClassSir, DirectAndVirtualCallsKeepTheirOwnBindingsAndSignatureChecks) {
    Fixture f;
    const auto receiver = f.emit(Op::ObjectNew, classType(ClassId{0}));
    f.emit(Op::ObjectCall, u2(64), {Operand::virtualSlot(VirtualSlotId{0}), receiver});
    f.emit(Op::ObjectCallDirect, u2(64),
           {Operand::subroutine(f.d.classes[1].virtualMethods[0]), receiver});
    ASSERT_EQ(errors(f.d), "");
    f.d.processes[0].blocks[0].instrs[1].operands[0].id = 99;
    EXPECT_FALSE(errors(f.d).empty());
    Fixture unrelated;
    unrelated.emit(
        Op::ObjectCall, u2(64),
        {Operand::virtualSlot(VirtualSlotId{0}), unrelated.constant(classType(ClassId{4}))});
    EXPECT_FALSE(errors(unrelated.d).empty());
}

TEST(ClassSir, WaitingMethodsNeedTaskFormContinuationAndValuesSavedAcrossWaits) {
    for (const bool direct : {false, true}) {
        Fixture f;
        auto method = f.instance(ClassId{0}, ClassRole::Method);
        method.kind = SubroutineKind::Task;
        method.mayWait = true;
        method.params.push_back({"argument", Direction::Input, f.scalar, kNone, FrameVarId{1}});
        method.frame.push_back({"argument", f.scalar});
        method.blocks.resize(2);
        method.blocks[0].instrs = {
            effect(Op::WaitDelay, {f.constant(u2(64)), Operand::block(BlockId{1})})};
        method.blocks[1].instrs = {effect(Op::Ret)};
        const SubroutineId id{static_cast<std::uint32_t>(f.d.subroutines.size())};
        f.d.subroutines.push_back(std::move(method));
        f.d.virtualSlots.push_back({"wait",
                                    ClassId{2},
                                    SubroutineKind::Task,
                                    voidType(),
                                    {{Direction::Input, f.scalar}},
                                    true});
        for (auto& type : f.d.classes)
            type.virtualMethods.push_back(kNone);
        f.d.classes[0].virtualMethods[1] = id;
        const auto receiver = f.emit(Op::ObjectNew, classType(ClassId{0}));
        f.emit(direct ? Op::ObjectCallTaskDirect : Op::ObjectCallTask, voidType(),
               {direct ? Operand::subroutine(id) : Operand::virtualSlot(VirtualSlotId{1}), receiver,
                f.constant(f.scalar), Operand::block(BlockId{1})});
        f.d.processes[0].blocks.resize(2);
        f.d.processes[0].blocks[1].instrs = {effect(Op::Ret)};
        ASSERT_EQ(errors(f.d), "");
        f.emit(Op::ObjectId, u2(64), {receiver}, 1);
        EXPECT_FALSE(errors(f.d).empty());
    }
}

TEST(ClassSir, WeakOperationsRetainTheirDeclaredTargetViewAndRejectOrdinaryClasses) {
    Fixture f;
    const auto target = f.emit(Op::ObjectNew, classType(ClassId{0}));
    const auto weak = f.emit(Op::ObjectWeakNew, classType(ClassId{3}), {target});
    f.emit(Op::ObjectWeakLock, classType(ClassId{1}), {weak});
    f.emit(Op::ObjectWeakClear, voidType(), {weak});
    f.emit(Op::ObjectWeakNew, classType(ClassId{3}), {f.constant({TypeKind::Chandle, 0})});
    ASSERT_EQ(errors(f.d), "");
    f.emit(Op::ObjectWeakLock, classType(ClassId{1}), {target});
    EXPECT_FALSE(errors(f.d).empty());
    Fixture wrong;
    wrong.emit(Op::ObjectWeakNew, classType(ClassId{3}), {wrong.constant(classType(ClassId{4}))});
    EXPECT_FALSE(errors(wrong.d).empty());
}

TEST(ClassSir, ClassReferencesProveScalarArrayAndElementOriginsIndependentlyOfTheirBits) {
    for (const auto direction : {Direction::Ref, Direction::ConstRef}) {
        for (const bool element : {false, true}) {
            Fixture f;
            f.d.slots[0].dims = {{2, 0}};
            const auto callee = f.refFunction(direction, element ? std::vector<ArrayDim>{}
                                                                 : std::vector<ArrayDim>{{0, 2}});
            std::vector<Operand> args{Operand::slot(SlotId{0})};
            if (element) args.push_back(f.constant(s4(32), 1));
            const auto reference = f.emit(Op::RefMake, classRefType(ClassId{1}), std::move(args));
            f.emit(Op::Call, voidType(), {Operand::subroutine(callee), reference});
            ASSERT_EQ(errors(f.d), "");
            f.d.subroutines[callee.index()].frame[0].dims =
                element ? std::vector<ArrayDim>{{2, 0}} : std::vector<ArrayDim>{};
            EXPECT_FALSE(errors(f.d).empty());
        }
    }
    Fixture forged;
    const auto integerRef = forged.emit(Op::RefMake, refType(), {Operand::slot(SlotId{1})});
    forged.emit(Op::LoadRef, classType(ClassId{1}), {integerRef});
    EXPECT_FALSE(errors(forged.d).empty());
}

TEST(ClassSir, ReadOnlyClassReferencesCannotBeDowngradedStoredOrPassedAsMutableRefs) {
    for (const bool readonly : {false, true}) {
        Fixture f;
        const auto fn = f.refFunction(Direction::ConstRef);
        auto& body = f.d.subroutines[fn.index()];
        body.numTemps = 1;
        body.blocks[0].instrs = {instruction(Op::RefMake, classRefType(ClassId{1}, readonly),
                                             TempId{0}, {Operand::frame(FrameVarId{0})}),
                                 effect(Op::Ret)};
        EXPECT_EQ(errors(f.d).empty(), readonly) << errors(f.d);
    }
    for (const auto operation : {Op::StoreRef, Op::NbaRef, Op::NbaRefDelay}) {
        Fixture f;
        const auto fn = f.refFunction(Direction::ConstRef);
        auto& sub = f.d.subroutines[fn.index()];
        sub.numTemps = 1;
        const auto value = f.constant(classType(ClassId{1}));
        const auto source = Operand::temp(TempId{0});
        sub.blocks[0].instrs = {
            instruction(Op::Fload, classRefType(ClassId{1}, true), TempId{0},
                        {Operand::frame(FrameVarId{0})}),
            instruction(Op::LoadRef, classType(ClassId{1}), TempId{1}, {source}), effect(Op::Ret)};
        sub.numTemps = 2;
        ASSERT_EQ(errors(f.d), "");
        std::vector<Operand> args{source, value};
        if (operation == Op::NbaRefDelay) args.push_back(f.constant(u2(64)));
        sub.blocks[0].instrs.insert(sub.blocks[0].instrs.end() - 1,
                                    effect(operation, std::move(args)));
        EXPECT_FALSE(errors(f.d).empty());
    }
    EXPECT_EQ(toString(classRefType(ClassId{1}, true)), "ref.readonly<class<!c1>>");
}

TEST(ClassSir, ClassReferenceShapesFollowAllIncomingBlockArguments) {
    for (const bool sameShape : {false, true}) {
        Fixture f;
        f.d.slots[0].dims = {{2, 0}};
        f.d.slots.push_back({"top.scalar", classType(ClassId{1})});
        const auto helper = f.refFunction(Direction::Ref, {{0, 2}});
        const auto condition = f.constant(u2(1), 1);
        auto& p = f.d.processes[0];
        p.numTemps = 3;
        p.blocks.resize(4);
        const auto reference = classRefType(ClassId{1});
        p.blocks[0].instrs = {
            instruction(Op::RefMake, reference, TempId{0}, {Operand::slot(SlotId{0})}),
            instruction(Op::RefMake, reference, TempId{1},
                        {Operand::slot(sameShape ? SlotId{0} : SlotId{2})}),
            effect(Op::BrIf, {condition, Operand::block(BlockId{1}), Operand::block(BlockId{2})})};
        auto target = Operand::block(BlockId{3});
        target.count = 1;
        p.blocks[1].instrs = {effect(Op::Br, {target, Operand::temp(TempId{0})})};
        p.blocks[2].instrs = {effect(Op::Br, {target, Operand::temp(TempId{1})})};
        p.blocks[3].params = {{TempId{2}, reference}};
        p.blocks[3].instrs = {
            effect(Op::Call, {Operand::subroutine(helper), Operand::temp(TempId{2})}),
            effect(Op::Ret)};
        EXPECT_EQ(errors(f.d).empty(), sameShape) << errors(f.d);
    }
}

TEST(ClassSir, ReadOnlyReferenceArraysRejectElementWritesAndMutableParameterBinding) {
    Fixture f;
    const auto fn = f.refFunction(Direction::ConstRef, {{2, 0}});
    const auto value = f.constant(classType(ClassId{1})), index = f.constant(s4(32));
    auto& sub = f.d.subroutines[fn.index()];
    sub.numTemps = 1;
    sub.blocks[0].instrs = {instruction(Op::LoadRefElem, classType(ClassId{1}), TempId{0},
                                        {Operand::frame(FrameVarId{0}), index}),
                            effect(Op::Ret)};
    ASSERT_EQ(errors(f.d), "");
    sub.blocks[0].instrs.insert(
        sub.blocks[0].instrs.end() - 1,
        effect(Op::StoreRefElem, {Operand::frame(FrameVarId{0}), index, value}));
    EXPECT_FALSE(errors(f.d).empty());
    Fixture mutableBinding;
    const auto readonly = mutableBinding.refFunction(Direction::ConstRef);
    const auto mutableFn = mutableBinding.refFunction(Direction::Ref);
    auto& caller = mutableBinding.d.subroutines[readonly.index()];
    caller.numTemps = 1;
    caller.blocks[0].instrs = {
        instruction(Op::Fload, classRefType(ClassId{1}, true), TempId{0},
                    {Operand::frame(FrameVarId{0})}),
        effect(Op::Call, {Operand::subroutine(mutableFn), Operand::temp(TempId{0})}),
        effect(Op::Ret)};
    EXPECT_FALSE(errors(mutableBinding.d).empty());
}

TEST(ClassSir, ImplicitReceiverCannotBeReboundByStoresReferencesOrArrayCopyback) {
    for (const bool readonly : {false, true}) {
        Fixture f;
        f.ctor().numTemps = 1;
        f.ctor().blocks[0].instrs.insert(f.ctor().blocks[0].instrs.begin(),
                                         instruction(Op::RefMake,
                                                     classRefType(ClassId{1}, readonly), TempId{0},
                                                     {Operand::frame(FrameVarId{0})}));
        EXPECT_EQ(errors(f.d).empty(), readonly) << errors(f.d);
    }
    Fixture store;
    const auto null = store.constant(classType(ClassId{1}));
    store.ctor().blocks[0].instrs.insert(store.ctor().blocks[0].instrs.begin(),
                                         effect(Op::Fstore, {Operand::frame(FrameVarId{0}), null}));
    EXPECT_FALSE(errors(store.d).empty());
    Fixture output;
    Subroutine helper;
    helper.name = "top.output";
    helper.automatic = true;
    helper.loc = {0, 1, 1};
    helper.params = {{"array", Direction::Output, classType(ClassId{1}), kNone, FrameVarId{0}}};
    FrameVar array;
    array.type = classType(ClassId{1});
    array.dims = {{0, 0}};
    helper.frame = {array};
    helper.blocks.resize(1);
    helper.blocks[0].instrs = {effect(Op::Ret)};
    const SubroutineId id{static_cast<std::uint32_t>(output.d.subroutines.size())};
    output.d.subroutines.push_back(std::move(helper));
    output.ctor().blocks[0].instrs.insert(
        output.ctor().blocks[0].instrs.begin(),
        effect(Op::Call, {Operand::subroutine(id), Operand::frame(FrameVarId{0})}));
    EXPECT_FALSE(errors(output.d).empty());
}

TEST(ClassSir, OrdinaryMethodsAndProcessBodiesCannotCreateConstructorPrivileges) {
    for (const auto op :
         {Op::ObjectStageBegin, Op::ObjectStageBody, Op::ObjectStageEnd, Op::ObjectBaseInit}) {
        Fixture process;
        process.emit(op);
        EXPECT_FALSE(errors(process.d).empty());
        Fixture method;
        auto& body =
            method.d.subroutines[method.d.classes[0].virtualMethods[0].index()].blocks[0].instrs;
        body.insert(body.begin(), effect(op));
        EXPECT_FALSE(errors(method.d).empty());
    }
    Fixture f;
    f.emit(Op::ObjectInit, voidType(),
           {Operand::classField({ClassId{1}, 0}), f.constant(f.scalar)});
    EXPECT_FALSE(errors(f.d).empty());
}

TEST(ClassSir, ValueAndMetadataInstructionKindsCannotManufactureClassHandles) {
    for (const auto op : {Op::Syscall, Op::Builtin}) {
        Fixture f;
        f.emit(op, classType(ClassId{1}), {Operand::str(op == Op::Syscall ? "$time" : "class")});
        EXPECT_FALSE(errors(f.d).empty());
    }
    Fixture metadata;
    metadata.emit(Op::Syscall, voidType(),
                  {Operand::str("$display"), Operand::classRow(ClassId{1})});
    EXPECT_FALSE(errors(metadata.d).empty());
    Fixture flags;
    const auto value = flags.constant(classType(ClassId{1}));
    flags.emit(Op::ObjectId, u2(64), {value});
    flags.d.processes[0].blocks[0].instrs[0].operands[0].member = 1;
    EXPECT_FALSE(errors(flags.d).empty());
    Fixture unknown;
    unknown.emit(static_cast<Op>(255), u2(1));
    EXPECT_FALSE(errors(unknown.d).empty());
}

TEST(ClassSir, InterfaceDiamondsAndDeepReverseIdInheritanceRemainIterative) {
    Fixture diamond;
    diamond.d.classes.resize(8);
    for (unsigned i = 5; i < 8; ++i) {
        diamond.d.classes[i].name = "Interface" + std::to_string(i);
        diamond.d.classes[i].interfaceClass = true;
        diamond.d.classes[i].virtualMethods = {SubroutineId{kNone}};
    }
    diamond.d.classes[2].interfaces = {ClassId{5}, ClassId{6}};
    diamond.d.classes[5].interfaces = {ClassId{7}};
    diamond.d.classes[6].interfaces = {ClassId{7}};
    ASSERT_EQ(errors(diamond.d), "");
    EXPECT_TRUE(validateClasses(diamond.d).assignable(ClassId{0}, ClassId{7}));
    Design d;
    d.sources = {"chain.sv"};
    d.instances = {{"top", "top", kNone}};
    d.classes.resize(512);
    for (std::uint32_t i = 0; i < d.classes.size(); ++i) {
        auto& type = d.classes[i];
        type.name = "Class" + std::to_string(i);
        if (i + 1 < d.classes.size()) type.base = ClassId{i + 1};
        type.constructor = SubroutineId{i};
        Subroutine sub;
        sub.name = "ctor" + std::to_string(i);
        sub.automatic = true;
        sub.loc = {0, 1, 1};
        sub.classRole = ClassRole::Constructor;
        sub.classOwner = ClassId{i};
        sub.params = {{"this", Direction::Input, classType(ClassId{i}), kNone, FrameVarId{0}}};
        sub.frame = {{"this", classType(ClassId{i})}};
        sub.blocks.resize(1);
        auto& code = sub.blocks[0].instrs;
        if (type.base != kNone) code.push_back(effect(Op::ObjectBaseInit));
        code.push_back(effect(Op::ObjectStageBegin));
        code.push_back(effect(Op::ObjectStageBody));
        code.push_back(effect(Op::ObjectStageEnd));
        code.push_back(effect(Op::Ret));
        d.subroutines.push_back(std::move(sub));
    }
    EXPECT_EQ(errors(d), "");
    EXPECT_TRUE(validateClasses(d).assignable(ClassId{0}, ClassId{511}));
}

TEST(ClassSir, TextDumpExposesStaticViewsFieldOwnersStagesAndVirtualBindings) {
    Fixture f;
    f.initialize(1, 0, true);
    const auto receiver = f.emit(Op::ObjectNew, classType(ClassId{0}));
    f.emit(Op::ObjectCall, u2(64), {Operand::virtualSlot(VirtualSlotId{0}), receiver});
    f.emit(Op::ObjectRead, f.scalar, {Operand::classField({ClassId{1}, 0}), receiver});
    ASSERT_EQ(errors(f.d), "");
    const auto text = toText(f.d);
    for (const std::string part :
         {"class !c0 \"Derived\" scope=0 base=!c1", "implements !c2",
          "field 0 \"value\" : s4<32> const initializer", "virtual !v0 -> @",
          "virtual !v0 \"value\" owner=!c1", "class=!c1 constructor", "object.stage.begin",
          "object.init !c1.field0", "object.read s4<32> !c1.field0", "object.call i2<64> !v0"})
        EXPECT_NE(text.find(part), std::string::npos) << part << '\n' << text;
}

TEST(ClassSir, ClassIngressDetectionFindsTypeAndOperandMetadataWithoutDescriptors) {
    Fixture f;
    f.d.classes.clear();
    f.d.virtualSlots.clear();
    EXPECT_TRUE(hasClassMetadata(f.d));
    EXPECT_FALSE(errors(f.d).empty());
    Design empty;
    EXPECT_FALSE(hasClassMetadata(empty));
    empty.sources = {"bad.sv"};
    empty.instances = {{"top", "top", kNone}};
    Process p;
    p.name = "top.initial";
    p.loc = {0, 1, 1};
    p.numTemps = 1;
    p.blocks.resize(1);
    p.blocks[0].instrs = {instruction(Op::ObjectNew, classType(ClassId{0}), TempId{0}),
                          effect(Op::Ret)};
    empty.processes.push_back(std::move(p));
    EXPECT_TRUE(hasClassMetadata(empty));
    EXPECT_FALSE(errors(empty).empty());
}

TEST(ClassSir, GuardFrameAliasesPreserveImplicitReceiverRights) {
    Fixture f;
    const auto owner = f.d.classes[1].virtualMethods[0];
    Subroutine guard;
    guard.name = "top.guard";
    guard.kind = SubroutineKind::Guard;
    guard.returnType = u2(1);
    guard.frameOf = Owner::of(owner);
    guard.loc = {0, 1, 1};
    guard.numTemps = 2;
    guard.blocks.resize(1);
    guard.blocks[0].instrs = {
        instruction(Op::Fload, classType(ClassId{1}), TempId{0}, {Operand::frame(FrameVarId{0})}),
        instruction(Op::Truth, u2(1), TempId{1}, {Operand::temp(TempId{0})}),
        effect(Op::Ret, {Operand::temp(TempId{1})})};
    const SubroutineId id{static_cast<std::uint32_t>(f.d.subroutines.size())};
    f.d.subroutines.push_back(std::move(guard));
    ASSERT_EQ(errors(f.d), "");
    const auto checked = validateClasses(f.d);
    ASSERT_EQ(checked.receivers(Owner::of(id)).size(), 1U);
    EXPECT_EQ(checked.receivers(Owner::of(id))[0], FrameVarId{0});
    const auto null = f.constant(classType(ClassId{1}));
    auto& code = f.d.subroutines[id.index()].blocks[0].instrs;
    code.insert(code.begin(), effect(Op::Fstore, {Operand::frame(FrameVarId{0}), null}));
    EXPECT_FALSE(errors(f.d).empty());
}

TEST(ClassSir, ForkCapturesRetainReceiverRightsThroughTheLexicalOwnershipGraph) {
    Fixture f;
    const auto owner = f.d.classes[1].virtualMethods[0];
    auto& method = f.d.subroutines[owner.index()];
    method.frame[0].home = FrameHome::Scoped;
    method.frame[0].activationScope = ActivationScopeId{0};
    method.activation.root = ActivationScopeId{0};
    method.activation.scopes = {{kNone, {FrameVarId{0}}}};
    method.blocks.resize(2);
    method.blocks[0].instrs = {
        effect(Op::Fork, {Operand::process(ProcessId{1}), Operand::str("join_none"),
                          Operand::block(BlockId{1})})};
    method.blocks[1].instrs = {effect(Op::Ret, {f.constant(u2(64), 7)})};
    Process child;
    child.name = "top.child";
    child.kind = ProcessKind::General;
    child.loc = {0, 1, 1};
    child.parent = Owner::of(owner);
    child.frame = {method.frame[0]};
    child.frame[0].home = FrameHome::Capture;
    child.frame[0].activationScope = kNone;
    child.frame[0].captureSource = FrameVarId{0};
    child.frame[0].readonly = true;
    child.activation.extents = {ActivationScopeId{0}};
    child.blocks.resize(1);
    child.numTemps = 1;
    child.blocks[0].instrs = {
        instruction(Op::Fload, classType(ClassId{1}), TempId{0}, {Operand::frame(FrameVarId{0})}),
        effect(Op::Ret)};
    f.d.processes.push_back(std::move(child));
    ASSERT_EQ(errors(f.d), "");
    const auto checked = validateClasses(f.d);
    ASSERT_EQ(checked.receivers(Owner::of(ProcessId{1})).size(), 1U);
    const auto null = f.constant(classType(ClassId{1}));
    auto& code = f.d.processes[1].blocks[0].instrs;
    code.insert(code.begin(), effect(Op::Fstore, {Operand::frame(FrameVarId{0}), null}));
    EXPECT_FALSE(errors(f.d).empty());
}

}  // namespace
