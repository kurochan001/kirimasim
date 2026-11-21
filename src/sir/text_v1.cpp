// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "text_v1.h"

#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "text_v1_schema.h"

namespace kirimasim::sir {
namespace {

using namespace text_v1;

constexpr std::string_view header = "sir";
constexpr unsigned version = 1;  // The draft wire version, independent of the public dump version.
constexpr std::string_view noValue = "none";
constexpr std::string_view hex = "0123456789abcdef";

class Writer {
public:
    std::optional<std::string> print(const Design& design) {
        text_ += header;
        text_ += ' ';
        value(version);
        text_ += ' ';
        value(design);
        text_ += '\n';
        if (!representable_) return std::nullopt;
        return std::move(text_);
    }

private:
    std::string text_;
    bool representable_ = true;

    void value(bool entry) { text_ += entry ? "true" : "false"; }

    void value(char entry) { value(std::string_view(&entry, 1)); }

    void value(std::string_view bytes) {
        text_ += '"';
        for (const unsigned char c : bytes) {
            if (c == '"' || c == '\\') {
                text_ += '\\';
                text_ += static_cast<char>(c);
            } else if (c >= 0x20 && c <= 0x7e) {
                text_ += static_cast<char>(c);
            } else {
                text_ += "\\x";
                text_ += hex[c >> 4U];
                text_ += hex[c & 15U];
            }
        }
        text_ += '"';
    }
    void value(const std::string& bytes) { value(std::string_view(bytes)); }

    template <std::integral T> void value(T entry) {
        std::array<char, 24> buf{};
        const auto [end, error] = std::to_chars(buf.data(), buf.data() + buf.size(), +entry);
        if (error == std::errc{}) text_.append(buf.data(), end);
    }

    template <typename Tag> void value(Id<Tag> id) {
        if (id == kNone)
            text_ += noValue;
        else
            value(id.index());
    }

    template <typename T> void value(const std::optional<T>& entry) {
        if (entry)
            value(*entry);
        else
            text_ += noValue;
    }

    template <typename... T> void value(const std::variant<T...>& entry) {
        if (entry.valueless_by_exception())
            representable_ = false;
        else
            std::visit([this](const auto& alternative) { this->value(alternative); }, entry);
    }

    template <typename T> void value(const std::vector<T>& entries) {
        list(std::span<const T>(entries));
    }

    template <typename T, std::size_t N> void value(const std::array<T, N>& entries) {
        list(std::span<const T>(entries));
    }

    template <typename T> void list(std::span<const T> entries) {
        text_ += '[';
        bool first = true;
        for (const auto& entry : entries) {
            if (!std::exchange(first, false)) text_ += ',';
            value(entry);
        }
        text_ += ']';
    }

    void checkOperand(const Operand& entry) {
        Operand defaults;
        defaults.kind = entry.kind;
        visitFields<Operand, false>(entry, [&](auto name, auto member) {
            if (entry.*member == defaults.*member) return;
            bool written = false;
            visitFields(entry, [&](auto key, auto) { written |= key == name; });
            if (!written) representable_ = false;
        });
    }

    template <typename T>
    void value(const T& entry)
        requires(!std::integral<T>)
    {
        if constexpr (std::is_enum_v<T>) {
            for (const auto& item : enumItems(T{}))
                if (item.value == entry) {
                    text_ += item.name;
                    return;
                }
            representable_ = false;
        } else {
            if constexpr (usesKindTag<T>) {
                if constexpr (std::is_same_v<T, Operand>) checkOperand(entry);
                bool found = false;
                visitTags<T>([&](auto name, auto kind) {
                    if (entry.kind == kind) {
                        text_ += name;
                        found = true;
                    }
                });
                if (!found) representable_ = false;
            } else {
                text_ += recordTag<T>();
            }
            text_ += '{';
            visitFields(entry, [&](auto name, auto member) {
                text_ += name;
                text_ += '=';
                value(entry.*member);
                text_ += ';';
            });
            text_ += '}';
        }
    }
};

bool letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool digit(char c) { return c >= '0' && c <= '9'; }
bool identifierChar(char c) { return letter(c) || digit(c) || c == '_' || c == '.'; }
bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

class Reader {
public:
    explicit Reader(std::string_view text) : rest_(text), size_(text.size()) {}

