// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/front/front.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/strength.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"
#include "verified_run.h"
#include <algorithm>
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
    const auto result =
        kirimasim::test::runVerified(compiled.design(), context, {.vpiCompileOnly = true});
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
    const auto result = kirimasim::test::runVerified(compiled.design(), context);
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
    const auto result = kirimasim::test::runVerified(compiled.design(), context);
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
    const auto result = kirimasim::test::runVerified(compiled.design(), context);
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
        auto design = compiled.design();
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
    const auto result =
        kirimasim::test::runVerified(compiled.design(), context, {.vpiCompileOnly = true});
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
    const auto result = kirimasim::test::runVerified(compiled.design(), context);
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
    const auto result =
        kirimasim::test::runVerified(compiled.design(), context, {.vpiCompileOnly = true});
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    k::Vpi::Entry enter(context);
    vpiHandle object = nullptr;
    for (const auto& row : compiled.design().foreignObjects)
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
    auto design = compiled.design();
    for (auto& body : design.processes)
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
    for (const auto& error : s::verify(design))
        messages += error.message + "\n";
    EXPECT_NE(messages.find("nullable foreign reference escapes"), std::string::npos) << messages;
    auto ordinary = design;
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
    const auto result = kirimasim::test::runVerified(compiled.design(), context);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(out.text(), "1 7\n");
}
TEST(VpiFront, WholeNetCapabilitiesPreservePhysicalAliasesAndExecute) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    const auto routines = registration(context, calls);
    Source source("module leaf(input wire[32:0]a,output wire[32:0]n);assign n=a;endmodule "
                  "module top;logic[32:0]a=0;wire[32:0]n;leaf u(a,n);"
                  "initial begin #1;$display(\"%0d\",$test_vpi(n));end endmodule\n");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_TRUE(s::verify(compiled.design()).empty());
    const auto object = [&](std::string_view name) -> const s::ForeignObject* {
        for (const auto& row : compiled.design().foreignObjects)
            if (row.name == name) return &row;
        return nullptr;
    };
    const auto* net = object("top.n");
    const auto* alias = object("top.u.n");
    const auto* input = object("top.u.a");
    const auto* variable = object("top.a");
    ASSERT_NE(net, nullptr);
    ASSERT_NE(alias, nullptr);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(variable, nullptr);
    EXPECT_TRUE(net->netWritable);
    EXPECT_TRUE(alias->netWritable);
    EXPECT_TRUE(input->netWritable);
    EXPECT_FALSE(variable->netWritable);
    EXPECT_EQ(s::physicalSlot(compiled.design(), net->slot),
              s::physicalSlot(compiled.design(), alias->slot));
    EXPECT_TRUE(s::isWholeNetWriteSlot(compiled.design(), net->slot));
    const auto result = kirimasim::test::runVerified(compiled.design(), context);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(calls.run, 1U);
    EXPECT_EQ(out.text(), "7\n");
}
TEST(VpiFront, MalformedWholeNetCapabilitiesFailBeforeRuntimeAllocation) {
    Capture out;
    k::Context compileContext({out.file, out.file});
    Calls compileCalls;
    const auto routines = registration(compileContext, compileCalls);
    Source source("module top;logic[32:0]a=0;wire[32:0]n=a;"
                  "initial #1 $display(\"%0d\",$test_vpi(n));endmodule\n");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_TRUE(s::verify(compiled.design()).empty());
    for (unsigned fault = 0; fault < 14; ++fault) {
        SCOPED_TRACE(fault);
        auto design = compiled.design();
        auto object = std::find_if(design.foreignObjects.begin(), design.foreignObjects.end(),
                                   [](const auto& row) { return row.name == "top.n"; });
        ASSERT_NE(object, design.foreignObjects.end());
        const auto root = s::physicalSlot(design, object->slot);
        auto variable = std::find_if(design.slots.begin(), design.slots.end(),
                                     [](const auto& row) { return row.name == "top.a"; });
        ASSERT_NE(variable, design.slots.end());
        s::Instr* callSite = nullptr;
        s::Instr* reference = nullptr;
        for (auto& process : design.processes)
            for (auto& block : process.blocks)
                for (auto& in : block.instrs) {
                    if (in.op == s::Op::Syscall && !in.operands.empty() &&
                        in.operands[0].text == "$vpi.call")
                        callSite = &in;
                    if (in.op == s::Op::RefMake) reference = &in;
                }
        ASSERT_NE(callSite, nullptr);
        ASSERT_NE(reference, nullptr);
        switch (fault) {
            case 0:
                object->slot =
                    s::SlotId{static_cast<std::uint32_t>(variable - design.slots.begin())};
                break;
            case 1: object->slot = s::kNone; break;
            case 2: object->slot = s::SlotId{UINT32_MAX - 1}; break;
            case 3: design.slot(root).flags &= ~s::kNeedsStrength; break;
            case 4: design.slot(root).dims.push_back({1, 0}); break;
            case 5: design.slot(root).kind = s::SlotKind::Var; break;
            case 6: design.slot(root).type = s::u2(33); break;
            case 7: design.strengthNodes.clear(); break;
            case 8:
                ASSERT_GT(design.strengthNodes.size(), 1U);
                design.strengthNodes.front().component = UINT32_MAX;
                break;
            case 9: object->scalar = true; break;
            case 10: design.constant(callSite->operands[9].asConst()).aWords[0] = 1; break;
            case 11: callSite->operands[11] = s::Operand::str("variable"); break;
            case 12: reference->operands.pop_back(); break;
            case 13: design.slot(root).subroutine = s::SubroutineId{UINT32_MAX - 1}; break;
            default: FAIL();
        }
        EXPECT_FALSE(s::verify(design).empty());
        // The gate keeps this Design from any run (docs/notes/verified-design.md).
        const auto outcome = s::verifyDesign(std::move(design));
        EXPECT_FALSE(outcome.verified.has_value());
        EXPECT_FALSE(outcome.errors.empty());
    }
}
TEST(VpiFront, WholeAutomaticArgumentDeclarationsKeepSourceShapes) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    const auto routines = registration(context, calls);
    Source source("module top;task automatic run();logic signed[-3:61] a;logic b;"
                  "logic[7:7] c;integer d;$display(\"%0d\",$test_vpi(a,b,c,d));"
                  "endtask initial run();endmodule");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto& rows = compiled.design().foreignArguments;
    ASSERT_EQ(rows.size(), 4U);
    for (unsigned i = 0; i < rows.size(); ++i) {
        EXPECT_EQ(rows[i].site, 0U);
        EXPECT_EQ(rows[i].argument, i);
    }
    EXPECT_EQ(rows[0].left, -3);
    EXPECT_EQ(rows[0].right, 61);
    EXPECT_FALSE(rows[0].scalar);
    EXPECT_TRUE(rows[1].scalar);
    EXPECT_EQ(rows[2].left, 7);
    EXPECT_EQ(rows[2].right, 7);
    EXPECT_FALSE(rows[2].scalar);
    EXPECT_TRUE(rows[3].integer);
    EXPECT_NE(
        s::toText(compiled.design()).find("foreign.argument site 0 index 0 bounds -3:61 vector"),
        std::string::npos);
    EXPECT_TRUE(s::verify(compiled.design()).empty());
}
TEST(VpiFront, NestedCallsAssociateDeclarationsWithTheirOwnSitesAndArguments) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    const auto routines = registration(context, calls);
    Source source("module top;function automatic void run();logic[-3:61] a;int b;"
                  "$display(\"%0d\",$test_vpi(1,a,$test_vpi(b)));"
                  "endfunction initial run();endmodule");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    const auto& rows = compiled.design().foreignArguments;
    ASSERT_EQ(rows.size(), 2U);
    EXPECT_EQ(rows[0].site, 0U);
    EXPECT_EQ(rows[0].argument, 1U);
    EXPECT_EQ(rows[0].left, -3);
    EXPECT_EQ(rows[1].site, 1U);
    EXPECT_EQ(rows[1].argument, 0U);
    EXPECT_TRUE(rows[1].integer);
    EXPECT_TRUE(s::verify(compiled.design()).empty());
}
TEST(VpiFront, MalformedArgumentDeclarationsFailBeforeRuntimeAllocation) {
    Capture out;
    k::Context compileContext({out.file, out.file});
    Calls compileCalls;
    const auto routines = registration(compileContext, compileCalls);
    Source source("module top;task automatic run();logic[-3:61] a;"
                  "$display(\"%0d\",$test_vpi(a,7));endtask initial run();endmodule");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    ASSERT_EQ(compiled.design().foreignArguments.size(), 1U);
    for (unsigned fault = 0; fault < 12; ++fault) {
        SCOPED_TRACE(fault);
        auto design = compiled.design();
        auto& row = design.foreignArguments.front();
        switch (fault) {
            case 0: ++row.left; break;
            case 1: row.scalar = true; break;
            case 2: row.integer = true; break;
            case 3: row.site = UINT32_MAX; break;
            case 4: row.argument = UINT32_MAX; break;
            case 5: design.foreignArguments.push_back(row); break;
            case 6:
                row.left = INT64_MIN;
                row.right = INT64_MAX;
                break;
            case 7:
                row.left = INT64_MAX;
                row.right = INT64_MIN;
                break;
            case 8:
                for (auto& body : design.subroutines)
                    for (auto& block : body.blocks)
                        for (auto& in : block.instrs)
                            if (in.op == s::Op::Syscall && in.operands[0].text == "$vpi.call")
                                design.constant(in.operands[9].asConst()).aWords[0] = 1;
                break;
            case 9:
                row.left = INT32_MAX;
                row.right = std::int64_t{INT32_MAX} + 64;
                break;
            case 10:
                row.left = std::int64_t{INT32_MIN} - 64;
                row.right = INT32_MIN;
                break;
            case 11: row.argument = 1; break;
            default: FAIL();
        }
        EXPECT_FALSE(s::verify(design).empty());
        // The gate keeps this Design from any run (docs/notes/verified-design.md).
        const auto outcome = s::verifyDesign(std::move(design));
        EXPECT_FALSE(outcome.verified.has_value());
        EXPECT_FALSE(outcome.errors.empty());
    }
}
TEST(VpiFront, OptionalArgumentDeclarationsDoNotInvalidateLegacyTransport) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    const auto routines = registration(context, calls);
    Source source("module top;task automatic run();int a=1;"
                  "$display(\"%0d\",$test_vpi(a));endtask initial run();endmodule");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    auto design = compiled.design();
    design.foreignArguments.clear();
    EXPECT_TRUE(s::verify(design).empty());
    const auto result = kirimasim::test::runVerified(design, context);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(out.text(), "7\n");
}
TEST(VpiFront, HostRejectsMalformedArgumentDeclarationsBeforeCompiletf) {
    for (unsigned fault = 0; fault < 7; ++fault) {
        SCOPED_TRACE(fault);
        Capture out;
        k::Context context({out.file, out.file});
        Calls calls;
        ASSERT_FALSE(registration(context, calls).empty());
        k::VpiArgument argument;
        argument.value.type = {65, true, true, false};
        argument.declaration = k::VpiDeclaration{-3, 61, false, false};
        switch (fault) {
            case 0: argument.declaration->left = 0; break;
            case 1: argument.declaration->scalar = true; break;
            case 2: argument.declaration->integer = true; break;
            case 3: argument.value.type.real = true; break;
            case 4: argument.value.type.width = 0; break;
            case 5:
                argument.declaration->left = INT32_MAX;
                argument.declaration->right = std::int64_t{INT32_MAX} + 64;
                break;
            case 6:
                argument.declaration->left = std::int64_t{INT32_MIN} - 64;
                argument.declaration->right = INT32_MIN;
                break;
            default: FAIL();
        }
        EXPECT_FALSE(context.vpi().compileCall(0, 0, "top", {&argument, 1},
                                               context.vpi().routines().front().result));
        EXPECT_EQ(calls.compile, 0U);
        EXPECT_EQ(context.store().size(), 0U);
    }
}
}  // namespace

