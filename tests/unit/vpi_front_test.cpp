// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/front/front.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {
namespace k = kirimasim::kernel;
namespace s = kirimasim::sir;
namespace f = kirimasim::front;
struct Capture {
    std::FILE* file = std::tmpfile();
    ~Capture() {
        if (file) std::fclose(file);
    }
    std::string text() {
        std::fflush(file);
        EXPECT_EQ(std::fseek(file, 0, SEEK_SET), 0);
        std::string out;
        std::array<char, 1024> b{};
        while (const auto n = std::fread(b.data(), 1, b.size(), file))
            out.append(b.data(), n);
        return out;
    }
};
struct Source {
    std::filesystem::path directory;
    std::string file;
    explicit Source(std::string_view text) {
        auto path = (std::filesystem::temp_directory_path() / "kirimasim-vpi-unit-XXXXXX").string();
        if (!mkdtemp(path.data())) return;
        directory = path;
        file = (directory / "top.sv").string();
        std::ofstream(file) << text;
    }
    ~Source() {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
};
struct Calls {
    unsigned compile = 0, run = 0, size = 0;
    bool write = false;
    std::vector<unsigned> widths;
};
PLI_INT32 compileCall(PLI_BYTE8* data) {
    auto& calls = *reinterpret_cast<Calls*>(data);
    ++calls.compile;
    const auto site = vpi_handle(vpiSysTfCall, nullptr);
    const auto iterator = vpi_iterate(vpiArgument, site);
    if (iterator) {
        const auto arg = vpi_scan(iterator);
        calls.widths.push_back(static_cast<unsigned>(vpi_get(vpiSize, arg)));
        vpi_free_object(iterator);
    }
    return 0;
}
PLI_INT32 sized(PLI_BYTE8* data) {
    ++reinterpret_cast<Calls*>(data)->size;
    return 129;
}
PLI_INT32 call(PLI_BYTE8* data) {
    auto& calls = *reinterpret_cast<Calls*>(data);
    ++calls.run;
    const auto site = vpi_handle(vpiSysTfCall, nullptr);
    s_vpi_value value{};
    value.format = vpiIntVal;
    value.value.integer = 7;
    if (calls.write) {
        const auto iterator = vpi_iterate(vpiArgument, site);
        const auto arg = vpi_scan(iterator);
        vpi_put_value(arg, &value, nullptr, vpiNoDelay);
        vpi_free_object(iterator);
    } else
        vpi_put_value(site, &value, nullptr, vpiNoDelay);
    return 0;
}
std::vector<f::ForeignRoutine> registration(k::Context& context, Calls& calls,
                                            int kind = vpiIntFunc) {
    k::Vpi::Entry enter(context);
    char name[] = "$test_vpi";
    s_vpi_systf_data data{};
    data.type = vpiSysFunc;
    data.sysfunctype = kind;
    data.tfname = name;
    data.calltf = &call;
    data.compiletf = &compileCall;
    data.sizetf = &sized;
    data.user_data = reinterpret_cast<char*>(&calls);
    EXPECT_NE(vpi_register_systf(&data), nullptr);
    std::vector<f::ForeignRoutine> out;
    for (const auto& routine : context.vpi().routines())
        out.push_back({routine.name, routine.result.real          ? s::Type{s::TypeKind::Real, 0}
                                     : routine.result.signedValue ? s::s4(routine.result.width)
                                                                  : s::u4(routine.result.width)});
    return out;
}
f::CompileResult compile(Source& source, std::span<const f::ForeignRoutine> declarations) {
    return f::compile(std::vector<std::string>{source.file},
                      {.foreignRoutines = declarations, .foreignEnabled = true});
}

TEST(VpiFront, CompileOnlyDoesNotRunForeignCodeOrSimulation) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    const auto routines = registration(context, calls);
    Source source("module top; initial $display(\"%0d\",$test_vpi(1)); endmodule\n");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    EXPECT_EQ(calls.compile, 0U);
    EXPECT_EQ(calls.run, 0U);
    const auto result = kirimasim::vm::run(compiled.design, context, {.vpiCompileOnly = true});
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(calls.compile, 1U);
    EXPECT_EQ(calls.run, 0U);
    EXPECT_EQ(out.text(), "");
}
TEST(VpiFront, CompiletfsArePerCallSiteNotPerExecution) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    const auto routines = registration(context, calls);
    Source source("module top; initial repeat(3) $display(\"%0d\",$test_vpi(1)); endmodule\n");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = kirimasim::vm::run(compiled.design, context);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(calls.compile, 1U);
    EXPECT_EQ(calls.run, 3U);
    EXPECT_EQ(out.text(), "7\n7\n7\n");
}
TEST(VpiFront, ArrayIndexIsCapturedOnceForValueAndWriteBinding) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    calls.write = true;
    const auto routines = registration(context, calls);
    Source source("module top; int a[2]='{0,0}; int i=0;initial begin "
                  "$test_vpi(a[i++]);$display(\"%0d %0d %0d\",a[0],a[1],i);end endmodule\n");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = kirimasim::vm::run(compiled.design, context);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(out.text(), "7 0 1\n");
}
TEST(VpiFront, SizedFunctionWidthIsCapturedAtRegistration) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    const auto routines = registration(context, calls, vpiSizedFunc);
    Source source("module top;initial repeat(2)$display(\"%0d\",$test_vpi());endmodule\n");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = kirimasim::vm::run(compiled.design, context);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(calls.size, 1U);
    EXPECT_EQ(calls.run, 2U);
    EXPECT_EQ(out.text(), "7\n7\n");
}
TEST(VpiFront, ForeignFunctionsAreNotConstantFolded) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    const auto routines = registration(context, calls);
    Source source(
        "module top;localparam int VALUE=$test_vpi();initial $display(\"%0d\",VALUE);endmodule\n");
    auto compiled = compile(source, routines);
    EXPECT_FALSE(compiled.success);
    EXPECT_EQ(calls.run, 0U);
    EXPECT_EQ(calls.compile, 0U);
    EXPECT_FALSE(compiled.diagnostics.empty());
}
TEST(VpiFront, MalformedForeignTransportIsRejectedBeforeExecution) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    const auto routines = registration(context, calls);
    Source source("module top;initial $display(\"%0d\",$test_vpi(1));endmodule\n");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    for (unsigned fault = 0; fault < 6; ++fault) {
        auto design = compiled.design;
        s::Instr* found = nullptr;
        for (auto& body : design.processes)
            for (auto& block : body.blocks)
                for (auto& in : block.instrs)
                    if (in.op == s::Op::Syscall && !in.operands.empty() &&
                        in.operands[0].text == "$vpi.call")
                        found = &in;
        ASSERT_NE(found, nullptr);
        switch (fault) {
            case 0: found->operands.pop_back(); break;
            case 1: found->operands[1] = s::Operand::str("bad"); break;
            case 2: found->operands[8] = found->operands[7]; break;
            case 3: found->operands[9] = s::Operand::str("bad"); break;
            case 4: found->operands[11] = s::Operand::str("invalid"); break;
            case 5: design.constant(found->operands[3].asConst()).aWords[0] = UINT32_MAX; break;
            default: FAIL() << fault;
        }
        EXPECT_FALSE(s::verify(design).empty()) << fault;
    }
    EXPECT_EQ(calls.run, 0U);
    EXPECT_EQ(calls.compile, 0U);
}
TEST(VpiFront, UnreferencedStaticNamesAndDeclaredBoundsAreRetained) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    const auto routines = registration(context, calls);
    Source source("module top; logic[0:64] unused; int a[-2:0]; initial $test_vpi();endmodule\n");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = kirimasim::vm::run(compiled.design, context, {.vpiCompileOnly = true});
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    k::Vpi::Entry enter(context);
    auto value = vpi_handle_by_name(const_cast<char*>("top.unused"), nullptr);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(vpi_get(vpiSize, value), 65);
    auto array = vpi_handle_by_name(const_cast<char*>("top.a"), nullptr);
    ASSERT_NE(array, nullptr);
    EXPECT_NE(vpi_handle_by_index(array, -2), nullptr);
    EXPECT_EQ(vpi_handle_by_index(array, 1), nullptr);
}

