// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "driver_checks.h"

#include <string>

#include "slang/analysis/AnalysisManager.h"
#include "slang/analysis/AnalysisOptions.h"
#include "slang/ast/Compilation.h"
#include "slang/diagnostics/AnalysisDiags.h"
#include "slang/diagnostics/DiagnosticEngine.h"
#include "slang/text/SourceManager.h"

namespace kirimasim::front {

DriverCheckResult checkExecutableDrivers(slang::ast::Compilation& compilation,
                                         slang::analysis::AnalysisManager& analysis,
                                         const slang::DiagnosticEngine& formatter) {
    DriverCheckResult result;
    const auto& sourceManager = *compilation.getSourceManager();
    const auto location = [&](slang::SourceLocation at) {
        return std::string(sourceManager.getFileName(at)) + ":" +
               std::to_string(sourceManager.getLineNumber(at)) + ":" +
               std::to_string(sourceManager.getColumnNumber(at));
    };
    const auto collect = [&](const slang::Diagnostics& diagnostics) {
        for (const auto& diagnostic : diagnostics) {
            if (diagnostic.code != slang::diag::MultipleAlwaysAssigns &&
                diagnostic.code != slang::diag::MultipleContAssigns &&
                diagnostic.code != slang::diag::MixedVarAssigns &&
                diagnostic.code != slang::diag::MultipleUWireDrivers)
                continue;
            ++result.numErrors;
            result.diagnostics += location(diagnostic.location) +
                                  ": error: unsupported: conflicting source drivers are not "
                                  "executable when diagnostics are relaxed: " +
                                  formatter.formatMessage(diagnostic) + "\n";
            for (const auto& note : diagnostic.notes)
                result.diagnostics +=
                    location(note.location) + ": note: " + formatter.formatMessage(note) + "\n";
        }
    };

    using Flags = slang::analysis::AnalysisFlags;
    auto options = analysis.getOptions();
    const auto relaxed = Flags::AllowDupInitialDrivers | Flags::AllowMultiDrivenLocals;
    if (options.flags.has(relaxed)) {
        // Check source conflicts omitted by compatibility without changing the
        // original driver. Format while the analysis still owns its arguments.
        options.flags &= ~relaxed;
        slang::analysis::AnalysisManager strict(options);
        compilation.freeze();
        strict.analyze(compilation);
        compilation.unfreeze();
        collect(strict.getDiagnostics());
    } else {
        collect(analysis.getDiagnostics());
    }
    return result;
}

}  // namespace kirimasim::front
