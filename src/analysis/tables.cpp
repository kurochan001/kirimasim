// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/analysis/tables.h"

#include <algorithm>
#include <span>
#include <utility>

#include "kirimasim/sir/text_v1.h"

namespace kirimasim::analysis {
namespace {
const std::vector<sir::BasicBlock>* blocks(const sir::Design& d, const ValueSite& site) {
    if (site.owner == sir::Owner::Kind::Process && site.row < d.processes.size())
        return &d.processes[site.row].blocks;
    if (site.owner == sir::Owner::Kind::Subroutine && site.row < d.subroutines.size())
        return &d.subroutines[site.row].blocks;
    return nullptr;
}

// Operand sites are sorted by body. Build the SSA type index at most once per body.
struct TempTypes {
    const std::vector<sir::BasicBlock>* body = nullptr;
    std::vector<sir::Type> types;
    sir::Type get(const std::vector<sir::BasicBlock>& current, std::uint32_t count,
                  std::uint32_t id) {
        if (body != &current) {
            body = &current;
            types.assign(count, sir::voidType());
            for (const auto& block : current) {
                for (const auto& param : block.params)
                    types[param.temp.index()] = param.type;
                for (const auto& in : block.instrs)
                    if (in.result != sir::kNone) types[in.result.index()] = in.type;
            }
        }
        return types[id];
    }
};

std::optional<sir::Type> valueType(const sir::Design& d, const ValueSite& site, TempTypes& temps) {
    if (site.kind == SiteKind::Slot) {
        if (site.owner != sir::Owner::Kind::None || site.block || site.instruction || site.index ||
            site.row >= d.slots.size())
            return std::nullopt;
        return d.slots[site.row].type;
    }
    const auto* body = blocks(d, site);
    if (!body || site.block >= body->size()) return std::nullopt;
    const auto& block = (*body)[site.block];
    if (site.kind == SiteKind::BlockParameter) {
        if (site.instruction != 0 || site.index >= block.params.size()) return std::nullopt;
        return block.params[site.index].type;
    }
    if (site.instruction >= block.instrs.size()) return std::nullopt;
    const auto& in = block.instrs[site.instruction];
    if (site.kind == SiteKind::Result) {
        if (site.index != 0 || in.result == sir::kNone) return std::nullopt;
        return in.type;
    }
    if (site.kind != SiteKind::Operand || site.index >= in.operands.size()) return std::nullopt;
    const auto& operand = in.operands[site.index];
    if (operand.kind == sir::Operand::Kind::Const) return d.constants[operand.id].type;
    if (operand.kind != sir::Operand::Kind::Temp) return std::nullopt;
    const auto count = site.owner == sir::Owner::Kind::Process ? d.processes[site.row].numTemps
                                                               : d.subroutines[site.row].numTemps;
    return temps.get(*body, count, operand.id);
}

template <class T, class Key, class Member>
const T* lookup(const std::vector<T>& rows, const Key& key, Member member) {
    const auto it = std::ranges::lower_bound(rows, key, {}, member);
    return it != rows.end() && (*it).*member == key ? &*it : nullptr;
}

template <class E> bool validReasons(const std::vector<E>& reasons, E last) {
    return (reasons.empty() || reasons.back() <= last) &&
           std::ranges::adjacent_find(reasons, std::greater_equal<>()) == reasons.end();
}

bool validValues(const sir::Design& d, std::span<const ValueFact> rows) {
    TempTypes temps;
    const ValueSite* previous = nullptr;
    for (const auto& row : rows) {
        const auto type = valueType(d, row.site, temps);
        if ((previous && *previous >= row.site) || !type || row.width != type->width ||
            row.noX > NoXProof::Dataflow || !validReasons(row.preserve, PreserveReason::Explicit))
            return false;
        if (row.noX != NoXProof::Unknown &&
            (!sir::isIntegral(*type) ||
             (row.noX == NoXProof::DeclaredTwoState && sir::isFourState(*type))))
            return false;
        previous = &row.site;
    }
    return true;
}

bool validMembers(const sir::Design& d, const std::vector<IslandMember>& rows) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& current = rows[i];
        if (current.process.index() >= d.processes.size() ||
            (i && rows[i - 1].process >= current.process) || current.island >= d.processes.size() ||
            current.level >= d.processes.size())
            return false;
    }
    for (const auto& current : rows) {
        for (std::size_t i = 0; i < current.dependencies.size(); ++i) {
            const auto id = current.dependencies[i];
            if (i && current.dependencies[i - 1] >= id) return false;
            const auto* other = lookup(rows, id, &IslandMember::process);
            if (!other || other->island != current.island || other->level >= current.level)
                return false;
        }
    }
    return true;
}
bool validExclusions(const sir::Design& design, const TableData& data) {
    for (std::size_t i = 0; i < data.excluded.size(); ++i) {
        const auto& row = data.excluded[i];
        if (row.process.index() >= design.processes.size() ||
            (i && data.excluded[i - 1].process >= row.process) ||
            lookup(data.members, row.process, &IslandMember::process) || row.reasons.empty() ||
            !validReasons(row.reasons, ExclusionReason::CodegenUnsupported))
            return false;
    }
    return true;
}
}  // namespace

std::optional<TableKey> makeTableKey(const sir::VerifiedDesign& design, std::string_view options,
                                     std::string layout, std::string producer) {
    if (layout.empty() || producer.empty()) return std::nullopt;
    const auto text = sir::print1(design);
    if (!text) return std::nullopt;
    return TableKey{sir::kTextV1Version,    kTableSchema,      contentDigest(*text),
                    contentDigest(options), std::move(layout), std::move(producer)};
}

BoundTables::BoundTables(TableData data) : data_(std::move(data)) {}

std::optional<BoundTables> BoundTables::bind(const sir::VerifiedDesign& design,
                                             const TableKey& expected, TableData data) {
    if (expected.schemaVersion != kTableSchema || expected.sirVersion != sir::kTextV1Version ||
        expected.layout.empty() || expected.producer.empty() || data.key != expected ||
        !validValues(design.design(), data.values) ||
        !validMembers(design.design(), data.members) || !validExclusions(design.design(), data))
        return std::nullopt;
    const auto text = sir::print1(design);
    if (!text || contentDigest(*text) != expected.design) return std::nullopt;
    return BoundTables(std::move(data));
}

const ValueFact* BoundTables::value(const ValueSite& site) const {
    return lookup(data_.values, site, &ValueFact::site);
}
const IslandMember* BoundTables::member(sir::ProcessId process) const {
    return lookup(data_.members, process, &IslandMember::process);
}
const IslandExclusion* BoundTables::exclusion(sir::ProcessId process) const {
    return lookup(data_.excluded, process, &IslandExclusion::process);
}
}  // namespace kirimasim::analysis
