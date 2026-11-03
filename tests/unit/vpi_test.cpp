// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/vpi.h"
#include <array>
#include <bit>
#include <cstdio>
#include <gtest/gtest.h>
#include <string>
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
    unsigned count = 0;
    vpiHandle token = nullptr;
    vpiHandle object = nullptr;
    std::vector<std::uint64_t> times;
    std::vector<int> values;
    bool remove = false, loop = false;
};
PLI_INT32 record(p_cb_data data) {
    auto& state = *reinterpret_cast<State*>(data->user_data);
    ++state.count;
    s_vpi_time time{};
    time.type = vpiSimTime;
    vpi_get_time(nullptr, &time);
    state.times.push_back((std::uint64_t{time.high} << 32) | time.low);
    if (data->obj) {
        s_vpi_value value{};
        value.format = vpiIntVal;
        vpi_get_value(data->obj, &value);
        state.values.push_back(value.value.integer);
        if (state.loop) {
            value.value.integer ^= 1;
            vpi_put_value(data->obj, &value, nullptr, vpiNoDelay);
        }
    }
    if (state.remove) vpi_remove_cb(state.token);
    return 0;
}
vpiHandle callback(k::Context& c, int reason, State& state, std::uint64_t ticks = 0,
                   vpiHandle object = nullptr) {
    k::Vpi::Entry enter(c);
    s_vpi_time time{};
    time.type = vpiSimTime;
    time.low = static_cast<std::uint32_t>(ticks);
    time.high = static_cast<std::uint32_t>(ticks >> 32);
    s_cb_data data{};
    data.reason = reason;
    data.cb_rtn = &record;
    data.user_data = reinterpret_cast<char*>(&state);
    data.time = &time;
    data.obj = object;
    state.token = vpi_register_cb(&data);
    return state.token;
}
vpiHandle variable(k::Context& c, std::uint32_t width = 32, bool four = true,
                   bool signedValue = true) {
    const auto slot =
        c.store().add(four ? k::Domain::Four : k::Domain::Two, width, 1, k::Init::Zero);
    k::VpiObjectInfo info;
    info.name = "top.value";
    info.scopeName = "top";
    info.parentName = "top";
    info.left = width - 1;
    info.type = {width, four, signedValue, false};
    info.binding.isSlot = true;
    info.binding.writable = true;
    info.binding.slot = {slot, 0, 0, width};
    return c.vpi().object(std::move(info));
}
void put(vpiHandle object, int value, int flags = vpiNoDelay) {
    s_vpi_value data{};
    data.format = vpiIntVal;
    data.value.integer = value;
    vpi_put_value(object, &data, nullptr, flags);
}
int get(vpiHandle object) {
    s_vpi_value value{};
    value.format = vpiIntVal;
    vpi_get_value(object, &value);
    return value.value.integer;
}

