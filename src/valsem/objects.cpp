// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/valsem/objects.h"

#include <algorithm>
#include <utility>

namespace kirimasim::valsem {
ObjectModel::Object* ObjectModel::find(std::uint64_t id) {
    for (auto& object : objects_)
        if (object.id == id && object.alive) return &object;
    return nullptr;
}
const ObjectModel::Object* ObjectModel::find(std::uint64_t id) const {
    for (const auto& object : objects_)
        if (object.id == id && object.alive) return &object;
    return nullptr;
}
std::uint64_t ObjectModel::create(std::uint32_t dynamicClass, std::vector<ObjectValue> defaults) {
    const auto id = next_++;
    objects_.push_back({id, dynamicClass, std::move(defaults)});
    return id;
}
std::uint64_t ObjectModel::copy(std::uint64_t source) {
    const auto* value = find(source);
    if (!value) return 0;
    const auto id = create(value->type, value->fields);
    auto* result = find(id);
    result->weak = value->weak;
    result->referent = value->referent;
    return id;
}
std::uint64_t ObjectModel::weak(std::uint32_t dynamicClass, std::uint64_t referent) {
    const auto id = create(dynamicClass, {});
    auto* value = find(id);
    value->weak = true;
    value->referent = live(referent) ? referent : 0;
    return id;
}
bool ObjectModel::live(std::uint64_t id) const { return find(id) != nullptr; }
std::uint32_t ObjectModel::dynamicClass(std::uint64_t id) const {
    const auto* value = find(id);
    return value ? value->type : UINT32_MAX;
}
ObjectValue ObjectModel::read(std::uint64_t id, std::size_t field) const {
    const auto* value = find(id);
    return value && field < value->fields.size() ? value->fields[field] : ObjectValue{};
}
bool ObjectModel::write(std::uint64_t id, std::size_t field, const ObjectValue& value) {
    auto* object = find(id);
    if (!object || field >= object->fields.size()) return false;
    object->fields[field] = value;
    return true;
}
std::uint64_t ObjectModel::lock(std::uint64_t weak) const {
    const auto* value = find(weak);
    return value && value->weak && live(value->referent) ? value->referent : 0;
}
void ObjectModel::collect(std::span<const std::uint64_t> roots) {
    // Compute the least fixed point of strong reachability. Weak edges
    // never enter this set. The list retains dead IDs forever.
    std::vector<std::uint64_t> reached(roots.begin(), roots.end());
    for (std::size_t i = 0; i < reached.size(); ++i) {
        const auto* value = find(reached[i]);
        if (!value) continue;
        std::vector<const ObjectValue*> values;
        values.reserve(value->fields.size());
        for (const auto& field : value->fields)
            values.push_back(&field);
        while (!values.empty()) {
            const auto* field = values.back();
            values.pop_back();
            if (field->kind == ObjectValue::Kind::Handle && field->handle != 0 &&
                std::find(reached.begin(), reached.end(), field->handle) == reached.end())
                reached.push_back(field->handle);
            if (field->kind == ObjectValue::Kind::Sequence)
                for (const auto& element : field->elements)
                    values.push_back(&element);
        }
    }
    for (auto& value : objects_)
        if (std::find(reached.begin(), reached.end(), value.id) == reached.end())
            value.alive = false;
    for (auto& value : objects_)
        if (value.weak && !live(value.referent)) value.referent = 0;
}
}  // namespace kirimasim::valsem
