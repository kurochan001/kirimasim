// front: drives slang and lowers its result to SIR.

#pragma once

#include <cstddef>
#include <span>
#include <string>

#include "kirimasim/sir/design.h"

namespace kirimasim::front {

/// Result of parsing and elaborating a set of source files.
struct ParseResult {
    /// True when slang reported no errors.
    bool success = false;
    std::size_t numFiles = 0;
    std::size_t numSyntaxTrees = 0;
    std::size_t numErrors = 0;
    std::size_t numWarnings = 0;
    /// Human-readable diagnostics (file:line:column, message, source line).
    std::string diagnostics;
};

/// Parses and elaborates the given files with slang and collects its
/// diagnostics. Problems that slang reports before parsing (for example, a
/// file that cannot be opened) are written to stderr by slang itself and
/// make the result unsuccessful.
ParseResult parseAndElaborate(std::span<const std::string> files);

/// Result of compiling a design to SIR.
struct CompileResult {
    /// True when the design was elaborated, lowered and verified.
    bool success = false;
    /// True when lowering stopped at a construct that is not supported yet.
    bool unsupported = false;
    std::size_t numErrors = 0;
    std::size_t numWarnings = 0;
    /// slang's diagnostics and the lowering errors, in the same text form.
    std::string diagnostics;
    /// Violations found by the SIR verifier (an internal error), if any.
    std::string verifyErrors;
    sir::Design design;
};

/// Parses and elaborates with slang, then lowers the design to SIR and runs
/// the verifier. `args` are slang command-line arguments: files and options
/// such as -f, +incdir+, +define+ and --top.
CompileResult compile(std::span<const std::string> args);

}  // namespace kirimasim::front
