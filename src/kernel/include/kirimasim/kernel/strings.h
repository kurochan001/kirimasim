// The string values of a simulation (SIR_SPEC.md 3.2.1).
//
// A string is a handle in the Handle domain: 0 is the empty string, and
// every other handle names one interned string. Equal strings get equal
// handles, so comparing handles compares strings, and a write that changes
// nothing keeps the handle (value-change events stay right). Strings are
// not freed in P1; garbage collection comes in P3.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace kirimasim::kernel {

class Strings {
public:
    /// The handle of a string (0 for the empty string).
    std::uint64_t intern(std::string_view s);
    /// The string of a handle; an unknown handle reads as empty.
    [[nodiscard]] std::string_view get(std::uint64_t handle) const {
        return handle == 0 || handle > texts_.size() ? std::string_view() : texts_[handle - 1];
    }

private:
    std::vector<std::string> texts_;
    // kirimasim: unordered-ok lookups only; nothing iterates it
    std::unordered_map<std::string, std::uint64_t> handles_;
};

}  // namespace kirimasim::kernel
