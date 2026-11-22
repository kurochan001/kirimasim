// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/managed_event.h"
#include "kirimasim/kernel/model_place.h"
#include "kirimasim/kernel/objects.h"
#include "kirimasim/kernel/udp.h"
#include <cstdint>
#include <map>
#include <memory_resource>
#include <optional>
#include <variant>

namespace kirimasim::kernel {
struct ModelPreparedWait {
    std::uint32_t first, count;
};
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

    template <class T, class Visitor> bool visit(Visitor visitor) const {
        for (const auto& [id, entry] : entries_)
            if (const auto* item = std::get_if<T>(&entry); item && !visitor(*item)) return false;
        return true;
    }
    static void appendHeapRoots(const void* frame, std::pmr::vector<HeapRoot>& roots) {
        static_cast<const ModelResources*>(frame)->visit<ModelPlace>([&](const ModelPlace& place) {
            if (auto* cell = std::get_if<ModelCell>(&place.source))
                roots.push_back(HeapRoot::cell(cell->token));
            return true;
        });
    }
    std::uint64_t hookTop = 0;

private:
    std::uint64_t nextTicket() const;
    std::pmr::map<std::uint64_t,
                  std::variant<ObjectPin, ObjectStage, ModelObjectField, ManagedEventPin,
                               ModelPreparedWait, UdpTable, ModelPlace, ModelHooks>>
        entries_;
    std::uint64_t issued_ = 0;
};
}  // namespace kirimasim::kernel
