// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Slow reference model of integral-element containers (7.5, 7.10, 13.5.2).
// Cells live in a list forever; references search it by identity. This model
// deliberately has no allocator, token-generation or tracing machinery.
#pragma once

#include <cstdint>
#include <deque>
#include <list>
#include <optional>
#include <utility>
#include <vector>

#include "kirimasim/valsem/value.h"

namespace kirimasim::valsem {

class ContainerValue {
public:
    explicit ContainerValue(Value initial, std::optional<std::size_t> capacity = {})
        : initial_(std::move(initial)), capacity_(capacity) {}
    [[nodiscard]] std::size_t size() const { return order_.size(); }
    [[nodiscard]] Value read(std::optional<std::int64_t> index) const;
    void write(std::optional<std::int64_t> index, const Value& value, bool queue);
    [[nodiscard]] std::uint64_t reference(std::optional<std::int64_t> index) const;
    [[nodiscard]] Value readReference(std::uint64_t cell) const;
    void writeReference(std::uint64_t cell, const Value& value);
    void replace(const std::vector<Value>& values);
    void resize(std::size_t size, bool keep);
    bool insert(std::optional<std::int64_t> index, const Value& value);
    bool erase(std::optional<std::int64_t> index);
    Value pop(bool front);
    void clear() { order_.clear(); }
    [[nodiscard]] std::vector<Value> values() const;

private:
    struct Cell {
        std::uint64_t identity;
        Value value;
    };
    Value initial_;
    std::optional<std::size_t> capacity_;
    std::list<Cell> storage_;
    std::deque<std::uint64_t> order_;
    std::uint64_t next_ = 1;
    std::uint64_t make(const Value& value);
};

}  // namespace kirimasim::valsem
