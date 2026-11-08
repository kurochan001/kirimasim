// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Runs a Design that a test builds or changes by hand, through the gate first
// (docs/notes/verified-design.md): a Design that does not pass sir::verifyDesign
// fails the test and is not run.

#pragma once

#include <gtest/gtest.h>
#include <utility>

#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/verified.h"
#include "kirimasim/vm/vm.h"

namespace kirimasim::test {

inline vm::RunResult runVerified(sir::Design design, kernel::Context& context,
                                 const vm::RunOptions& options = {}) {
    auto outcome = sir::verifyDesign(std::move(design));
    for (const sir::VerifyError& error : outcome.errors)
        ADD_FAILURE() << "the Design does not pass the gate: " << error.where << ": "
                      << error.message;
    if (!outcome.verified.has_value())
        return {kernel::EndReason::Internal, "the Design does not pass sir::verifyDesign"};
    return vm::run(*outcome.verified, context, options);
}

}  // namespace kirimasim::test
