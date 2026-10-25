// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Includes every public kernel header so that they are compiled as C++20.
// Keep this list in sync with src/kernel/include/kirimasim/kernel/.

#include <type_traits>
#include <utility>

#include "kirimasim/kernel/abi.h"
#include "kirimasim/kernel/activation.h"
#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/counters.h"
#include "kirimasim/kernel/dump.h"
#include "kirimasim/kernel/edge.h"
#include "kirimasim/kernel/files.h"
#include "kirimasim/kernel/format.h"
#include "kirimasim/kernel/heap.h"
#include "kirimasim/kernel/id.h"
#include "kirimasim/kernel/managed_event.h"
#include "kirimasim/kernel/memfile.h"
#include "kirimasim/kernel/mos.h"
#include "kirimasim/kernel/pattern.h"
#include "kirimasim/kernel/random.h"
#include "kirimasim/kernel/scan.h"
#include "kirimasim/kernel/sim.h"
#include "kirimasim/kernel/stack.h"
#include "kirimasim/kernel/store.h"
#include "kirimasim/kernel/strength.h"
#include "kirimasim/kernel/strings.h"
#include "kirimasim/kernel/udp.h"
#include "kirimasim/kernel/urandom.h"
#include "kirimasim/kernel/w4.h"
#include "kirimasim/kernel/w4ops.h"
#include "kirimasim/kernel/wide.h"

static_assert(__cplusplus == 202002L, "this file must be compiled as C++20");

namespace {
using kirimasim::kernel::PatternValue;

// Recursive borrowed ranges must support every value-transfer operation in
// C++20, without allocating or copying the pointees.
constexpr bool patternValueTransfersBorrowedRanges() {
    const PatternValue children[1]{};
    const PatternValue keys[1]{};
    PatternValue original;
    original.kind = PatternValue::Kind::Associative;
    original.children = {children, 1};
    original.keys = {keys, 1};
    const PatternValue copied(original);
    PatternValue assigned;
    assigned = copied;
    PatternValue moved(std::move(original));
    PatternValue moveAssigned;
    moveAssigned = std::move(assigned);
    const PatternValue* views[]{&copied, &moved, &moveAssigned};
    for (const auto* view : views)
        if (view->kind != PatternValue::Kind::Associative || view->children.data() != children ||
            view->children.size() != 1 || view->keys.data() != keys || view->keys.size() != 1)
            return false;
    return true;
}
static_assert(patternValueTransfersBorrowedRanges());
static_assert(std::is_nothrow_copy_constructible_v<PatternValue>);
static_assert(std::is_nothrow_move_constructible_v<PatternValue>);
static_assert(std::is_nothrow_copy_assignable_v<PatternValue>);
static_assert(std::is_nothrow_move_assignable_v<PatternValue>);
static_assert(std::is_trivially_destructible_v<PatternValue>);
}  // namespace

// Probe public C++20 types/noexcept; MosStrength checks numeric semantics.
static_assert(std::is_same_v<decltype(kirimasim::kernel::mosStrength(
                                 kirimasim::kernel::MosKind::Rcmos, {7, 7}, {1, 0}, {0, 0})),
                             std::optional<kirimasim::kernel::StrengthRange>>);
static_assert(std::is_same_v<decltype(kirimasim::kernel::transferStrength(
                                 kirimasim::kernel::StrengthTransfer::Identity, {0, 0})),
                             std::optional<kirimasim::kernel::StrengthRange>>);
static_assert(noexcept(kirimasim::kernel::mosStrength(kirimasim::kernel::MosKind::Cmos, {0, 0},
                                                      {1, 1}, {0, 1})));
