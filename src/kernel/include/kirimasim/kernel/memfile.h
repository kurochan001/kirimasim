// The files of $readmemb, $readmemh, $writememb and $writememh
// (SYSTEM_TASKS.md 7, 21.4, 21.5): which elements of a memory get which
// words of a file, decided here for every tier (SIR_SPEC.md 8).
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "kirimasim/kernel/w4.h"

namespace kirimasim::kernel {

/// A dimension of a memory as declared: [left:right].
struct MemDim {
    std::int64_t left = 0;
    std::int64_t right = 0;
};

/// A word put into a memory: the element, numbered as slots number them
/// (row-major, each dimension counted from its left bound), and its value
/// in chunks of the element's width.
struct MemWord {
    std::uint32_t elem = 0;
    std::vector<W4> value;
};

/// What $readmemb or $readmemh does to a memory (SYSTEM_TASKS.md 7.1).
/// Each text is empty when it does not happen.
struct MemLoad {
    std::vector<MemWord> words;  ///< In the order they are read.
    /// kirima-readmem-range: an address outside the range; reading stopped.
    std::string range;
    /// kirima-readmem-short: a file with no address whose number of words
    /// differs from the number of words in the range.
    std::string count;
    /// Text that is no number, address or comment, or a word that is no
    /// value of the memory's enumerated type; reading stopped there.
    std::string error;
};

/// Reads `text` as $readmemh (hex) or $readmemb does into a memory of
/// `dims` whose elements have `width` bits, 2-state ones taking x and z
/// as 0; start and finish are the task's address arguments, when given.
/// For a memory of an enumerated type, `items` holds the values of its
/// items, each in chunks of the width, and a word that is none of them
/// stops reading (21.4.2, SYSTEM_TASKS.md 7.1).
MemLoad loadMemory(std::string_view text, bool hex, const std::vector<MemDim>& dims,
                   std::uint32_t width, std::optional<std::int64_t> start,
                   std::optional<std::int64_t> finish, bool twoState,
                   std::span<const std::vector<W4>> items = {});

/// The elements $writememb or $writememh writes, in order, for the same
/// arguments: the range of $readmem, each dimension below the first from
/// its low to its high index (21.4.3, 21.5).
std::vector<std::uint32_t> dumpOrder(const std::vector<MemDim>& dims,
                                     std::optional<std::int64_t> start,
                                     std::optional<std::int64_t> finish);

/// The whole content of a file, or nothing when it cannot be read.
std::optional<std::string> readFile(const std::string& name);

}  // namespace kirimasim::kernel
