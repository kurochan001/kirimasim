#include "haremasim/kernel/counters.h"

#include <cstdio>

namespace haremasim::kernel {

namespace {

void appendJsonString(std::string& out, std::string_view s) {
    out += '"';
    for (const char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    out += '"';
}

}  // namespace

CounterId Counters::add(std::string_view name) {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].name == name) return static_cast<CounterId>(i);
    }
    entries_.push_back(Entry{std::string(name), 0, false, {}});
    return static_cast<CounterId>(entries_.size() - 1);
}

CounterId Counters::addText(std::string_view name) {
    const CounterId id = add(name);
    entries_[id].isText = true;
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

}  // namespace haremasim::kernel
