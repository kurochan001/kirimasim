// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/heap.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

#include "heap_internal.h"
#include "kirimasim/kernel/bits.h"

namespace kirimasim::kernel {
namespace {
struct ObjectFault {
    ObjectError error;
};
template <class T, class F> ObjectResult<T> protectObject(F&& fn) noexcept {
    try {
        return {fn(), ObjectError::None};
    } catch (const ObjectFault& fault) {
        return {{}, fault.error};
    } catch (const detail::HeapFault& fault) {
        return {{},
                fault.error == HeapError::ResourceLimit ? ObjectError::ResourceLimit
                                                        : ObjectError::InvalidValue};
    } catch (const std::bad_alloc&) {
        return {{}, ObjectError::ResourceLimit};
    } catch (const std::length_error&) {
        return {{}, ObjectError::ResourceLimit};
    }
}
void require(ObjectError error) {
    if (error != ObjectError::None) throw ObjectFault{error};
}
bool sameWords(std::span<const W4> left, std::span<const W4> right) noexcept {
    return left.size() == right.size() &&
           std::equal(left.begin(), left.end(), right.begin(),
                      [](W4 a, W4 b) { return a.a == b.a && a.b == b.b; });
}
}  // namespace

ObjectPin::ObjectPin(ObjectPin&& other) noexcept : value_(std::exchange(other.value_, {})) {}
ObjectPin& ObjectPin::operator=(ObjectPin&& other) noexcept {
    if (this != &other) {
        reset();
        value_ = std::exchange(other.value_, {});
    }
    return *this;
}
ObjectPin::~ObjectPin() { reset(); }
void ObjectPin::reset() noexcept {
    if (value_.owner != nullptr) const_cast<Heap*>(value_.owner)->unpinObject(value_);
    value_ = {};
}
ObjectStage::ObjectStage(ObjectStage&& other) noexcept
    : object_(std::exchange(other.object_, {})), serial_(std::exchange(other.serial_, 0)) {}
ObjectStage& ObjectStage::operator=(ObjectStage&& other) noexcept {
    if (this != &other) {
        reset();
        object_ = std::exchange(other.object_, {});
        serial_ = std::exchange(other.serial_, 0);
    }
    return *this;
}
ObjectStage::~ObjectStage() { reset(); }
void ObjectStage::reset() noexcept {
    if (object_.owner != nullptr) const_cast<Heap*>(object_.owner)->abandonObjectStage(*this);
    object_ = {};
    serial_ = 0;
}

ObjectError Heap::addClasses(std::span<const ObjectClass> descriptions) noexcept {
    return protectObject<bool>([&] {
               if (collecting_) throw ObjectFault{ObjectError::Busy};
               if (classesReady_ || !objects_.entries.empty())
                   throw ObjectFault{ObjectError::InvalidClass};
               if (descriptions.size() >= kNone) throw ObjectFault{ObjectError::ResourceLimit};
               const auto valid = [&](ObjectClassId id) {
                   return id.index() < descriptions.size();
               };
               std::pmr::vector<Class> proposed(memory_);
               proposed.reserve(descriptions.size());
               for (std::size_t i = 0; i < descriptions.size(); ++i) {
                   const auto& source = descriptions[i];
                   if (source.fields.size() >= kNone || source.interfaces.size() >= kNone ||
                       source.virtualMethods.size() >= kNone)
                       throw ObjectFault{ObjectError::ResourceLimit};
                   if (source.base != kNone &&
                       (!valid(source.base) ||
                        descriptions[source.base.index()].interfaceClass != source.interfaceClass))
                       throw ObjectFault{ObjectError::InvalidClass};
                   if (source.interfaceClass && !source.fields.empty())
                       throw ObjectFault{ObjectError::InvalidClass};
                   if (source.weakTarget != kNone && !valid(source.weakTarget))
                       throw ObjectFault{ObjectError::InvalidClass};
                   for (std::size_t j = 0; j < source.interfaces.size(); ++j) {
                       const auto parent = source.interfaces[j];
                       if (!valid(parent) || !descriptions[parent.index()].interfaceClass ||
                           std::find(source.interfaces.begin(), source.interfaces.begin() + j,
                                     parent) != source.interfaces.begin() + j)
                           throw ObjectFault{ObjectError::InvalidClass};
                   }
                   proposed.emplace_back(memory_);
                   auto& target = proposed.back();
                   target.name.assign(source.name);
                   target.base = source.base;
                   target.fields.assign(source.fields.begin(), source.fields.end());
                   target.virtualMethods.assign(source.virtualMethods.begin(),
                                                source.virtualMethods.end());
                   target.abstract = source.abstract || source.interfaceClass;
                   target.interfaceClass = source.interfaceClass;
                   target.weakTarget = source.weakTarget;
                   for (const auto field : source.fields)
                       if (!layoutOf(field.layout)) throw ObjectFault{ObjectError::InvalidField};
               }
               // Iterative postorder also validates interface inheritance diamonds.
               struct Visit {
                   ObjectClassId type;
                   bool exit;
               };
               std::pmr::vector<Visit> work(memory_);
               std::pmr::vector<std::uint8_t> colors(descriptions.size(), 0, memory_);
               std::pmr::vector<ObjectClassId> order(memory_);
               order.reserve(descriptions.size());
               for (std::uint32_t i = 0; i < descriptions.size(); ++i) {
                   work.push_back({ObjectClassId{i}, false});
                   while (!work.empty()) {
                       const auto visit = work.back();
                       work.pop_back();
                       auto& color = colors[visit.type.index()];
                       if (visit.exit) {
                           color = 2;
                           order.push_back(visit.type);
                           continue;
                       }
                       if (color == 2) continue;
                       if (color == 1) throw ObjectFault{ObjectError::InvalidClass};
                       color = 1;
                       work.push_back({visit.type, true});
                       const auto& source = descriptions[visit.type.index()];
                       for (const auto parent : source.interfaces)
                           work.push_back({parent, false});
                       if (source.base != kNone) work.push_back({source.base, false});
                   }
               }
               for (const auto type : order) {
                   auto& target = proposed[type.index()];
                   const auto& source = descriptions[type.index()];
                   target.ancestors.push_back(type);
                   const auto inherit = [&](ObjectClassId parent) {
                       const auto& ancestors = proposed[parent.index()].ancestors;
                       target.ancestors.insert(target.ancestors.end(), ancestors.begin(),
                                               ancestors.end());
                   };
                   if (source.base != kNone) {
                       const auto& base = proposed[source.base.index()];
                       inherit(source.base);
                       target.flatFields = base.flatFields;
                       target.stages = base.stages;
                       if (target.virtualMethods.empty())
                           target.virtualMethods = base.virtualMethods;
                       if (target.virtualMethods.size() < base.virtualMethods.size())
                           throw ObjectFault{ObjectError::InvalidMethod};
                   }
                   for (const auto parent : source.interfaces)
                       inherit(parent);
                   std::sort(target.ancestors.begin(), target.ancestors.end());
                   target.ancestors.erase(
                       std::unique(target.ancestors.begin(), target.ancestors.end()),
                       target.ancestors.end());
                   if (!source.interfaceClass) target.stages.push_back(type);
                   for (std::uint32_t i = 0; i < target.fields.size(); ++i)
                       target.flatFields.push_back({type, i});
               }
               for (const auto& layout : layouts_)
                   for (const auto& word : layout.words)
                       if (word.object != kNone && !valid(word.object))
                           throw ObjectFault{ObjectError::InvalidClass};
               classes_.swap(proposed);
               classesReady_ = true;
               return true;
           })
        .error;
}
bool Heap::assignable(ObjectClassId dynamic, ObjectClassId view) const noexcept {
    if (dynamic.index() >= classes_.size() || view.index() >= classes_.size()) return false;
    const auto& ancestors = classes_[dynamic.index()].ancestors;
    return std::binary_search(ancestors.begin(), ancestors.end(), view);
}
ObjectError Heap::checkObject(ObjectToken value) const noexcept {
    if (value.bits == 0)
        return value.owner == nullptr ? ObjectError::Null : ObjectError::InvalidToken;
    if (value.owner != this || objects_.get(value.bits) == nullptr)
        return ObjectError::InvalidToken;
    return ObjectError::None;
}
W4 Heap::objectWord(ObjectToken value) const noexcept {
    return {value.bits, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(value.owner))};
}
ObjectResult<ObjectToken> Heap::wordObject(W4 value, ObjectClassId view) const noexcept {
    if (view.index() >= classes_.size()) return {{}, ObjectError::InvalidClass};
    if (value.a == 0 && value.b == 0) return {};
    if (value.b != static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(this)))
        return {{}, ObjectError::InvalidToken};
    const ObjectToken token{value.a, this};
    const auto error = checkObject(token);
    if (error != ObjectError::None) return {{}, error};
    if (!assignable(objects_.get(value.a)->type, view)) return {{}, ObjectError::InvalidClass};
    return {token, ObjectError::None};
}

