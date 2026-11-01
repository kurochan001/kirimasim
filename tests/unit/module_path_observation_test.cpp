// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Source module-path observations share the delayed physical public net.
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/front/front.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace k = kirimasim::kernel;
struct Temporary {
    std::filesystem::path directory;
    Temporary() {
        auto path =
            (std::filesystem::temp_directory_path() / "kirimasim-path-observe-XXXXXX").string();
        if (mkdtemp(path.data())) directory = path;
    }
    ~Temporary() {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
};
struct Capture {
    std::FILE* file = std::tmpfile();
    ~Capture() {
        if (file) std::fclose(file);
    }
    std::string text() {
        std::fflush(file);
        EXPECT_EQ(std::fseek(file, 0, SEEK_SET), 0);
        std::string result;
        std::array<char, 512> buffer{};
        while (const auto n = std::fread(buffer.data(), 1, buffer.size(), file))
            result.append(buffer.data(), n);
        return result;
    }
};
struct Observations {
    std::vector<std::pair<std::uint64_t, unsigned>> changes;
    unsigned registrations = 0;
};
PLI_INT32 changed(p_cb_data cb) {
    auto& observations = *reinterpret_cast<Observations*>(cb->user_data);
    s_vpi_value value{};
    value.format = vpiVectorVal;
    vpi_get_value(cb->obj, &value);
    if (!value.value.vector) return 0;
    s_vpi_time time{};
    time.type = vpiSimTime;
    vpi_get_time(cb->obj, &time);
    const auto logic =
        (value.value.vector[0].aval & 1U) | ((value.value.vector[0].bval & 1U) << 1U);
    observations.changes.emplace_back((std::uint64_t{time.high} << 32U) | time.low, logic);
    return 0;
}
PLI_INT32 watch(PLI_BYTE8* data) {
    auto& observations = *reinterpret_cast<Observations*>(data);
    const auto call = vpi_handle(vpiSysTfCall, nullptr);
    const auto iterator = vpi_iterate(vpiArgument, call);
    if (!iterator) return 0;
    const auto argument = vpi_scan(iterator);
    s_cb_data cb{};
    cb.reason = cbValueChange;
    cb.cb_rtn = &changed;
    cb.obj = argument;
    cb.user_data = data;
    if (vpi_register_cb(&cb)) ++observations.registrations;
    vpi_free_object(iterator);
    return 0;
}
TEST(ModulePathObservation, VpiBothPortNamesFormattingAndWaveformSeeCommittedPathSnapshot) {
    Temporary tmp;
    ASSERT_FALSE(tmp.directory.empty());
    const auto source = tmp.directory / "top.sv";
    const auto wave = tmp.directory / "path.vcd";
    std::ofstream(source) << R"(`timescale 1ns/1ns
module path_cell(input wire a,output wire q);
assign(weak1,weak0)q=a;
specify(a=>q)=3;endspecify
endmodule
module top;logic a=0;wire q;path_cell u(a,q);
initial begin #10;$watch_path(q);$watch_path(u.q);#11;a=1;
#1;$display("early %b %v %b %v",q,q,u.q,u.q);
#3;$display("late %b %v %b %v",q,q,u.q,u.q);end
endmodule
)";
    Capture output, errors;
    ASSERT_NE(output.file, nullptr);
    ASSERT_NE(errors.file, nullptr);
    k::Context ctx({output.file, errors.file});
    Observations observations;
    {
        k::Vpi::Entry entry(ctx);
        s_vpi_systf_data registration{};
        registration.type = vpiSysTask;
        registration.tfname = const_cast<PLI_BYTE8*>("$watch_path");
        registration.calltf = &watch;
        registration.user_data = reinterpret_cast<PLI_BYTE8*>(&observations);
        ASSERT_NE(vpi_register_systf(&registration), nullptr);
    }
    const std::array<kirimasim::front::ForeignRoutine, 1> routines{
        {{"$watch_path", kirimasim::sir::voidType()}}};
    auto compiled =
        kirimasim::front::compile(std::vector<std::string>{source.string()},
                                  {.foreignRoutines = routines, .foreignEnabled = true});
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_EQ(compiled.design.modulePaths.size(), 1U);
    EXPECT_TRUE(kirimasim::sir::verify(compiled.design).empty());
    // A VPI net binding cannot escape into a language ref write, or be
    // relabelled as a writable foreign variable in hand-built IR.
    for (const bool escape : {false, true}) {
        auto invalid = compiled.design;
        bool changedIr = false;
        for (auto& process : invalid.processes)
            for (auto& block : process.blocks) {
                if (changedIr) continue;
                for (std::size_t i = 0; i < block.instrs.size(); ++i) {
                    const auto reference = block.instrs[i];
                    if (reference.op != kirimasim::sir::Op::RefMake) continue;
                    const auto ref = kirimasim::sir::Operand::temp(reference.result);
                    if (escape) {
                        const auto value =
                            std::ranges::find_if(invalid.constants, [](const auto& c) {
                                return c.type == kirimasim::sir::u4(1);
                            });
                        ASSERT_NE(value, invalid.constants.end());
                        const auto id = kirimasim::sir::ConstId{
                            static_cast<unsigned>(value - invalid.constants.begin())};
                        block.instrs.insert(block.instrs.begin() +
                                                static_cast<std::ptrdiff_t>(i + 1),
                                            {kirimasim::sir::Op::StoreRef,
                                             kirimasim::sir::voidType(),
                                             kirimasim::sir::kNone,
                                             {ref, kirimasim::sir::Operand::constant(id)},
                                             reference.loc});
                    } else {
                        for (auto& consumer : block.instrs)
                            for (std::size_t n = 8; n + 3 < consumer.operands.size(); n += 5)
                                if (consumer.operands[n].kind == ref.kind &&
                                    consumer.operands[n].id == ref.id)
                                    consumer.operands[n + 3] =
                                        kirimasim::sir::Operand::str("variable");
                    }
                    changedIr = true;
                    break;
                }
            }
        ASSERT_TRUE(changedIr);
        const auto violations = kirimasim::sir::verify(invalid);
        EXPECT_TRUE(std::ranges::any_of(violations, [](const auto& e) {
            return e.message.find("strength net operand must be read-only") != std::string::npos;
        }));
    }
    const auto result = kirimasim::vm::run(compiled.design, ctx, {.waveFile = wave.string()});
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error << errors.text();
    EXPECT_EQ(observations.registrations, 2U);
    EXPECT_EQ(observations.changes,
              (std::vector<std::pair<std::uint64_t, unsigned>>{{24, 1}, {24, 1}}));
    EXPECT_EQ(output.text(), "early 0 We0 0 We0\nlate 1 We1 1 We1\n");
    EXPECT_EQ(ctx.strengthDrivers().originCount(), 1U);
    std::ifstream input(wave);
    ASSERT_TRUE(input.good());
    std::string line, publicCode;
    unsigned qNames = 0;
    std::uint64_t time = 0;
    std::vector<std::pair<std::uint64_t, char>> commits;
    while (std::getline(input, line)) {
        if (line.starts_with("$var ")) {
            std::istringstream fields(line);
            std::string token, kind, width, code, name;
            fields >> token >> kind >> width >> code >> name;
            if (name == "q") {
                ++qNames;
                if (publicCode.empty()) publicCode = code;
                EXPECT_EQ(code, publicCode);  // aliases retain one physical public slot
            }
            EXPECT_EQ(name.find("$driver"), std::string::npos);
        } else if (line.starts_with('#')) {
            time = std::stoull(line.substr(1));
        } else if (!publicCode.empty() && line.size() == 1 + publicCode.size() &&
                   line.substr(1) == publicCode) {
            commits.emplace_back(time, line[0]);
        }
    }
    EXPECT_EQ(qNames, 2U);
    EXPECT_EQ(commits,
              (std::vector<std::pair<std::uint64_t, char>>{{0, 'x'}, {3, '0'}, {24, '1'}}));
}
}  // namespace
