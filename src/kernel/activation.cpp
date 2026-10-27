// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/activation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

#include "kirimasim/kernel/bits.h"

namespace kirimasim::kernel {
namespace {
std::uint32_t chunks(std::uint32_t width) {
    return static_cast<std::uint32_t>((std::uint64_t{width} + 63) / 64);
}
bool integral(ActivationKind kind) {
    return kind == ActivationKind::Two || kind == ActivationKind::Four;
}
ActivationToken tokenOf(std::uint32_t index, std::uint32_t generation) {
    return {(std::uint64_t{generation} << 32) | (std::uint64_t{index} + 1)};
}
}  // namespace

ActivationLease::ActivationLease(ActivationLease&& other) noexcept
    : store_(std::exchange(other.store_, nullptr)), token_(std::exchange(other.token_, {})),
      readonly_(std::exchange(other.readonly_, false)) {}
ActivationLease& ActivationLease::operator=(ActivationLease&& other) noexcept {
    if (this != &other) {
        reset();
        store_ = std::exchange(other.store_, nullptr);
        token_ = std::exchange(other.token_, {});
        readonly_ = std::exchange(other.readonly_, false);
    }
    return *this;
}
ActivationLease::~ActivationLease() { reset(); }
ActivationLease::operator bool() const noexcept {
    return store_ != nullptr && store_->find(token_) != nullptr;
}
void ActivationLease::reset() noexcept {
    if (store_ != nullptr) store_->release(token_);
    store_ = nullptr;
    token_ = {};
    readonly_ = false;
}

ActivationStore::ActivationStore(std::pmr::memory_resource* memory)
    : memory_(memory), layouts_(memory), records_(memory) {}

ActivationResult<ActivationLayoutId>
ActivationStore::addLayout(std::span<const ActivationField> fields) noexcept {
    if (layouts_.size() >= kNone || fields.size() >= kNone)
        return {{}, ActivationError::ResourceLimit};
    // Check every type, dimension and offset before allocating or publishing
    // anything. The second pass only copies already validated immutable input.
    const auto elementsOf = [](const ActivationField& input) -> ActivationResult<std::uint32_t> {
        if (input.width == 0) return {{}, ActivationError::InvalidLayout};
        switch (input.kind) {
            case ActivationKind::Two:
            case ActivationKind::Four: break;
            case ActivationKind::ShortReal:
                if (input.width != 32) return {{}, ActivationError::InvalidLayout};
                break;
            case ActivationKind::Real:
            case ActivationKind::String:
            case ActivationKind::Chandle:
            case ActivationKind::Container:
            case ActivationKind::Cell:
            case ActivationKind::Event:
                if (input.width != 64) return {{}, ActivationError::InvalidLayout};
                break;
            default: return {{}, ActivationError::InvalidLayout};
        }
        std::uint64_t elements = 1;
        for (const auto& dim : input.dims) {
            const auto lo = static_cast<std::uint64_t>(std::min(dim.left, dim.right));
            const auto hi = static_cast<std::uint64_t>(std::max(dim.left, dim.right));
            const auto span = hi - lo;
            if (span >= std::numeric_limits<std::uint32_t>::max() ||
                elements > std::numeric_limits<std::uint32_t>::max() / (span + 1))
                return {{}, ActivationError::InvalidLayout};
            elements *= span + 1;
        }
        return {static_cast<std::uint32_t>(elements), ActivationError::None};
    };
    std::uint64_t total = 0;
    for (const auto& input : fields) {
        const auto elements = elementsOf(input);
        if (!elements) return {{}, elements.error};
        const auto stride = input.kind == ActivationKind::Four ? 2U : 1U;
        const std::uint64_t perElement = std::uint64_t{chunks(input.width)} * stride;
        const auto limit = static_cast<std::uint64_t>(std::numeric_limits<std::ptrdiff_t>::max()) /
                           sizeof(std::uint64_t);
        if (elements.value > limit / perElement || total > limit - elements.value * perElement)
            return {{}, ActivationError::ResourceLimit};
        total += elements.value * perElement;
    }
    try {
        Layout layout(memory_);
        layout.fields.reserve(fields.size());
        for (const auto& input : fields) {
            const auto elements = elementsOf(input);
            if (!elements) return {{}, elements.error};
            Field field(memory_);
            field.kind = input.kind;
            field.width = input.width;
            field.elements = elements.value;
            field.chunks = chunks(input.width);
            field.offset = layout.words;
            field.dims.assign(input.dims.begin(), input.dims.end());
            layout.words += std::uint64_t{field.elements} * field.chunks *
                            (input.kind == ActivationKind::Four ? 2U : 1U);
            layout.fields.push_back(std::move(field));
        }
        const ActivationLayoutId id{static_cast<std::uint32_t>(layouts_.size())};
        layouts_.push_back(std::move(layout));
        return {id, ActivationError::None};
    } catch (const std::bad_alloc&) {
        return {{}, ActivationError::ResourceLimit};
    } catch (const std::length_error&) {
        return {{}, ActivationError::ResourceLimit};
    }
}

ActivationResult<ActivationLease> ActivationStore::create(ActivationLayoutId layout) noexcept {
    if (layout.index() >= layouts_.size()) return {{}, ActivationError::InvalidLayout};
    const auto count = layouts_[layout.index()].words;
    if (leases_ == UINT64_MAX || live_ == UINT64_MAX || words_ > UINT64_MAX - count ||
        (free_ == kNone && records_.size() >= kNone))
        return {{}, ActivationError::ResourceLimit};
    try {
        std::pmr::vector<std::uint64_t> values(memory_);
        values.resize(static_cast<std::size_t>(count), 0);
        const auto index = free_ == kNone ? static_cast<std::uint32_t>(records_.size()) : free_;
        if (free_ == kNone) records_.emplace_back(memory_);
        auto& record = records_[index];
        if (free_ != kNone) free_ = record.next;
        record.next = kNone;
        record.layout = layout;
        record.values = std::move(values);
        record.leases = 1;
        ++live_;
        ++leases_;
        words_ += count;
        rootsChanged_ = true;
        eventRootsChanged_ = true;
        return {ActivationLease(this, tokenOf(index, record.generation)), ActivationError::None};
    } catch (const std::bad_alloc&) {
        return {{}, ActivationError::ResourceLimit};
    } catch (const std::length_error&) {
        return {{}, ActivationError::ResourceLimit};
    }
}

const ActivationStore::Record* ActivationStore::find(ActivationToken token) const noexcept {
    const auto low = static_cast<std::uint32_t>(token.bits);
    if (low == 0 || low - 1 >= records_.size()) return nullptr;
    const auto& record = records_[low - 1];
    return record.leases != 0 && record.generation == (token.bits >> 32) ? &record : nullptr;
}
ActivationStore::Record* ActivationStore::find(ActivationToken token) noexcept {
    return const_cast<Record*>(std::as_const(*this).find(token));
}
const ActivationStore::Field*
ActivationStore::fieldOf(const ActivationBinding& binding) const noexcept {
    if (binding.store_ != this) return nullptr;
    const auto* record = find(binding.token_);
    if (record == nullptr) return nullptr;
    const auto& fields = layouts_[record->layout.index()].fields;
    return binding.field_ < fields.size() ? &fields[binding.field_] : nullptr;
}
ActivationResult<ActivationBinding> ActivationStore::bind(const ActivationLease& lease,
                                                          std::uint32_t field,
                                                          bool readonly) const noexcept {
    ActivationBinding binding;
    binding.store_ = lease.store_;
    binding.token_ = lease.token_;
    binding.field_ = field;
    binding.readonly_ = readonly || lease.readonly_;
    if (fieldOf(binding) == nullptr) return {{}, ActivationError::InvalidBinding};
    return {binding, ActivationError::None};
}
ActivationResult<ActivationBinding>
ActivationStore::readOnly(const ActivationBinding& binding) const noexcept {
    if (fieldOf(binding) == nullptr) return {{}, ActivationError::InvalidBinding};
    auto result = binding;
    result.readonly_ = true;
    return {result, ActivationError::None};
}
ActivationResult<ActivationLease> ActivationStore::retainToken(ActivationToken token,
                                                               bool readonly) noexcept {
    auto* record = find(token);
    if (record == nullptr) return {{}, ActivationError::InvalidBinding};
    if (record->leases == UINT64_MAX || leases_ == UINT64_MAX)
        return {{}, ActivationError::ResourceLimit};
    ++record->leases;
    ++leases_;
    return {ActivationLease(this, token, readonly), ActivationError::None};
}
ActivationResult<ActivationLease> ActivationStore::retain(const ActivationLease& lease) noexcept {
    if (lease.store_ != this) return {{}, ActivationError::InvalidBinding};
    return retainToken(lease.token_, lease.readonly_);
}
ActivationResult<ActivationLease>
ActivationStore::retain(const ActivationBinding& binding) noexcept {
    if (fieldOf(binding) == nullptr) return {{}, ActivationError::InvalidBinding};
    return retainToken(binding.token_, binding.readonly_);
}
void ActivationStore::release(ActivationToken token) noexcept {
    auto* record = find(token);
    if (record == nullptr) return;
    --record->leases;
    --leases_;
    if (record->leases == 0) reclaim(static_cast<std::uint32_t>(token.bits) - 1);
}
void ActivationStore::reclaim(std::uint32_t index) noexcept {
    auto& record = records_[index];
    words_ -= layouts_[record.layout.index()].words;
    --live_;
    std::pmr::vector<std::uint64_t>(memory_).swap(record.values);
    rootsChanged_ = true;
    eventRootsChanged_ = true;
    if (record.generation != UINT32_MAX) {
        ++record.generation;
        record.next = free_;
        free_ = index;
    }
}

ActivationResult<ActivationRegion>
ActivationStore::region(const ActivationPart& part) const noexcept {
    const auto* field = fieldOf(part.binding);
    if (field == nullptr) return {{}, ActivationError::InvalidBinding};
    const auto width = part.width == 0 ? field->width : part.width;
    if (part.numElems == 0 || part.firstElem >= field->elements ||
        part.numElems > field->elements - part.firstElem || part.lsb > field->width ||
        width > field->width - part.lsb ||
        (!integral(field->kind) && (part.lsb != 0 || width != field->width)))
        return {{}, ActivationError::OutOfRange};
    return {
        {part.binding.token_, part.binding.field_, part.firstElem, part.numElems, part.lsb, width},
        ActivationError::None};
}

ActivationError ActivationStore::read(const ActivationBinding& binding, std::uint32_t element,
                                      std::span<W4> value, std::uint32_t lsb,
                                      std::uint32_t width) const noexcept {
    const auto* field = fieldOf(binding);
    if (field == nullptr) return ActivationError::InvalidBinding;
    if (width == 0) width = field->width;
    if (element >= field->elements || lsb > field->width || width > field->width - lsb ||
        (!integral(field->kind) && (lsb != 0 || width != field->width)))
        return ActivationError::OutOfRange;
    if (value.size() != chunks(width)) return ActivationError::InvalidValue;
    const auto stride = field->kind == ActivationKind::Four ? 2U : 1U;
    const auto* words = find(binding.token_)->values.data() + field->offset +
                        std::uint64_t{element} * field->chunks * stride;
    for (std::uint32_t i = 0; i < chunks(width); ++i) {
        const auto offset = std::uint64_t{i} * 64;
        const auto bits =
            static_cast<unsigned>(std::min(std::uint64_t{width} - offset, std::uint64_t{64}));
        value[i] = {readField(words, stride, std::uint64_t{lsb} + offset, bits),
                    stride == 2 ? readField(words + 1, 2, std::uint64_t{lsb} + offset, bits) : 0};
    }
    if (readHook_.fn != nullptr)
        readHook_.fn(readHook_.frame, {binding.token_, binding.field_, element, lsb, width});
    return ActivationError::None;
}
ActivationError ActivationStore::write(const ActivationBinding& binding, std::uint32_t element,
                                       std::span<const W4> value, std::uint32_t lsb,
                                       std::uint32_t width) noexcept {
    const auto* field = fieldOf(binding);
    if (field == nullptr) return ActivationError::InvalidBinding;
    if (binding.readonly_) return ActivationError::ReadOnly;
    if (width == 0) width = field->width;
    if (element >= field->elements || lsb > field->width || width > field->width - lsb ||
        (!integral(field->kind) && (lsb != 0 || width != field->width)))
        return ActivationError::OutOfRange;
    if (value.size() != chunks(width) || (!integral(field->kind) && value[0].b != 0))
        return ActivationError::InvalidValue;
    const auto stride = field->kind == ActivationKind::Four ? 2U : 1U;
    auto* words = find(binding.token_)->values.data() + field->offset +
                  std::uint64_t{element} * field->chunks * stride;
    bool changed = false;
    for (std::uint32_t i = 0; i < chunks(width); ++i) {
        const auto offset = std::uint64_t{i} * 64;
        const auto bits =
            static_cast<unsigned>(std::min(std::uint64_t{width} - offset, std::uint64_t{64}));
        const auto position = std::uint64_t{lsb} + offset;
        const auto normalized = field->kind == ActivationKind::Two ? to2(value[i]) : value[i];
        const auto a = normalized.a & mask(bits);
        const auto b = normalized.b & mask(bits);
        changed = changed || readField(words, stride, position, bits) != a ||
                  (stride == 2 && readField(words + 1, 2, position, bits) != b);
        writeField(words, stride, position, bits, a);
        if (stride == 2) writeField(words + 1, 2, position, bits, b);
    }
    if (changed) {
        if (field->kind == ActivationKind::Event) eventRootsChanged_ = true;
        if (field->kind == ActivationKind::Container || field->kind == ActivationKind::Cell)
            rootsChanged_ = true;
        if (changeHook_.fn != nullptr)
            changeHook_.fn(changeHook_.frame,
                           {binding.token_, binding.field_, element, lsb, width});
    }
    return ActivationError::None;
}
ActivationHook ActivationStore::setReadHook(ActivationHook hook) noexcept {
    return std::exchange(readHook_, hook);
}
ActivationHook ActivationStore::setChangeHook(ActivationHook hook) noexcept {
    return std::exchange(changeHook_, hook);
}
void ActivationStore::visitRoots(void (*visitor)(void*, HeapRoot) noexcept,
                                 void* frame) const noexcept {
    if (visitor == nullptr) return;
    for (const auto& record : records_) {
        if (record.leases == 0) continue;
        for (const auto& field : layouts_[record.layout.index()].fields) {
            if (field.kind != ActivationKind::Container && field.kind != ActivationKind::Cell)
                continue;
            for (std::uint32_t e = 0; e < field.elements; ++e) {
                const auto bits = record.values[static_cast<std::size_t>(field.offset) + e];
                if (bits == 0) continue;
                visitor(frame, {field.kind == ActivationKind::Container ? HeapRoot::Kind::Container
                                                                        : HeapRoot::Kind::Cell,
                                bits});
            }
        }
    }
}
bool ActivationStore::visitStringRoots(bool (*visitor)(void*, std::uint64_t) noexcept,
                                       void* frame) const noexcept {
    for (const auto& record : records_) {
        if (record.leases == 0) continue;
        for (const auto& field : layouts_[record.layout.index()].fields) {
            if (field.kind != ActivationKind::String) continue;
            for (std::uint32_t e = 0; e < field.elements; ++e)
                if (visitor == nullptr ||
                    !visitor(frame, record.values[static_cast<std::size_t>(field.offset) + e]))
                    return false;
        }
    }
    return true;
}
ManagedEventError ActivationStore::visitEventRoots(ManagedEventRoots& roots) const noexcept {
    for (const auto& record : records_) {
        if (record.leases == 0) continue;
        for (const auto& field : layouts_[record.layout.index()].fields) {
            if (field.kind != ActivationKind::Event) continue;
            for (std::uint32_t e = 0; e < field.elements; ++e) {
                const auto bits = record.values[static_cast<std::size_t>(field.offset) + e];
                // Generation zero is reserved for permanent legacy event slots.
                if ((bits >> 32) == 0) continue;
                if (const auto error = roots.add(ManagedEventToken{bits});
                    error != ManagedEventError::None)
                    return error;
            }
        }
    }
    return ManagedEventError::None;
}
ManagedEventRootProvider ActivationStore::eventRootProvider() const noexcept {
    return {this, [](const void* frame, ManagedEventRoots& roots) noexcept {
                return static_cast<const ActivationStore*>(frame)->visitEventRoots(roots);
            }};
}
ActivationStats ActivationStore::stats() const noexcept {
    return {live_, leases_, words_, records_.size()};
}
void ActivationStore::clear() noexcept {
    for (std::uint32_t i = 0; i < records_.size(); ++i) {
        if (records_[i].leases == 0) continue;
        leases_ -= records_[i].leases;
        records_[i].leases = 0;
        reclaim(i);
    }
}

}  // namespace kirimasim::kernel
