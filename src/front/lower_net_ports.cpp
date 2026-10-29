// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Strength-preserving static partial ports (approved MOS/port contract 4).
// Ordinary directed value connections remain unchanged unless their nets
// need physical strength connectivity. Identity edges import physical roots,
// never the resolved value of the opposite port.
#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "lowerer.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/SelectExpressions.h"

namespace kirimasim::front {

std::optional<std::vector<sir::SlotRange>> Lowerer::staticNetBits(const ast::Expression& expression,
                                                                  bool netsOnly) {
    const auto& type = *expression.type;
    if (!type.isIntegral() || (netsOnly && !type.isFourState()) || type.isUnpackedArray())
        return std::nullopt;
    const auto width = type.getBitWidth();
    const auto sameType = [&](const ast::Type& other) {
        return other.isIntegral() && other.getBitWidth() == width &&
               other.isSigned() == type.isSigned() && other.isFourState() == type.isFourState();
    };
    if (expression.kind == ast::ExpressionKind::Conversion) {
        const auto& conversion = expression.as<ast::ConversionExpression>();
        if (!conversion.isImplicit() || !sameType(*conversion.operand().type)) return std::nullopt;
        return staticNetBits(conversion.operand(), netsOnly);
    }
    if (expression.kind == ast::ExpressionKind::Concatenation) {
        const auto operands = expression.as<ast::ConcatenationExpression>().operands();
        std::vector<sir::SlotRange> result;
        for (auto it = operands.rbegin(); it != operands.rend(); ++it) {
            auto bits = staticNetBits(**it, netsOnly);
            if (!bits) return std::nullopt;
            result.insert(result.end(), bits->begin(), bits->end());
        }
        if (result.size() != width) return std::nullopt;
        return result;
    }
    const ast::Expression* value = nullptr;
    std::optional<std::int64_t> first, last;
    if (expression.kind == ast::ExpressionKind::ElementSelect) {
        const auto& select = expression.as<ast::ElementSelectExpression>();
        value = &select.value();
        first = last = constInt(select.selector());
    } else if (expression.kind == ast::ExpressionKind::RangeSelect) {
        const auto& select = expression.as<ast::RangeSelectExpression>();
        value = &select.value();
        first = constInt(select.left());
        last = constInt(select.right());
        if (!first || !last || value->type->isUnpackedArray()) return std::nullopt;
        const auto range = value->type->getFixedRange();
        if (*first < range.lower() || *first > range.upper()) return std::nullopt;
        if (select.getSelectionKind() != ast::RangeSelectionKind::Simple) {
            if (*last <= 0 || *last > range.width()) return std::nullopt;
            last = select.getSelectionKind() == ast::RangeSelectionKind::IndexedUp
                       ? *first + *last - 1
                       : *first - *last + 1;
        }
    }
    if (value != nullptr) {
        if (!first || !last || !value->type->isIntegral() || value->type->isUnpackedArray())
            return std::nullopt;
        const auto range = value->type->getFixedRange();
        if (*first < range.lower() || *first > range.upper() || *last < range.lower() ||
            *last > range.upper())
            return std::nullopt;
        const auto position = [&](std::int64_t index) {
            return range.left >= range.right ? index - range.right : range.right - index;
        };
        const auto unit = value->type->getBitWidth() / range.width();
        const auto offset =
            static_cast<std::uint64_t>(std::min(position(*first), position(*last))) * unit;
        auto bits = staticNetBits(*value, netsOnly);
        if (!bits || offset > bits->size() || width > bits->size() - offset) return std::nullopt;
        return std::vector<sir::SlotRange>(bits->begin() + static_cast<std::ptrdiff_t>(offset),
                                           bits->begin() +
                                               static_cast<std::ptrdiff_t>(offset + width));
    }
    const auto* symbol = namedSymbol(expression);
    if (symbol == nullptr || !inSlot(*symbol) ||
        (netsOnly && symbol->kind != ast::SymbolKind::Net) ||
        symbol->kind == ast::SymbolKind::ClockVar)
        return std::nullopt;
    if (netsOnly && symbol->as<ast::NetSymbol>().getDelay() != nullptr) return std::nullopt;
    const auto found = slots_.find(symbol);
    if (found == slots_.end()) return std::nullopt;
    const auto slot = sir::physicalSlot(design_, found->second);
    const auto& storage = design_.slot(slot);
    if (!storage.dims.empty() || storage.type.width != width) return std::nullopt;
    std::vector<sir::SlotRange> result;
    result.reserve(width);
    for (std::uint32_t bit = 0; bit < width; ++bit)
        result.push_back({slot, 0, 1, bit, 1});
    return result;
}

void Lowerer::connectPartialNetPorts() {
    if (netPortCandidates_.empty()) return;
    std::map<sir::SlotId, std::vector<std::pair<sir::ProcessId, sir::SlotRange>>> writes;
    for (std::uint32_t i = 0; i < design_.processes.size(); ++i) {
        const auto& process = design_.processes[i];
        if (process.kind != sir::ProcessKind::Cont) continue;
        for (const auto& range : process.writes)
            writes[range.slot].push_back({sir::ProcessId{i}, range});
    }
    std::set<sir::SlotId> needs;
    for (const auto& [slot, drivers] : writes) {
        for (std::size_t i = 0; i < drivers.size(); ++i)
            for (std::size_t j = i + 1; j < drivers.size(); ++j)
                if (drivers[i].first != drivers[j].first &&
                    sir::overlaps(design_.slot(slot), drivers[i].second, drivers[j].second))
                    needs.insert(slot);
    }
    for (const auto& [slot, location] : strengthNets_) {
        (void)location;
        needs.insert(slot);
    }
    std::set<sir::SlotId> forced;
    for (std::uint32_t s = 0; s < design_.slots.size(); ++s)
        if ((design_.slots[s].flags & sir::kForceTarget) != 0)
            forced.insert(sir::physicalSlot(design_, sir::SlotId{s}));
    struct Candidate {
        const NetPortCandidate* port;
        std::vector<sir::SlotRange> bits;
        bool active = false;
    };
    std::vector<Candidate> candidates;
    for (const auto& port : netPortCandidates_) {
        const auto inner = sir::physicalSlot(design_, port.inner);
        const auto& slot = design_.slot(inner);
        // Mixed-kind effective-bit layouts and force-observable views keep
        // their existing diagnostics until those view representations exist.
        if (!slot.dims.empty() || !sir::isIntegral(slot.type) || !sir::isFourState(slot.type) ||
            (slot.kind != sir::SlotKind::Wire && slot.kind != sir::SlotKind::Tri) ||
            forced.contains(inner))
            continue;
        const auto& type = *port.outer->type;
        if (type.getBitWidth() != slot.type.width || type.isSigned() != sir::isSigned(slot.type) ||
            type.isFourState() != sir::isFourState(slot.type))
            continue;
        auto bits = staticNetBits(*port.outer);
        if (!bits || bits->size() != slot.type.width ||
            std::ranges::any_of(*bits, [&](const auto& at) {
                return design_.slot(at.slot).kind != slot.kind || forced.contains(at.slot);
            }))
            continue;
        candidates.push_back({&port, std::move(*bits)});
    }
    bool changed;
    do {
        changed = false;
        for (auto& candidate : candidates) {
            if (candidate.active) continue;
            const auto inner = sir::physicalSlot(design_, candidate.port->inner);
            if (!needs.contains(inner) && std::ranges::none_of(candidate.bits, [&](const auto& at) {
                    return needs.contains(at.slot);
                }))
                continue;
            candidate.active = true;
            needs.insert(inner);
            for (const auto& at : candidate.bits)
                needs.insert(at.slot);
            changed = true;
        }
    } while (changed);
    for (const auto& candidate : candidates) {
        if (!candidate.active) continue;
        const auto& port = *candidate.port;
        const auto inner = sir::physicalSlot(design_, port.inner);
        auto& carrier = design_.process(port.carrier);
        carrier.numTemps = 0;
        carrier.triggers.clear();
        carrier.writes.clear();
        carrier.drives.clear();
        carrier.blocks.clear();
        carrier.blocks.emplace_back();
        carrier.blocks.back().instrs.push_back(
            {sir::Op::Ret, sir::voidType(), sir::kNone, {}, port.loc});
        netPorts_.erase(port.carrier);
        strengthNets_.try_emplace(inner, port.loc);
        for (std::uint32_t bit = 0; bit < candidate.bits.size(); ++bit) {
            const auto& outer = candidate.bits[bit];
            strengthNets_.try_emplace(outer.slot, port.loc);
            if (outer.slot == inner && outer.lsb == bit) continue;
            passiveConnections_.push_back({{inner, 0, 1, bit, 1},
                                           outer,
                                           sir::StrengthEdgeKind::Identity,
                                           sir::ProcessId{sir::kNone},
                                           false,
                                           port.loc});
        }
    }
}

}  // namespace kirimasim::front