TEST(Vpi, EntryRestoresNestedContextsAndHasNoDefault) {
    Capture out;
    k::Context a({out.file, out.file}), b({out.file, out.file});
    a.vpi().configure({"a"}, "A", -9);
    b.vpi().configure({"b"}, "B", -12);
    s_vpi_vlog_info info{};
    EXPECT_EQ(vpi_get_vlog_info(&info), 0);
    {
        k::Vpi::Entry outer(a);
        ASSERT_EQ(vpi_get_vlog_info(&info), 1);
        EXPECT_STREQ(info.version, "A");
        {
            k::Vpi::Entry inner(b);
            ASSERT_EQ(vpi_get_vlog_info(&info), 1);
            EXPECT_STREQ(info.version, "B");
        }
        ASSERT_EQ(vpi_get_vlog_info(&info), 1);
        EXPECT_STREQ(info.version, "A");
    }
    EXPECT_EQ(vpi_get_vlog_info(&info), 0);
}
TEST(Vpi, CopiesDelayedValueAndCancelsInertialUpdates) {
    Capture out;
    k::Context c({out.file, out.file});
    auto object = variable(c);
    c.vpi().beginRuntime();
    {
        k::Vpi::Entry enter(c);
        s_vpi_time time{};
        time.type = vpiSimTime;
        time.low = 2;
        s_vpi_value value{};
        value.format = vpiIntVal;
        value.value.integer = 7;
        vpi_put_value(object, &value, &time, vpiInertialDelay);
        value.value.integer = 9;
        time.low = 3;
        vpi_put_value(object, &value, &time, vpiInertialDelay);
        value.value.integer = 100;
        time.low = 20;
    }
    State state;
    ASSERT_NE(callback(c, cbValueChange, state, 0, object), nullptr);
    c.scheduler().run();
    EXPECT_EQ(state.values, std::vector<int>{9});
    EXPECT_EQ(state.times, std::vector<std::uint64_t>{3});
    k::Vpi::Entry enter(c);
    EXPECT_EQ(get(object), 9);
}
TEST(Vpi, CancelledTimerAndNextObserverDoNotAdvanceQuiescence) {
    Capture out;
    k::Context c({out.file, out.file});
    c.vpi().beginRuntime();
    State timer, next;
    ASSERT_NE(callback(c, cbAfterDelay, timer, 100000), nullptr);
    ASSERT_NE(callback(c, cbNextSimTime, next), nullptr);
    {
        k::Vpi::Entry enter(c);
        ASSERT_EQ(vpi_remove_cb(timer.token), 1);
    }
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(c.scheduler().time(), 0U);
    EXPECT_EQ(timer.count, 0U);
    EXPECT_EQ(next.count, 0U);
}
TEST(Vpi, CallbackSelfRemovalAndFourStateNoChange) {
    Capture out;
    k::Context c({out.file, out.file});
    auto object = variable(c, 65);
    c.vpi().beginRuntime();
    State state, persistent;
    state.remove = true;
    ASSERT_NE(callback(c, cbValueChange, persistent, 0, object), nullptr);
    ASSERT_NE(callback(c, cbValueChange, state, 0, object), nullptr);
    k::Vpi::Entry enter(c);
    std::array<s_vpi_vecval, 3> words{};
    s_vpi_value value{};
    value.format = vpiVectorVal;
    value.value.vector = words.data();
    words[0] = {1, 1};
    vpi_put_value(object, &value, nullptr, vpiNoDelay);
    vpi_put_value(object, &value, nullptr, vpiNoDelay);
    words[0] = {0, 1};
    vpi_put_value(object, &value, nullptr, vpiNoDelay);
    EXPECT_EQ(state.count, 1U);
    words[0] = {0, 0};
    vpi_put_value(object, &value, nullptr, vpiNoDelay);
    EXPECT_EQ(persistent.count, 3U);
    EXPECT_EQ(state.values, std::vector<int>{0});
}
TEST(Vpi, TransportPreservesEachOwnedPayload) {
    Capture out;
    k::Context c({out.file, out.file});
    auto object = variable(c);
    c.vpi().beginRuntime();
    State state;
    ASSERT_NE(callback(c, cbValueChange, state, 0, object), nullptr);
    {
        k::Vpi::Entry enter(c);
        s_vpi_time time{};
        time.type = vpiSimTime;
        time.low = 2;
        s_vpi_value value{};
        value.format = vpiIntVal;
        value.value.integer = 7;
        vpi_put_value(object, &value, &time, vpiTransportDelay);
        time.low = 3;
        value.value.integer = 8;
        vpi_put_value(object, &value, &time, vpiTransportDelay);
        value.value.integer = 999;
    }
    c.scheduler().run();
    EXPECT_EQ(state.values, (std::vector<int>{7, 8}));
    EXPECT_EQ(state.times, (std::vector<std::uint64_t>{2, 3}));
}
TEST(Vpi, ForceIsPersistentAndReleaseRetainsVariableValue) {
    Capture out;
    k::Context c({out.file, out.file});
    auto object = variable(c);
    c.vpi().beginRuntime();
    k::Vpi::Entry enter(c);
    put(object, 7, vpiForceFlag);
    put(object, 3);
    EXPECT_EQ(get(object), 7);
    put(object, 0, vpiReleaseFlag);
    EXPECT_EQ(get(object), 7);
    put(object, 5);
    EXPECT_EQ(get(object), 5);
}
TEST(Vpi, InvalidAndExhaustedIteratorsAreDiagnosed) {
    Capture out;
    k::Context c({out.file, out.file});
    k::VpiObjectInfo scope;
    scope.kind = vpiModule;
    scope.name = "top";
    c.vpi().object(scope);
    auto object = variable(c);
    k::Vpi::Entry enter(c);
    auto iterator = vpi_iterate(vpiReg, vpi_handle_by_name(const_cast<char*>("top"), nullptr));
    ASSERT_NE(iterator, nullptr);
    EXPECT_EQ(vpi_scan(iterator), object);
    EXPECT_EQ(vpi_scan(iterator), nullptr);
    EXPECT_EQ(vpi_scan(iterator), nullptr);
    s_vpi_error_info error{};
    EXPECT_EQ(vpi_chk_error(&error), vpiError);
    EXPECT_NE(error.code, nullptr);
    EXPECT_EQ(vpi_get(vpiType, reinterpret_cast<vpiHandle>(&object)), 0);
}
TEST(Vpi, InvalidCallbackDescriptorsDoNotScheduleWork) {
    Capture out;
    k::Context c({out.file, out.file});
    c.vpi().beginRuntime();
    State state;
    k::Vpi::Entry enter(c);
    s_cb_data cb{};
    cb.reason = cbAfterDelay;
    cb.cb_rtn = &record;
    cb.user_data = reinterpret_cast<char*>(&state);
    EXPECT_EQ(vpi_register_cb(&cb), nullptr);
    s_vpi_time time{};
    time.type = vpiSuppressTime;
    cb.time = &time;
    EXPECT_EQ(vpi_register_cb(&cb), nullptr);
    cb.reason = 999;
    time.type = vpiSimTime;
    EXPECT_EQ(vpi_register_cb(&cb), nullptr);
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(state.count, 0U);
    auto handle = callback(c, cbNextSimTime, state);
    ASSERT_NE(handle, nullptr);
    EXPECT_EQ(vpi_get(vpiType, handle), 0);
    EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
    EXPECT_EQ(vpi_get_str(vpiName, handle), nullptr);
    EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
}
TEST(Vpi, SynchronousCallbackLoopUsesExistingSlotLimit) {
    Capture out;
    k::Context c({out.file, out.file});
    auto object = variable(c);
    c.vpi().beginRuntime();
    k::Limits limits;
    limits.maxSlotEvents = 8;
    c.scheduler().setLimits(limits);
    State state;
    state.loop = true;
    ASSERT_NE(callback(c, cbValueChange, state, 0, object), nullptr);
    {
        k::Vpi::Entry enter(c);
        put(object, 1);
    }
    EXPECT_EQ(c.scheduler().reason(), k::EndReason::ZeroTimeLimit);
    EXPECT_EQ(state.count, 8U);
}
TEST(Vpi, MalformedWriteCannotTouchOutsideTheSlot) {
    Capture out;
    k::Context c({out.file, out.file});
    const auto slot = c.store().add(k::Domain::Four, 4, 1, k::Init::Zero);
    k::VpiObjectInfo info;
    info.name = "bad";
    info.type = {4, true, false, false};
    info.binding.isSlot = true;
    info.binding.writable = true;
    info.binding.slot = {slot, 0, 99, 4};
    const auto object = c.vpi().object(info);
    c.vpi().beginRuntime();
    k::Vpi::Entry enter(c);
    put(object, 15);
    EXPECT_EQ(c.store().element(slot, 0)[0], 0U);
    EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
}
TEST(Vpi, TwoStateWritesClearUnknownBitsAndMaskPadding) {
    Capture out;
    k::Context c({out.file, out.file});
    auto object = variable(c, 65, false);
    c.vpi().beginRuntime();
    k::Vpi::Entry enter(c);
    std::array<s_vpi_vecval, 3> words{{{-1, -1}, {-1, 0}, {-1, 0}}};
    s_vpi_value value{};
    value.format = vpiVectorVal;
    value.value.vector = words.data();
    vpi_put_value(object, &value, nullptr, vpiNoDelay);
    vpi_get_value(object, &value);
    EXPECT_EQ(value.value.vector[0].aval, 0);
    EXPECT_EQ(value.value.vector[0].bval, 0);
    EXPECT_EQ(value.value.vector[1].aval, -1);
    EXPECT_EQ(value.value.vector[2].aval, 1);
}
struct LibraryLease {
    std::vector<int>* closed;
    unsigned* callbacks;
    int id;
};
void closeLease(void* raw) {
    auto& lease = *static_cast<LibraryLease*>(raw);
    EXPECT_EQ(*lease.callbacks, 1U);
    lease.closed->push_back(lease.id);
}
TEST(Vpi, EndCallbackRunsOnceAndAfterStop) {
    Capture out;
    State state;
    std::vector<int> closed;
    std::array<LibraryLease, 2> leases{{{&closed, &state.count, 1}, {&closed, &state.count, 2}}};
    {
        k::Context c({out.file, out.file});
        c.vpi().ownLibrary(&leases[0], &closeLease);
        c.vpi().ownLibrary(&leases[1], &closeLease);
        c.vpi().beginRuntime();
        ASSERT_NE(callback(c, cbEndOfSimulation, state), nullptr);
        c.scheduler().requestEnd(k::EndReason::Finish, 0);
        c.vpi().finish();
        c.vpi().finish();
        EXPECT_EQ(state.count, 1U);
        EXPECT_TRUE(closed.empty());
    }
    EXPECT_EQ(closed, (std::vector<int>{2, 1}));
}

