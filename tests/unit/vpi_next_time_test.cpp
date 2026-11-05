// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/vpi.h"
#include <array>
#include <cstdio>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

namespace {
namespace k = kirimasim::kernel;
struct Capture {
    std::FILE* file = std::tmpfile();
    ~Capture() {
        if (file) std::fclose(file);
    }
};
struct State {
    unsigned calls = 0;
    std::vector<std::uint64_t> times;
    double real = 0;
    int format = 0;
};
PLI_INT32 record(p_cb_data data) {
    auto& state = *reinterpret_cast<State*>(data->user_data);
    ++state.calls;
    s_vpi_time now{};
    now.type = vpiSimTime;
    vpi_get_time(nullptr, &now);
    state.times.push_back((std::uint64_t{now.high} << 32) | now.low);
    if (data->time) {
        state.format = data->time->type;
        state.real = data->time->real;
        if (data->time->type == vpiSimTime) {
            EXPECT_EQ(data->time->high, now.high);
            EXPECT_EQ(data->time->low, now.low);
        }
    }
    return 0;
}
vpiHandle add(int reason, p_vpi_time time, State& state, vpiHandle object = nullptr) {
    s_cb_data data{};
    data.reason = reason;
    data.cb_rtn = &record;
    data.user_data = reinterpret_cast<char*>(&state);
    data.time = time;
    data.obj = object;
    return vpi_register_cb(&data);
}
void timer(std::uint32_t ticks, State& state) {
    s_vpi_time time{};
    time.type = vpiSimTime;
    time.low = ticks;
    ASSERT_NE(add(cbAfterDelay, &time, state), nullptr);
}

TEST(VpiNextTime, PayloadDoesNotDefineATimeOrRunAtRegistration) {
    const std::array payloads{0.0,
                              1.25,
                              -1.0,
                              1.0e300,
                              std::numeric_limits<double>::infinity(),
                              std::numeric_limits<double>::quiet_NaN()};
    for (const bool runtime : {false, true})
        for (const int format : {vpiSimTime, vpiScaledRealTime})
            for (const double payload : payloads) {
                SCOPED_TRACE(::testing::Message() << runtime << ' ' << format << ' ' << payload);
                Capture out;
                k::Context context({out.file, out.file});
                context.vpi().configure({"test"}, "test", -12);
                k::VpiObjectInfo info;
                info.kind = vpiModule;
                info.name = "top";
                info.unit = -9;
                info.precision = -12;
                const auto scope = context.vpi().object(info);
                State next, after;
                k::Vpi::Entry enter(context);
                if (runtime) context.vpi().beginRuntime();
                s_vpi_time time{};
                time.type = format;
                time.high = time.low = std::numeric_limits<std::uint32_t>::max();
                time.real = payload;
                ASSERT_NE(add(cbNextSimTime, &time, next, scope), nullptr);
                EXPECT_EQ(vpi_chk_error(nullptr), 0);
                EXPECT_EQ(next.calls, 0U);
                // Mutating the caller's descriptor must not change reporting.
                time.type = vpiSuppressTime;
                time.real = -17;
                if (!runtime) context.vpi().beginRuntime();
                timer(7001, after);
                EXPECT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
                EXPECT_EQ(next.times, std::vector<std::uint64_t>{7001});
                EXPECT_EQ(next.calls, 1U);
                EXPECT_EQ(after.calls, 1U);
                EXPECT_EQ(next.format, format);
                if (format == vpiScaledRealTime) EXPECT_DOUBLE_EQ(next.real, 7.001);
                EXPECT_EQ(context.scheduler().time(), 7001U);
            }
}

TEST(VpiNextTime, MissingSuppressedAndUnknownFormatsStillFail) {
    for (const bool runtime : {false, true}) {
        Capture out;
        k::Context context({out.file, out.file});
        context.vpi().configure({"test"}, "test", -9);
        if (runtime) context.vpi().beginRuntime();
        State state;
        k::Vpi::Entry enter(context);
        EXPECT_EQ(add(cbNextSimTime, nullptr, state), nullptr);
        EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
        for (const int format : {vpiSuppressTime, -1, 999}) {
            s_vpi_time time{};
            time.type = format;
            EXPECT_EQ(add(cbNextSimTime, &time, state), nullptr);
            EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
        }
        if (!runtime) context.vpi().beginRuntime();
        EXPECT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(context.scheduler().time(), 0U);
        EXPECT_EQ(state.calls, 0U);
    }
}

TEST(VpiNextTime, RealTimerReasonsStillValidateAndRoundTheirDelay) {
    for (const int reason : {cbAfterDelay, cbReadWriteSynch, cbReadOnlySynch}) {
        for (const double invalid : {-1.0, 1.0e300, std::numeric_limits<double>::infinity(),
                                     std::numeric_limits<double>::quiet_NaN()}) {
            Capture out;
            k::Context context({out.file, out.file});
            context.vpi().configure({"test"}, "test", -9);
            context.vpi().beginRuntime();
            State state;
            k::Vpi::Entry enter(context);
            s_vpi_time time{};
            time.type = vpiScaledRealTime;
            time.real = invalid;
            EXPECT_EQ(add(reason, &time, state), nullptr);
            EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
            EXPECT_EQ(context.scheduler().run(), k::EndReason::Error);
            EXPECT_EQ(state.calls, 0U);
        }
        Capture out;
        k::Context context({out.file, out.file});
        context.vpi().configure({"test"}, "test", -9);
        context.vpi().beginRuntime();
        State state;
        k::Vpi::Entry enter(context);
        s_vpi_time time{};
        time.type = vpiScaledRealTime;
        time.real = 1.5;
        ASSERT_NE(add(reason, &time, state), nullptr);
        EXPECT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(state.times, std::vector<std::uint64_t>{2});
    }
}

struct ReadOnly {
    State next;
    int format;
    unsigned registrations = 0;
};
PLI_INT32 fromReadOnly(p_cb_data data) {
    auto& state = *reinterpret_cast<ReadOnly*>(data->user_data);
    s_vpi_time time{};
    time.type = state.format;
    EXPECT_NE(add(cbNextSimTime, &time, state.next), nullptr);
    EXPECT_EQ(vpi_chk_error(nullptr), 0);
    ++state.registrations;
    // Actual same-slot timed requests remain forbidden in Postponed.
    time.type = vpiSimTime;
    for (const int reason : {cbAfterDelay, cbReadWriteSynch, cbReadOnlySynch}) {
        EXPECT_EQ(add(reason, &time, state.next), nullptr);
        EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
    }
    return 0;
}
TEST(VpiNextTime, ReadOnlyRegistrationWaitsForTheNextActualSlot) {
    for (const int format : {vpiSimTime, vpiScaledRealTime}) {
        Capture out;
        k::Context context({out.file, out.file});
        context.vpi().configure({"test"}, "test", -9);
        context.vpi().beginRuntime();
        k::Vpi::Entry enter(context);
        ReadOnly state{{}, format};
        s_vpi_time time{};
        time.type = vpiSimTime;
        s_cb_data data{};
        data.reason = cbReadOnlySynch;
        data.cb_rtn = &fromReadOnly;
        data.time = &time;
        data.user_data = reinterpret_cast<char*>(&state);
        ASSERT_NE(vpi_register_cb(&data), nullptr);
        State after;
        timer(7, after);
        EXPECT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(state.registrations, 1U);
        EXPECT_EQ(state.next.times, std::vector<std::uint64_t>{7});
        EXPECT_EQ(state.next.calls, 1U);
        EXPECT_EQ(after.calls, 1U);
    }
}

TEST(VpiNextTime, CancellationAndQuiescenceNeverCreateATimeSlot) {
    for (const bool cancel : {false, true})
        for (const int format : {vpiSimTime, vpiScaledRealTime}) {
            Capture out;
            k::Context context({out.file, out.file});
            context.vpi().configure({"test"}, "test", -9);
            context.vpi().beginRuntime();
            k::Vpi::Entry enter(context);
            State state;
            s_vpi_time time{};
            time.type = format;
            time.high = time.low = 17;
            time.real = std::numeric_limits<double>::infinity();
            const auto token = add(cbNextSimTime, &time, state);
            ASSERT_NE(token, nullptr);
            if (cancel) {
                EXPECT_EQ(vpi_remove_cb(token), 1);
                EXPECT_EQ(vpi_remove_cb(token), 0);
            }
            EXPECT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
            EXPECT_EQ(context.scheduler().time(), 0U);
            EXPECT_EQ(state.calls, 0U);
        }
}
}  // namespace
