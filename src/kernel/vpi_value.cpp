// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/format.h"
#include "kirimasim/kernel/scan.h"
#include "kirimasim/kernel/wide.h"
#include "vpi_internal.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace kirimasim::kernel {
bool Vpi::Impl::convert(p_vpi_value target, const VpiValue& input,
                        std::vector<s_vpi_vecval>& storage, std::string& text,
                        const VpiObjectInfo& object) {
    if (!target) {
        fail("missing VPI value output");
        return false;
    }
    if (target->format == vpiSuppressVal) return true;
    if (target->format == vpiObjTypeVal)
        target->format = input.type.real                ? vpiRealVal
                         : object.kind == vpiIntegerVar ? vpiIntVal
                         : object.scalar                ? vpiScalarVal
                                                        : vpiVectorVal;
    if (input.words.empty()) {
        fail("VPI object has no readable value");
        return false;
    }
    if (target->format == vpiRealVal) {
        target->value.real = input.type.real ? std::bit_cast<double>(input.words[0].a)
                                             : wide::toReal(input.words.data(), input.type.width,
                                                            input.type.signedValue);
        return true;
    }
    VpiValue value = input;
    if (value.type.real) {
        value.type = {64, false, true, false};
        value.words.resize(1);
        wide::fromReal(value.words.data(), 64, std::bit_cast<double>(input.words[0].a), false);
    }
    if (target->format == vpiIntVal) {
        auto bits = static_cast<std::uint32_t>(value.words[0].a & ~value.words[0].b);
        if (value.type.signedValue && value.type.width < 32 &&
            ((bits >> (value.type.width - 1)) & 1U) != 0)
            bits |= ~static_cast<std::uint32_t>(mask(value.type.width));
        target->value.integer = static_cast<std::int32_t>(bits);
        return true;
    }
    if (target->format == vpiScalarVal) {
        const auto a = value.words[0].a & 1U, b = value.words[0].b & 1U;
        target->value.scalar = b ? (a ? vpiX : vpiZ) : (a ? vpi1 : vpi0);
        return true;
    }
    if (target->format == vpiVectorVal) {
        storage.assign((value.type.width - 1) / 32 + 1, {});
        for (std::uint32_t i = 0; i < storage.size(); ++i) {
            const auto& w = value.words[i / 2];
            const auto shift = 32 * (i % 2);
            storage[i] = {static_cast<std::int32_t>(w.a >> shift),
                          static_cast<std::int32_t>(w.b >> shift)};
        }
        if (value.type.width % 32) {
            const auto mask =
                static_cast<std::uint32_t>((std::uint64_t{1} << (value.type.width % 32)) - 1);
            storage.back().aval =
                static_cast<std::int32_t>(static_cast<std::uint32_t>(storage.back().aval) & mask);
            storage.back().bval =
                static_cast<std::int32_t>(static_cast<std::uint32_t>(storage.back().bval) & mask);
        }
        target->value.vector = storage.data();
        return true;
    }
    if (target->format == vpiStringVal) {
        text = wide::toText(value.words.data(), value.type.width);
        target->value.str = text.data();
        return true;
    }
    char conversion = 0;
    switch (target->format) {
        case vpiBinStrVal: conversion = 'b'; break;
        case vpiOctStrVal: conversion = 'o'; break;
        case vpiHexStrVal: conversion = 'h'; break;
        case vpiDecStrVal: conversion = 'd'; break;
        default: fail("unsupported VPI value format"); return false;
    }
    std::vector<std::uint64_t> words;
    words.reserve(value.words.size() * 2);
    for (const auto w : value.words) {
        words.push_back(w.a);
        words.push_back(w.b);
    }
    FormatSpec spec;
    spec.conv = conversion;
    spec.width = conversion == 'd' ? 0 : -1;
    text = formatInt({words.data(), value.type.width, true, value.type.signedValue}, spec);
    target->value.str = text.data();
    return true;
}
std::optional<VpiValue> Vpi::Impl::decode(p_vpi_value input, VpiType type) {
    if (!input || type.width == 0) {
        fail("missing VPI value or writable type");
        return std::nullopt;
    }
    VpiValue out{type, std::vector<W4>(wide::chunks(type.width))};
    if (type.real) {
        if (input->format == vpiRealVal)
            out.words[0].a = std::bit_cast<std::uint64_t>(input->value.real);
        else {
            auto integral = decode(input, {64, true, true, false});
            if (!integral) return std::nullopt;
            out.words[0].a =
                std::bit_cast<std::uint64_t>(wide::toReal(integral->words.data(), 64, true));
        }
        return out;
    }
    switch (input->format) {
        case vpiIntVal: {
            const auto value = static_cast<std::int64_t>(input->value.integer);
            for (auto& word : out.words)
                word.a = value < 0 ? UINT64_MAX : 0;
            out.words[0].a = static_cast<std::uint64_t>(value);
            break;
        }
        case vpiScalarVal: {
            const auto v = input->value.scalar;
            if (v != vpi0 && v != vpi1 && v != vpiX && v != vpiZ) {
                fail("invalid VPI scalar encoding");
                return std::nullopt;
            }
            out.words[0] = {static_cast<std::uint64_t>(v == vpi1 || v == vpiX),
                            static_cast<std::uint64_t>(v == vpiX || v == vpiZ)};
            break;
        }
        case vpiVectorVal: {
            if (!input->value.vector) {
                fail("missing VPI vector buffer");
                return std::nullopt;
            }
            for (std::uint32_t i = 0; i < (type.width - 1) / 32 + 1; ++i) {
                const auto& w = input->value.vector[i];
                out.words[i / 2].a |= std::uint64_t{static_cast<std::uint32_t>(w.aval)}
                                      << (32 * (i % 2));
                out.words[i / 2].b |= std::uint64_t{static_cast<std::uint32_t>(w.bval)}
                                      << (32 * (i % 2));
            }
            break;
        }
        case vpiRealVal:
            wide::fromReal(out.words.data(), type.width, input->value.real, type.four);
            break;
        case vpiStringVal:
            if (!input->value.str) {
                fail("missing VPI string");
                return std::nullopt;
            }
            wide::fromText(out.words.data(), type.width, input->value.str);
            break;
        case vpiBinStrVal:
        case vpiOctStrVal:
        case vpiDecStrVal:
        case vpiHexStrVal: {
            if (!input->value.str) {
                fail("missing VPI numeric string");
                return std::nullopt;
            }
            const char conv = input->format == vpiBinStrVal   ? 'b'
                              : input->format == vpiOctStrVal ? 'o'
                              : input->format == vpiDecStrVal ? 'd'
                                                              : 'h';
            std::vector<ScanValue> values;
            const std::uint32_t width = type.width;
            if (scanString(input->value.str, std::string("%") + conv, values,
                           std::span(&width, 1)) != 1) {
                fail("invalid VPI numeric string");
                return std::nullopt;
            }
            const auto& value = values[0];
            if (value.fill) {
                std::fill(out.words.begin(), out.words.end(), value.bits);
            } else {
                std::copy_n(value.wide.begin(), std::min(value.wide.size(), out.words.size()),
                            out.words.begin());
                if (value.negative) wide::neg(out.words.data(), out.words.data(), type.width);
            }
            break;
        }
        default: fail("unsupported VPI input value format"); return std::nullopt;
    }
    if (!type.four)
        for (auto& word : out.words) {
            word.a &= ~word.b;
            word.b = 0;
        }
    out.words.back().a &= mask(type.width % 64 == 0 ? 64 : type.width % 64);
    out.words.back().b &= mask(type.width % 64 == 0 ? 64 : type.width % 64);
    return out;
}
bool Vpi::Impl::timeFormat(p_vpi_time time) {
    if (!time) {
        fail("VPI time request needs a time descriptor");
        return false;
    }
    if (time->type != vpiSimTime && time->type != vpiScaledRealTime) {
        fail("unsupported or suppressed VPI time type");
        return false;
    }
    return true;
}
std::optional<std::uint64_t> Vpi::Impl::delay(p_vpi_time time, int unit) {
    if (!timeFormat(time)) return std::nullopt;
    if (time->type == vpiSimTime) return (std::uint64_t{time->high} << 32) | time->low;
    const long double ticks =
        static_cast<long double>(time->real) * std::pow(10.0L, unit - precision);
    const auto rounded = std::round(ticks);
    if (!std::isfinite(ticks) || rounded < 0 || rounded >= std::ldexp(1.0L, 64)) {
        fail("VPI scaled time overflow");
        context.scheduler().runtimeError();
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(rounded);
}
}  // namespace kirimasim::kernel
