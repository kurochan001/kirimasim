// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/pattern.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace kirimasim::sir {
namespace {
using Kind = PatternLayout::Kind;
bool packed(Kind k) { return k == Kind::PackedRecord || k == Kind::PackedUnion; }
bool record(Kind k) { return k == Kind::Record || k == Kind::Union || packed(k); }
bool oneValue(Kind k) {
    return k == Kind::Integral || k == Kind::Real || k == Kind::String || k == Kind::Enumeration ||
           k == Kind::Container || packed(k);
}
bool nameOk(const std::string& s, bool escaped) {
    if (s.empty()) return false;
    const auto first = [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    };
    if (!escaped && !first(static_cast<unsigned char>(s.front()))) return false;
    for (const unsigned char c : s)
        if (c < 33 || c > 126 || (!escaped && !first(c) && c != '$' && (c < '0' || c > '9')))
            return false;
    return true;
}
std::vector<PatternId> children(const PatternLayout& layout) {
    std::vector<PatternId> result;
    if (record(layout.kind))
        for (const auto& member : layout.members)
            result.push_back(member.layout);
    else if (layout.kind == Kind::Array || layout.kind == Kind::TaggedUnion ||
             layout.kind == Kind::Container)
        result.push_back(layout.element);
    return result;
}
}  // namespace

PatternValidation validatePatterns(const Design& d) {
    PatternValidation result;
    result.shapes.resize(d.patterns.size());
    const auto bad = [&](std::size_t i, const std::string& message) {
        result.errors.push_back({"pattern !p" + std::to_string(i), message, {}});
    };
    for (std::size_t i = 0; i < d.patterns.size(); ++i) {
        const auto& p = d.patterns[i];
        if (!record(p.kind) && !p.members.empty()) bad(i, "unexpected members");
        if (p.kind != Kind::Enumeration && !p.enumerators.empty()) bad(i, "unexpected enumerators");
        if (p.kind != Kind::Array && p.count != 0) bad(i, "unexpected array count");
        if (p.kind != Kind::TaggedUnion && (!p.tag.empty() || p.tagEscaped))
            bad(i, "unexpected tag");
        const bool element =
            p.kind == Kind::Array || p.kind == Kind::TaggedUnion || p.kind == Kind::Container;
        if (!element && p.element != kNone) bad(i, "unexpected element layout");
        if (element && p.element.index() >= d.patterns.size())
            bad(i, "element layout out of range");
        const bool primitive =
            p.kind == Kind::Integral || p.kind == Kind::Enumeration || packed(p.kind);
        if (primitive) {
            if (!isIntegral(p.value) || p.value.width == 0 || p.value.detail != kNone)
                bad(i, "integral layout needs an integral value type");
        } else if (p.kind == Kind::Real) {
            if ((p.value.kind != TypeKind::Real && p.value.kind != TypeKind::ShortReal) ||
                p.value.width != 0 || p.value.detail != kNone)
                bad(i, "real layout needs a real value type");
        } else if (p.kind == Kind::String) {
            if (p.value != Type{TypeKind::String, 0}) bad(i, "string layout needs string");
        } else if (p.kind == Kind::Container) {
            if (!isContainer(p.value) || p.value.width != 0 ||
                p.value.detail.index() >= d.types.size()) {
                bad(i, "container layout needs a valid container type");
            } else {
                const auto& t = d.types[p.value.detail.index()];
                const auto want = p.value.kind == TypeKind::Queue ? TypeEntryKind::Queue
                                                                  : TypeEntryKind::DynArray;
                if (t.kind != want) bad(i, "container descriptor kind differs");
                if (p.element.index() < d.patterns.size() &&
                    (!oneValue(d.patterns[p.element.index()].kind) ||
                     d.patterns[p.element.index()].value != t.value))
                    bad(i, "container element layout differs from element value type");
            }
        } else if (p.value != voidType()) {
            bad(i, "aggregate layout must not carry a scalar value type");
        }
        switch (p.kind) {
            case Kind::Integral:
            case Kind::Real:
            case Kind::String:
            case Kind::Enumeration:
            case Kind::Record:
            case Kind::PackedRecord:
            case Kind::Array:
            case Kind::Union:
            case Kind::PackedUnion:
            case Kind::TaggedUnion:
            case Kind::Container: break;
            default: bad(i, "unknown layout kind"); break;
        }
        if ((p.kind == Kind::Union || p.kind == Kind::PackedUnion) && p.members.empty())
            bad(i, "union needs a first member");
        if (p.kind == Kind::TaggedUnion && !nameOk(p.tag, p.tagEscaped)) bad(i, "invalid tag");
        for (const auto& e : p.enumerators) {
            if (!nameOk(e.name, e.escaped)) bad(i, "invalid enumerator name");
            if (e.value.index() >= d.constants.size() ||
                d.constants[e.value.index()].type != p.value)
                bad(i, "enumerator constant needs the layout's value type");
        }
        for (const auto& m : p.members) {
            if (!nameOk(m.name, m.escaped)) bad(i, "invalid member name");
            if (m.layout.index() >= d.patterns.size()) {
                bad(i, "member layout out of range");
            } else if (packed(p.kind)) {
                const auto& child = d.patterns[m.layout.index()];
                if ((!isIntegral(child.value) ||
                     (child.kind != Kind::Integral && child.kind != Kind::Enumeration &&
                      !packed(child.kind))) ||
                    m.lsb > p.value.width || child.value.width > p.value.width - m.lsb)
                    bad(i, "packed member needs an in-range integral layout");
            } else if (m.lsb != 0) {
                bad(i, "unpacked member has a packed offset");
            }
        }
    }
    std::vector<std::uint8_t> state(d.patterns.size(), 0);
    struct Visit {
        std::size_t id;
        std::vector<PatternId> children;
        std::size_t next = 0;
    };
    std::vector<Visit> stack;
    for (std::size_t root = 0; root < d.patterns.size(); ++root) {
        if (state[root] != 0) continue;
        state[root] = 1;
        stack.push_back({root, children(d.patterns[root])});
        while (!stack.empty()) {
            auto& at = stack.back();
            if (at.next < at.children.size()) {
                const auto child = at.children[at.next++].index();
                if (child >= d.patterns.size()) continue;
                if (state[child] == 1) bad(at.id, "cyclic layout reference");
                if (state[child] != 0) continue;
                state[child] = 1;
                stack.push_back({child, children(d.patterns[child])});
                continue;
            }
            const auto id = at.id;
            const auto& p = d.patterns[id];
            PatternShape shape{oneValue(p.kind) ? 1U : 0U, 1, p.kind == Kind::Container};
            const auto add = [&](std::uint64_t& target, std::uint64_t value, std::uint64_t count) {
                const auto max = std::numeric_limits<std::uint64_t>::max();
                if (count != 0 && value > (max - target) / count) {
                    bad(id, "layout size arithmetic overflows");
                    return;
                }
                target += value * count;
            };
            if (p.kind != Kind::Container) {
                const auto n = (p.kind == Kind::Union || p.kind == Kind::PackedUnion)
                                   ? std::min<std::size_t>(1, at.children.size())
                                   : at.children.size();
                for (std::size_t j = 0; j < n; ++j) {
                    const auto child = at.children[j].index();
                    if (child >= result.shapes.size()) continue;
                    const auto count = p.kind == Kind::Array ? p.count : 1;
                    if (count != 0) shape.heap = shape.heap || result.shapes[child].heap;
                    if (!oneValue(p.kind)) add(shape.leaves, result.shapes[child].leaves, count);
                    add(shape.nodes, result.shapes[child].nodes, count);
                }
            }
            result.shapes[id] = shape;
            state[id] = 2;
            stack.pop_back();
        }
    }
    return result;
}

