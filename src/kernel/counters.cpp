// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/kernel/counters.h"

#include "../json_string.h"

namespace kirimasim::kernel {

CounterId Counters::add(std::string_view name) {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].name == name) return CounterId{static_cast<std::uint32_t>(i)};
    }
    if (siteGeneration_ && entries_.size() == entries_.capacity()) *siteGeneration_ = 0;
    entries_.push_back(Entry{std::string(name), 0, false, {}});
    return CounterId{static_cast<std::uint32_t>(entries_.size() - 1)};
}

CounterId Counters::addText(std::string_view name) {
    const CounterId id = add(name);
    entries_[id.index()].isText = true;
    return id;
}

std::string Counters::toJson() const {
    std::string out = "{";
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (i != 0) out += ", ";
        appendJsonString(out, entries_[i].name);
        out += ": ";
        if (entries_[i].isText)
            appendJsonString(out, entries_[i].text);
        else
            out += std::to_string(entries_[i].value);
    }
    out += "}\n";
    return out;
}

}  // namespace kirimasim::kernel
