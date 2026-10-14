// front: drives slang and (in later phases) lowers its result to SIR.

#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace haremasim::front {

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

}  // namespace haremasim::front
