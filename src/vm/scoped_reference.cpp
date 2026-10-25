// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "scoped_reference.h"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>

namespace kirimasim::vm::detail {
namespace {
std::uint32_t valueWidth(const sir::Type& type) noexcept {
    return sir::isIntegral(type) ? type.width : type.kind == sir::TypeKind::ShortReal ? 32 : 64;
}
bool sameBinding(kernel::ActivationBinding a, kernel::ActivationBinding b) noexcept {
    return a.token() == b.token() && a.field() == b.field() && a.readonly() == b.readonly();
}
}  // namespace

ScopedReferences::ScopedReferences(kernel::ActivationStore& store,
                                   std::pmr::memory_resource* memory)
    : store_(store), memory_(memory), shapes_(memory), entries_(memory), owners_(memory) {}

ScopedReferences::Result<std::uint32_t>
ScopedReferences::addShape(const ActivationPlan& plan) noexcept {
    try {
        if (plan.store_ != &store_) return {0, Error::InvalidLayout};
        if (shapes_.size() >= sir::kNone) return {0, Error::ResourceLimit};
        Shape shape(plan, memory_);
        shape.fields.resize(plan.frame().size(), sir::kNone);
        for (const auto& scope : plan.scopes())
            for (std::uint32_t field = 0; field < scope.fields.size(); ++field)
                shape.fields[scope.fields[field].index()] = field;
        for (std::uint32_t row = 0; row < plan.frame().size(); ++row)
            if (plan.frame()[row].home == sir::FrameHome::Scoped) shape.ordered.push_back(row);
        std::ranges::sort(shape.ordered, [&shape](auto a, auto b) {
            return shape.layout.logical[a] < shape.layout.logical[b];
        });
        const auto id = static_cast<std::uint32_t>(shapes_.size());
        shapes_.push_back(std::move(shape));
        return {id};
    } catch (const std::bad_alloc&) {
        return {0, Error::ResourceLimit};
    } catch (const std::length_error&) {
        return {0, Error::ResourceLimit};
    }
}

ScopedReferences::Result<std::uint64_t>
ScopedReferences::make(std::uint32_t shape, const ActivationOrigin& origin, std::uint32_t element,
                       std::uint32_t owner, std::size_t depth) noexcept {
    try {
        if (shape >= shapes_.size() || !origin.valid()) return {0, Error::InvalidBinding};
        const auto& decoded = shapes_[shape];
        const auto row = origin.row().index();
        if (&origin.plan() != decoded.plan || row >= decoded.fields.size() ||
            decoded.fields[row] == sir::kNone || decoded.fields[row] != origin.binding().field())
            return {0, Error::InvalidBinding};
        const auto& field = decoded.plan->frame()[row];
        if (element >= sir::elementCount(field)) return {0, Error::OutOfRange};
        Target source{origin.binding(), shape, row,
                      std::uint64_t{element} * decoded.layout.chunks[row]};
        const auto valid = target(source, 0);
        if (!valid) return {0, valid.error};
        return insert(valid.value, owner, depth);
    } catch (const std::bad_alloc&) {
        return {0, Error::ResourceLimit};
    } catch (const std::length_error&) {
        return {0, Error::ResourceLimit};
    }
}

const ScopedReferences::Entry* ScopedReferences::find(std::uint64_t token) const noexcept {
    const auto index = static_cast<std::uint32_t>(token);
    if (index == 0 || index - 1 >= entries_.size()) return nullptr;
    const auto& entry = entries_[index - 1];
    return entry.live && entry.generation == token >> 32 ? &entry : nullptr;
}

ScopedReferences::Result<ScopedReferences::Target>
ScopedReferences::target(const Target& source, std::uint64_t words) const noexcept {
    if (!store_.region({source.binding})) return {{}, Error::InvalidBinding};
    const auto& shape = shapes_[source.shape];
    const auto& layout = shape.layout;
    const auto start = layout.logical[source.row];
    const auto max = std::numeric_limits<std::uint64_t>::max();
    if (source.word > max - start || words > max - start - source.word)
        return {{}, Error::OutOfRange};
    const auto at = start + source.word + words;
    auto found = std::upper_bound(
        shape.ordered.begin(), shape.ordered.end(), at,
        [&layout](auto address, auto row) { return address < layout.logical[row]; });
    if (found == shape.ordered.begin()) return {{}, Error::OutOfRange};
    const auto row = *--found;
    const auto& field = shape.plan->frame()[row];
    const auto within = at - layout.logical[row];
    if (field.activationScope != shape.plan->frame()[source.row].activationScope ||
        within >= sir::elementCount(field) * layout.chunks[row])
        return {{}, Error::OutOfRange};
    if (row == source.row) return {{source.binding, source.shape, row, within}};
    // The temporary lease preserves effective rights while selecting a sibling
    // field. It gives the borrowed descriptor no lasting ownership.
    auto lease = store_.retain(source.binding);
    if (!lease) return {{}, lease.error};
    const auto binding = store_.bind(lease.value, shape.fields[row]);
    if (!binding) return {{}, binding.error};
    return {{binding.value, source.shape, row, within}};
}

ScopedReferences::Result<std::uint64_t> ScopedReferences::step(std::uint64_t token,
                                                               std::uint64_t words,
                                                               std::uint32_t owner,
                                                               std::size_t depth) noexcept {
    try {
        const auto* entry = find(token);
        if (entry == nullptr) return {0, Error::InvalidBinding};
        const auto moved = target(entry->target, words);
        if (!moved) return {0, moved.error};
        return insert(moved.value, owner, depth);
    } catch (const std::bad_alloc&) {
        return {0, Error::ResourceLimit};
    } catch (const std::length_error&) {
        return {0, Error::ResourceLimit};
    }
}

ScopedReferences::Result<std::uint64_t> ScopedReferences::insert(Target target, std::uint32_t owner,
                                                                 std::size_t depth) {
    auto [where, created] = owners_.try_emplace(owner);
    auto& tickets = where->second;
    for (const auto id : tickets) {
        const auto& entry = entries_[id];
        const auto& old = entry.target;
        if (entry.depth == depth && old.shape == target.shape && old.row == target.row &&
            old.word == target.word && sameBinding(old.binding, target.binding))
            return {(std::uint64_t{entry.generation} << 32) | (std::uint64_t{id} + 1)};
    }
    bool ticket = false;
    try {
        tickets.push_back(sir::kNone);
        ticket = true;
        std::uint32_t index = free_;
        if (index == sir::kNone) {
            if (entries_.size() >= sir::kNone) {
                tickets.pop_back();
                if (created) owners_.erase(where);
                return {0, Error::ResourceLimit};
            }
            index = static_cast<std::uint32_t>(entries_.size());
            entries_.emplace_back();
        } else {
            free_ = entries_[index].next;
        }
        auto& entry = entries_[index];
        entry.target = target;
        entry.depth = depth;
        entry.live = true;
        entry.next = sir::kNone;
        tickets.back() = index;
        ++live_;
        return {(std::uint64_t{entry.generation} << 32) | (std::uint64_t{index} + 1)};
    } catch (...) {
        if (ticket) tickets.pop_back();
        if (created) owners_.erase(where);
        throw;
    }
}

ScopedReferences::Result<ScopedReferences::Selection>
ScopedReferences::selection(const Target& target, std::uint32_t width, bool word) const noexcept {
    const auto& shape = shapes_[target.shape];
    const auto chunks = shape.layout.chunks[target.row];
    const auto bits = valueWidth(shape.plan->frame()[target.row].type);
    const auto lsb = static_cast<std::uint32_t>(target.word % chunks) * 64;
    const auto count = word ? std::min(64U, bits - lsb) : width;
    if (count == 0 || count > bits - lsb) return {{}, Error::OutOfRange};
    Selection result{target.binding, static_cast<std::uint32_t>(target.word / chunks), lsb, count};
    const auto valid = store_.region({result.binding, result.element, 1, lsb, count});
    return valid ? Result<Selection>{result} : Result<Selection>{{}, valid.error};
}

ScopedReferences::Result<ScopedReferences::Selection>
ScopedReferences::resolve(std::uint64_t token, std::uint64_t words,
                          std::uint32_t width) const noexcept {
    const auto* entry = find(token);
    if (entry == nullptr) return {{}, Error::InvalidBinding};
    const auto moved = target(entry->target, words);
    return moved ? selection(moved.value, width, false) : Result<Selection>{{}, moved.error};
}
ScopedReferences::Result<ScopedReferences::Selection>
ScopedReferences::resolveWord(std::uint64_t token, std::uint64_t words) const noexcept {
    const auto* entry = find(token);
    if (entry == nullptr) return {{}, Error::InvalidBinding};
    const auto moved = target(entry->target, words);
    return moved ? selection(moved.value, 0, true) : Result<Selection>{{}, moved.error};
}

void ScopedReferences::release(std::uint32_t owner, std::size_t minimumDepth) noexcept {
    const auto found = owners_.find(owner);
    if (found == owners_.end()) return;
    auto& tickets = found->second;
    std::size_t kept = 0;
    for (const auto index : tickets) {
        auto& entry = entries_[index];
        if (entry.depth < minimumDepth) {
            tickets[kept++] = index;
            continue;
        }
        entry.live = false;
        entry.target = {};
        --live_;
        if (entry.generation == std::numeric_limits<std::uint32_t>::max()) {
            ++retired_;
        } else {
            ++entry.generation;
            entry.next = free_;
            free_ = index;
        }
    }
    tickets.erase(tickets.begin() + static_cast<std::ptrdiff_t>(kept), tickets.end());
    if (tickets.empty()) owners_.erase(found);
}

}  // namespace kirimasim::vm::detail