TEST(Vpi, HandlesBelongToTheOwningLiveContext) {
    Capture out;
    k::Context a({out.file, out.file}), b({out.file, out.file});
    auto first = variable(a), second = variable(b);
    EXPECT_NE(first, second);
    k::Vpi::Entry enter(b);
    EXPECT_EQ(vpi_get(vpiType, first), 0);
    EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
    EXPECT_EQ(vpi_get(vpiType, second), vpiReg);
}
struct Registering {
    vpiHandle object = nullptr, token = nullptr;
    unsigned first = 0, second = 0;
};
PLI_INT32 later(p_cb_data cb) {
    ++reinterpret_cast<Registering*>(cb->user_data)->second;
    return 0;
}
PLI_INT32 registering(p_cb_data cb) {
    auto& state = *reinterpret_cast<Registering*>(cb->user_data);
    ++state.first;
    if (!state.token) {
        s_cb_data next = *cb;
        next.cb_rtn = &later;
        state.token = vpi_register_cb(&next);
    }
    return 0;
}
TEST(Vpi, NewSubscriberDoesNotJoinTheCurrentNotificationBatch) {
    Capture out;
    k::Context c({out.file, out.file});
    Registering state;
    state.object = variable(c);
    c.vpi().beginRuntime();
    k::Vpi::Entry enter(c);
    s_cb_data cb{};
    cb.reason = cbValueChange;
    cb.obj = state.object;
    cb.cb_rtn = &registering;
    cb.user_data = reinterpret_cast<char*>(&state);
    ASSERT_NE(vpi_register_cb(&cb), nullptr);
    put(state.object, 1);
    EXPECT_EQ(state.first, 1U);
    EXPECT_EQ(state.second, 0U);
    put(state.object, 2);
    EXPECT_EQ(state.first, 2U);
    EXPECT_EQ(state.second, 1U);
}
PLI_INT32 readonlyWrite(p_cb_data cb) {
    put(cb->obj, 9);
    return 0;
}
TEST(Vpi, ReadOnlyCallbackWritesPreserveTheExistingRuntimeError) {
    Capture out;
    k::Context c({out.file, out.file});
    auto object = variable(c);
    c.vpi().beginRuntime();
    k::Vpi::Entry enter(c);
    s_vpi_time time{};
    time.type = vpiSimTime;
    s_cb_data cb{};
    cb.reason = cbReadOnlySynch;
    cb.obj = object;
    cb.time = &time;
    cb.cb_rtn = &readonlyWrite;
    ASSERT_NE(vpi_register_cb(&cb), nullptr);
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Error);
    EXPECT_EQ(get(object), 0);
}
TEST(Vpi, DeclaredAscendingPackedAndNegativeMemoryIndicesArePreserved) {
    Capture out;
    k::Context c({out.file, out.file});
    auto packed = variable(c, 8);
    c.vpi().beginRuntime();
    k::VpiObjectInfo memory;
    memory.name = "top.a";
    memory.scopeName = "top";
    memory.type = {32, true, true, false};
    memory.kind = vpiMemory;
    memory.dimensions = {{-2, 0}};
    memory.binding.isSlot = true;
    memory.binding.writable = true;
    memory.binding.slot = {c.store().add(k::Domain::Four, 32, 3, k::Init::Zero), 0, 0, 32};
    auto array = c.vpi().object(memory);
    k::Vpi::Entry enter(c);
    const auto iterator = vpi_iterate(vpiMemoryWord, array);
    ASSERT_NE(iterator, nullptr);
    for (int i = -2; i <= 0; ++i) {
        const auto element = vpi_scan(iterator);
        ASSERT_NE(element, nullptr);
        EXPECT_EQ(get(vpi_handle(vpiIndex, element)), i);
        EXPECT_EQ(vpi_handle(vpiParent, element), array);
        put(element, i + 5);
    }
    EXPECT_EQ(vpi_scan(iterator), nullptr);
    EXPECT_EQ(get(vpi_handle_by_name(const_cast<char*>("top.a[-1]"), nullptr)), 4);
    auto bit = vpi_handle_by_index(packed, 6);
    ASSERT_NE(bit, nullptr);
    put(bit, 1);
    EXPECT_EQ(get(packed), 64);
    EXPECT_EQ(get(vpi_handle(vpiLeftRange, packed)), 7);
    EXPECT_EQ(get(vpi_handle(vpiRightRange, packed)), 0);
    k::VpiObjectInfo ascending;
    ascending.name = "top.ascending";
    ascending.type = {8, true, false, false};
    ascending.left = 0;
    ascending.right = 7;
    ascending.binding = memory.binding;
    ascending.binding.slot = {c.store().add(k::Domain::Four, 8, 1, k::Init::Zero), 0, 0, 8};
    auto asc = c.vpi().object(ascending);
    put(vpi_handle_by_index(asc, 0), 1);
    EXPECT_EQ(get(asc), 128);
}
TEST(Vpi, InvalidReadsAreSuppressedAndErrorRecoveryIsExplicit) {
    Capture out;
    k::Context c({out.file, out.file});
    auto object = variable(c);
    k::Vpi::Entry enter(c);
    s_vpi_value value{};
    value.format = vpiIntVal;
    vpi_get_value(reinterpret_cast<vpiHandle>(&object), &value);
    EXPECT_EQ(value.format, vpiSuppressVal);
    s_vpi_error_info error{};
    EXPECT_EQ(vpi_chk_error(&error), vpiError);
    EXPECT_STREQ(error.code, "VPI-INVALID");
    EXPECT_EQ(vpi_chk_error(nullptr), 0);
    EXPECT_EQ(vpi_get(999, object), 0);
    EXPECT_EQ(vpi_get(vpiSize, object), 32);
    EXPECT_EQ(vpi_chk_error(nullptr), 0);
    EXPECT_EQ(vpi_iterate(999, nullptr), nullptr);
    EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
}
struct Scaled {
    double time = -1;
};
PLI_INT32 scaledCallback(p_cb_data cb) {
    reinterpret_cast<Scaled*>(cb->user_data)->time = cb->time->real;
    return 0;
}
TEST(Vpi, ScaledCallbackDescriptorsAreCopiedAndUseTheObjectUnit) {
    Capture out;
    k::Context c({out.file, out.file});
    c.vpi().configure({"probe"}, "test", -12);
    c.vpi().beginRuntime();
    k::VpiObjectInfo info;
    info.kind = vpiModule;
    info.name = "top";
    info.unit = -9;
    info.precision = -12;
    auto scope = c.vpi().object(info);
    Scaled state;
    k::Vpi::Entry enter(c);
    s_vpi_time time{};
    time.type = vpiScaledRealTime;
    time.real = 1.5;
    s_cb_data cb{};
    cb.reason = cbAfterDelay;
    cb.obj = scope;
    cb.time = &time;
    cb.cb_rtn = &scaledCallback;
    cb.user_data = reinterpret_cast<char*>(&state);
    ASSERT_NE(vpi_register_cb(&cb), nullptr);
    time.real = 100;
    cb.cb_rtn = nullptr;
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(c.scheduler().time(), 1500U);
    EXPECT_DOUBLE_EQ(state.time, 1.5);
}
bool readTransient(const k::VpiBinding&, std::span<k::W4> words) {
    words[0] = {7, 0};
    return true;
}
TEST(Vpi, DetachedTransientHandlesCannotReadFakeValues) {
    Capture out;
    k::Context c({out.file, out.file});
    int owner = 0;
    k::VpiObjectInfo info;
    info.name = "temporary";
    info.type = {32, true, false, false};
    info.binding.owner = &owner;
    info.binding.read = &readTransient;
    auto object = c.vpi().object(info);
    k::Vpi::Entry enter(c);
    EXPECT_EQ(get(object), 7);
    c.vpi().detachBindings(&owner);
    s_vpi_value value{};
    value.format = vpiIntVal;
    vpi_get_value(object, &value);
    EXPECT_EQ(value.format, vpiSuppressVal);
    EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
}
TEST(Vpi, KnownUnsupportedNetWritesAndFlagsDoNotBecomeSuccess) {
    Capture out;
    k::Context c({out.file, out.file});
    auto object = variable(c);
    c.vpi().beginRuntime();
    k::Vpi::Entry enter(c);
    put(object, 9, 999);
    EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
    EXPECT_EQ(get(object), 0);
    k::VpiObjectInfo info;
    info.name = "net";
    info.kind = vpiNet;
    info.type = {1, true, false, false};
    info.binding.isSlot = true;
    info.binding.net = true;
    info.binding.slot = {c.store().add(k::Domain::Four, 1, 1, k::Init::Zero), 0, 0, 1};
    auto net = c.vpi().object(info);
    put(net, 1);
    EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
    EXPECT_EQ(get(net), 0);
}
TEST(Vpi, ReleaseReturnsTheCurrentValueInEveryExplicitFormat) {
    for (const auto width : {1U, 8U, 31U, 32U, 33U, 65U, 129U}) {
        for (const bool four : {false, true}) {
            Capture out;
            k::Context c({out.file, out.file});
            auto object = variable(c, width, four, false);
            c.vpi().beginRuntime();
            k::Vpi::Entry enter(c);
            const int want = width == 1 ? 1 : 21;
            for (const int format :
                 {vpiScalarVal, vpiIntVal, vpiVectorVal, vpiBinStrVal, vpiOctStrVal, vpiDecStrVal,
                  vpiHexStrVal, vpiStringVal, vpiRealVal, vpiSuppressVal}) {
                SCOPED_TRACE(::testing::Message() << width << ' ' << four << ' ' << format);
                put(object, want, vpiForceFlag);
                s_vpi_value value{};
                value.format = format;
                vpi_put_value(object, &value, nullptr, vpiReleaseFlag);
                ASSERT_EQ(vpi_chk_error(nullptr), 0);
                EXPECT_EQ(value.format, format);
                switch (value.format) {
                    case vpiScalarVal: EXPECT_EQ(value.value.scalar, vpi1); break;
                    case vpiIntVal: EXPECT_EQ(value.value.integer, want); break;
                    case vpiVectorVal:
                        ASSERT_NE(value.value.vector, nullptr);
                        for (unsigned i = 0; i < (width + 31) / 32; ++i) {
                            EXPECT_EQ(value.value.vector[i].aval, i == 0 ? want : 0);
                            EXPECT_EQ(value.value.vector[i].bval, 0);
                        }
                        break;
                    case vpiRealVal: EXPECT_EQ(value.value.real, static_cast<double>(want)); break;
                    case vpiSuppressVal: break;
                    default:
                        ASSERT_NE(value.value.str, nullptr);
                        if (format == vpiStringVal)
                            EXPECT_EQ(std::string(value.value.str),
                                      std::string(1, static_cast<char>(want)));
                        else {
                            const int base = format == vpiBinStrVal   ? 2
                                             : format == vpiOctStrVal ? 8
                                             : format == vpiHexStrVal ? 16
                                                                      : 10;
                            EXPECT_EQ(std::stoul(value.value.str, nullptr, base),
                                      static_cast<unsigned>(want));
                        }
                }
                EXPECT_EQ(get(object), want);
            }
        }
    }
}

