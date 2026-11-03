// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/vpi.h"
#include <deque>
#include <optional>
#include <string_view>
#include <utility>

namespace kirimasim::kernel {
Context* vpiContext() noexcept;
struct Vpi::Impl {
    enum class Kind { Object, Bound, Routine, Call, Argument, Iterator, Callback, Constant };
    struct Object {
        Kind kind = Kind::Object;
        VpiObjectInfo info;
        bool live = true;
        std::uint32_t site = 0, argument = 0;
        std::vector<vpiHandle> items;
        std::size_t cursor = 0;
        std::int64_t constant = 0;
    };
    struct Routine {
        s_vpi_systf_data data{};
        VpiRoutine description;
    };
    struct Site {
        vpiHandle handle = nullptr;
        std::uint32_t routine = 0;
        std::vector<VpiArgument> arguments;
        std::vector<vpiHandle> handles;
        VpiType result;
    };
    struct Invocation {
        std::uint32_t site;
        std::span<const VpiArgument> arguments;
        VpiValue result;
    };
    struct Callback {
        s_cb_data data{};
        s_vpi_time time{};
        s_vpi_value value{};
        VpiObjectInfo target;
        VpiValue last;
        std::vector<s_vpi_vecval> vector;
        std::string text;
        bool live = true, globalTime = false;
        std::uint64_t generation = 1;
    };
    struct Pending {
        VpiObjectInfo target;
        VpiValue value;
        bool live = true;
        std::uint64_t generation = 1;
    };
    Context& context;
    std::vector<std::pair<void*, void (*)(void*)>> libraries;
    std::vector<std::unique_ptr<Object>> objects;
    std::deque<Routine> routines;
    std::vector<Site> sites;
    std::vector<Invocation*> stack;
    std::vector<std::unique_ptr<Callback>> callbacks;
    std::vector<Pending> pending;
    std::vector<std::string> argv;
    std::vector<char*> argvPointers;
    std::string version, message, code, scratch, registrationMessage;
    bool errorPending = false, resourceFailure = false;
    std::vector<s_vpi_vecval> vector;
    int precision = -9;
    bool runtime = false, finished = false, stoppedCall = false;
    std::uint64_t controlSerial = 0;
    explicit Impl(Context& c) : context(c) {}
    ~Impl();
    void fail(std::string text, std::string id = "VPI-INVALID");
    void registrationError(std::string);
    vpiHandle add(Object);
    Object* get(vpiHandle);
    VpiObjectInfo info(vpiHandle);
    const VpiArgument* argument(Object&);
    VpiValue read(vpiHandle);
    VpiValue read(const VpiObjectInfo&);
    bool write(const VpiObjectInfo&, const VpiValue&, int flag);
    bool convert(p_vpi_value, const VpiValue&, std::vector<s_vpi_vecval>&, std::string&,
                 const VpiObjectInfo&);
    std::optional<VpiValue> decode(p_vpi_value, VpiType);
    std::optional<std::uint64_t> delay(p_vpi_time, int unit);
    void invoke(Callback&);
};
}  // namespace kirimasim::kernel