std::uint64_t Heap::makeObject(Transaction& tx, ObjectClassId type, ObjectToken copy) {
    if (type.index() >= classes_.size()) throw ObjectFault{ObjectError::InvalidClass};
    const auto& descriptor = classes_[type.index()];
    if ((copy.bits == 0 && descriptor.abstract) || nextObjectId_ > INT64_MAX ||
        descriptor.ordinal == UINT64_MAX)
        throw ObjectFault{descriptor.abstract ? ObjectError::InvalidClass
                                              : ObjectError::ResourceLimit};
    Object value{type, std::pmr::vector<CellToken>(memory_),
                 std::pmr::vector<std::uint8_t>(memory_)};
    value.fields.reserve(descriptor.flatFields.size());
    value.initialized.resize(descriptor.flatFields.size(), 0);
    value.identity = nextObjectId_;
    value.ordinal = descriptor.ordinal + 1;
    value.pins = 1;
    if (copy.bits != 0) {
        const auto* source = objects_.get(copy.bits);
        value.initialized = source->initialized;
        value.randomState = source->randomState;
        value.weakTarget = source->weakTarget;
        value.nextStage = descriptor.stages.size();
    }
    const auto made = objects_.add(std::move(value), tx.objects);
    for (std::size_t i = 0; i < descriptor.flatFields.size(); ++i) {
        const auto field = descriptor.flatFields[i];
        const auto layout = classes_[field.declaring.index()].fields[field.index].layout;
        std::pmr::vector<W4> data(memory_);
        if (copy.bits != 0) {
            const auto old = objects_.get(copy.bits)->fields[i];
            const auto& words = cells_.get(old.bits)->words;
            data.assign(words.begin(), words.end());
            recordRead(HeapRoot::cell(old));
        } else {
            data.resize(words(layout));
            defaults(layout, data);
        }
        const auto cell = cloneCell(tx, layout, data);
        cells_.get(cell.bits)->objectOwner = made;
        objects_.get(made)->fields.push_back(cell);
    }
    return made;
}
ObjectResult<ObjectPin> Heap::allocateObject(ObjectClassId type, Rng* parent, ObjectToken copy,
                                             ObjectToken weak) noexcept {
    return protectObject<ObjectPin>([&] {
        if (collecting_) throw ObjectFault{ObjectError::Busy};
        Transaction tx(*this);
        const auto bits = makeObject(tx, type, copy);
        auto& value = *objects_.get(bits);
        if (parent != nullptr) {
            value.randomState = parent->next();
            value.weakTarget = weak;
        }
        ++classes_[type.index()].ordinal;
        ++nextObjectId_;
        tx.committed = true;
        return ObjectPin{ObjectToken{bits, this}};
    });
}
ObjectResult<ObjectPin> Heap::newObject(ObjectClassId type, Rng& parent) noexcept {
    return allocateObject(type, &parent, {}, {});
}
ObjectResult<ObjectPin> Heap::copyObject(ObjectToken source) noexcept {
    const auto error = checkObject(source);
    if (error != ObjectError::None) return {{}, error};
    return allocateObject(objects_.get(source.bits)->type, nullptr, source, {});
}
ObjectResult<ObjectPin> Heap::pinObject(ObjectToken value) noexcept {
    const auto error = checkObject(value);
    if (error == ObjectError::Null) return {};
    if (error != ObjectError::None) return {{}, error};
    if (collecting_) return {{}, ObjectError::Busy};
    auto& object = *objects_.get(value.bits);
    if (object.pins == UINT64_MAX) return {{}, ObjectError::ResourceLimit};
    ++object.pins;
    return {ObjectPin{value}, ObjectError::None};
}
void Heap::unpinObject(ObjectToken value) noexcept {
    if (checkObject(value) != ObjectError::None) return;
    auto& object = *objects_.get(value.bits);
    if (object.pins != 0) --object.pins;
    changed_ = true;
}
ObjectResult<ObjectInfo> Heap::objectInfo(ObjectToken value) const noexcept {
    const auto error = checkObject(value);
    if (error != ObjectError::None) return {{}, error};
    const auto& object = *objects_.get(value.bits);
    return {{object.type, object.identity, object.ordinal, object.randomState,
             !object.abandoned && object.nextStage == classes_[object.type.index()].stages.size()},
            ObjectError::None};
}
ObjectResult<std::uint64_t> Heap::objectId(ObjectToken value) const noexcept {
    const auto error = checkObject(value);
    if (error == ObjectError::Null) return {};
    if (error != ObjectError::None) return {{}, error};
    return {objects_.get(value.bits)->identity, ObjectError::None};
}
ObjectResult<bool> Heap::castObject(ObjectToken value, ObjectClassId source,
                                    ObjectClassId destination) const noexcept {
    if (destination.index() >= classes_.size()) return {{}, ObjectError::InvalidClass};
    const auto error = checkObject(value);
    if (error != ObjectError::None && error != ObjectError::Null) return {{}, error};
    if (source == kNone) {
        if (error != ObjectError::Null) return {{}, ObjectError::InvalidClass};
        return {true, ObjectError::None};
    }
    if (source.index() >= classes_.size()) return {{}, ObjectError::InvalidClass};
    if (error != ObjectError::Null && !assignable(objects_.get(value.bits)->type, source))
        return {{}, ObjectError::InvalidClass};
    if (assignable(source, destination)) return {true, ObjectError::None};
    return {error != ObjectError::Null && assignable(objects_.get(value.bits)->type, destination),
            ObjectError::None};
}
ObjectError Heap::seedObject(ObjectToken value, std::uint32_t seed) noexcept {
    const auto error = checkObject(value);
    if (error != ObjectError::None) return error;
    if (collecting_) return ObjectError::Busy;
    objects_.get(value.bits)->randomState = seed;
    return ObjectError::None;
}
ObjectResult<std::uint32_t> Heap::virtualMethod(ObjectToken value, ObjectClassId view,
                                                std::uint32_t slot) const noexcept {
    const auto error = checkObject(value);
    if (error != ObjectError::None) return {{}, error};
    const auto type = objects_.get(value.bits)->type;
    if (!assignable(type, view)) return {{}, ObjectError::InvalidClass};
    const auto& methods = classes_[type.index()].virtualMethods;
    if (slot >= methods.size() || methods[slot] == kNone) return {{}, ObjectError::InvalidMethod};
    return {methods[slot], ObjectError::None};
}
ObjectResult<std::size_t> Heap::objectField(ObjectToken value, ObjectFieldId field) const noexcept {
    const auto error = checkObject(value);
    if (error != ObjectError::None) return {{}, error};
    const auto type = objects_.get(value.bits)->type;
    if (!assignable(type, field.declaring) ||
        field.index >= classes_[field.declaring.index()].fields.size())
        return {{}, ObjectError::InvalidField};
    const auto& fields = classes_[type.index()].flatFields;
    const auto found = std::find(fields.begin(), fields.end(), field);
    if (found == fields.end()) return {{}, ObjectError::InvalidField};
    return {static_cast<std::size_t>(found - fields.begin()), ObjectError::None};
}
ObjectResult<ObjectRef> Heap::objectRef(ObjectToken value, ObjectFieldId field,
                                        bool readonly) const noexcept {
    const auto index = objectField(value, field);
    if (!index) return {{}, index.error};
    const auto layout = classes_[field.declaring.index()].fields[field.index].layout;
    return {{value, field, 0, words(layout), readonly}, ObjectError::None};
}
ObjectError Heap::readObject(ObjectRef ref, std::span<W4> output) const noexcept {
    const auto index = objectField(ref.object, ref.field);
    if (!index) return index.error;
    const auto cell = objects_.get(ref.object.bits)->fields[index.value];
    const auto& values = cells_.get(cell.bits)->words;
    if (ref.first > values.size() || ref.count == 0 || ref.count > values.size() - ref.first ||
        output.size() != ref.count)
        return ObjectError::InvalidField;
    std::copy_n(values.begin() + static_cast<std::ptrdiff_t>(ref.first), ref.count, output.begin());
    recordRead(HeapRoot::cell(cell));
    return ObjectError::None;
}
ObjectResult<ObjectBitRef> Heap::selectObjectBits(ObjectRef ref, std::uint64_t lsb,
                                                  std::uint64_t width) const noexcept {
    const auto index = objectField(ref.object, ref.field);
    if (!index) return {{}, index.error};
    const auto layout = classes_[ref.field.declaring.index()].fields[ref.field.index].layout;
    const auto& words = layouts_[layout.index()].words;
    if (ref.first > words.size() || ref.count == 0 || ref.count > words.size() - ref.first ||
        ref.count > UINT64_MAX / 64)
        return {{}, ObjectError::InvalidField};
    const auto selected = std::span(words).subspan(ref.first, ref.count);
    for (std::size_t i = 0; i < selected.size(); ++i) {
        const auto& word = selected[i];
        if (word.string || word.nested != kNone || word.object != kNone ||
            (word.mask & (word.mask + 1)) != 0 ||
            (i + 1 != selected.size() && word.mask != UINT64_MAX))
            return {{}, ObjectError::InvalidField};
    }
    const auto bits = (ref.count - 1) * 64 + std::bit_width(selected.back().mask);
    if (width == 0 || lsb >= bits || width > bits - lsb) return {{}, ObjectError::InvalidField};
    return {{ref, lsb, width}, ObjectError::None};
}
ObjectResult<ObjectBitRef> Heap::selectObjectBits(ObjectBitRef ref, std::uint64_t lsb,
                                                  std::uint64_t width) const noexcept {
    const auto parent = selectObjectBits(ref.field, ref.lsb, ref.width);
    if (!parent) return parent;
    if (width == 0 || lsb >= ref.width || width > ref.width - lsb)
        return {{}, ObjectError::InvalidField};
    return {{ref.field, ref.lsb + lsb, width}, ObjectError::None};
}
ObjectError Heap::readObject(ObjectBitRef ref, std::span<W4> output) const noexcept {
    const auto checked = selectObjectBits(ref.field, ref.lsb, ref.width);
    if (!checked) return checked.error;
    if (output.size() != (ref.width - 1) / 64 + 1) return ObjectError::InvalidValue;
    const auto index = objectField(ref.field.object, ref.field.field);
    const auto cell = objects_.get(ref.field.object.bits)->fields[index.value];
    const auto words = std::span(cells_.get(cell.bits)->words).subspan(ref.field.first);
    for (std::size_t i = 0; i < output.size(); ++i) {
        const auto position = ref.lsb + i * 64;
        const auto word = position / 64;
        const auto shift = static_cast<unsigned>(position % 64);
        const auto count = static_cast<unsigned>(std::min(std::uint64_t{64}, ref.width - i * 64));
        W4 value{words[word].a >> shift, words[word].b >> shift};
        if (shift != 0 && shift + count > 64) {
            value.a |= words[word + 1].a << (64 - shift);
            value.b |= words[word + 1].b << (64 - shift);
        }
        output[i] = {value.a & mask(count), value.b & mask(count)};
    }
    recordRead(HeapRoot::cell(cell));
    return ObjectError::None;
}
ObjectError Heap::writeObject(ObjectBitRef ref, std::span<const W4> input,
                              const ObjectStage* stage) noexcept {
    if (collecting_) return ObjectError::Busy;
    const auto checked = selectObjectBits(ref.field, ref.lsb, ref.width);
    if (!checked) return checked.error;
    if (input.size() != (ref.width - 1) / 64 + 1) return ObjectError::InvalidValue;
    return protectObject<bool>([&] {
               const auto index = objectField(ref.field.object, ref.field.field);
               const auto cell = objects_.get(ref.field.object.bits)->fields[index.value];
               const auto old = std::span(cells_.get(cell.bits)->words).subspan(ref.field.first);
               const auto first = ref.lsb / 64;
               const auto last = (ref.lsb + ref.width - 1) / 64;
               std::pmr::vector<W4> values(old.begin() + static_cast<std::ptrdiff_t>(first),
                                           old.begin() + static_cast<std::ptrdiff_t>(last + 1),
                                           memory_);
               for (std::size_t i = 0; i < input.size(); ++i) {
                   const auto position = ref.lsb % 64 + i * 64;
                   const auto word = position / 64;
                   const auto shift = static_cast<unsigned>(position % 64);
                   const auto count =
                       static_cast<unsigned>(std::min(std::uint64_t{64}, ref.width - i * 64));
                   const auto m = mask(count);
                   values[word].a = (values[word].a & ~(m << shift)) | ((input[i].a & m) << shift);
                   values[word].b = (values[word].b & ~(m << shift)) | ((input[i].b & m) << shift);
                   if (shift != 0 && shift + count > 64) {
                       const auto done = 64 - shift;
                       values[word + 1].a =
                           (values[word + 1].a & ~(m >> done)) | ((input[i].a & m) >> done);
                       values[word + 1].b =
                           (values[word + 1].b & ~(m >> done)) | ((input[i].b & m) >> done);
                   }
               }
               auto target = ref.field;
               target.first += first;
               target.count = values.size();
               require(writeObject(target, values, stage));
               return true;
           })
        .error;
}
bool Heap::validObjectStage(const ObjectStage& stage, const Object& object) const noexcept {
    return stage.object_.owner == this && stage.serial_ != 0 &&
           stage.serial_ == object.stageSerial && !object.abandoned;
}
ObjectError Heap::writeObject(ObjectRef ref, std::span<const W4> input,
                              const ObjectStage* stage) noexcept {
    CellToken changed;
    const auto result = protectObject<bool>([&] {
        if (collecting_) throw ObjectFault{ObjectError::Busy};
        const auto index = objectField(ref.object, ref.field);
        require(index.error);
        const auto& descriptor = classes_[ref.field.declaring.index()].fields[ref.field.index];
        auto* object = objects_.get(ref.object.bits);
        const bool privileged =
            stage != nullptr && stage->object_ == ref.object && validObjectStage(*stage, *object);
        if (stage != nullptr && !privileged) throw ObjectFault{ObjectError::InvalidStage};
        const bool ownStage =
            privileged &&
            classes_[object->type.index()].stages[object->nextStage] == ref.field.declaring;
        if (ref.readonly ||
            (descriptor.constant && (!ownStage || object->initialized[index.value] ||
                                     object->body == descriptor.hasInitializer)))
            throw ObjectFault{ObjectError::ConstWrite};
        const auto cell = object->fields[index.value];
        const auto& old = cells_.get(cell.bits)->words;
        if (ref.first > old.size() || ref.count == 0 || ref.count > old.size() - ref.first ||
            input.size() != ref.count)
            throw ObjectFault{ObjectError::InvalidField};
        std::pmr::vector<W4> assembled(old.begin(), old.end(), memory_);
        std::copy(input.begin(), input.end(),
                  assembled.begin() + static_cast<std::ptrdiff_t>(ref.first));
        if (checkData(descriptor.layout, assembled) != HeapError::None)
            throw ObjectFault{ObjectError::InvalidValue};
        Transaction tx(*this);
        auto normalized = cloneWordValues(
            tx, std::span(layouts_[descriptor.layout.index()].words).subspan(ref.first, ref.count),
            input);
        // Reacquire after nested allocations into the cell pool.
        auto& target = cells_.get(cell.bits)->words;
        const bool different =
            !sameWords(std::span(target).subspan(ref.first, ref.count), normalized);
        std::copy(normalized.begin(), normalized.end(),
                  target.begin() + static_cast<std::ptrdiff_t>(ref.first));
        if (descriptor.constant || (ownStage && !objects_.get(ref.object.bits)->body))
            objects_.get(ref.object.bits)->initialized[index.value] = 1;
        tx.committed = true;
        if (different) changed = cell;
        return true;
    });
    if (result && changed.bits != 0) recordChange(HeapRoot::cell(changed));
    return result.error;
}
ObjectResult<ObjectStage> Heap::beginObjectStage(ObjectToken value,
                                                 ObjectClassId declaring) noexcept {
    const auto error = checkObject(value);
    if (error != ObjectError::None) return {{}, error};
    if (collecting_) return {{}, ObjectError::Busy};
    auto& object = *objects_.get(value.bits);
    const auto& stages = classes_[object.type.index()].stages;
    if (object.abandoned || object.stageSerial != 0 || object.nextStage >= stages.size() ||
        stages[object.nextStage] != declaring)
        return {{}, ObjectError::InvalidStage};
    if (nextObjectStage_ == UINT64_MAX || object.pins == UINT64_MAX)
        return {{}, ObjectError::ResourceLimit};
    object.stageSerial = nextObjectStage_++;
    object.body = false;
    ++object.pins;
    ObjectStage stage;
    stage.object_ = value;
    stage.serial_ = object.stageSerial;
    return {std::move(stage), ObjectError::None};
}
ObjectError Heap::beginObjectBody(ObjectStage& stage) noexcept {
    const auto error = checkObject(stage.object_);
    if (error != ObjectError::None) return error;
    if (collecting_) return ObjectError::Busy;
    auto& object = *objects_.get(stage.object_.bits);
    if (!validObjectStage(stage, object) || object.body) return ObjectError::InvalidStage;
    const auto& type = classes_[object.type.index()];
    const auto declaring = type.stages[object.nextStage];
    for (std::size_t i = 0; i < type.flatFields.size(); ++i) {
        const auto field = type.flatFields[i];
        if (field.declaring == declaring &&
            classes_[declaring.index()].fields[field.index].hasInitializer &&
            !object.initialized[i])
            return ObjectError::InvalidStage;
    }
    object.body = true;
    return ObjectError::None;
}
ObjectError Heap::finishObjectStage(ObjectStage& stage) noexcept {
    const auto error = checkObject(stage.object_);
    if (error != ObjectError::None) return error;
    if (collecting_) return ObjectError::Busy;
    auto& object = *objects_.get(stage.object_.bits);
    if (!validObjectStage(stage, object) || !object.body) return ObjectError::InvalidStage;
    object.stageSerial = 0;
    ++object.nextStage;
    --object.pins;
    stage.object_ = {};
    stage.serial_ = 0;
    changed_ = true;
    return ObjectError::None;
}
void Heap::abandonObjectStage(ObjectStage& stage) noexcept {
    if (checkObject(stage.object_) != ObjectError::None) return;
    auto& object = *objects_.get(stage.object_.bits);
    if (!validObjectStage(stage, object)) return;
    object.abandoned = true;
    object.stageSerial = 0;
    if (object.pins != 0) --object.pins;
    changed_ = true;
}
ObjectResult<ObjectPin> Heap::newWeakObject(ObjectClassId type, ObjectToken target,
                                            Rng& parent) noexcept {
    if (type.index() >= classes_.size() || classes_[type.index()].weakTarget == kNone)
        return {{}, ObjectError::InvalidClass};
    const auto error = checkObject(target);
    if (error != ObjectError::None && error != ObjectError::Null) return {{}, error};
    if (target.bits != 0 &&
        !assignable(objects_.get(target.bits)->type, classes_[type.index()].weakTarget))
        return {{}, ObjectError::InvalidClass};
    return allocateObject(type, &parent, {}, target);
}
ObjectResult<ObjectPin> Heap::lockWeakObject(ObjectToken weak) noexcept {
    const auto error = checkObject(weak);
    if (error != ObjectError::None) return {{}, error};
    const auto& value = *objects_.get(weak.bits);
    if (classes_[value.type.index()].weakTarget == kNone) return {{}, ObjectError::InvalidClass};
    recordRead(HeapRoot::object(weak));
    return pinObject(value.weakTarget);
}
ObjectError Heap::clearWeakObject(ObjectToken weak) noexcept {
    const auto error = checkObject(weak);
    if (error != ObjectError::None) return error;
    if (collecting_) return ObjectError::Busy;
    auto& value = *objects_.get(weak.bits);
    if (classes_[value.type.index()].weakTarget == kNone) return ObjectError::InvalidClass;
    if (value.weakTarget.bits != 0) {
        value.weakTarget = {};
        changed_ = true;
        recordChange(HeapRoot::object(weak));
    }
    return ObjectError::None;
}
ObjectStats Heap::objectStats() const noexcept {
    ObjectStats stats{0, objects_.entries.size(), 0};
    for (const auto& entry : objects_.entries)
        if (entry.value) {
            ++stats.live;
            if (entry.value->pins != 0) ++stats.pinned;
        }
    return stats;
}

}  // namespace kirimasim::kernel