TEST(Vpi, ReleasedVectorsRetainUnknownsHighBitsAndContextOwnedStorage) {
    for (const auto width : {8U, 33U, 65U, 129U}) {
        Capture out;
        k::Context c({out.file, out.file});
        auto object = variable(c, width);
        c.vpi().beginRuntime();
        k::Vpi::Entry enter(c);
        std::array<s_vpi_vecval, 5> words{};
        words[0] = {5, 12};
        words[(width - 1) / 32].aval |= static_cast<PLI_INT32>(1U << ((width - 1) % 32));
        s_vpi_value input{};
        input.format = vpiVectorVal;
        input.value.vector = words.data();
        vpi_put_value(object, &input, nullptr, vpiForceFlag);
        s_vpi_value output{};
        output.format = vpiVectorVal;
        vpi_put_value(object, &output, nullptr, vpiReleaseFlag);
        ASSERT_EQ(vpi_chk_error(nullptr), 0);
        ASSERT_NE(output.value.vector, nullptr);
        EXPECT_NE(output.value.vector, words.data());
        for (unsigned i = 0; i < (width + 31) / 32; ++i) {
            EXPECT_EQ(output.value.vector[i].aval, words[i].aval);
            EXPECT_EQ(output.value.vector[i].bval, words[i].bval);
        }
    }
}

