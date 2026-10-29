// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <cstddef>
#include <string>

namespace slang::ast {
class Compilation;
}

namespace kirimasim::front {

struct QueryCheckResult {
    std::size_t numErrors = 0;
    std::string diagnostics;
};

/// Check query-expression restrictions that the pinned front end does not
/// diagnose. Uses the bound expression types without evaluating expressions.
QueryCheckResult checkQueryExpressions(slang::ast::Compilation& compilation);

}  // namespace kirimasim::front
