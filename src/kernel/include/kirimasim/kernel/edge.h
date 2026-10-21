// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Edges of a four-state bit (VALUE_SEMANTICS.md 4.3, IEEE 1800-2023 table
// 9-2).
//
// The reference definitions are valsem::isPosedge and valsem::isNegedge;
// tests/unit/scheduler_test.cpp compares the two on every pair of values.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

namespace kirimasim::kernel {

/// A four-state bit as a number: 0, 1, 2 (z) or 3 (x).
inline unsigned logicOf(bool a, bool b) { return b ? (a ? 3U : 2U) : (a ? 1U : 0U); }

/// 0 to 1, x or z; or x or z to 1.
inline bool isPosedge(unsigned from, unsigned to) {
    return (from == 0 && to != 0) || (from >= 2 && to == 1);
}

/// 1 to 0, x or z; or x or z to 0.
inline bool isNegedge(unsigned from, unsigned to) {
    return (from == 1 && to != 1) || (from >= 2 && to == 0);
}

}  // namespace kirimasim::kernel
