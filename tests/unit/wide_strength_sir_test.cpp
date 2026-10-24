// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

namespace {
namespace k = kirimasim::kernel;
namespace s = kirimasim::sir;
struct WideSir {
    s::Design d;
    WideSir() {
        d.sources = {"wide_strength.sv"};
        d.instances = {{"top", "top", s::kNone}};
        s::Process process;
        process.name = "top.test";
        process.kind = s::ProcessKind::Initial;
        process.blocks.resize(1);
        d.processes.push_back(std::move(process));
    }
    s::Operand constant(s::Type type, std::vector<std::uint64_t> a,
                        std::vector<std::uint64_t> b = {}) {
        const s::ConstId id{static_cast<std::uint32_t>(d.constants.size())};
        d.constants.push_back({type, std::move(a), std::move(b)});
        return s::Operand::constant(id);
    }
};
TEST(StrengthTransport, WideResolveRetainsBothUnalignedMaskHalvesAndUnmaskedDrivers) {
    for (const unsigned width : {1U, 32U, 33U, 63U, 64U, 65U, 129U, 257U}) {
        for (unsigned background = 0; background < 2; ++background) {
            for (const bool reversed : {false, true}) {
                WideSir program;
                program.d.slots = {{"top.result", s::u4(width)}};
                program.d.processes[0].kind = s::ProcessKind::Initial;
                program.d.processes[0].writes = {{s::SlotId{0}, 0, 0, 0, 0}};
                program.d.processes[0].numTemps = 1;
                const auto words = (width + 63) / 64;
                std::vector<std::uint64_t> a(words), b(words), mask((2 * width + 63) / 64);
                for (unsigned bit = 0; bit < width; ++bit) {
                    a[bit / 64] |= std::uint64_t{1} << (bit % 64);
                    b[bit / 64] |= std::uint64_t{1} << (bit % 64);
                    if (bit % 3 != 0) mask[bit / 64] |= std::uint64_t{1} << (bit % 64);
                    if (bit % 5 != 0)
                        mask[(width + bit) / 64] |= std::uint64_t{1} << ((width + bit) % 64);
                }
                const auto x = program.constant(s::u4(width), a, b);
                const auto side = program.constant(s::u2(2 * width), mask);
                const auto other = program.constant(
                    s::u4(width), background ? a : std::vector<std::uint64_t>(words),
                    std::vector<std::uint64_t>(words));
                const auto strong = program.constant(s::u2(3), {6});
                const auto weak = program.constant(s::u2(3), {3});
                const std::vector<s::Operand> drivers =
                    reversed
                        ? std::vector<s::Operand>{s::Operand::str("wire"),
                                                  other,
                                                  weak,
                                                  weak,
                                                  x,
                                                  strong,
                                                  strong,
                                                  side}
                        : std::vector<s::Operand>{
                              s::Operand::str("wire"), x, strong, strong, side, other, weak, weak};
                program.d.processes[0].blocks[0].instrs = {
                    {s::Op::Resolve, s::u4(width), s::TempId{0}, drivers, {0, 1, 1}},
                    {s::Op::Store,
                     s::voidType(),
                     s::kNone,
                     {s::Operand::slot(s::SlotId{0}), s::Operand::temp(s::TempId{0})},
                     {0, 1, 1}},
                    {s::Op::Ret, s::voidType(), s::kNone, {}, {0, 1, 1}}};
                ASSERT_TRUE(s::verify(program.d).empty()) << width;
                k::Context ctx;
                const auto result = kirimasim::vm::run(program.d, ctx);
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                const auto* value = ctx.store().element(k::SlotId{0}, 0);
                for (unsigned bit = 0; bit < width; ++bit) {
                    const bool unknown = background ? bit % 5 != 0 : bit % 3 != 0;
                    EXPECT_EQ(k::bitAt(value + 1, 2, bit), unknown) << width << ':' << bit;
                    EXPECT_EQ(k::bitAt(value, 2, bit), unknown || background != 0)
                        << width << ':' << bit;
                }
            }
        }
    }
}
}  // namespace
