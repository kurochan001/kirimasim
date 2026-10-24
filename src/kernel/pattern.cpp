// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/pattern.h"

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace kirimasim::kernel {
namespace {
using Kind = PatternValue::Kind;

bool validInteger(const IntValue& value) { return value.width != 0 && value.words != nullptr; }

bool sameBits(const IntValue& lhs, const IntValue& rhs) {
    const std::uint64_t count = (std::uint64_t{lhs.width} + 63) / 64;
    for (std::uint64_t i = 0; i < count; ++i) {
        const std::uint64_t mask = i + 1 == count && lhs.width % 64 != 0
                                       ? (std::uint64_t{1} << (lhs.width % 64)) - 1
                                       : ~std::uint64_t{0};
        const auto li = i * (lhs.fourState ? 2U : 1U);
        const auto ri = i * (rhs.fourState ? 2U : 1U);
        if ((lhs.words[li] & mask) != (rhs.words[ri] & mask)) return false;
        const std::uint64_t lb = lhs.fourState ? lhs.words[li + 1] & mask : 0;
        const std::uint64_t rb = rhs.fourState ? rhs.words[ri + 1] & mask : 0;
        if (lb != rb) return false;
    }
    return true;
}

bool ordinaryFirst(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

bool validIdentifier(PatternName name) {
    if (name.text.empty()) return false;
    if (!name.escaped && !ordinaryFirst(name.text.front())) return false;
    for (const unsigned char ch : name.text) {
        if (ch < 33 || ch > 126) return false;
        if (!name.escaped && !ordinaryFirst(static_cast<char>(ch)) && ch != '$' &&
            (ch < '0' || ch > '9'))
            return false;
    }
    return true;
}

void identifier(std::pmr::string& out, PatternName name) {
    if (name.escaped) out += '\\';
    out += name.text;
    if (name.escaped) out += ' ';
}

void quoted(std::pmr::string& out, std::string_view text) {
    out += '"';
    for (const unsigned char ch : text) {
        switch (ch) {
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\v': out += "\\v"; break;
            case '\f': out += "\\f"; break;
            case '\a': out += "\\a"; break;
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            default:
                if (ch >= 32 && ch <= 126) {
                    out += static_cast<char>(ch);
                } else {
                    out += '\\';
                    out += static_cast<char>('0' + ((ch >> 6U) & 7U));
                    out += static_cast<char>('0' + ((ch >> 3U) & 7U));
                    out += static_cast<char>('0' + (ch & 7U));
                }
        }
    }
    out += '"';
}

bool scalar(Kind kind) {
    return kind == Kind::Integral || kind == Kind::Real || kind == Kind::String ||
           kind == Kind::Enumeration || kind == Kind::Handle;
}

bool valid(const PatternValue& value) {
    if (value.kind != Kind::Associative && !value.keys.empty()) return false;
    if (value.kind != Kind::Record && value.kind != Kind::Union && !value.names.empty())
        return false;
    if (value.kind != Kind::Enumeration && !value.enumerators.empty()) return false;
    if (scalar(value.kind) && !value.children.empty()) return false;
    switch (value.kind) {
        case Kind::Integral: return validInteger(value.integral);
        case Kind::Enumeration:
            if (!validInteger(value.integral)) return false;
            for (const auto& item : value.enumerators)
                if (!validInteger(item.value) || item.value.width != value.integral.width ||
                    !validIdentifier(item.name))
                    return false;
            return true;
        case Kind::Real:
        case Kind::String:
        case Kind::Array: return true;
        case Kind::Handle: return value.ordinal == 0 || !value.text.empty();
        case Kind::Record:
        case Kind::Union:
            if (value.names.size() != value.children.size() ||
                (value.kind == Kind::Union && value.children.empty()))
                return false;
            for (const auto name : value.names)
                if (!validIdentifier(name)) return false;
            return true;
        case Kind::Associative: return value.keys.size() == value.children.size();
        case Kind::TaggedUnion: return value.children.size() == 1 && validIdentifier(value.tag);
    }
    return false;
}

void scalarText(std::pmr::string& out, const PatternValue& value) {
    switch (value.kind) {
        case Kind::Integral:
        case Kind::Enumeration:
            if (value.kind == Kind::Enumeration)
                for (const auto& item : value.enumerators)
                    if (sameBits(value.integral, item.value)) {
                        identifier(out, item.name);
                        return;
                    }
            out += formatInt(value.integral, FormatSpec{'d', 0, {}});
            break;
        case Kind::Real: out += formatReal(value.real, FormatSpec{'g', -1, "%g"}); break;
        case Kind::String: quoted(out, value.text); break;
        case Kind::Handle:
            if (value.ordinal == 0) {
                out += "null";
            } else {
                out += value.text;
                out += '@';
                out += std::to_string(value.ordinal);
            }
            break;
        default: break;
    }
}

struct Frame {
    const PatternValue* value;
    std::size_t next = 0;
    bool opened = false;
    bool pendingValue = false;
};
}  // namespace

PatternFormatResult formatPattern(const PatternValue& value, bool positional,
                                  std::pmr::memory_resource* memory) {
    PatternFormatResult result;
    try {
        auto* resource = memory ? memory : std::pmr::get_default_resource();
        std::pmr::string out(resource);
        std::pmr::vector<Frame> frames(resource);
        std::pmr::set<const PatternValue*> active(resource);
        frames.push_back({&value});
        while (!frames.empty()) {
            Frame& frame = frames.back();
            const PatternValue& current = *frame.value;
            if (!frame.opened) {
                if (!valid(current)) return {{}, PatternError::InvalidValue};
                if (scalar(current.kind)) {
                    scalarText(out, current);
                    frames.pop_back();
                    continue;
                }
                if (!active.insert(&current).second) return {{}, PatternError::Cycle};
                frame.opened = true;
                if (current.kind == Kind::TaggedUnion) {
                    identifier(out, current.tag);
                    out += ':';
                } else {
                    out += "'{";
                }
            }
            if (current.kind == Kind::Associative && frame.pendingValue) {
                frame.pendingValue = false;
                const auto* child = &current.children[frame.next++];
                out += ':';
                frames.push_back({child});
                continue;
            }
            const auto count = current.kind == Kind::Union ? 1 : current.children.size();
            if (frame.next == count) {
                if (current.kind != Kind::TaggedUnion) out += '}';
                active.erase(&current);
                frames.pop_back();
                continue;
            }
            if (frame.next != 0) out += ", ";
            if (current.kind == Kind::Associative) {
                frame.pendingValue = true;
                const auto* child = &current.keys[frame.next];
                frames.push_back({child});
            } else {
                const auto index = frame.next++;
                if (!positional && (current.kind == Kind::Record || current.kind == Kind::Union)) {
                    identifier(out, current.names[index]);
                    out += ':';
                }
                frames.push_back({&current.children[index]});
            }
        }
        result.text.assign(out.data(), out.size());
    } catch (...) {
        result.text.clear();
        result.error = PatternError::ResourceLimit;
    }
    return result;
}

}  // namespace kirimasim::kernel