TEST(Vpi, ReleaseReturnsRealVariableValues) {
    Capture out;
    k::Context c({out.file, out.file});
    k::VpiObjectInfo info;
    info.name = "real_value";
    info.kind = vpiRealVar;
    info.type = {64, false, true, true};
    info.binding.isSlot = true;
    info.binding.writable = true;
    info.binding.slot = {c.store().add(k::Domain::Real, 64, 1, k::Init::Zero), 0, 0, 64};
    const auto object = c.vpi().object(std::move(info));
    c.vpi().beginRuntime();
    k::Vpi::Entry enter(c);
    s_vpi_value value{};
    value.format = vpiRealVal;
    value.value.real = 2.25;
    vpi_put_value(object, &value, nullptr, vpiForceFlag);
    value.value.real = -1.0;
    vpi_put_value(object, &value, nullptr, vpiReleaseFlag);
    EXPECT_EQ(vpi_chk_error(nullptr), 0);
    EXPECT_EQ(value.value.real, 2.25);
}

TEST(Vpi, RejectedReleaseLeavesTheCallersOutputUnchanged) {
    for (const bool net : {false, true}) {
        Capture out;
        k::Context c({out.file, out.file});
        const auto slot = c.store().add(k::Domain::Four, 32, 1, k::Init::Zero);
        k::VpiObjectInfo info;
        info.name = "rejected";
        info.kind = net ? vpiNet : vpiReg;
        info.type = {32, true, false, false};
        info.binding.isSlot = true;
        info.binding.net = net;
        info.binding.writable = net;
        info.binding.slot = {slot, 0, 0, 32};
        const auto object = c.vpi().object(std::move(info));
        c.vpi().beginRuntime();
        k::Vpi::Entry enter(c);
        s_vpi_value value{};
        value.format = vpiIntVal;
        value.value.integer = 999;
        vpi_put_value(object, &value, nullptr, vpiReleaseFlag);
        EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
        EXPECT_EQ(value.format, vpiIntVal);
        EXPECT_EQ(value.value.integer, 999);
        EXPECT_EQ(c.store().element(slot, 0)[0], 0U);
    }
}