    VerifyOutcome parse() {
        Design design;
        unsigned found = 0;
        if (size_ > maxTextBytes) {
            fail("text size limit exceeded");
        } else if (identifier() != header || !value(found) || found != version) {
            fail("expected sir 1 header");
        } else if (value(design)) {
            skip();
            if (!rest_.empty())
                fail("unexpected text after design");
            else if (withinVerifierLimits(design))
                return verifyDesign(std::move(design));
        }
        return {std::nullopt,
                {{"sir v1 byte " + std::to_string(errorOffset_), std::move(error_), {}}}};
    }

private:
    // Provisional limits for the internal reader. Vectors grow only as entries are parsed.
    static constexpr std::size_t maxTextBytes = std::size_t{64} * 1024 * 1024;
    static constexpr std::size_t maxListBytes = std::size_t{128} * 1024 * 1024;
    // The verifier allocates dense SSA tables and a quadratic dominance matrix per body.
    static constexpr std::size_t maxTemps = std::size_t{1024} * 1024;
    static constexpr std::size_t maxBlocks = std::size_t{16} * 1024;

    std::string_view rest_;
    std::size_t size_;
    std::size_t listBytes_ = 0;
    std::size_t errorOffset_ = 0;
    std::string error_;

    bool fail(std::string_view message) {
        if (error_.empty()) {
            errorOffset_ = size_ - rest_.size();
            error_ = message;
        }
        return false;
    }

    bool withinVerifierLimits(const Design& design) {
        const auto fits = [](const auto& body) {
            return body.numTemps <= maxTemps && body.blocks.size() <= maxBlocks;
        };
        for (const auto& body : design.processes)
            if (!fits(body)) return fail("process exceeds verifier limits");
        for (const auto& body : design.subroutines)
            if (!fits(body)) return fail("subroutine exceeds verifier limits");
        return true;
    }

    void skip() {
        while (!rest_.empty()) {
            if (space(rest_.front())) {
                rest_.remove_prefix(1);
            } else if (rest_.starts_with("//")) {
                const auto end = rest_.find('\n');
                rest_.remove_prefix(end == rest_.npos ? rest_.size() : end);
            } else {
                break;
            }
        }
    }

    bool take(char c) {
        skip();
        if (!rest_.starts_with(c)) return false;
        rest_.remove_prefix(1);
        return true;
    }

    bool expect(char c) { return take(c) || fail(std::string("expected '") + c + "'"); }

    std::string_view identifier() {
        skip();
        if (rest_.empty() || !letter(rest_.front())) return {};
        std::size_t length = 1;
        while (length < rest_.size() && identifierChar(rest_[length]))
            ++length;
        const auto token = rest_.substr(0, length);
        rest_.remove_prefix(length);
        return token;
    }

    bool takeNone() {
        skip();
        if (!rest_.starts_with(noValue)) return false;
        if (rest_.size() > noValue.size() && identifierChar(rest_[noValue.size()])) return false;
        rest_.remove_prefix(noValue.size());
        return true;
    }

    bool value(bool& entry) {
        const auto token = identifier();
        if (token != "true" && token != "false") return fail("expected boolean");
        entry = token == "true";
        return true;
    }

    bool value(char& entry) {
        std::string bytes;
        if (!value(bytes)) return false;
        if (bytes.size() != 1) return fail("expected one byte");
        entry = bytes.front();
        return true;
    }

    bool escape(std::string& bytes) {
        if (rest_.empty()) return fail("unfinished escape");
        const char c = rest_.front();
        rest_.remove_prefix(1);
        switch (c) {
            case '"':
            case '\\': bytes += c; return true;
            case 'n': bytes += '\n'; return true;
            case 't': bytes += '\t'; return true;
            case 'x': {
                if (rest_.size() < 2) return fail("unfinished hex escape");
                unsigned byte = 0;
                const auto digits = rest_.substr(0, 2);
                const auto [end, error] =
                    std::from_chars(digits.data(), digits.data() + digits.size(), byte, 16);
                if (error != std::errc{} || end != digits.data() + digits.size())
                    return fail("expected two hex digits");
                bytes += static_cast<char>(byte);
                rest_.remove_prefix(2);
                return true;
            }
            default: return fail("unknown byte escape");
        }
    }

