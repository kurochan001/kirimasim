// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "tier_report.h"
#include "../json_string.h"
#include "kirimasim/analysis/islands.h"

#include <ostream>

namespace kirimasim::driver {
namespace {
std::string jsonString(std::string_view text) {
    std::string result;
    appendJsonString(result, text);
    return result;
}
}  // namespace

void writeTierReport(std::ostream& out, const sir::Design& design, const analysis::TableData* table,
                     const IslandModules& modules, std::span<const vm::ProcessTierCounts> counts) {
    const auto& emissions = modules.emissions();
    // Many processes can share the same rejected island. Format its list once.
    std::vector<std::string> blockers;
    if (table && !emissions.empty()) {
        for (const auto& member : table->members) {
            if (emissions[member.process.index()].rejection.empty()) continue;
            if (blockers.size() <= member.island) blockers.resize(member.island + 1);
            auto& list = blockers[member.island];
            if (!list.empty()) list += ',';
            list += std::to_string(member.process.index());
        }
    }

    std::size_t member = 0, excluded = 0;
    for (std::size_t p = 0; p < counts.size(); ++p) {
        const auto& process = design.processes[p];
        out << "{\"process_id\":" << p << ",\"name\":" << jsonString(process.name)
            << ",\"island\":";
        std::string_view blocked;
        if (table && member < table->members.size() &&
            table->members[member].process.index() == p) {
            const auto island = table->members[member++].island;
            out << island << ",\"exclusions\":[";
            if (island < blockers.size()) blocked = blockers[island];
        } else {
            out << "null,\"exclusions\":[";
            if (table && excluded < table->excluded.size() &&
                table->excluded[excluded].process.index() == p) {
                bool first = true;
                for (const auto reason : table->excluded[excluded++].reasons) {
                    if (!first) out << ',';
                    first = false;
                    out << jsonString(analysis::exclusionName(reason));
                }
            } else {
                out << "\"unknown-entry\"";
            }
        }
        std::string_view generation = "not-attempted";
        std::string_view rejection;
        if (!emissions.empty() && emissions[p].attempted) {
            rejection = emissions[p].rejection;
            if (!rejection.empty())
                generation = "rejected";
            else if (!blocked.empty())
                generation = "blocked-by-peer";
            else if (modules.loaded())
                generation = "available";
            else
                generation = "compiler-unavailable";
        }
        const auto& c = counts[p];
        out << "],\"generation\":" << jsonString(generation)
            << ",\"rejection\":" << (rejection.empty() ? "null" : jsonString(rejection))
            << ",\"blocking_members\":[" << blocked << "],\"vm_entries\":" << c.vmEntries
            << ",\"vm_instructions\":" << c.vmInstructions << ",\"native_calls\":" << c.nativeCalls
            << ",\"check_vm_entries\":" << c.checkVmEntries
            << ",\"check_vm_instructions\":" << c.checkVmInstructions
            << ",\"check_native_calls\":" << c.checkNativeCalls << "}\n";
    }
}
}  // namespace kirimasim::driver