TEST(Vpi, NarrowSignedIntegerConversionsAndNullReleaseArePreserved) {
    Capture out;
    k::Context c({out.file, out.file});
    auto object = variable(c, 8);
    c.vpi().beginRuntime();
    k::Vpi::Entry enter(c);
    put(object, -2);
    EXPECT_EQ(get(object), -2);
    put(object, 7, vpiForceFlag);
    vpi_put_value(object, nullptr, nullptr, vpiReleaseFlag);
    EXPECT_EQ(vpi_chk_error(nullptr), 0);
    EXPECT_EQ(get(object), 7);
}
vpiHandle physicalNet(k::Context& context, bool grant) {
    const auto slot = context.store().add(k::Domain::Four, 1, 1, k::Init::Z);
    const k::StrengthRange initial{};
    EXPECT_EQ(context.scheduler().enableStrengths(slot, {&initial, 1}), k::StrengthError::None);
    const k::StrengthNodeSpec node{{slot, 0, 0, 1}};
    const k::StrengthOriginSpec origin{0, {-6, -6}};
    EXPECT_EQ(context.scheduler().configureStrengthDrivers({&node, 1}, {&origin, 1}),
              k::StrengthError::None);
    EXPECT_EQ(context.scheduler().startStrengthDrivers(), k::StrengthError::None);
    EXPECT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
    k::VpiObjectInfo info;
    info.name = "top.net";
    info.kind = vpiNet;
    info.type = {1, true, false, false};
    info.binding.slot = {slot, 0, 0, 1};
    info.binding.isSlot = true;
    info.binding.net = true;
    info.binding.netWritable = grant;
    return context.vpi().object(std::move(info));
}
TEST(VpiNet, PhysicalRegistrationRequiresCapabilityAndContextOwnership) {
    Capture out;
    k::Context rejected({out.file, out.file}), allowed({out.file, out.file});
    const auto readonly = physicalNet(rejected, false);
    const auto writable = physicalNet(allowed, true);
    rejected.vpi().beginRuntime();
    allowed.vpi().beginRuntime();
    {
        k::Vpi::Entry enter(rejected);
        put(readonly, 1, vpiForceFlag);
        EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
        EXPECT_EQ(get(readonly), 0);
        EXPECT_FALSE(rejected.strengthDrivers().forced(0).has_value());
        put(writable, 1, vpiForceFlag);
        EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
    }
    {
        k::Vpi::Entry enter(allowed);
        EXPECT_EQ(get(writable), 0);
        put(writable, 1, vpiForceFlag);
        EXPECT_EQ(vpi_chk_error(nullptr), 0);
        EXPECT_EQ(get(writable), 1);
        put(readonly, 1);
        EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
    }
    put(writable, 0);
    {
        k::Vpi::Entry enter(allowed);
        EXPECT_EQ(get(writable), 1);
    }
}
struct ReplacementForce {
    unsigned calls = 0;
    std::array<int, 2> snapshots{};
    bool stable = true;
    int error = 0;
};
PLI_INT32 replaceReleasedForce(p_cb_data data) {
    auto& state = *reinterpret_cast<ReplacementForce*>(data->user_data);
    const auto snapshot = data->value->value.integer;
    if (state.calls < state.snapshots.size()) state.snapshots[state.calls] = snapshot;
    ++state.calls;
    if (state.calls == 1) {
        put(data->obj, 0, vpiForceFlag);
        state.error = vpi_chk_error(nullptr);
        state.stable = data->value->value.integer == snapshot && get(data->obj) == 0;
    }
    return 0;
}
TEST(VpiNet, ReleaseCallbackReplacementKeepsItsForceOwnershipAndOwnedSnapshot) {
    Capture out;
    k::Context context({out.file, out.file});
    const auto object = physicalNet(context, true);
    context.vpi().beginRuntime();
    k::Vpi::Entry enter(context);
    put(object, 0, vpiForceFlag);
    ASSERT_EQ(vpi_chk_error(nullptr), 0);
    ASSERT_EQ(context.scheduler().driveStrength(0, {6, 6}), k::StrengthError::None);
    ASSERT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
    ReplacementForce state;
    s_vpi_value value{};
    value.format = vpiIntVal;
    s_cb_data callback{};
    callback.reason = cbValueChange;
    callback.cb_rtn = replaceReleasedForce;
    callback.obj = object;
    callback.value = &value;
    callback.user_data = reinterpret_cast<char*>(&state);
    const auto token = vpi_register_cb(&callback);
    ASSERT_NE(token, nullptr);
    vpi_put_value(object, &value, nullptr, vpiReleaseFlag);
    EXPECT_EQ(vpi_chk_error(nullptr), 0);
    EXPECT_EQ(state.error, 0);
    EXPECT_EQ(state.calls, 2U);
    EXPECT_EQ(state.snapshots, (std::array<int, 2>{1, 0}));
    EXPECT_TRUE(state.stable);
    EXPECT_EQ(value.value.integer, 0);
    EXPECT_EQ(get(object), 0);
    EXPECT_TRUE(context.scheduler().holdsAny(k::Hold::Force, k::ProcessId{UINT32_MAX - 1}));
    ASSERT_EQ(vpi_remove_cb(token), 1);
    vpi_put_value(object, nullptr, nullptr, vpiReleaseFlag);
    EXPECT_EQ(vpi_chk_error(nullptr), 0);
    EXPECT_EQ(get(object), 1);
    EXPECT_FALSE(context.scheduler().holdsAny(k::Hold::Force, k::ProcessId{UINT32_MAX - 1}));
}
}  // namespace

