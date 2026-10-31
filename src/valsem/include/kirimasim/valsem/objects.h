// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <cstdint>
#include <list>
#include <span>
#include <vector>

#include "kirimasim/valsem/value.h"

namespace kirimasim::valsem {

/// Reference values copy by value except for explicitly typed object IDs.
/// This model has no pool indices, generations, pins or allocator machinery.
struct ObjectValue {
    enum class Kind { Bits, Handle, Sequence };
    Kind kind = Kind::Bits;
    Value bits;
    std::uint64_t handle = 0;
    std::vector<ObjectValue> elements;
    friend bool operator==(const ObjectValue&, const ObjectValue&) = default;
};
class ObjectModel {
public:
    std::uint64_t create(std::uint32_t dynamicClass, std::vector<ObjectValue> defaults);
    std::uint64_t copy(std::uint64_t source);
    std::uint64_t weak(std::uint32_t dynamicClass, std::uint64_t referent);
    [[nodiscard]] bool live(std::uint64_t id) const;
    [[nodiscard]] std::uint32_t dynamicClass(std::uint64_t id) const;
    [[nodiscard]] ObjectValue read(std::uint64_t id, std::size_t field) const;
    bool write(std::uint64_t id, std::size_t field, const ObjectValue& value);
    [[nodiscard]] std::uint64_t lock(std::uint64_t weak) const;
    void collect(std::span<const std::uint64_t> roots);

private:
    struct Object {
        std::uint64_t id;
        std::uint32_t type;
        std::vector<ObjectValue> fields;
        std::uint64_t referent = 0;
        bool weak = false, alive = true;
    };
    std::list<Object> objects_;
    std::uint64_t next_ = 1;
    Object* find(std::uint64_t id);
    const Object* find(std::uint64_t id) const;
};

}  // namespace kirimasim::valsem
