// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/wide.h"
#include "vpi_internal.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace kirimasim::kernel {
namespace {
// Approved P1 VPI contract 5 / RUNTIME_ABI 9.4 item 4.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,custom-kirimasim-global-state)
thread_local Context* currentVpiContext = nullptr;
}  // namespace
Context* vpiContext() noexcept { return currentVpiContext; }
Vpi::Entry::Entry(Context& c) noexcept : previous_(currentVpiContext) { currentVpiContext = &c; }
Vpi::Entry::~Entry() { currentVpiContext = previous_; }
Vpi::Vpi(Context& c) : context_(c) {}
Vpi::~Vpi() = default;
Vpi::Impl& Vpi::implementation() {
    if (!impl_) impl_ = std::make_unique<Impl>(context_);
    return *impl_;
}
bool Vpi::enabled() const noexcept { return impl_ != nullptr; }
Vpi::Impl::~Impl() {
    callbacks.clear();
    sites.clear();
    objects.clear();
    routines.clear();
    for (auto i = libraries.rbegin(); i != libraries.rend(); ++i)
        i->second(i->first);
}
void Vpi::Impl::fail(std::string text, std::string id) {
    message = std::move(text);
    code = std::move(id);
    errorPending = true;
}
void Vpi::reportError(std::string message) { implementation().fail(std::move(message)); }
void Vpi::Impl::registrationError(std::string text) {
    fail(std::move(text), "VPI-REGISTRATION");
    registrationMessage = message;
}
std::string Vpi::error() const {
    if (!impl_) return {};
    if (impl_->resourceFailure) return "VPI resource";
    if (!impl_->registrationMessage.empty()) return impl_->registrationMessage;
    return impl_->errorPending ? impl_->message : std::string{};
}
vpiHandle Vpi::Impl::add(Object object) {
    if (objects.size() >= std::numeric_limits<std::uint32_t>::max() - 1)
        throw std::length_error("VPI object identifiers exhausted");
    objects.push_back(std::make_unique<Object>(std::move(object)));
    return reinterpret_cast<vpiHandle>(objects.back().get());
}
Vpi::Impl::Object* Vpi::Impl::get(vpiHandle handle) {
    const auto found = std::find_if(objects.begin(), objects.end(), [&](const auto& object) {
        return reinterpret_cast<vpiHandle>(object.get()) == handle;
    });
    if (found == objects.end() || !(*found)->live) {
        fail("invalid or released VPI handle");
        return nullptr;
    }
    return found->get();
}
const VpiArgument* Vpi::Impl::argument(Object& object) {
    for (auto i = stack.rbegin(); i != stack.rend(); ++i)
        if ((*i)->site == object.site && object.argument < (*i)->arguments.size())
            return &(*i)->arguments[object.argument];
    if (object.site < sites.size() && object.argument < sites[object.site].arguments.size())
        return &sites[object.site].arguments[object.argument];
    fail("argument outside its VPI call");
    return nullptr;
}
VpiObjectInfo Vpi::Impl::info(vpiHandle handle) {
    auto* object = get(handle);
    if (!object) return {};
    auto result = object->info;
    if (object->kind == Kind::Argument) {
        const auto* arg = argument(*object);
        if (!arg) return {};
        if (arg->binding.isSlot)
            for (const auto& candidate : objects)
                if (candidate->kind == Kind::Object && candidate->info.name == arg->name) {
                    result = candidate->info;
                    break;
                }
        result.type = arg->value.type;
        result.binding = arg->binding;
        if (!result.dimensions.empty()) {
            result.dimensions.clear();
            result.kind = vpiMemoryWord;
        }

        if (!arg->name.empty()) result.name = arg->name;
        if (result.kind != vpiMemoryWord)
            result.kind = arg->name.empty()              ? vpiConstant
                          : arg->value.type.real         ? vpiRealVar
                          : arg->binding.net             ? vpiNet
                          : result.kind == vpiIntegerVar ? vpiIntegerVar
                                                         : vpiReg;
    }
    return result;
}
VpiValue Vpi::Impl::read(const VpiObjectInfo& info) {
    if (!info.dimensions.empty()) {
        fail("VPI memory value requires an element handle");
        return {};
    }
    VpiValue out{info.type, std::vector<W4>(wide::chunks(info.type.width))};
    const auto& binding = info.binding;
    if (binding.isSlot) {
        if (binding.slot.slot.index() >= context.store().size()) {
            fail("VPI slot unavailable in this phase");
            return out;
        }
        const auto& layout = context.store().layout(binding.slot.slot);
        if (layout.domain == Domain::Handle || binding.slot.elem >= layout.elements ||
            binding.slot.lsb > layout.width || info.type.width > layout.width - binding.slot.lsb) {
            fail("VPI selection is outside its object");
            return out;
        }
        const auto* words = context.store().element(binding.slot.slot, binding.slot.elem);
        const std::size_t stride = layout.domain == Domain::Four ? 2 : 1;
        for (std::uint32_t bit = 0; bit < info.type.width; ++bit) {
            const auto b = bit + binding.slot.lsb;
            out.words[bit / 64].a |= ((words[(b / 64) * stride] >> (b % 64)) & 1U) << (bit % 64);
            if (stride == 2)
                out.words[bit / 64].b |= ((words[(b / 64) * stride + 1] >> (b % 64)) & 1U)
                                         << (bit % 64);
        }
    } else if (binding.read) {
        if (!binding.read(binding, out.words)) {
            fail("expired VPI argument binding");
            return {};
        }
    } else {
        fail("VPI object has no live value binding");
        return {};
    }
    return out;
}
VpiValue Vpi::Impl::read(vpiHandle handle) {
    auto* object = get(handle);
    if (!object) return {};
    if (object->kind == Kind::Constant)
        return {{32, false, true, false}, {{static_cast<std::uint64_t>(object->constant), 0}}};
    if (object->kind == Kind::Argument) {
        const auto* arg = argument(*object);
        if (!arg) return {};
        if (arg->binding.isSlot || arg->binding.read) return read(info(handle));
        if (!arg->available) {
            fail("VPI expression value is unavailable during compilation");
            return {};
        }
        return arg->value;
    }
    if (object->kind == Kind::Call) {
        for (auto i = stack.rbegin(); i != stack.rend(); ++i)
            if ((*i)->site == object->site) return (*i)->result;
        fail("VPI call value outside its invocation");
        return {};
    }
    return read(object->info);
}
bool Vpi::Impl::write(const VpiObjectInfo& target, const VpiValue& value, int flag) {
    if (!target.dimensions.empty()) {
        fail("VPI memory write requires an element handle");
        return false;
    }
    if (!runtime) {
        fail("VPI writes are not allowed during compilation");
        return false;
    }
    if (context.scheduler().inPostponed()) {
        fail("VPI write in the read-only region");
        context.scheduler().runtimeError();
        return false;
    }
    const auto& binding = target.binding;
    if (!binding.writable || binding.net) {
        fail("VPI write to readonly or unsupported net target");
        return false;
    }
    if (binding.isSlot) {
        auto& scheduler = context.scheduler();
        const auto& at = binding.slot;
        if (at.slot.index() >= context.store().size()) {
            fail("invalid VPI slot target");
            return false;
        }
        const auto& layout = context.store().layout(at.slot);
        if (layout.domain == Domain::Handle || at.elem >= layout.elements ||
            at.lsb > layout.width || value.type.width > layout.width - at.lsb) {
            fail("VPI write is outside its object");
            return false;
        }
        std::vector<std::uint64_t> words;
        words.reserve(value.words.size() * (layout.domain == Domain::Four ? 2 : 1));
        for (const auto w : value.words) {
            words.push_back(w.a);
            if (layout.domain == Domain::Four) words.push_back(w.b);
        }
        if (flag == vpiNoDelay)
            scheduler.write(at, words.data());
        else if (flag == vpiForceFlag || flag == vpiReleaseFlag) {
            if (at.lsb != 0 || (at.width != 0 && at.width != layout.width)) {
                fail("partial variable VPI force is unsupported");
                return false;
            }
            scheduler.vpiForce(at, words.data(), flag == vpiReleaseFlag);
        } else {
            fail("unsupported VPI write flag");
            return false;
        }
        return !scheduler.ending();
    }
    if (flag != vpiNoDelay || !binding.write) {
        fail("unsupported write to transient VPI binding");
        return false;
    }
    if (!binding.write(binding, value.words)) {
        fail("expired or readonly VPI binding");
        return false;
    }
    return true;
}
void Vpi::ownLibrary(void* library, void (*close)(void*)) {
    implementation().libraries.emplace_back(library, close);
}
bool Vpi::startup(void (**routines)()) {
    linkApi();
    auto& host = implementation();
    Entry entry(context_);
    try {
        for (auto* p = routines; *p; ++p)
            (*p)();
    } catch (...) {
        host.fail("exception from VPI startup");
    }
    return host.registrationMessage.empty() && !host.errorPending && !host.resourceFailure;
}
std::vector<VpiRoutine> Vpi::routines() const {
    std::vector<VpiRoutine> out;
    if (impl_)
        for (const auto& routine : impl_->routines)
            out.push_back(routine.description);
    return out;
}
void Vpi::configure(std::vector<std::string> argv, std::string version, int precision) {
    auto& host = implementation();
    host.argv = std::move(argv);
    host.version = std::move(version);
    host.precision = precision;
    host.argvPointers.clear();
    for (auto& arg : host.argv)
        host.argvPointers.push_back(arg.data());
}
vpiHandle Vpi::object(VpiObjectInfo info) {
    Impl::Object row;
    row.info = std::move(info);
    return implementation().add(std::move(row));
}
bool Vpi::compileCall(std::uint32_t site, std::uint32_t routine, std::string scope,
                      std::span<const VpiArgument> arguments, VpiType result, int unit,
                      int precision, std::string file, std::uint32_t line) {
    auto& host = implementation();
    if (routine >= host.routines.size()) {
        host.fail("invalid VPI routine ID");
        return false;
    }
    if (host.sites.size() <= site) host.sites.resize(std::size_t{site} + 1);
    auto& row = host.sites[site];
    if (row.handle) return true;
    if (host.routines[routine].description.result != result) {
        host.fail("VPI call result differs from registered signature");
        return false;
    }
    row.routine = routine;
    row.arguments.assign(arguments.begin(), arguments.end());
    row.result = result;
    Impl::Object call;
    call.kind = Impl::Kind::Call;
    call.site = site;
    call.info.name = std::move(scope);
    call.info.type = result;
    call.info.scopeName = call.info.name;
    call.info.parentName = call.info.name;
    call.info.unit = unit;
    call.info.precision = precision;
    call.info.file = std::move(file);
    call.info.line = line;
    call.info.kind = host.routines[routine].description.task ? vpiSysTaskCall : vpiSysFuncCall;
    row.handle = host.add(std::move(call));
    for (std::uint32_t i = 0; i < arguments.size(); ++i) {
        Impl::Object arg;
        arg.kind = Impl::Kind::Argument;
        arg.site = site;
        arg.argument = i;
        arg.info.type = arguments[i].value.type;
        arg.info.unit = unit;
        arg.info.precision = precision;
        arg.info.scopeName = host.get(row.handle)->info.scopeName;
        row.handles.push_back(host.add(std::move(arg)));
    }
    auto& data = host.routines[routine].data;
    if (data.compiletf) {
        Impl::Invocation invocation{
            site, row.arguments, {result, std::vector<W4>(wide::chunks(result.width))}};
        host.stack.push_back(&invocation);
        Entry entry(context_);
        try {
            data.compiletf(data.user_data);
        } catch (...) {
            host.fail("exception from VPI compiletf");
        }
        host.stack.pop_back();
    }
    return host.registrationMessage.empty() && !host.errorPending && !host.resourceFailure;
}
VpiValue Vpi::call(std::uint32_t site, std::span<const VpiArgument> arguments) {
    auto& host = implementation();
    if (site >= host.sites.size() || !host.sites[site].handle) {
        host.fail("uncompiled VPI call site");
        return {};
    }
    const auto routine = host.sites[site].routine;
    const auto result = host.sites[site].result;
    Impl::Invocation invocation{
        site, arguments, {result, std::vector<W4>(wide::chunks(result.width))}};
    const auto control = host.controlSerial;
    host.stack.push_back(&invocation);
    Entry entry(context_);
    try {
        const auto& data = host.routines[routine].data;
        data.calltf(data.user_data);
    } catch (...) {
        host.fail("exception from VPI calltf");
        context_.scheduler().runtimeError();
    }
    host.stack.pop_back();
    host.stoppedCall = host.controlSerial != control && context_.scheduler().ending();
    return std::move(invocation.result);
}
bool Vpi::lastCallStopped() const noexcept { return impl_ && impl_->stoppedCall; }
void Vpi::detachBindings(void* owner) noexcept {
    if (!impl_) return;
    const auto detach = [&](VpiBinding& binding) {
        if (binding.owner == owner) {
            binding.owner = nullptr;
            binding.read = nullptr;
            binding.write = nullptr;
            binding.writable = false;
        }
    };
    for (auto& object : impl_->objects)
        detach(object->info.binding);
    for (auto& site : impl_->sites)
        for (auto& argument : site.arguments)
            detach(argument.binding);
    for (auto& callback : impl_->callbacks)
        detach(callback->target.binding);
    for (auto& pending : impl_->pending)
        detach(pending.target.binding);
}
void Vpi::beginRuntime() {
    if (!impl_ || impl_->runtime) return;
    auto& h = *impl_;
    h.runtime = true;
    for (std::uint32_t i = 0; i < h.callbacks.size(); ++i) {
        auto& cb = *h.callbacks[i];
        if (!cb.live) continue;
        if (cb.globalTime) cb.target.unit = h.precision;
        Region region = Region::PreActive;
        switch (cb.data.reason) {
            case cbAfterDelay: break;
            case cbReadWriteSynch: region = Region::PostNba; break;
            case cbReadOnlySynch: region = Region::Postponed; break;
            default: continue;
        }
        const auto ticks = h.delay(&cb.time, cb.target.unit);
        if (ticks) context_.scheduler().vpiSchedule(region, i, cb.generation, *ticks);
    }
}
void Vpi::start() {
    if (!impl_) return;
    impl_->runtime = true;
    const auto count = impl_->callbacks.size();
    for (std::uint32_t i = 0; i < count; ++i)
        if (impl_->callbacks[i]->data.reason == cbStartOfSimulation) dispatch(i, 1);
}
void Vpi::finish() {
    if (!impl_ || impl_->finished) return;
    impl_->finished = true;
    const auto count = impl_->callbacks.size();
    for (std::uint32_t i = 0; i < count; ++i)
        if (impl_->callbacks[i]->data.reason == cbEndOfSimulation) dispatch(i, 1);
}
bool Vpi::watches(SlotId slot) const noexcept {
    if (!impl_) return false;
    for (const auto& cb : impl_->callbacks)
        if (cb->live && cb->data.reason == cbValueChange && cb->target.binding.isSlot &&
            cb->target.binding.slot.slot == slot)
            return true;
    return false;
}
void Vpi::changed(SlotId slot, std::uint32_t element, const std::uint64_t* words) {
    if (!impl_) return;
    auto& host = *impl_;
    const auto count = host.callbacks.size();
    for (std::size_t i = 0; i < count; ++i) {
        auto& cb = *host.callbacks[i];
        if (!cb.live || cb.data.reason != cbValueChange || !cb.target.binding.isSlot ||
            cb.target.binding.slot.slot != slot || cb.target.binding.slot.elem != element)
            continue;
        auto value = host.read(cb.target);
        const auto& layout = context_.store().layout(slot);
        const std::size_t stride = layout.domain == Domain::Four ? 2U : 1U;
        for (std::uint32_t b = 0; b < value.type.width; ++b) {
            const auto at = b + cb.target.binding.slot.lsb;
            const auto mask = std::uint64_t{1} << (b % 64);
            value.words[b / 64].a = (value.words[b / 64].a & ~mask) |
                                    (((words[(at / 64) * stride] >> (at % 64)) & 1U) << (b % 64));
            value.words[b / 64].b =
                (value.words[b / 64].b & ~mask) |
                (stride == 2 ? (((words[(at / 64) * stride + 1] >> (at % 64)) & 1U) << (b % 64))
                             : 0);
        }
        if (std::equal(value.words.begin(), value.words.end(), cb.last.words.begin(),
                       cb.last.words.end(), [](W4 a, W4 b) { return a.a == b.a && a.b == b.b; }))
            continue;
        cb.last = std::move(value);
        host.invoke(cb);
    }
}
bool Vpi::scheduledLive(std::uint32_t id, std::uint64_t generation) const noexcept {
    if (!impl_) return false;
    if (id >= 0x80000000U) {
        const auto i = id - 0x80000000U;
        return i < impl_->pending.size() && impl_->pending[i].live &&
               impl_->pending[i].generation == generation;
    }
    return id < impl_->callbacks.size() && impl_->callbacks[id]->live &&
           impl_->callbacks[id]->generation == generation;
}
void Vpi::nextTime() {
    if (!impl_) return;
    for (std::uint32_t i = 0; i < impl_->callbacks.size(); ++i)
        if (impl_->callbacks[i]->live && impl_->callbacks[i]->data.reason == cbNextSimTime)
            context_.scheduler().vpiSchedule(Region::PreActive, i, impl_->callbacks[i]->generation,
                                             0);
}
void Vpi::dispatch(std::uint32_t id, std::uint64_t generation) {
    if (!impl_) return;
    auto& host = *impl_;
    if (id >= 0x80000000U) {
        const auto i = id - 0x80000000U;
        if (i < host.pending.size()) {
            auto& p = host.pending[i];
            if (p.live && p.generation == generation) {
                p.live = false;
                auto update = std::move(p);
                host.write(update.target, update.value, vpiNoDelay);
            }
        }
        return;
    }
    if (id >= host.callbacks.size()) return;
    auto& cb = *host.callbacks[id];
    if (!cb.live || cb.generation != generation) return;
    if (cb.data.reason != cbValueChange) cb.live = false;
    host.invoke(cb);
}
void Vpi::Impl::invoke(Callback& cb) {
    if (cb.data.reason == cbValueChange || cb.data.reason == cbStartOfSimulation ||
        cb.data.reason == cbEndOfSimulation) {
        const auto region = cb.data.reason == cbStartOfSimulation ? Region::PreActive
                            : cb.data.reason == cbEndOfSimulation
                                ? Region::Postponed
                                : context.scheduler().executingRegion();
        if (!context.scheduler().vpiCallbackPoint(region, cb.data.reason == cbEndOfSimulation))
            return;
    }

    auto data = cb.data;
    auto time = cb.time;
    auto value = cb.value;
    if (data.time) {
        time.high = static_cast<std::uint32_t>(context.scheduler().time() >> 32);
        time.low = static_cast<std::uint32_t>(context.scheduler().time());
        if (time.type == vpiScaledRealTime)
            time.real = static_cast<double>(context.scheduler().time()) *
                        std::pow(10.0, precision - cb.target.unit);
        data.time = &time;
    }
    if (data.value) {
        if (value.format != vpiSuppressVal) {
            const auto snapshot = cb.data.reason == cbValueChange ? cb.last : read(cb.target);
            if (errorPending || !convert(&value, snapshot, cb.vector, cb.text))
                value.format = vpiSuppressVal;
        }
        data.value = &value;
    }
    Entry entry(context);
    try {
        data.cb_rtn(&data);
    } catch (...) {
        fail("exception from VPI callback");
        context.scheduler().runtimeError();
    }
}
}  // namespace kirimasim::kernel
