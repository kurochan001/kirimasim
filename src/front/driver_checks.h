// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <cstddef>
#include <string>

namespace slang {
class DiagnosticEngine;
namespace ast {
class Compilation;
}
namespace analysis {
class AnalysisManager;
}
}  // namespace slang

namespace kirimasim::front {

struct DriverCheckResult {
    std::size_t numErrors = 0;
    std::string diagnostics;
};

/// Reject source driver conflicts that relaxed diagnostics can hide, before
/// constructing executable SIR. Never reclassifies an SIR verifier failure.
DriverCheckResult checkExecutableDrivers(slang::ast::Compilation& compilation,
                                         slang::analysis::AnalysisManager& analysis,
                                         const slang::DiagnosticEngine& formatter);

}  // namespace kirimasim::front
