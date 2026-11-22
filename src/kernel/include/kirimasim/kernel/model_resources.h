// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/managed_event.h"
#include "kirimasim/kernel/objects.h"
#include <cstdint>
#include <map>
#include <memory_resource>
#include <optional>
#include <variant>

namespace kirimasim::kernel {
struct ModelObjectField {
    ObjectPin pin;
    ObjectRef field;
    std::optional<ObjectBitRef> bits;
};
// Reserve the ticket before native allocation, RNG advancement or beginning a
// constructor. Map nodes keep borrowed entries stable while allocating a peer.
class ModelResources {
public:
    explicit ModelResources(std::pmr::memory_resource* memory) : entries_(memory) {}
    template <class T> struct Ticket {
        std::uint64_t id;
        T& entry;
    };
    template <class T> Ticket<T> reserve() {
        const auto id = nextTicket();
        const auto it = entries_.try_emplace(id, std::in_place_type<T>).first;
        issued_ = id;
        return {id, *std::get_if<T>(&it->second)};
    }
    template <class T> T* get(std::uint64_t id) noexcept {
        auto it = entries_.find(id);
        return it == entries_.end() ? nullptr : std::get_if<T>(&it->second);
    }
    template <class T> bool release(std::uint64_t id) noexcept {
        auto it = entries_.find(id);
        if (it == entries_.end() || !std::holds_alternative<T>(it->second)) return false;
        entries_.erase(it);
        return true;
    }

private:
    std::uint64_t nextTicket() const;
    std::pmr::map<std::uint64_t,
                  std::variant<ObjectPin, ObjectStage, ModelObjectField, ManagedEventPin>>
        entries_;
    std::uint64_t issued_ = 0;
};
}  // namespace kirimasim::kernel
