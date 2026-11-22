// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/scalar_task_abi.h"

#include <limits>
#include <optional>
#include <span>

#include "abi_boundary.h"
#include "abi_views.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/random.h"
#include "kirimasim/kernel/urandom.h"
#include "service_tables.h"

namespace kirimasim::kernel {
namespace {
bool valid(kirima_index i) { return !i.reserved && i.known <= 1; }
std::optional<std::int64_t> index(kirima_index i) {
    return i.known ? std::optional{i.value} : std::nullopt;
}
std::uint32_t intern(kirima_context* c, kirima_bytes data, std::uint32_t clean,
                     std::uint64_t* out) {
    if (!c || !out || clean > 1 || !abi::valid(data)) return KIRIMA_VALUE_INVALID;
    auto& s = fromC(c).strings();
    *out = clean ? s.internRead(abi::view(data)) : s.intern(abi::view(data));
    return KIRIMA_VALUE_OK;
}
std::uint32_t get(kirima_context* c, std::uint64_t h, kirima_byte_sink sink, void* user) {
    if (!c || !sink) return KIRIMA_VALUE_INVALID;
    sink(user, abi::bytesOf(fromC(c).strings().get(h)));
    return KIRIMA_VALUE_OK;
}
std::uint32_t byte(kirima_context* c, std::uint64_t h, kirima_index i, std::uint32_t* out) {
    if (!c || !out || !valid(i)) return KIRIMA_VALUE_INVALID;
    *out = stringByte(fromC(c).strings().get(h), index(i));
    return KIRIMA_VALUE_OK;
}
std::uint32_t put(kirima_context* c, std::uint64_t h, kirima_index i, std::uint32_t b,
                  std::uint64_t* out) {
    if (!c || !out || !valid(i) || b > UINT8_MAX) return KIRIMA_VALUE_INVALID;
    auto& s = fromC(c).strings();
    *out = s.intern(putChar(s.get(h), index(i), static_cast<std::uint8_t>(b)));
    return KIRIMA_VALUE_OK;
}
std::uint32_t sub(kirima_context* c, std::uint64_t h, kirima_index i, kirima_index j,
                  std::uint64_t* out) {
    if (!c || !out || !valid(i) || !valid(j)) return KIRIMA_VALUE_INVALID;
    auto& s = fromC(c).strings();
    *out = s.intern(substring(s.get(h), index(i), index(j)));
    return KIRIMA_VALUE_OK;
}
std::uint32_t changeCase(kirima_context* c, std::uint64_t h, std::uint32_t upper,
                         std::uint64_t* out) {
    if (!c || !out || upper > 1) return KIRIMA_VALUE_INVALID;
    auto& s = fromC(c).strings();
    *out = s.intern(stringCase(s.get(h), upper != 0));
    return KIRIMA_VALUE_OK;
}
std::uint32_t compare(kirima_context* c, std::uint64_t x, std::uint64_t y, std::uint32_t ignore,
                      std::int32_t* out) {
    if (!c || !out || ignore > 1) return KIRIMA_VALUE_INVALID;
    const auto& s = fromC(c).strings();
    *out = compareStrings(s.get(x), s.get(y), ignore != 0);
    return KIRIMA_VALUE_OK;
}
std::uint32_t toInteger(kirima_context* c, std::uint64_t h, std::uint32_t base,
                        std::uint32_t* out) {
    if (!c || !out || (base != 2 && base != 8 && base != 10 && base != 16))
        return KIRIMA_VALUE_INVALID;
    *out = stringToInteger(fromC(c).strings().get(h), base);
    return KIRIMA_VALUE_OK;
}
std::uint32_t toReal(kirima_context* c, std::uint64_t h, double* out) {
    if (!c || !out) return KIRIMA_VALUE_INVALID;
    const auto parsed = stringToReal(fromC(c).strings().get(h));
    if (!parsed) return KIRIMA_VALUE_RESOURCE;
    *out = *parsed;
    return KIRIMA_VALUE_OK;
}
std::uint32_t begin(kirima_context* c, const std::uint64_t* roots, std::uint64_t n) {
    if (!c || (!roots && n) || n > static_cast<std::uint64_t>(PTRDIFF_MAX) / sizeof(*roots))
        return KIRIMA_VALUE_INVALID;
    return fromC(c).strings().beginCollection({roots, static_cast<std::size_t>(n)}) ==
                   StringError::None
               ? KIRIMA_VALUE_OK
               : KIRIMA_VALUE_INVALID;
}
std::uint32_t mark(kirima_context* c, std::uint64_t h) {
    if (!c) return KIRIMA_VALUE_INVALID;
    return fromC(c).strings().mark(h) == StringError::None ? KIRIMA_VALUE_OK : KIRIMA_VALUE_INVALID;
}
std::uint32_t end(kirima_context* c, std::uint32_t commit) {
    if (!c || commit > 1) return KIRIMA_VALUE_INVALID;
    auto& s = fromC(c).strings();
    if (!s.collecting()) return KIRIMA_VALUE_INVALID;
    if (commit && !s.collectionFailed())
        s.finishCollection();
    else {
        const bool failed = commit != 0;
        s.cancelCollection();
        if (failed) return KIRIMA_VALUE_INVALID;
    }
    return KIRIMA_VALUE_OK;
}
std::uint32_t stats(kirima_context* c, kirima_string_stats* out) {
    if (!c || !out) return KIRIMA_VALUE_INVALID;
    const auto& s = fromC(c).strings();
    const auto v = s.stats();
    *out = {v.live, v.entries, v.bytes, s.underPressure(), 0};
    return KIRIMA_VALUE_OK;
}
std::uint32_t initialRng(std::uint64_t seed, kirima_bytes name, std::uint64_t* out) {
    if (!out || !abi::valid(name)) return KIRIMA_VALUE_INVALID;
    *out = initialState(seed, abi::view(name));
    return KIRIMA_VALUE_OK;
}
std::uint32_t childRng(std::uint64_t* parent, std::uint64_t* child) {
    if (!parent || !child || parent == child) return KIRIMA_VALUE_INVALID;
    Rng g{*parent};
    *child = g.child().state;
    *parent = g.state;
    return KIRIMA_VALUE_OK;
}
std::uint32_t draw(std::uint64_t* state, const std::uint32_t* seed, std::uint32_t* out) {
    if (!state || !out) return KIRIMA_VALUE_INVALID;
    Rng g{*state};
    *out = urandom(g, seed ? std::optional{*seed} : std::nullopt);
    *state = g.state;
    return KIRIMA_VALUE_OK;
}
std::uint32_t range(std::uint64_t* state, std::uint32_t hi, std::uint32_t lo, std::uint32_t* out) {
    if (!state || !out) return KIRIMA_VALUE_INVALID;
    Rng g{*state};
    *out = urandomRange(g, hi, lo);
    *state = g.state;
    return KIRIMA_VALUE_OK;
}
std::uint32_t distribution(kirima_context* c, std::uint32_t kind, std::int32_t* external,
                           std::int32_t a, std::int32_t b, kirima_dist_result* out) {
    static_assert(static_cast<unsigned>(Distribution::Erlang) == KIRIMA_DIST_ERLANG);
    static_assert(static_cast<unsigned>(DistArgument::Stage) == KIRIMA_DIST_STAGE);
    if (!c || !out || kind > KIRIMA_DIST_ERLANG || (!external && kind != KIRIMA_RANDOM))
        return KIRIMA_VALUE_INVALID;
    auto& seed = external ? *external : fromC(c).randomSeed();
    const auto r = drawDistribution(static_cast<Distribution>(kind), seed, a, b);
    *out = {r.value, static_cast<unsigned>(r.bad)};
    return KIRIMA_VALUE_OK;
}
constexpr kirima_scalar_task_api api{
    Boundary<intern>::call,  Boundary<get>::call,        Boundary<byte>::call,
    Boundary<put>::call,     Boundary<sub>::call,        Boundary<changeCase>::call,
    Boundary<compare>::call, Boundary<toInteger>::call,  Boundary<toReal>::call,
    Boundary<begin>::call,   Boundary<mark>::call,       Boundary<end>::call,
    Boundary<stats>::call,   Boundary<initialRng>::call, Boundary<childRng>::call,
    Boundary<draw>::call,    Boundary<range>::call,      Boundary<distribution>::call};
}  // namespace
const void* scalarTaskService() noexcept { return &api; }
}  // namespace kirimasim::kernel
