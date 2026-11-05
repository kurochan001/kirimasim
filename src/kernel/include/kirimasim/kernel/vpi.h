// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/kernel/store.h"
#include "kirimasim/kernel/w4.h"
#include "vpi_user.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kirimasim::kernel {
class Context;
enum class Region : std::uint8_t;

struct VpiType {
    std::uint32_t width = 0;
    bool four = false, signedValue = false, real = false;
    friend bool operator==(VpiType, VpiType) = default;
};
struct VpiValue {
    VpiType type;
    std::vector<W4> words;
};
/// Captured access capability behind opaque public handles; no SIR/slang type.
struct VpiBinding {
    SlotBits slot;
    bool isSlot = false, writable = false, net = false;
    std::uint32_t rootWidth = 0;
    void* owner = nullptr;
    std::uint64_t first = 0, second = 0;
    bool (*read)(const VpiBinding&, std::span<W4>) = nullptr;
    bool (*write)(const VpiBinding&, std::span<const W4>) = nullptr;
    bool netWritable = false;      ///< Verified whole physical declaration, never a language ref.
    std::uint64_t generation = 0;  ///< Owner-checked activation generation for transient views.
};
struct VpiDeclaration {
    std::int64_t left = 0, right = 0;
    bool scalar = false, integer = false;
};
struct VpiArgument {
    VpiValue value;
    VpiBinding binding;
    std::string name;
    bool available = true;
    std::optional<VpiDeclaration> declaration;
};
struct VpiObjectInfo {
    std::string name, file, scopeName, parentName, leafName;
    std::uint32_t line = 0;
    int kind = vpiReg, unit = -9, precision = -9;
    VpiType type;
    VpiBinding binding;
    std::vector<std::pair<std::int64_t, std::int64_t>> dimensions;
    std::int64_t left = 0, right = 0;
    std::int64_t index = 0;
    bool indexed = false;
    bool scalar = false;  ///< Declaration metadata, independent of packed width.
};
struct VpiRoutine {
    std::string name;
    VpiType result;
    bool task = true;
};

class Vpi {
public:
    explicit Vpi(Context&);
    ~Vpi();
    Vpi(const Vpi&) = delete;
    Vpi& operator=(const Vpi&) = delete;
    class Entry {
    public:
        explicit Entry(Context&) noexcept;
        ~Entry();
        Entry(const Entry&) = delete;
        Entry& operator=(const Entry&) = delete;

    private:
        Context* previous_;
    };
    [[nodiscard]] bool enabled() const noexcept;
    bool startup(void (**routines)());
    void ownLibrary(void*, void (*close)(void*));
    [[nodiscard]] std::vector<VpiRoutine> routines() const;
    [[nodiscard]] std::string error() const;
    void reportError(std::string);
    void configure(std::vector<std::string> argv, std::string version, int precision);
    vpiHandle object(VpiObjectInfo);
    bool compileCall(std::uint32_t site, std::uint32_t routine, std::string scope,
                     std::span<const VpiArgument>, VpiType result, int unit = -9,
                     int precision = -9, std::string file = {}, std::uint32_t line = 0);
    VpiValue call(std::uint32_t site, std::span<const VpiArgument>);
    void detachBindings(void* owner) noexcept;
    [[nodiscard]] bool lastCallStopped() const noexcept;
    void beginRuntime();
    void start();
    void finish();
    [[nodiscard]] bool watches(SlotId) const noexcept;
    void changed(SlotId, std::uint32_t element, const std::uint64_t* words);
    void nextTime();
    [[nodiscard]] bool scheduledLive(std::uint32_t, std::uint64_t) const noexcept;
    void dispatch(std::uint32_t id, std::uint64_t generation);
    struct Impl;
    Impl& implementation();

private:
    static void linkApi() noexcept;
    Context& context_;
    std::unique_ptr<Impl> impl_;
};
}  // namespace kirimasim::kernel
