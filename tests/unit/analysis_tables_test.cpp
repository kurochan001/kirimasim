// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>

#include "kirimasim/analysis/islands.h"
#include "kirimasim/analysis/tables.h"
#include "kirimasim/analysis/values.h"
#include "kirimasim/sir/text_v1.h"

namespace {
namespace a = kirimasim::analysis;
namespace s = kirimasim::sir;
std::string digestHex(std::string_view bytes) {
    static constexpr std::string_view hex = "0123456789abcdef";
    std::string result;
    for (auto byte : a::contentDigest(bytes)) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return result;
}

s::VerifyOutcome design() {
    s::Design d;
    d.sources = {"design.sv"};
    d.instances = {{"top", "top", s::kNone}};
    s::Slot slot;
    slot.name = "top.q";
    slot.type = s::u2(8);
    slot.instance = s::InstanceId{0};
    d.slots = {slot};
    s::Process p;
    p.name = "top.comb0";
    p.instance = s::InstanceId{0};
    p.kind = s::ProcessKind::Comb;
    p.loc = {0, 1, 1};
    p.numTemps = 2;
    p.blocks = {{{},
                 {{s::Op::Load, s::u2(8), s::TempId{0}, {s::Operand::slot(s::SlotId{0})}, p.loc},
                  {s::Op::Not, s::u2(8), s::TempId{1}, {s::Operand::temp(s::TempId{0})}, p.loc},
                  {s::Op::Ret, s::voidType(), s::kNone, {}, p.loc}}}};
    d.processes = {p, p, p};
    d.processes[1].name = "top.comb1";
    d.processes[2].name = "top.initial2";
    d.processes[2].kind = s::ProcessKind::Initial;
    return s::verifyDesign(std::move(d));
}

TEST(AnalysisTables, MissingStaleAndMalformedFactsYieldNoOptimization) {
    EXPECT_EQ(digestHex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(digestHex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(digestHex(std::string(55, 'a')),
              "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    EXPECT_EQ(digestHex(std::string(56, 'a')),
              "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    EXPECT_EQ(digestHex(std::string(64, 'a')),
              "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
    EXPECT_EQ(digestHex(std::string(1000, '\0')),
              "541b3e9daa09b20bf85fa273e5cbd3e80185aa4ec298e765db87742b70138a53");
    auto source = design();
    if (!source.verified) FAIL() << "fixture design failed verification";
    const auto& verified = *source.verified;
    const auto derived = a::makeTableKey(verified, "test-options", "layout-v1", "fixture-1");
    if (!derived) FAIL() << "missing derived";
    const auto& key = *derived;
    const auto text = s::print1(verified);
    if (!text) FAIL() << "missing text";
    EXPECT_EQ(key.design, a::contentDigest(*text));
    EXPECT_EQ(key.options, a::contentDigest("test-options"));
    a::TableData empty{key, {}, {}, {}};
    const a::ValueSite slot{};
    auto unknown = a::BoundTables::bind(verified, key, empty);
    if (!unknown) FAIL() << "missing unknown";
    EXPECT_EQ(unknown->value(slot), nullptr);
    EXPECT_EQ(unknown->member(s::ProcessId{0}), nullptr);
    const a::ValueSite temp{a::SiteKind::Result, s::Owner::Kind::Process, 0, 0, 0, 0};
    const a::ValueSite use{a::SiteKind::Operand, s::Owner::Kind::Process, 0, 0, 1, 0};
    a::TableData data{key,
                      {{slot,
                        8,
                        a::NoXProof::DeclaredTwoState,
                        {a::PreserveReason::Waveform, a::PreserveReason::Force}},
                       {temp, 8, a::NoXProof::Dataflow, {}},
                       {use, 8, a::NoXProof::DeclaredTwoState, {}}},
                      {{s::ProcessId{0}, 0, 0, {}}, {s::ProcessId{1}, 0, 1, {s::ProcessId{0}}}},
                      {{s::ProcessId{2}, {a::ExclusionReason::Kind, a::ExclusionReason::Delay}}}};
    auto bound = a::BoundTables::bind(verified, key, data);
    if (!bound) FAIL() << "missing bound";
    ASSERT_NE(bound->value(slot), nullptr);
    EXPECT_EQ(bound->value(slot)->preserve, data.values.front().preserve);
    ASSERT_NE(bound->value(use), nullptr);
    ASSERT_NE(bound->member(s::ProcessId{1}), nullptr);
    EXPECT_EQ(bound->member(s::ProcessId{1})->level, 1u);
    ASSERT_NE(bound->exclusion(s::ProcessId{2}), nullptr);
    EXPECT_EQ(bound->exclusion(s::ProcessId{2})->reasons, data.excluded.front().reasons);
    ASSERT_NE(bound->value(temp), nullptr);
    EXPECT_EQ(bound->value(temp)->noX, a::NoXProof::Dataflow);
    for (unsigned change = 0; change < 6; ++change) {
        auto stale = key;
        switch (change) {
            case 0: ++stale.sirVersion; break;
            case 1: ++stale.schemaVersion; break;
            case 2: ++stale.design[0]; break;
            case 3: ++stale.options[0]; break;
            case 4: stale.layout += "-other"; break;
            case 5: stale.producer += "-other"; break;
            default: FAIL() << "unexpected identity field";
        }
        EXPECT_FALSE(a::BoundTables::bind(verified, stale, data));
    }
    auto wrongDesign = key;
    ++wrongDesign.design.front();
    empty.key = wrongDesign;
    EXPECT_FALSE(a::BoundTables::bind(verified, wrongDesign, empty));
    auto invalid = data;
    invalid.values.back().width = 32;
    EXPECT_FALSE(a::BoundTables::bind(verified, key, invalid));
    invalid = data;
    invalid.values.push_back(data.values.front());
    EXPECT_FALSE(a::BoundTables::bind(verified, key, invalid));
    invalid = data;
    invalid.values.back().site.block = 4;
    EXPECT_FALSE(a::BoundTables::bind(verified, key, invalid));
    invalid = data;
    invalid.members.front().dependencies = {s::ProcessId{1}};
    EXPECT_FALSE(a::BoundTables::bind(verified, key, invalid));
    invalid = data;
    invalid.members.back().dependencies = {s::ProcessId{2}};
    EXPECT_FALSE(a::BoundTables::bind(verified, key, invalid));
    invalid = data;
    invalid.excluded.front().process = s::ProcessId{0};
    EXPECT_FALSE(a::BoundTables::bind(verified, key, invalid));
    invalid = data;
    invalid.excluded.front().reasons.clear();
    EXPECT_FALSE(a::BoundTables::bind(verified, key, invalid));
}
TEST(AnalysisTables, KnownBitsFollowVerifiedOperandsAndPreserveFourStateStorage) {
    auto initial = design();
    if (!initial.verified) FAIL() << "missing initial.verified";
    auto d = std::move(*initial.verified).release();
    auto& body = d.processes.front();
    d.slots[0].type = s::u4(8);
    d.slots[0].flags |= s::kObservable;
    for (auto& p : d.processes) {
        p.blocks[0].instrs[0].type = s::u4(8);
        p.blocks[0].instrs[1].type = s::u4(8);
    }
    body.numTemps = 5;
    auto& instructions = body.blocks.front().instrs;
    const auto loc = body.loc;
    const auto input = s::Operand::temp(s::TempId{0});
    instructions.resize(1);
    instructions.push_back({s::Op::Ceq, s::u2(1), s::TempId{1}, {input, input}, loc});
    instructions.push_back(
        {s::Op::To4, s::u4(1), s::TempId{4}, {s::Operand::temp(s::TempId{1})}, loc});
    instructions.push_back(
        {s::Op::Not, s::u4(1), s::TempId{2}, {s::Operand::temp(s::TempId{4})}, loc});
    instructions.push_back({s::Op::Div, s::u4(8), s::TempId{3}, {input, input}, loc});
    instructions.push_back({s::Op::Ret, s::voidType(), s::kNone, {}, loc});
    auto verified = s::verifyDesign(std::move(d));
    if (!verified.verified) FAIL() << "missing verified.verified";
    auto table = a::analyzeIslands(*verified.verified);
    if (!table) FAIL() << "missing table";
    table->values = a::analyzeValues(*verified.verified, false);
    const auto key = table->key;
    const auto bound = a::BoundTables::bind(*verified.verified, key, std::move(*table));
    if (!bound) FAIL() << "missing bound";
    const auto* slot = bound->value({a::SiteKind::Slot, s::Owner::Kind::None, 0, 0, 0, 0});
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->noX, a::NoXProof::Unknown);
    EXPECT_EQ(slot->preserve, (std::vector{a::PreserveReason::Explicit}));
    const auto result = [&](std::uint32_t i) {
        return bound->value({a::SiteKind::Result, s::Owner::Kind::Process, 0, 0, i, 0});
    };
    EXPECT_EQ(result(0), nullptr);
    ASSERT_NE(result(1), nullptr);
    ASSERT_NE(result(2), nullptr);
    EXPECT_EQ(result(2)->noX, a::NoXProof::Dataflow);
    EXPECT_EQ(result(4), nullptr);
}

}  // namespace