bool patternArguments(const Design& d, PatternId root, const std::vector<PatternShape>& shapes,
                      const std::vector<Type>& actual) {
    if (root.index() >= d.patterns.size() || root.index() >= shapes.size() ||
        shapes[root.index()].leaves != actual.size())
        return false;
    struct Work {
        PatternId id;
        std::uint64_t count;
    };
    std::vector<Work> stack{{root, 1}};
    std::size_t next = 0;
    while (!stack.empty()) {
        const auto item = stack.back();
        stack.pop_back();
        if (item.count == 0 || shapes[item.id.index()].leaves == 0) continue;
        if (item.count > 1) stack.push_back({item.id, item.count - 1});
        const auto& p = d.patterns[item.id.index()];
        if (oneValue(p.kind)) {
            if (next >= actual.size() || actual[next++] != p.value) return false;
        } else if (p.kind == Kind::Array) {
            stack.push_back({p.element, p.count});
        } else if (p.kind == Kind::TaggedUnion) {
            stack.push_back({p.element, 1});
        } else if (p.kind == Kind::Union) {
            stack.push_back({p.members.front().layout, 1});
        } else {
            for (auto i = p.members.rbegin(); i != p.members.rend(); ++i)
                stack.push_back({i->layout, 1});
        }
    }
    return next == actual.size();
}
}  // namespace kirimasim::sir
