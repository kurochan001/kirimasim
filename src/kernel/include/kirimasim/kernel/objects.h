// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "kirimasim/kernel/id.h"
#include "kirimasim/kernel/w4.h"

namespace kirimasim::kernel {

class Heap;
struct HeapLayoutTag;
using HeapLayoutId = Id<HeapLayoutTag>;
struct ObjectClassTag;
using ObjectClassId = Id<ObjectClassTag>;

/// The owner validates the token's Context; only numeric class/slot IDs
/// determine dispatch. Null has neither an owner nor an index.
struct ObjectToken {
    std::uint64_t bits = 0;
    const Heap* owner = nullptr;
    friend bool operator==(ObjectToken, ObjectToken) = default;
};
struct ObjectFieldId {
    ObjectClassId declaring;
    std::uint32_t index = 0;
    friend bool operator==(ObjectFieldId, ObjectFieldId) = default;
};
struct ObjectField {
    HeapLayoutId layout;
    bool constant = false;
    bool hasInitializer = false;
};
/// Immutable descriptors are installed together, allowing recursive fields
/// and first-encounter IDs independently of the base-declaration order.
struct ObjectClass {
    std::string_view name;
    ObjectClassId base = kNone;
    std::span<const ObjectClassId> interfaces;
    std::span<const ObjectField> fields;
    /// Design-wide numeric virtual slots; unrelated slots may be kNone.
    std::span<const std::uint32_t> virtualMethods;
    bool abstract = false;
    bool interfaceClass = false;
    ObjectClassId weakTarget = kNone;
};
enum class ObjectError : std::uint8_t {
    None,
    Null,
    InvalidToken,
    InvalidClass,
    InvalidField,
    InvalidValue,
    InvalidStage,
    ConstWrite,
    InvalidMethod,
    ResourceLimit,
    Busy
};
template <class T> struct ObjectResult {
    T value{};
    ObjectError error = ObjectError::None;
    explicit operator bool() const noexcept { return error == ObjectError::None; }
};

/// A selected object/field retains identity independently of the variable
/// that supplied its handle. Source selections are word intervals within
/// the field's immutable typed layout, never native addresses.
struct ObjectRef {
    ObjectToken object;
    ObjectFieldId field;
    std::size_t first = 0;
    std::size_t count = 0;
    bool readonly = false;
};
struct ObjectBitRef {
    ObjectRef field;
    std::uint64_t lsb = 0, width = 0;
};

/// Pins protect unpublished values and suspended runtime operations. The
/// Context must outlive its pins. Source holders use exact typed GC roots.
class ObjectPin {
public:
    ObjectPin() noexcept = default;
    ObjectPin(const ObjectPin&) = delete;
    ObjectPin& operator=(const ObjectPin&) = delete;
    ObjectPin(ObjectPin&& other) noexcept;
    ObjectPin& operator=(ObjectPin&& other) noexcept;
    ~ObjectPin();
    void reset() noexcept;
    [[nodiscard]] ObjectToken token() const noexcept { return value_; }
    explicit operator bool() const noexcept { return value_.bits != 0; }

private:
    friend class Heap;
    explicit ObjectPin(ObjectToken value) noexcept : value_(value) {}
    ObjectToken value_;
};

/// An unforgeable capability for one invocation of one constructor stage.
/// Destroying an unfinished capability abandons that construction without
/// rolling back source writes or invalidating an escaped receiver.
class ObjectStage {
public:
    ObjectStage() noexcept = default;
    ObjectStage(const ObjectStage&) = delete;
    ObjectStage& operator=(const ObjectStage&) = delete;
    ObjectStage(ObjectStage&& other) noexcept;
    ObjectStage& operator=(ObjectStage&& other) noexcept;
    ~ObjectStage();
    void reset() noexcept;
    explicit operator bool() const noexcept { return serial_ != 0; }

private:
    friend class Heap;
    ObjectToken object_;
    std::uint64_t serial_ = 0;
};
struct ObjectInfo {
    ObjectClassId dynamicClass;
    std::uint64_t identity = 0;
    std::uint64_t ordinal = 0;
    std::uint64_t randomState = 0;
    bool constructed = false;
};
struct ObjectStats {
    std::size_t live = 0;
    std::size_t entries = 0;
    std::size_t pinned = 0;
};

}  // namespace kirimasim::kernel