namespace {
struct NaturalCase {
    std::uint32_t width;
    int kind, format;
    bool scalar = false, real = false;
};
constexpr std::array naturalCases{
    NaturalCase{1, vpiReg, vpiScalarVal, true},
    NaturalCase{1, vpiReg, vpiVectorVal},
    NaturalCase{33, vpiReg, vpiVectorVal},
    NaturalCase{129, vpiReg, vpiVectorVal},
    NaturalCase{32, vpiIntegerVar, vpiIntVal},
    NaturalCase{64, vpiRealVar, vpiRealVal, false, true},
};
vpiHandle naturalObject(k::Context& context, const NaturalCase& test) {
    k::VpiObjectInfo info;
    info.name = "top.natural";
    info.kind = test.kind;
    info.scalar = test.scalar;
    info.type = {test.width, !test.real, test.kind == vpiIntegerVar, test.real};
    info.left = test.width - 1;
    info.binding.isSlot = true;
    info.binding.writable = true;
    info.binding.slot = {context.store().add(test.real ? k::Domain::Real : k::Domain::Four,
                                             test.width, 1, k::Init::Zero),
                         0, 0, test.width};
    return context.vpi().object(std::move(info));
}
void expectNatural(const s_vpi_value& value, const NaturalCase& test, int expected) {
    ASSERT_EQ(value.format, test.format);
    switch (test.format) {
        case vpiScalarVal: EXPECT_EQ(value.value.scalar, expected ? vpi1 : vpi0); break;
        case vpiIntVal: EXPECT_EQ(value.value.integer, expected); break;
        case vpiRealVal: EXPECT_DOUBLE_EQ(value.value.real, expected); break;
        case vpiVectorVal:
            ASSERT_NE(value.value.vector, nullptr);
            for (unsigned word = 0; word < (test.width + 31) / 32; ++word) {
                EXPECT_EQ(value.value.vector[word].aval, word == 0 ? expected : 0);
                EXPECT_EQ(value.value.vector[word].bval, 0);
            }
            break;
        default: FAIL() << "unexpected oracle format";
    }
}
TEST(Vpi, ObjectTypeReadsUseDeclaredScalarIntegerRealAndVectorFormats) {
    for (const auto& test : naturalCases) {
        SCOPED_TRACE(::testing::Message() << test.width << ' ' << test.format);
        Capture out;
        k::Context context({out.file, out.file});
        const auto object = naturalObject(context, test);
        context.vpi().beginRuntime();
        k::Vpi::Entry enter(context);
        put(object, 1);
        s_vpi_value value{};
        value.format = vpiObjTypeVal;
        vpi_get_value(object, &value);
        expectNatural(value, test, 1);
        EXPECT_EQ(vpi_chk_error(nullptr), 0);
        if (test.kind == vpiReg) {
            EXPECT_EQ(vpi_get(vpiScalar, object), int(test.scalar));
            EXPECT_EQ(vpi_get(vpiVector, object), int(!test.scalar));
        }
        value.format = vpiVectorVal;
        vpi_get_value(object, &value);
        ASSERT_EQ(value.format, vpiVectorVal);
        ASSERT_NE(value.value.vector, nullptr);
        EXPECT_EQ(value.value.vector[0].aval, 1);
    }
}
TEST(Vpi, ObjectTypeReleaseUsesTheSameDeclaredFormat) {
    for (const auto& test : naturalCases) {
        Capture out;
        k::Context context({out.file, out.file});
        const auto object = naturalObject(context, test);
        context.vpi().beginRuntime();
        k::Vpi::Entry enter(context);
        put(object, 1, vpiForceFlag);
        s_vpi_value value{};
        value.format = vpiObjTypeVal;
        vpi_put_value(object, &value, nullptr, vpiReleaseFlag);
        expectNatural(value, test, 1);
        EXPECT_EQ(vpi_chk_error(nullptr), 0);
    }
}
struct NaturalCallback {
    NaturalCase test;
    unsigned calls = 0;
    int expected = 0;
};
PLI_INT32 naturalCallback(p_cb_data data) {
    auto& state = *reinterpret_cast<NaturalCallback*>(data->user_data);
    ++state.calls;
    EXPECT_NE(data->value, nullptr);
    if (data->value) expectNatural(*data->value, state.test, state.expected);
    // A nested API read must not replace the callback's owned vector storage.
    s_vpi_value nested{};
    nested.format = vpiVectorVal;
    vpi_get_value(data->obj, &nested);
    if (data->value) expectNatural(*data->value, state.test, state.expected);
    return 0;
}
TEST(Vpi, ObjectTypeCallbacksOwnFormatsAndValuesAcrossNestedReads) {
    for (const auto& test : naturalCases) {
        Capture out;
        k::Context context({out.file, out.file});
        const auto object = naturalObject(context, test);
        context.vpi().beginRuntime();
        k::Vpi::Entry enter(context);
        NaturalCallback state{test};
        s_vpi_value value{};
        value.format = vpiObjTypeVal;
        s_cb_data callback{};
        callback.reason = cbValueChange;
        callback.cb_rtn = naturalCallback;
        callback.obj = object;
        callback.value = &value;
        callback.user_data = reinterpret_cast<char*>(&state);
        const auto token = vpi_register_cb(&callback);
        ASSERT_NE(token, nullptr);
        state.expected = 1;
        put(object, 1);
        state.expected = 0;
        put(object, 0);
        EXPECT_EQ(state.calls, 2U);
        EXPECT_EQ(value.format, vpiObjTypeVal);
        EXPECT_EQ(vpi_remove_cb(token), 1);
        EXPECT_EQ(vpi_chk_error(nullptr), 0);
    }
}
TEST(Vpi, ObjectTypeScalarRetainsUnknownsAndIntegerRetainsSign) {
    Capture out;
    k::Context context({out.file, out.file});
    const auto scalar = naturalObject(context, naturalCases[0]);
    const auto integer = naturalObject(context, naturalCases[4]);
    context.vpi().beginRuntime();
    k::Vpi::Entry enter(context);
    for (const auto symbol : {vpi0, vpi1, vpiX, vpiZ}) {
        s_vpi_value value{};
        value.format = vpiScalarVal;
        value.value.scalar = symbol;
        vpi_put_value(scalar, &value, nullptr, vpiNoDelay);
        value.format = vpiObjTypeVal;
        vpi_get_value(scalar, &value);
        ASSERT_EQ(value.format, vpiScalarVal);
        EXPECT_EQ(value.value.scalar, symbol);
    }
    put(integer, -3);
    s_vpi_value value{};
    value.format = vpiObjTypeVal;
    vpi_get_value(integer, &value);
    ASSERT_EQ(value.format, vpiIntVal);
    EXPECT_EQ(value.value.integer, -3);
    value.format = vpiObjTypeVal;
    vpi_get_value(nullptr, &value);
    EXPECT_EQ(value.format, vpiSuppressVal);
    EXPECT_EQ(vpi_chk_error(nullptr), vpiError);
}
TEST(Vpi, ObjectTypePackedIndexIsScalarEvenWhenParentIsVector) {
    Capture out;
    k::Context context({out.file, out.file});
    const auto parent = naturalObject(context, naturalCases[2]);
    context.vpi().beginRuntime();
    k::Vpi::Entry enter(context);
    const auto bit = vpi_handle_by_index(parent, 32);
    ASSERT_NE(bit, nullptr);
    put(parent, 0);
    s_vpi_value value{};
    value.format = vpiObjTypeVal;
    vpi_get_value(bit, &value);
    EXPECT_EQ(value.format, vpiScalarVal);
    EXPECT_EQ(value.value.scalar, vpi0);
    EXPECT_EQ(vpi_get(vpiScalar, bit), 1);
    EXPECT_EQ(vpi_get(vpiVector, parent), 1);
}
}  // namespace