namespace {
TEST(VpiFront, DeclaredScalarMetadataDoesNotCollapseOneBitVectors) {
    Capture out;
    k::Context context({out.file, out.file});
    Calls calls;
    const auto routines = registration(context, calls);
    Source source("module top;typedef logic scalar_t;scalar_t scalar;"
                  "logic [0:0] vector;logic [64:0] wide;int word;"
                  "initial $test_vpi();endmodule\n");
    auto compiled = compile(source, routines);
    ASSERT_TRUE(compiled.success) << compiled.diagnostics << compiled.verifyErrors;
    auto design = compiled.design();
    const auto find = [&](const std::string& name) -> s::ForeignObject* {
        for (auto& object : design.foreignObjects)
            if (object.name == name) return &object;
        return nullptr;
    };
    const auto scalar = find("top.scalar"), vector = find("top.vector");
    ASSERT_NE(scalar, nullptr);
    ASSERT_NE(vector, nullptr);
    EXPECT_TRUE(scalar->scalar);
    EXPECT_FALSE(vector->scalar);
    EXPECT_EQ(design.slot(scalar->slot).type.width, 1U);
    EXPECT_EQ(design.slot(vector->slot).type.width, 1U);
    const auto text = s::toText(design);
    EXPECT_NE(text.find(" scalar\n"), std::string::npos);
    EXPECT_NE(text.find(" vector\n"), std::string::npos);
    auto wide = find("top.wide");
    ASSERT_NE(wide, nullptr);
    wide->scalar = true;
    const auto errors = s::verify(design);
    bool diagnosed = false;
    for (const auto& error : errors)
        diagnosed |= error.message == "reflected scalar requires one-bit integral storage";
    EXPECT_TRUE(diagnosed);
}

// A force and a release from VPI act on a variable as the statements do
// (10.6.2, SCHEDULER_SPEC.md 14.4): one that a continuous assignment drives
// takes the driver's value again when released, and one without a driver
// keeps the forced value until it is assigned. The matrix: the type x the
// driver (a continuous assignment, the output port of an instance, none) x
// the place of the variable (the top module, a generate block, an instance
// below). Each cell forces one variable from VPI and its twin with the
// statements, and prints both at each step.
PLI_INT32 forceOrRelease(PLI_BYTE8* data) {
    const auto site = vpi_handle(vpiSysTfCall, nullptr);
    const auto iterator = vpi_iterate(vpiArgument, site);
    const auto arg = vpi_scan(iterator);
    s_vpi_value value{};
    value.format = vpiIntVal;
    value.value.integer = 5;
    vpi_put_value(arg, &value, nullptr, data[0] == 'f' ? vpiForceFlag : vpiReleaseFlag);
    vpi_free_object(iterator);
    return 0;
}
std::vector<f::ForeignRoutine> forceTasks(k::Context& context) {
    k::Vpi::Entry enter(context);
    char force[] = "$vforce", release[] = "$vrelease", f[] = "f", r[] = "r";
    for (const auto& [name, tag] : {std::pair{force, f}, std::pair{release, r}}) {
        s_vpi_systf_data data{};
        data.type = vpiSysTask;
        data.tfname = name;
        data.calltf = &forceOrRelease;
        data.user_data = tag;
        EXPECT_NE(vpi_register_systf(&data), nullptr);
    }
    std::vector<f::ForeignRoutine> out;
    for (const auto& routine : context.vpi().routines())
        out.push_back({routine.name, s::voidType()});
    return out;
}
TEST(VpiFront, ForceAndReleaseOfAVariableActAsTheStatementsDo) {
    struct TypeCell {
        const char* type;
        unsigned mask;
    };
    const TypeCell types[] = {{"logic", 1}, {"logic [3:0]", 15}, {"bit [7:0]", 255}, {"int", ~0U}};
    const char* const drivers[] = {"assign", "port", "none"};
    const char* const places[] = {"top", "generate", "instance"};
    unsigned cells = 0;
    for (const auto& type : types)
        for (const std::string driver : drivers)
            for (const std::string place : places) {
                // The output port of an instance drives a variable of the
                // module that holds the instance.
                if (driver == "port" && place == "instance") continue;
                const std::string t = type.type;
                const std::string declare =
                    t + " v, w;" +
                    (driver == "assign" ? " assign v = a; assign w = a;"
                     : driver == "port" ? " pass pv(.i(a), .o(v)); pass pw(.i(a), .o(w));"
                                        : "");
                const std::string prefix = place == "top" ? "" : place == "generate" ? "g." : "u.";
                std::string text =
                    "module pass(input " + t + " i, output " + t + " o); assign o = i; endmodule\n";
                if (place == "instance")
                    text += "module low(input " + t + " a); " + declare + " endmodule\n";
                text += "module top; " + t + " a;\n";
                if (place == "top") text += declare + "\n";
                if (place == "generate") text += "if (1) begin : g " + declare + " end\n";
                if (place == "instance") text += "low u(.a(a));\n";
                const std::string v = prefix + "v", w = prefix + "w";
                const std::string show = "%0d %0d\\n\", " + v + ", " + w + ");\n";
                text += "initial begin a = 6; #1 $vforce(" + v + "); force " + w + " = 5;\n";
                text += "#1 $write(\"forced " + show;
                text += "a = 2;\n#1 $write(\"driver changed " + show;
                text += "$vrelease(" + v + "); release " + w + ";\n";
                text += "#1 $write(\"released " + show;
                text += driver == "none" ? v + " = 4; " + w + " = 4;\n" : "a = 4;\n";
                text += "#1 $write(\"next " + show + "end endmodule\n";
                const auto pair = [&](unsigned value) {
                    const std::string one = std::to_string(value & type.mask);
                    return one + " " + one + "\n";
                };
                const std::string want = "forced " + pair(5) + "driver changed " + pair(5) +
                                         "released " + pair(driver == "none" ? 5 : 2) + "next " +
                                         pair(4);
                Capture out;
                k::Context context({out.file, out.file});
                const auto routines = forceTasks(context);
                Source source(text);
                auto compiled = compile(source, routines);
                ASSERT_TRUE(compiled.success)
                    << text << compiled.diagnostics << compiled.verifyErrors;
                const auto result = kirimasim::test::runVerified(compiled.design(), context);
                EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                EXPECT_EQ(out.text(), want) << t << ", " << driver << ", " << place;
                ++cells;
            }
    EXPECT_EQ(cells, 32U);
}
}  // namespace