TEST(VpiFront, OutOfBoundsActualIsAnUnknownReadonlyValue) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    calls.write = true;
    const auto routines = registration(context, calls);
    Source source("module top; int a[2]='{3,4};int i=9;initial begin "
                  "$test_vpi(a[i++]);$display(\"%0d %0d %0d\",a[0],a[1],i);end endmodule\n");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = kirimasim::vm::run(compiled.design, context);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(out.text(), "3 4 10\n");
}
TEST(VpiFront, EscapedLeafNamesRetainEmbeddedDots) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    const auto routines = registration(context, calls);
    Source source("module top; int \\a.b ;initial $test_vpi();endmodule\n");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = kirimasim::vm::run(compiled.design, context, {.vpiCompileOnly = true});
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    k::Vpi::Entry enter(context);
    vpiHandle object = nullptr;
    for (const auto& row : compiled.design.foreignObjects)
        if (row.leafName == "a.b")
            object = vpi_handle_by_name(const_cast<char*>(row.name.c_str()), nullptr);
    ASSERT_NE(object, nullptr);
    EXPECT_STREQ(vpi_get_str(vpiName, object), "a.b");
}
TEST(VpiFront, NullableReferencesCannotEscapeForeignBindings) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    calls.write = true;
    const auto routines = registration(context, calls);
    Source source("module top;int a;initial $test_vpi(a);endmodule\n");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    for (auto& body : compiled.design.processes)
        for (auto& block : body.blocks) {
            for (std::size_t i = 0; i < block.instrs.size(); ++i) {
                const auto& in = block.instrs[i];
                if (in.op != s::Op::Syscall || in.operands.empty() ||
                    in.operands[0].text != "$vpi.call")
                    continue;
                s::Instr escaped;
                escaped.op = s::Op::LoadRef;
                escaped.type = s::s4(32);
                escaped.result = s::TempId{body.numTemps++};
                escaped.operands = {in.operands[8]};
                escaped.loc = in.loc;
                block.instrs.insert(block.instrs.begin() + static_cast<std::ptrdiff_t>(i),
                                    std::move(escaped));
                break;
            }
        }
    std::string messages;
    for (const auto& error : s::verify(compiled.design))
        messages += error.message + "\n";
    EXPECT_NE(messages.find("nullable foreign reference escapes"), std::string::npos) << messages;
    auto ordinary = compiled.design;
    for (auto& body : ordinary.processes)
        for (auto& block : body.blocks)
            for (auto& in : block.instrs)
                if (in.op == s::Op::RefMake && !in.operands.empty() &&
                    in.operands.back().kind == s::Operand::Kind::Text)
                    in.operands.pop_back();
    std::string ordinaryErrors;
    for (const auto& error : s::verify(ordinary))
        ordinaryErrors += error.message + "\n";
    EXPECT_TRUE(ordinaryErrors.empty()) << ordinaryErrors;
}
TEST(VpiFront, AggregateRefLeafKeepsItsOwnBinding) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    calls.write = true;
    const auto routines = registration(context, calls);
    Source source(
        "module top;typedef struct {int a;int b;} pair;pair value;task automatic run(ref pair "
        "x);$test_vpi(x.b);endtask initial begin value.a=1;value.b=2;run(value);$display(\"%0d "
        "%0d\",value.a,value.b);end endmodule\n");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto result = kirimasim::vm::run(compiled.design, context);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(out.text(), "1 7\n");
}
}  // namespace