    bool value(std::string& bytes) {
        if (!expect('"')) return false;
        while (!rest_.empty()) {
            const auto c = static_cast<unsigned char>(rest_.front());
            rest_.remove_prefix(1);
            if (c == '"') return true;
            if (c == '\\') {
                if (!escape(bytes)) return false;
            } else if (c >= 0x20 && c <= 0x7e) {
                bytes += static_cast<char>(c);
            } else {
                return fail("expected a printable byte or escape");
            }
        }
        return fail("unterminated byte string");
    }

    template <std::integral T> bool value(T& entry) {
        skip();
        if (rest_.empty()) return fail("expected integer");
        const auto [end, error] = std::from_chars(rest_.data(), rest_.data() + rest_.size(), entry);
        if (error != std::errc{}) return fail("invalid or out-of-range integer");
        if (end != rest_.data() + rest_.size() && identifierChar(*end))
            return fail("invalid integer suffix");
        rest_.remove_prefix(static_cast<std::size_t>(end - rest_.data()));
        return true;
    }

    template <typename Tag> bool value(Id<Tag>& id) {
        if (takeNone()) {
            id = kNone;
            return true;
        }
        std::uint32_t index = 0;
        if (!value(index)) return false;
        if (index == kNone) return fail("reserved numeric ID");
        id = Id<Tag>{index};
        return true;
    }

    template <typename T> bool value(std::optional<T>& entry) {
        if (takeNone()) return true;
        return value(entry.emplace());
    }

    template <typename T> bool value(std::vector<T>& entries) {
        if (!expect('[')) return false;
        if (take(']')) return true;
        do {
            if (maxListBytes - listBytes_ < sizeof(T)) return fail("list storage limit exceeded");
            listBytes_ += sizeof(T);
            T entry;
            if (!value(entry)) return false;
            entries.push_back(std::move(entry));
        } while (take(','));
        return expect(']');
    }

    template <typename T, std::size_t N> bool value(std::array<T, N>& entries) {
        if (!expect('[')) return false;
        bool first = true;
        for (auto& entry : entries) {
            if (!std::exchange(first, false) && !expect(',')) return false;
            if (!value(entry)) return false;
        }
        return expect(']');
    }

    template <typename... T> bool value(std::variant<T...>& entry) {
        const auto tag = identifier();
        return ((tag == recordTag<T>() && record(entry.template emplace<T>())) || ...) ||
               fail("unexpected variant tag");
    }

    template <typename T>
    bool value(T& entry)
        requires(!std::integral<T>)
    {
        const auto tag = identifier();
        if constexpr (std::is_enum_v<T>) {
            for (const auto& item : enumItems(T{}))
                if (item.name == tag) {
                    entry = item.value;
                    return true;
                }
            return fail("unknown enum value");
        } else {
            bool recognized = false;
            if constexpr (usesKindTag<T>) {
                visitTags<T>([&](auto name, auto kind) {
                    if (tag == name) {
                        entry.kind = kind;
                        recognized = true;
                    }
                });
            } else {
                recognized = tag == recordTag<T>();
            }
            return (recognized || fail("unexpected record tag")) && record(entry);
        }
    }

    template <typename T> bool record(T& entry) {
        if (!expect('{')) return false;
        std::uint64_t remaining = 0;
        std::size_t count = 0;
        visitFields(entry, [&](auto, auto) { ++count; });
        if (count >= 64) return fail("record schema exceeds field limit");
        remaining = (std::uint64_t{1} << count) - 1;
        while (!take('}')) {
            const auto key = identifier();
            if (key.empty() || !expect('=')) return fail("expected record field");
            bool matched = false;
            bool okay = false;
            std::uint64_t mask = 1;
            visitFields(entry, [&](auto name, auto member) {
                if (name == key) {
                    matched = true;
                    okay = (remaining & mask) != 0 ? value(entry.*member)
                                                   : fail("duplicate record field");
                    remaining &= ~mask;
                }
                mask <<= 1;
            });
            if (!matched) return fail("unknown record field");
            if (!okay || !expect(';')) return false;
        }
        return remaining == 0 || fail("missing record field");
    }
};

}  // namespace

std::optional<std::string> print1(const VerifiedDesign& design) {
    return Writer{}.print(design.design());
}

VerifyOutcome parse1(std::string_view text) { return Reader(text).parse(); }

}  // namespace kirimasim::sir
