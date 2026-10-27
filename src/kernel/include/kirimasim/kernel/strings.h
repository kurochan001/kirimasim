// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The string values of a simulation (SIR_SPEC.md 3.2.1).
//
// A string is a handle in the Handle domain: 0 is the empty string, and
// every other handle names one interned string. Equal strings get equal
// handles, so comparing handles compares strings, and a write that changes
// nothing keeps the handle (value-change events stay right). Unreachable
// values can be reclaimed at a complete typed safepoint (RUNTIME_ABI 6).
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdint>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kirimasim::kernel {

/// atoreal (6.16.10): scan a real-constant prefix, ignoring its underscores.
/// No value means allocation failure; malformed input still converts to zero.
std::optional<double> stringToReal(std::string_view text) noexcept;

enum class StringError : std::uint8_t { None, InvalidHandle, InvalidState, ResourceLimit };
struct StringStats {
    std::uint64_t live = 0, entries = 0, bytes = 0;
};

class Strings {
public:
    explicit Strings(std::pmr::memory_resource* memory = std::pmr::new_delete_resource())
        : memory_(memory), texts_(memory), free_(memory), handles_(memory) {}
    Strings(const Strings&) = delete;
    Strings& operator=(const Strings&) = delete;
    /// A native owner across VM reentry. The table must outlive its pins.
    class Pin {
    public:
        Pin() = default;
        Pin(const Pin&) = delete;
        Pin& operator=(const Pin&) = delete;
        Pin(Pin&& other) noexcept
            : owner_(std::exchange(other.owner_, nullptr)),
              handle_(std::exchange(other.handle_, 0)) {}
        Pin& operator=(Pin&& other) noexcept {
            if (this != &other) {
                reset();
                owner_ = std::exchange(other.owner_, nullptr);
                handle_ = std::exchange(other.handle_, 0);
            }
            return *this;
        }
        ~Pin() { reset(); }
        void reset() noexcept;
        [[nodiscard]] std::uint64_t handle() const noexcept { return handle_; }

    private:
        friend class Strings;
        Strings* owner_ = nullptr;
        std::uint64_t handle_ = 0;
    };
    /// Retain a valid handle without allocating; failure leaves output unchanged.
    StringError pin(std::uint64_t handle, Pin& output) noexcept;
    /// The handle of a string (0 for the empty string).
    std::uint64_t intern(std::string_view s);
    /// Borrowed until mutation or collection; an unknown handle reads as empty.
    [[nodiscard]] std::string_view get(std::uint64_t handle) const {
        const auto low = static_cast<std::uint32_t>(handle);
        if (low == 0 || low > texts_.size()) return {};
        const auto& entry = texts_[low - 1];
        if (!entry.live || entry.generation != handle >> 32) return {};
        return entry.text;
    }
    /// Begin/mark do not free values; finish is nonallocating. Cancel a failed
    /// complete-root mark before attempting a later collection.
    StringError beginCollection(std::span<const std::uint64_t> roots) noexcept;
    StringError mark(std::uint64_t handle) noexcept;
    void finishCollection() noexcept;
    void cancelCollection() noexcept { collecting_ = false; }
    [[nodiscard]] bool underPressure() const noexcept {
        return live_ >= nextObjects_ || bytes_ >= nextBytes_;
    }
    [[nodiscard]] StringStats stats() const noexcept { return {live_, texts_.size(), bytes_}; }

private:
    struct Entry {
        explicit Entry(std::pmr::memory_resource* memory) : text(memory) {}
        std::pmr::string text;
        std::uint64_t pins = 0;
        std::uint32_t generation = 1;
        bool live = false, marked = false;
    };
    struct Hash {
        using is_transparent = void;
        std::size_t operator()(std::string_view text) const noexcept {
            return std::hash<std::string_view>{}(text);
        }
    };
    struct Equal {
        using is_transparent = void;
        bool operator()(std::string_view a, std::string_view b) const noexcept { return a == b; }
    };
    std::pmr::memory_resource* memory_;
    std::pmr::vector<Entry> texts_;
    std::pmr::vector<std::uint32_t> free_;
    std::size_t freeCount_ = 0;
    // kirimasim: unordered-ok lookups only; nothing iterates it
    std::pmr::unordered_map<std::pmr::string, std::uint64_t, Hash, Equal> handles_;
    std::uint64_t live_ = 0, bytes_ = 0;
    std::uint64_t nextObjects_ = 256, nextBytes_ = 65536;
    bool collecting_ = false, failed_ = false;
};

}  // namespace kirimasim::kernel
