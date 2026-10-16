// Event counters reported by `--stats=json`.
//
// Counts do not depend on machine load or optimization level, so CI uses
// them to catch performance regressions (the PerfCount tests).
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kirimasim::kernel {

/// Index of a counter inside a Counters table.
using CounterId = std::uint32_t;

/// A small table of named 64-bit counters.
///
/// Counters keep the order in which they were added, so the JSON output is
/// deterministic. A Counters table is owned by a Context; there is no global
/// instance.
class Counters {
public:
    /// Adds a counter with the given name and returns its id. If a counter
    /// with the same name already exists, returns the existing id.
    CounterId add(std::string_view name);
    /// Adds an entry that holds text instead of a number (only
    /// sim.end_reason; PERFORMANCE.md 3.1).
    CounterId addText(std::string_view name);
    void setText(CounterId id, std::string_view text) { entries_[id].text = text; }

    void increment(CounterId id, std::uint64_t n = 1) { entries_[id].value += n; }
    void set(CounterId id, std::uint64_t value) { entries_[id].value = value; }
    [[nodiscard]] std::uint64_t value(CounterId id) const { return entries_[id].value; }
    [[nodiscard]] std::string_view name(CounterId id) const { return entries_[id].name; }
    [[nodiscard]] std::size_t size() const { return entries_.size(); }

    /// Returns the counters as a single JSON object, in insertion order,
    /// followed by a newline. Example: {"front.files": 1, "sim.end_reason": "finish"}
    [[nodiscard]] std::string toJson() const;

private:
    struct Entry {
        std::string name;
        std::uint64_t value = 0;
        bool isText = false;
        std::string text;
    };
    std::vector<Entry> entries_;
};

}  // namespace kirimasim::kernel
