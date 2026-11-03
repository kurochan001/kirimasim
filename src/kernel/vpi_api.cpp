// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/wide.h"
#include "vpi_internal.h"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>

namespace {
using Host = kirimasim::kernel::Vpi::Impl;
using Kind = Host::Kind;
Host* host() noexcept {
    auto* c = kirimasim::kernel::vpiContext();
    if (!c) return nullptr;
    try {
        return &c->vpi().implementation();
    } catch (const std::bad_alloc&) {
        c->scheduler().resourceLimit();
    } catch (const std::length_error&) {
        c->scheduler().resourceLimit();
    } catch (...) {
        c->scheduler().requestEnd(kirimasim::kernel::EndReason::Internal, 1);
    }
    return nullptr;
}
template <class Result, class F>
Result checked(Result failed, F function, bool clear = true) noexcept {
    auto* h = host();
    if (!h) return failed;
    if (clear) {
        h->errorPending = false;
        h->message.clear();
        h->code.clear();
    }
    try {
        return function(*h);
    } catch (const std::bad_alloc&) {
        h->resourceFailure = true;
        h->context.scheduler().resourceLimit();
        return failed;
    } catch (const std::length_error&) {
        h->resourceFailure = true;
        h->context.scheduler().resourceLimit();
        return failed;
    } catch (...) {
        h->context.scheduler().requestEnd(kirimasim::kernel::EndReason::Internal, 1);
        return failed;
    }
}
}  // namespace
extern "C" vpiHandle vpi_register_systf(const s_vpi_systf_data* data) {
    return checked<vpiHandle>(nullptr, [&](Host& h) {
        if (h.runtime || !data || !data->tfname || data->tfname[0] != '$' || !data->calltf) {
            h.registrationError("invalid VPI system routine registration");
            return vpiHandle{};
        }
        const std::string_view name = data->tfname;
        if (std::any_of(h.routines.begin(), h.routines.end(),
                        [&](const auto& r) { return r.description.name == name; })) {
            h.registrationError("duplicate VPI system routine");
            return vpiHandle{};
        }
        Host::Routine routine;
        routine.data = *data;
        routine.description.name = name;
        routine.description.task = data->type == vpiSysTask;
        if (data->type != vpiSysTask && data->type != vpiSysFunc) {
            h.registrationError("invalid VPI routine kind");
            return vpiHandle{};
        }
        if (!routine.description.task) {
            auto& type = routine.description.result;
            type.width = 32;
            type.signedValue = true;
            type.four = true;
            switch (data->sysfunctype) {
                case vpiIntFunc: break;
                case vpiRealFunc: type = {64, false, false, true}; break;
                case vpiSizedFunc:
                case vpiSizedSignedFunc: {
                    if (!data->sizetf) {
                        h.registrationError("sized VPI function needs sizetf");
                        return vpiHandle{};
                    }
                    PLI_INT32 width = 0;
                    try {
                        width = data->sizetf(data->user_data);
                    } catch (...) {
                        h.registrationError("exception from VPI sizetf");
                        return vpiHandle{};
                    }
                    if (width <= 0) {
                        h.registrationError("invalid VPI function width");
                        return vpiHandle{};
                    }
                    type.width = static_cast<std::uint32_t>(width);
                    type.signedValue = data->sysfunctype == vpiSizedSignedFunc;
                    break;
                }
                default:
                    h.registrationError("unsupported VPI function result kind");
                    return vpiHandle{};
            }
        }
        const auto id = static_cast<std::uint32_t>(h.routines.size());
        h.routines.push_back(std::move(routine));
        h.routines.back().data.tfname = h.routines.back().description.name.data();
        Host::Object object;
        object.kind = Kind::Routine;
        object.site = id;
        return h.add(std::move(object));
    });
}
extern "C" vpiHandle vpi_handle_by_name(PLI_BYTE8* name, vpiHandle scope) {
    return checked<vpiHandle>(nullptr, [&](Host& h) {
        if (!name) {
            h.fail("missing VPI object name");
            return vpiHandle{};
        }
        std::string path = name;
        if (scope) {
            if (!h.get(scope)) return vpiHandle{};
            const auto prefix = h.info(scope).name;
            if (!prefix.empty()) path = prefix + "." + path;
        }
        for (const auto& object : h.objects)
            if (object->live && (object->kind == Kind::Object || object->kind == Kind::Bound) &&
                object->info.name == path)
                return reinterpret_cast<vpiHandle>(object.get());
        // Resolve declared array indices only after exact (including escaped) names.
        const auto bracket = path.rfind('[');
        if (bracket != std::string::npos && path.back() == ']') {
            const auto digits =
                std::string_view(path).substr(bracket + 1, path.size() - bracket - 2);
            PLI_INT32 index = 0;
            const auto parsed =
                std::from_chars(digits.data(), digits.data() + digits.size(), index);
            if (parsed.ec == std::errc{} && parsed.ptr == digits.data() + digits.size()) {
                path.resize(bracket);
                const auto base = vpi_handle_by_name(path.data(), nullptr);
                if (base) return vpi_handle_by_index(base, index);
            }
        }
        return vpiHandle{};
    });
}
extern "C" vpiHandle vpi_handle(PLI_INT32 relation, vpiHandle reference) {
    return checked<vpiHandle>(nullptr, [&](Host& h) {
        if (relation == vpiSysTfCall) {
            if (h.stack.empty()) {
                h.fail("no current VPI system call");
                return vpiHandle{};
            }
            return h.sites[h.stack.back()->site].handle;
        }
        if (relation == vpiLeftRange || relation == vpiRightRange || relation == vpiIndex) {
            if (!h.get(reference)) return vpiHandle{};
            const auto info = h.info(reference);
            if (relation == vpiIndex && !info.indexed) {
                h.fail("VPI index requires an element handle");
                return vpiHandle{};
            }
            Host::Object constant;
            constant.kind = Kind::Constant;
            constant.info.kind = vpiConstant;
            constant.info.type = {32, false, true, false};
            constant.constant =
                relation == vpiIndex ? info.index
                : relation == vpiLeftRange
                    ? (info.dimensions.empty() ? info.left : info.dimensions.front().first)
                    : (info.dimensions.empty() ? info.right : info.dimensions.front().second);
            return h.add(std::move(constant));
        }
        if (relation != vpiScope && relation != vpiParent) {
            h.fail("unsupported VPI handle relation");
            return vpiHandle{};
        }
        auto* object = h.get(reference);
        if (!object) return vpiHandle{};
        auto name = h.info(reference).name;
        const auto metadata = h.info(reference);
        if (relation == vpiParent && !metadata.parentName.empty())
            name = metadata.parentName;
        else if (!metadata.scopeName.empty())
            name = metadata.scopeName;
        else if (object->kind != Kind::Call) {
            const auto dot = name.rfind('.');
            if (dot == std::string::npos) return vpiHandle{};
            name.resize(dot);
        }
        return vpi_handle_by_name(name.data(), nullptr);
    });
}
extern "C" vpiHandle vpi_iterate(PLI_INT32 kind, vpiHandle parent) {
    return checked<vpiHandle>(nullptr, [&](Host& h) {
        Host::Object iterator;
        iterator.kind = Kind::Iterator;
        iterator.info.kind = vpiIterator;
        if (kind == vpiMemoryWord) {
            if (!h.get(parent)) return vpiHandle{};
            const auto info = h.info(parent);
            if (info.dimensions.size() != 1) {
                h.fail("memory-word iteration requires a one-dimensional memory");
                return vpiHandle{};
            }
            const auto [left, right] = info.dimensions.front();
            for (auto i = left;; i += left <= right ? 1 : -1) {
                iterator.items.push_back(vpi_handle_by_index(parent, static_cast<PLI_INT32>(i)));
                if (i == right) break;
            }
        } else if (kind == vpiArgument) {
            auto* call = h.get(parent);
            if (!call || call->kind != Kind::Call) {
                h.fail("VPI arguments need a call handle");
                return vpiHandle{};
            }
            iterator.items = h.sites[call->site].handles;
        } else {
            switch (kind) {
                case vpiModule:
                case vpiNet:
                case vpiReg:
                case vpiIntegerVar:
                case vpiRealVar:
                case vpiMemory:
                case vpiNamedBegin:
                case vpiNamedFork:
                case vpiTask:
                case vpiFunction:
                case vpiGenScope: break;
                default: h.fail("unsupported VPI iterator kind"); return vpiHandle{};
            }
            std::string ownerName;
            if (parent) {
                if (!h.get(parent)) return vpiHandle{};
                ownerName = h.info(parent).name;
            }
            for (std::size_t i = 0; i < h.objects.size(); ++i) {
                const auto& o = *h.objects[i];
                if (!o.live || o.kind != Kind::Object || o.info.kind != kind) continue;
                if (o.info.scopeName == ownerName)
                    iterator.items.push_back(reinterpret_cast<vpiHandle>(h.objects[i].get()));
            }
        }
        return iterator.items.empty() ? vpiHandle{} : h.add(std::move(iterator));
    });
}
extern "C" vpiHandle vpi_scan(vpiHandle iterator) {
    return checked<vpiHandle>(nullptr, [&](Host& h) {
        auto* object = h.get(iterator);
        if (!object || object->kind != Kind::Iterator) {
            h.fail("VPI scan needs an iterator");
            return vpiHandle{};
        }
        if (object->cursor == object->items.size()) {
            object->live = false;
            object->items.clear();
            return vpiHandle{};
        }
        return object->items[object->cursor++];
    });
}
extern "C" PLI_INT32 vpi_free_object(vpiHandle handle) {
    return checked<PLI_INT32>(0, [&](Host& h) {
        auto* object = h.get(handle);
        if (!object) return 0;
        if (object->kind == Kind::Iterator) {
            object->live = false;
            object->items.clear();
        }
        return 1;
    });
}
extern "C" vpiHandle vpi_handle_by_index(vpiHandle handle, PLI_INT32 index) {
    return checked<vpiHandle>(nullptr, [&](Host& h) {
        if (!h.get(handle)) return vpiHandle{};
        auto info = h.info(handle);
        if (info.dimensions.empty()) {
            if (info.type.real || (!info.binding.isSlot && !info.binding.read) ||
                static_cast<std::uint64_t>(std::abs(info.left - info.right)) + 1 !=
                    info.type.width) {
                h.fail("unsupported VPI packed index target");
                return vpiHandle{};
            }
            if (index < std::min(info.left, info.right) || index > std::max(info.left, info.right))
                return vpiHandle{};
            info.parentName = info.name;
            info.name += "[" + std::to_string(index) + "]";
            info.index = index;
            info.indexed = true;
            info.leafName += "[" + std::to_string(index) + "]";
            info.binding.slot.lsb += static_cast<std::uint32_t>(
                info.left >= info.right ? index - info.right : info.right - index);
            info.binding.slot.width = 1;
            info.binding.netWritable = false;
            info.type.width = 1;
            info.scalar = true;
            info.kind = info.binding.net ? vpiNetBit : vpiRegBit;
            info.left = info.right = 0;
            Host::Object object;
            object.kind = Kind::Bound;
            object.info = std::move(info);
            return h.add(std::move(object));
        }
        const auto [left, right] = info.dimensions.front();
        if (index < std::min(left, right) || index > std::max(left, right)) return vpiHandle{};
        std::uint64_t stride = 1;
        for (std::size_t i = 1; i < info.dimensions.size(); ++i)
            stride *= static_cast<std::uint64_t>(
                          std::abs(info.dimensions[i].first - info.dimensions[i].second)) +
                      1;
        info.binding.slot.elem +=
            static_cast<std::uint32_t>((left <= right ? index - left : left - index) * stride);
        info.dimensions.erase(info.dimensions.begin());
        info.binding.netWritable = false;
        info.kind = info.dimensions.empty() ? vpiMemoryWord : vpiMemory;
        info.parentName = info.name;
        info.name += "[" + std::to_string(index) + "]";
        info.index = index;
        info.indexed = true;
        info.leafName += "[" + std::to_string(index) + "]";
        for (std::size_t i = 0; i < h.objects.size(); ++i)
            if (h.objects[i]->live &&
                (h.objects[i]->kind == Kind::Object || h.objects[i]->kind == Kind::Bound) &&
                h.objects[i]->info.name == info.name)
                return reinterpret_cast<vpiHandle>(h.objects[i].get());
        Host::Object object;
        object.kind = Kind::Bound;
        object.info = std::move(info);
        return h.add(std::move(object));
    });
}
extern "C" PLI_INT32 vpi_get(PLI_INT32 property, vpiHandle handle) {
    return checked<PLI_INT32>(0, [&](Host& h) {
        auto* object = h.get(handle);
        if (!object) return 0;
        if (object->kind == Kind::Routine || object->kind == Kind::Callback) {
            h.fail("properties of VPI registration handles are unsupported");
            return 0;
        }
        const auto info = h.info(handle);
        switch (property) {
            case vpiType: return info.kind;
            case vpiSize: {
                if (!info.dimensions.empty()) {
                    const auto [l, r] = info.dimensions.front();
                    return static_cast<int>(std::abs(l - r) + 1);
                }
                return static_cast<int>(info.type.width);
            }
            case vpiSigned: return int(info.type.signedValue);
            case vpiLineNo: return static_cast<int>(info.line);
            case vpiTimeUnit: return info.unit;
            case vpiTimePrecision: return info.precision;
            case vpiScalar: return int(info.scalar);
            case vpiVector: return int(!info.scalar && info.type.width > 0);
            default: h.fail("unsupported VPI property"); return 0;
        }
    });
}
extern "C" PLI_BYTE8* vpi_get_str(PLI_INT32 property, vpiHandle handle) {
    return checked<PLI_BYTE8*>(nullptr, [&](Host& h) {
        auto* object = h.get(handle);
        if (!object) return static_cast<char*>(nullptr);
        if (object->kind == Kind::Routine || object->kind == Kind::Callback) {
            h.fail("properties of VPI registration handles are unsupported");
            return static_cast<char*>(nullptr);
        }
        const auto info = h.info(handle);
        switch (property) {
            case vpiFullName: h.scratch = info.name; break;
            case vpiName: {
                const auto dot = info.name.rfind('.');
                h.scratch = !info.leafName.empty()     ? info.leafName
                            : dot == std::string::npos ? info.name
                                                       : info.name.substr(dot + 1);
                break;
            }
            case vpiFile: h.scratch = info.file; break;
            default: h.fail("unsupported VPI string property"); return static_cast<char*>(nullptr);
        }
        return h.scratch.data();
    });
}
extern "C" void vpi_get_value(vpiHandle handle, p_vpi_value value) {
    checked(0, [&](Host& h) {
        auto input = h.read(handle);
        const auto object = !h.errorPending && value && value->format == vpiObjTypeVal
                                ? h.info(handle)
                                : kirimasim::kernel::VpiObjectInfo{};
        if ((h.errorPending || !h.convert(value, input, h.vector, h.scratch, object)) && value)
            value->format = vpiSuppressVal;
        return 0;
    });
}
extern "C" vpiHandle vpi_put_value(vpiHandle handle, p_vpi_value value, p_vpi_time time,
                                   PLI_INT32 flags) {
    return checked<vpiHandle>(nullptr, [&](Host& h) {
        auto* object = h.get(handle);
        if (!object) return vpiHandle{};
        if (object->kind == Kind::Call) {
            if (flags != vpiNoDelay) {
                h.fail("VPI function result requires no delay");
                return vpiHandle{};
            }
            auto decoded = h.decode(value, object->info.type);
            if (!decoded) return vpiHandle{};
            for (auto i = h.stack.rbegin(); i != h.stack.rend(); ++i)
                if ((*i)->site == object->site) {
                    (*i)->result = std::move(*decoded);
                    return vpiHandle{};
                }
            h.fail("VPI function result outside invocation");
            return vpiHandle{};
        }
        auto target = h.info(handle);
        auto decoded =
            flags == vpiReleaseFlag
                ? std::optional{kirimasim::kernel::VpiValue{
                      target.type, std::vector<kirimasim::kernel::W4>(
                                       kirimasim::kernel::wide::chunks(target.type.width))}}
                : h.decode(value, target.type);
        if (!decoded) return vpiHandle{};
        if (flags == vpiReleaseFlag && target.binding.net && value) {
            // Validate the requested output format without modifying caller storage.
            auto checkedValue = *value;
            const auto current = h.read(target);
            std::vector<s_vpi_vecval> probeVector;
            std::string probeText;
            if (h.errorPending ||
                !h.convert(&checkedValue, current, probeVector, probeText, target))
                return vpiHandle{};
        }
        if (flags == vpiInertialDelay || flags == vpiTransportDelay ||
            flags == vpiPureTransportDelay) {
            if (!h.runtime || !target.binding.writable || target.binding.net) {
                h.fail("unsupported delayed VPI target or phase");
                return vpiHandle{};
            }
            const auto ticks = h.delay(time, target.unit);
            if (!ticks) return vpiHandle{};
            if (flags == vpiInertialDelay)
                for (auto& p : h.pending)
                    if (p.live && p.target.binding.isSlot == target.binding.isSlot &&
                        (target.binding.isSlot
                             ? p.target.binding.slot.slot == target.binding.slot.slot
                             : p.target.binding.owner == target.binding.owner &&
                                   p.target.binding.first == target.binding.first &&
                                   p.target.binding.second == target.binding.second) &&
                        p.target.binding.slot.elem == target.binding.slot.elem &&
                        p.target.binding.slot.lsb == target.binding.slot.lsb &&
                        p.target.type == target.type) {
                        p.live = false;
                        ++p.generation;
                    }
            if (h.pending.size() >= 0x80000000U) {
                h.fail("VPI pending identifiers exhausted");
                h.context.scheduler().resourceLimit();
                return vpiHandle{};
            }
            const auto id = static_cast<std::uint32_t>(h.pending.size());
            h.pending.push_back({std::move(target), std::move(*decoded)});
            h.context.scheduler().vpiSchedule(kirimasim::kernel::Region::Active, id + 0x80000000U,
                                              1, *ticks);
            return vpiHandle{};
        }
        if (h.write(target, *decoded, flags) && flags == vpiReleaseFlag && value) {
            // 38.34 returns the released value in the caller's requested format.
            const auto released = h.read(target);
            if (h.errorPending || !h.convert(value, released, h.vector, h.scratch, target))
                value->format = vpiSuppressVal;
        }
        return vpiHandle{};
    });
}
extern "C" void vpi_get_time(vpiHandle object, p_vpi_time time) {
    checked(0, [&](Host& h) {
        if (!time) {
            h.fail("missing VPI time result");
            return 0;
        }
        const auto ticks = h.context.scheduler().time();
        if (time->type == vpiSimTime) {
            time->high = static_cast<std::uint32_t>(ticks >> 32);
            time->low = static_cast<std::uint32_t>(ticks);
        } else if (time->type == vpiScaledRealTime) {
            const auto unit = object ? h.info(object).unit : h.precision;
            time->real = static_cast<double>(ticks) * std::pow(10.0, h.precision - unit);
        } else
            h.fail("unsupported VPI time result type");
        return 0;
    });
}
extern "C" vpiHandle vpi_register_cb(p_cb_data data) {
    return checked<vpiHandle>(nullptr, [&](Host& h) {
        if (!data || !data->cb_rtn) {
            h.fail("invalid VPI callback registration");
            return vpiHandle{};
        }
        if (data->value) {
            switch (data->value->format) {
                case vpiBinStrVal:
                case vpiOctStrVal:
                case vpiDecStrVal:
                case vpiHexStrVal:
                case vpiScalarVal:
                case vpiIntVal:
                case vpiRealVal:
                case vpiStringVal:
                case vpiVectorVal:
                case vpiObjTypeVal:
                case vpiSuppressVal: break;
                default: h.fail("unsupported VPI callback value format"); return vpiHandle{};
            }
        }
        if (data->reason == cbValueChange && data->time && data->time->type != vpiSuppressTime &&
            data->time->type != vpiSimTime && data->time->type != vpiScaledRealTime) {
            h.fail("invalid value callback time format");
            return vpiHandle{};
        }
        auto cb = std::make_unique<Host::Callback>();
        cb->globalTime = !data->obj && h.stack.empty();
        cb->target.unit = h.stack.empty() ? h.precision
                          : h.sites[h.stack.back()->site].handle
                              ? h.info(h.sites[h.stack.back()->site].handle).unit
                              : h.precision;
        cb->data = *data;
        if (data->time) cb->time = *data->time;
        if (data->value) cb->value = *data->value;
        if (data->obj && !h.get(data->obj)) return vpiHandle{};
        if (data->obj) {
            cb->target = h.info(data->obj);
            Host::Object bound;
            bound.kind = Kind::Bound;
            bound.info = cb->target;
            cb->data.obj = h.add(std::move(bound));
        }
        std::optional<std::uint64_t> ticks;
        kirimasim::kernel::Region region = kirimasim::kernel::Region::PreActive;
        switch (data->reason) {
            case cbStartOfSimulation:
            case cbEndOfSimulation:
                if (data->reason == cbStartOfSimulation && h.runtime) {
                    h.fail("start callback registered after simulation started");
                    return vpiHandle{};
                }
                break;
            case cbValueChange:
                if (!data->obj) {
                    h.fail("value callback requires an object");
                    return vpiHandle{};
                }
                cb->target = h.info(data->obj);
                if (!cb->target.binding.isSlot || !cb->target.dimensions.empty()) {
                    h.fail("value callback target has no stable slot");
                    return vpiHandle{};
                }
                cb->last = h.read(data->obj);
                if (h.errorPending) return vpiHandle{};
                break;
            case cbNextSimTime:
            case cbAfterDelay:
            case cbReadWriteSynch:
            case cbReadOnlySynch:
                if (data->reason == cbReadWriteSynch) region = kirimasim::kernel::Region::PostNba;
                if (data->reason == cbReadOnlySynch) region = kirimasim::kernel::Region::Postponed;
                ticks = h.delay(data->time, cb->target.unit);
                if (!ticks) return vpiHandle{};
                if (h.context.scheduler().inPostponed() && *ticks == 0) {
                    h.fail("same-slot VPI callback from read-only region");
                    return vpiHandle{};
                }
                break;
            default: h.fail("unsupported VPI callback reason"); return vpiHandle{};
        }
        if (h.callbacks.size() >= 0x80000000U) {
            h.context.scheduler().resourceLimit();
            return vpiHandle{};
        }
        const auto id = static_cast<std::uint32_t>(h.callbacks.size());
        h.callbacks.push_back(std::move(cb));
        Host::Object handle;
        handle.kind = Kind::Callback;
        handle.site = id;
        if (ticks && h.runtime && data->reason != cbNextSimTime)
            h.context.scheduler().vpiSchedule(region, id, 1, *ticks);
        return h.add(std::move(handle));
    });
}
extern "C" PLI_INT32 vpi_remove_cb(vpiHandle handle) {
    return checked<PLI_INT32>(0, [&](Host& h) {
        auto* object = h.get(handle);
        if (!object || object->kind != Kind::Callback || object->site >= h.callbacks.size()) {
            h.fail("VPI removal requires a callback handle");
            return 0;
        }
        auto& cb = *h.callbacks[object->site];
        if (!cb.live) return 0;
        cb.live = false;
        ++cb.generation;
        return 1;
    });
}
extern "C" PLI_INT32 vpi_get_vlog_info(p_vpi_vlog_info info) {
    return checked<PLI_INT32>(0, [&](Host& h) {
        if (!info) {
            h.fail("missing VPI argv result");
            return 0;
        }
        info->argc = static_cast<std::int32_t>(h.argvPointers.size());
        info->argv = h.argvPointers.data();
        info->product = const_cast<char*>("kirimasim");
        info->version = h.version.data();
        return 1;
    });
}
extern "C" PLI_INT32 vpi_chk_error(p_vpi_error_info info) {
    return checked<PLI_INT32>(
        0,
        [&](Host& h) {
            if (!h.errorPending) return 0;
            if (info) {
                *info = {h.runtime ? vpiRun : vpiCompile,
                         vpiError,
                         h.message.data(),
                         const_cast<char*>("kirimasim"),
                         h.code.data(),
                         nullptr,
                         0};
            }
            h.errorPending = false;
            return vpiError;
        },
        false);
}
extern "C" PLI_INT32 vpi_printf(const PLI_BYTE8* format, ...) {
    auto* h = host();
    if (!h || !format) return 0;
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    const int size = std::vsnprintf(nullptr, 0, format, copy);
    va_end(copy);
    int result = 0;
    try {
        if (size >= 0) {
            std::vector<char> text(static_cast<std::size_t>(size) + 1);
            std::vsnprintf(text.data(), text.size(), format, args);
            h->context.output().write(
                std::string_view(text.data(), static_cast<std::size_t>(size)));
            result = size;
        }
    } catch (...) {
        h->context.scheduler().resourceLimit();
    }
    va_end(args);
    return result;
}
extern "C" PLI_INT32 vpi_control(PLI_INT32 operation, ...) {
    auto* h = host();
    if (!h) return 0;
    if (operation != vpiFinish && operation != vpiStop) {
        return checked<PLI_INT32>(0, [&](Host& x) {
            x.fail("unsupported VPI control operation");
            return 0;
        });
    }
    va_list args;
    va_start(args, operation);
    const int level = va_arg(args, int);
    va_end(args);
    return checked<PLI_INT32>(0, [&](Host& x) {
        if (!x.runtime) {
            x.fail("VPI simulation control during compilation");
            return 0;
        }
        ++x.controlSerial;
        x.context.scheduler().requestEnd(operation == vpiFinish
                                             ? kirimasim::kernel::EndReason::Finish
                                             : kirimasim::kernel::EndReason::Stop,
                                         level);
        return 1;
    });
}

namespace kirimasim::kernel {
void Vpi::linkApi() noexcept {}
}  // namespace kirimasim::kernel
