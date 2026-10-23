// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/valsem/containers.h"

#include <algorithm>
#include <utility>

namespace kirimasim::valsem {

std::uint64_t ContainerValue::make(const Value& value) {
    const std::uint64_t identity = next_++;
    storage_.push_back({identity, value});
    return identity;
}
std::uint64_t ContainerValue::reference(std::optional<std::int64_t> index) const {
    if (!index || *index < 0 || static_cast<std::uint64_t>(*index) >= order_.size()) return 0;
    return order_[static_cast<std::size_t>(*index)];
}
Value ContainerValue::readReference(std::uint64_t cell) const {
    for (const Cell& c : storage_)
        if (c.identity == cell) return c.value;
    return initial_;
}
void ContainerValue::writeReference(std::uint64_t cell, const Value& value) {
    for (Cell& c : storage_)
        if (c.identity == cell) {
            c.value = value;
            return;
        }
}
Value ContainerValue::read(std::optional<std::int64_t> index) const {
    return readReference(reference(index));
}
void ContainerValue::write(std::optional<std::int64_t> index, const Value& value, bool queue) {
    if (queue && index && *index >= 0 && static_cast<std::uint64_t>(*index) == order_.size()) {
        (void)insert(index, value);
        return;
    }
    writeReference(reference(index), value);
}
std::vector<Value> ContainerValue::values() const {
    std::vector<Value> out;
    for (const std::uint64_t cell : order_)
        out.push_back(readReference(cell));
    return out;
}
void ContainerValue::replace(const std::vector<Value>& values) {
    order_.clear();
    for (const Value& v : values) {
        if (capacity_ && order_.size() == *capacity_) break;
        order_.push_back(make(v));
    }
}
void ContainerValue::resize(std::size_t size, bool keep) {
    std::vector<Value> out = keep ? values() : std::vector<Value>{};
    out.resize(size, initial_);
    replace(out);
}
bool ContainerValue::insert(std::optional<std::int64_t> index, const Value& value) {
    if (!index || *index < 0 || static_cast<std::uint64_t>(*index) > order_.size()) return false;
    order_.insert(order_.begin() + static_cast<std::ptrdiff_t>(*index), make(value));
    if (capacity_ && order_.size() > *capacity_) order_.pop_back();
    return true;
}
bool ContainerValue::erase(std::optional<std::int64_t> index) {
    if (reference(index) == 0) return false;
    order_.erase(order_.begin() + static_cast<std::ptrdiff_t>(*index));
    return true;
}
Value ContainerValue::pop(bool front) {
    if (order_.empty()) return initial_;
    const Value result = readReference(front ? order_.front() : order_.back());
    if (front)
        order_.pop_front();
    else
        order_.pop_back();
    return result;
}

}  // namespace kirimasim::valsem
