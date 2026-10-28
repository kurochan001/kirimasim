// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Fixed net arrays share the scalar value resolver. Front makes explicit
// physical ordinals and compact driver coverage (SIR_SPEC 3.5, 10;
// fixed-array-ports-proposal 2, 4). VM executes ordinary range operations.
#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "kirimasim/sir/types.h"
#include "lowerer.h"

namespace kirimasim::front {

void Lowerer::resolveArrayNet(sir::SlotId net, const std::vector<sir::ProcessId>& drivers,
                              sir::SourceLoc where) {
    const sir::Slot shape = design_.slot(net);
    const auto reject = [&](const std::string& reason) {
        ++numErrors_;
        diags_ += whereOf(where) + ": error: unsupported: " + reason + " (not implemented yet)\n";
    };
    if (!sir::isIntegral(shape.type)) {
        reject("strength resolution of a nonintegral net array");
        return;
    }
    for (const auto p : drivers)
        if (sides_.contains(p)) {
            reject("three-state side resolution of a net array");
            return;
        }
    const sir::Type carrier =
        sir::isFourState(shape.type) ? shape.type : sir::otherDomain(shape.type);
    const auto index = [&](const sir::Operand& o) -> std::optional<std::int64_t> {
        if (o.kind != sir::Operand::Kind::Const) return std::nullopt;
        const auto& c = design_.constant(o.asConst());
        if (!sir::isIntegral(c.type) || c.type.width > 64 || c.aWords.empty() ||
            std::ranges::any_of(c.bWords, [](auto b) { return b != 0; }))
            return std::nullopt;
        std::uint64_t v = c.aWords[0];
        if (sir::isSigned(c.type) && c.type.width < 64 && ((v >> (c.type.width - 1)) & 1U) != 0)
            v |= ~std::uint64_t{0} << c.type.width;
        return static_cast<std::int64_t>(v);
    };
    struct Target {
        std::uint32_t element = 0, lsb = 0, width = 0;
        std::size_t value = 1;
    };
    struct Driver {
        sir::ProcessId process{0};
        std::vector<sir::Type> types;
        std::vector<std::uint32_t> elements;
        sir::SlotId slot{0};
    };
    std::vector<Driver> all;
    const auto valueType = [&](const Driver& driver, const sir::Operand& o) {
        if (o.kind == sir::Operand::Kind::Const) return design_.constant(o.asConst()).type;
        if (o.kind == sir::Operand::Kind::Temp) return driver.types[o.id];
        return sir::voidType();
    };
    const auto targetOf = [&](const Driver& driver, const sir::Instr& in) -> std::optional<Target> {
        const auto& ops = in.operands;
        const auto& o = ops[0];
        Target t;
        t.lsb = o.lsb;
        t.width = o.width;
        if (in.op == sir::Op::Store || in.op == sir::Op::DriveDelay) {
            if (o.numElems != 1 || o.firstElem >= sir::elementCount(shape)) return std::nullopt;
            t.element = o.firstElem;
            return t;
        }
        if (in.op != sir::Op::StoreElem && in.op != sir::Op::StoreBits) return std::nullopt;
        const auto& dims = design_.slot(o.asSlot()).dims;
        std::uint64_t element = 0;
        for (std::size_t k = 0; k < dims.size(); ++k) {
            const auto at = index(ops[1 + k]);
            const auto lo = std::min(dims[k].left, dims[k].right);
            const auto hi = std::max(dims[k].left, dims[k].right);
            if (!at || *at < lo || *at > hi) return std::nullopt;
            const auto extent = static_cast<std::uint64_t>(hi) - static_cast<std::uint64_t>(lo) + 1;
            const auto offset =
                dims[k].left <= dims[k].right
                    ? static_cast<std::uint64_t>(*at) - static_cast<std::uint64_t>(lo)
                    : static_cast<std::uint64_t>(hi) - static_cast<std::uint64_t>(*at);
            element = element * extent + offset;
        }
        t.element = static_cast<std::uint32_t>(element);
        t.value = dims.size() + 1;
        if (in.op == sir::Op::StoreBits) {
            const auto pos = index(ops[t.value++]);
            const auto width = valueType(driver, ops[t.value]).width;
            const auto limit = o.width == 0 ? shape.type.width : o.width;
            if (!pos || *pos < 0 || static_cast<std::uint64_t>(*pos) > limit ||
                width > limit - static_cast<std::uint64_t>(*pos))
                return std::nullopt;
            t.lsb += static_cast<std::uint32_t>(*pos);
            t.width = width;
        }
        return t;
    };
    const auto writesNet = [&](const sir::Instr& in) {
        return sir::writesSlot(in.op) && !in.operands.empty() &&
               in.operands[0].kind == sir::Operand::Kind::Slot &&
               sir::physicalSlot(design_, in.operands[0].asSlot()) == net;
    };
    // Validate all targets before changing any process. A moving target
    // needs coverage retirement, rather than a permanent driver per cell.
    for (const auto p : drivers) {
        Driver driver;
        driver.process = p;
        driver.types.resize(design_.process(p).numTemps);
        for (const auto& block : design_.process(p).blocks) {
            for (const auto& param : block.params)
                driver.types[param.temp.index()] = param.type;
            for (const auto& in : block.instrs)
                if (in.result != sir::kNone) driver.types[in.result.index()] = in.type;
        }
        for (const auto& block : design_.process(p).blocks)
            for (const auto& in : block.instrs) {
                if (!writesNet(in)) continue;
                const auto t = targetOf(driver, in);
                if (!t) {
                    reject("a resolved net array target selected at run time or outside its range");
                    return;
                }
                driver.elements.push_back(t->element);
            }
        std::ranges::sort(driver.elements);
        driver.elements.erase(std::unique(driver.elements.begin(), driver.elements.end()),
                              driver.elements.end());
        if (driver.elements.empty()) {
            reject("a resolved net array written through a subroutine");
            return;
        }
        all.push_back(std::move(driver));
    }
    design_.slot(net).flags |= sir::kMultiDriver;
    // Only covered elements have private storage: O(sum of driver coverage),
    // including when many drivers each cover one element of a large array.
    std::map<std::uint32_t, std::vector<std::pair<sir::SlotId, std::uint32_t>>> cells;
    for (std::size_t k = 0; k < all.size(); ++k) {
        auto& driver = all[k];
        sir::Slot slot;
        slot.name = shape.name + ".$driver" + std::to_string(k);
        slot.type = carrier;
        slot.kind = sir::SlotKind::Wire;
        slot.instance = shape.instance;
        slot.dims = {{0, static_cast<std::int64_t>(driver.elements.size() - 1)}};
        slot.driverOf = net;
        slot.driverElements = driver.elements;
        if (const auto it = strengths_.find(driver.process); it != strengths_.end())
            std::tie(slot.strength1, slot.strength0) = it->second;
        driver.slot = sir::SlotId{static_cast<std::uint32_t>(design_.slots.size())};
        design_.slots.push_back(std::move(slot));
        for (std::uint32_t e = 0; e < driver.elements.size(); ++e)
            cells[driver.elements[e]].push_back({driver.slot, e});
        auto& process = design_.process(driver.process);
        std::erase_if(process.writes, [&](const auto& w) { return w.slot == net; });
        for (auto& block : process.blocks) {
            std::vector<sir::Instr> rewritten;
            for (auto& in : block.instrs) {
                if (writesNet(in)) {
                    const auto decoded = targetOf(driver, in);
                    if (!decoded) {
                        reject("an unavailable fixed net-array driver target");
                        return;
                    }
                    const auto t = *decoded;
                    auto value = in.operands[t.value];
                    auto type = valueType(driver, value);
                    if (!sir::isFourState(type)) {
                        type = sir::otherDomain(type);
                        const sir::TempId temp{process.numTemps++};
                        rewritten.push_back({sir::Op::To4, type, temp, {value}, in.loc});
                        value = sir::Operand::temp(temp);
                    }
                    auto target = sir::Operand::slot(driver.slot, t.lsb, t.width);
                    target.firstElem = static_cast<std::uint32_t>(
                        std::lower_bound(driver.elements.begin(), driver.elements.end(),
                                         t.element) -
                        driver.elements.begin());
                    target.numElems = 1;
                    process.writes.push_back({driver.slot, target.firstElem, 1, t.lsb, t.width});
                    if (in.op == sir::Op::DriveDelay) {
                        in.operands[0] = target;
                        in.operands[1] = value;
                    } else {
                        in.op = sir::Op::Store;
                        in.operands = {target, value};
                    }
                }
                rewritten.push_back(std::move(in));
            }
            block.instrs = std::move(rewritten);
        }
    }
    const bool own = shape.kind == sir::SlotKind::Tri0 || shape.kind == sir::SlotKind::Tri1 ||
                     shape.kind == sir::SlotKind::Supply0 || shape.kind == sir::SlotKind::Supply1;
    if (own)
        for (std::uint32_t e = 0; e < sir::elementCount(shape); ++e)
            cells.try_emplace(e);
    sir::Process resolver;
    resolver.name = shape.name + ".$resolve";
    resolver.kind = sir::ProcessKind::Cont;
    resolver.netResolver = true;
    resolver.instance = shape.instance;
    resolver.loc = where;
    resolver.blocks.resize(1);
    auto& code = resolver.blocks[0].instrs;
    for (const auto& driver : all)
        resolver.triggers.push_back({sir::SlotRange{driver.slot, 0, 0, 0, 0}, sir::Edge::Any});
    for (const auto& [element, inputs] : cells) {
        std::vector<sir::Operand> ops{sir::Operand::str(std::string(sir::kindName(shape.kind)))};
        for (const auto& [slot, ordinal] : inputs) {
            auto source = sir::Operand::slot(slot);
            source.firstElem = ordinal;
            source.numElems = 1;
            const sir::TempId temp{resolver.numTemps++};
            code.push_back({sir::Op::Load, carrier, temp, {source}, where});
            ops.push_back(sir::Operand::temp(temp));
            ops.push_back(constant(sir::u2(3), design_.slot(slot).strength1).op);
            ops.push_back(constant(sir::u2(3), design_.slot(slot).strength0).op);
        }
        sir::TempId result{resolver.numTemps++};
        code.push_back({sir::Op::Resolve, carrier, result, std::move(ops), where});
        if (!sir::isFourState(shape.type)) {
            const sir::TempId converted{resolver.numTemps++};
            code.push_back(
                {sir::Op::To2, shape.type, converted, {sir::Operand::temp(result)}, where});
            result = converted;
        }
        auto target = sir::Operand::slot(net);
        target.firstElem = element;
        target.numElems = 1;
        code.push_back({sir::Op::Store,
                        sir::voidType(),
                        sir::kNone,
                        {target, sir::Operand::temp(result)},
                        where});
        resolver.writes.push_back({net, element, 1, 0, 0});
    }
    code.push_back({sir::Op::Ret, sir::voidType(), sir::kNone, {}, where});
    netResolvers_[net] = sir::ProcessId{static_cast<std::uint32_t>(design_.processes.size())};
    design_.processes.push_back(std::move(resolver));
}

}  // namespace kirimasim::front
