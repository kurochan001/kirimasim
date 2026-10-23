// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/valsem/strength.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

namespace kirimasim::valsem {

Signal driverSignal(const Driver& d) {
    switch (d.value) {
        case Logic::Zero: return {-d.s0, -d.s0};
        case Logic::One: return {d.s1, d.s1};
        case Logic::X: return {-d.s0, d.s1};
        case Logic::Z: return {0, 0};
    }
    return {0, 0};
}

namespace {

/// The signal of one choice of points, one from each driver (5.4, step 2):
/// the strongest point wins; points of both values at the greatest
/// strength give x on a plain net, 0 on a wired-AND net and 1 on a
/// wired-OR net.
Signal combine(NetKind kind, const std::vector<int>& points) {
    int m = 0;
    for (int p : points)
        m = std::max(m, std::abs(p));
    if (m == 0) return {0, 0};
    bool zero = false;
    bool one = false;
    for (int p : points) {
        if (p == -m) zero = true;
        if (p == m) one = true;
    }
    if (zero && one) {
        if (kind == NetKind::Wand || kind == NetKind::Triand) return {-m, -m};
        if (kind == NetKind::Wor || kind == NetKind::Trior) return {m, m};
        return {-m, m};
    }
    return zero ? Signal{-m, -m} : Signal{m, m};
}

}  // namespace

Signal resolve(NetKind kind, const std::vector<Driver>& drivers) {
    std::vector<Signal> ranges;
    ranges.reserve(drivers.size() + 1);
    for (const Driver& d : drivers)
        ranges.push_back(driverSignal(d));
    return resolveSignals(kind, ranges);
}

Signal resolveSignals(NetKind kind, const std::vector<Signal>& signals) {
    std::vector<Signal> ranges = signals;
    // The net's own driver (5.3: 28.15.1, 28.15.3).
    if (kind == NetKind::Tri0) ranges.push_back({-5, -5});
    if (kind == NetKind::Tri1) ranges.push_back({5, 5});
    if (kind == NetKind::Supply0) ranges.push_back({-7, -7});
    if (kind == NetKind::Supply1) ranges.push_back({7, 7});
    if (ranges.empty()) return {0, 0};

    // Every way of taking one point from each range (step 2), counted like
    // an odometer; the narrowest range holding every result (step 3).
    std::vector<int> points;
    points.reserve(ranges.size());
    for (const Signal& r : ranges)
        points.push_back(r.lo);
    Signal hull{8, -8};
    while (true) {
        const Signal s = combine(kind, points);
        hull.lo = std::min(hull.lo, s.lo);
        hull.hi = std::max(hull.hi, s.hi);
        std::size_t i = 0;
        while (i < points.size() && points[i] == ranges[i].hi) {
            points[i] = ranges[i].lo;
            ++i;
        }
        if (i == points.size()) break;
        ++points[i];
    }
    return hull;
}

Logic valueOf(Signal s) {
    if (s.lo == 0 && s.hi == 0) return Logic::Z;
    if (s.hi < 0) return Logic::Zero;
    if (s.lo > 0) return Logic::One;
    return Logic::X;  // x, and L and H
}

Value resolveNet(NetKind kind, std::uint32_t width, const std::vector<Value>& values,
                 const std::vector<int>& s1, const std::vector<int>& s0) {
    Value out = filled(Logic::Z, width, false);
    for (std::uint32_t i = 0; i < width; ++i) {
        std::vector<Driver> ds;
        ds.reserve(values.size());
        for (std::size_t d = 0; d < values.size(); ++d)
            ds.push_back({values[d].bits[i], s1[d], s0[d]});
        out.bits[i] = valueOf(resolve(kind, ds));
    }
    return out;
}

}  // namespace kirimasim::valsem
