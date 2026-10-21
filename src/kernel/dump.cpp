#include "kirimasim/kernel/dump.h"

#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/sim.h"

namespace kirimasim::kernel {

namespace {

constexpr std::uint32_t kNoRow = UINT32_MAX;

const char* kindName(DumpScopeKind k) {
    switch (k) {
        case DumpScopeKind::Module: return "module";
        case DumpScopeKind::Begin: return "begin";
        case DumpScopeKind::Fork: return "fork";
        case DumpScopeKind::Task: return "task";
        case DumpScopeKind::Function: return "function";
    }
    return "module";
}

const char* typeName(DumpType t) {
    switch (t) {
        case DumpType::Reg: return "reg";
        case DumpType::Integer: return "integer";
        case DumpType::Time: return "time";
        case DumpType::Real: return "real";
        case DumpType::Realtime: return "realtime";
        case DumpType::Event: return "event";
        case DumpType::Wire: return "wire";
        case DumpType::Tri: return "tri";
        case DumpType::Wand: return "wand";
        case DumpType::Triand: return "triand";
        case DumpType::Wor: return "wor";
        case DumpType::Trior: return "trior";
        case DumpType::Tri0: return "tri0";
        case DumpType::Tri1: return "tri1";
        case DumpType::Supply0: return "supply0";
        case DumpType::Supply1: return "supply1";
    }
    return "reg";
}

bool isReal(DumpType t) { return t == DumpType::Real || t == DumpType::Realtime; }

/// The code of the n-th variable written: n in base 94 with the digits !
/// to ~, most significant first (WAVEFORM.md 2).
std::string codeText(std::uint64_t n) {
    std::string s;
    do {
        s.insert(s.begin(), static_cast<char>('!' + n % 94));
        n /= 94;
    } while (n != 0);
    return s;
}

}  // namespace

bool globMatch(std::string_view pattern, std::string_view text) {
    // On a mismatch, the last * takes one more character.
    std::size_t p = 0;
    std::size_t t = 0;
    std::size_t star = std::string_view::npos;
    std::size_t mark = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p;
            ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            mark = t;
        } else if (star != std::string_view::npos) {
            p = star + 1;
            t = ++mark;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*')
        ++p;
    return p == pattern.size();
}

Dump::~Dump() { close(); }

void Dump::setup(DumpSetup s) {
    setup_ = std::move(s);
    glitches_ = setup_.glitches;
    if (setup_.waveFile.empty()) return;
    // As if $dumpfile and $dumpvars with no arguments ran at time 0 before
    // every process (WAVEFORM.md 5).
    commandLine_ = true;
    name_ = setup_.waveFile;
    call_ = "--wave=" + setup_.waveFile;
    state_ = State::Starting;
    startTime_ = 0;
    requests_.push_back({0, {}, {}, true});
}

DumpCall Dump::file(std::string name, std::string call) {
    if (commandLine_) return DumpCall::CommandLine;
    if (state_ != State::None && state_ != State::Starting) return DumpCall::Late;
    name_ = std::move(name);
    call_ = std::move(call);
    return DumpCall::Done;
}

DumpCall Dump::vars(std::uint64_t time, std::uint32_t levels, std::span<const std::uint32_t> scopes,
                    std::span<const std::uint32_t> vars, bool all) {
    if (commandLine_) return DumpCall::CommandLine;
    // The calls of the first time slot that has one add up (WAVEFORM.md 4).
    if (state_ == State::None) {
        state_ = State::Starting;
        startTime_ = time;
    } else if (state_ != State::Starting || time != startTime_) {
        return DumpCall::Late;
    }
    requests_.push_back({levels, {scopes.begin(), scopes.end()}, {vars.begin(), vars.end()}, all});
    return DumpCall::Done;
}

void Dump::control(Control c) {
    if (state_ == State::None || state_ == State::Done) return;  // before $dumpvars
    controls_.push_back(c);
}

void Dump::flush() {
    if (file_ != nullptr) std::fflush(file_);
}

void Dump::endSlot(const Scheduler& s) {
    if (state_ == State::Starting)
        start(s);
    else if (state_ == State::On)
        writeChanges(s);
    // The control tasks, after the changes, in the order they ran.
    for (const Control c : controls_) {
        if (c == Control::Off && state_ == State::On) {
            stamp(s.time());
            section("$dumpoff", true);
            state_ = State::Off;
        } else if (c == Control::On && state_ == State::Off) {
            stamp(s.time());
            section("$dumpon", false);
            state_ = State::On;
        } else if (c == Control::All && state_ == State::On) {
            stamp(s.time());
            section("$dumpall", false);
        }
    }
    controls_.clear();
    emit();
}

void Dump::changed(SlotId s, std::uint64_t time) {
    if (state_ != State::On || s >= codeOf_.size() || codeOf_[s] == kNoRow) return;
    Code& c = codes_[codeOf_[s]];
    if (c.type == DumpType::Event || !take(c)) return;
    stamp(time);
    value(c, false);
    emit();
}

void Dump::triggered(std::uint32_t e, std::uint64_t time) {
    if (state_ != State::On) return;
    for (Code& c : codes_) {
        if (c.type != DumpType::Event || *store_.element(c.slot, 0) != std::uint64_t{e} + 1)
            continue;
        stamp(time);
        chunk_ += '1';
        chunk_ += c.id;
        chunk_ += '\n';
        c.eventAt = time;
    }
    emit();
}

void Dump::finish(const Scheduler& s) {
    if (busy()) endSlot(s);
    close();
    state_ = State::Done;
}

void Dump::start(const Scheduler& s) {
    file_ = std::fopen(name_.c_str(), "w");
    if (file_ == nullptr) {
        output_.message("kirimasim: warning: the dump file \"" + name_ + "\" cannot be opened (" +
                        std::generic_category().message(errno) + "); nothing is dumped");
        state_ = State::Done;
        return;
    }
    std::vector<bool> scopes;
    std::vector<bool> vars;
    choose(scopes, vars);
    requests_.clear();
    codeOf_.assign(store_.size(), kNoRow);
    header(scopes, vars);
    stamp(s.time());
    section("$dumpvars", false);
    state_ = State::On;
}

void Dump::choose(std::vector<bool>& scopes, std::vector<bool>& vars) const {
    const std::vector<DumpScope>& rows = setup_.scopes;
    const auto n = static_cast<std::uint32_t>(rows.size());
    scopes.assign(n, false);
    vars.assign(setup_.vars.size(), false);
    for (const Request& r : requests_) {
        std::vector<std::uint32_t> roots = r.scopes;
        if (r.all)
            for (std::uint32_t i = 0; i < n; ++i)
                if (rows[i].parent == kNoRow) roots.push_back(i);
        for (const std::uint32_t root : roots) {
            scopes[root] = true;
            // The rows below a scope follow it (depth first). The scope
            // is level 1, and each module instance below adds one; other
            // scopes go with their module (WAVEFORM.md 4).
            for (std::uint32_t c = root + 1; c < n; ++c) {
                std::uint32_t level = 1;
                std::uint32_t a = c;
                while (a != kNoRow && a != root) {
                    if (rows[a].kind == DumpScopeKind::Module) ++level;
                    a = rows[a].parent;
                }
                if (a != root) break;
                if (r.levels == 0 || level <= r.levels) scopes[c] = true;
            }
        }
        for (const std::uint32_t v : r.vars)
            vars[v] = true;
    }
    for (std::size_t v = 0; v < vars.size(); ++v)
        if (scopes[setup_.vars[v].scope]) vars[v] = true;
    if (setup_.scopePatterns.empty()) return;
    // --wave-scope: a scope stays when its name or an enclosing scope's
    // matches (WAVEFORM.md 5).
    std::vector<bool> keep(n, false);
    for (std::uint32_t i = 0; i < n; ++i) {
        keep[i] = rows[i].parent != kNoRow && keep[rows[i].parent];
        for (const std::string& p : setup_.scopePatterns)
            keep[i] = keep[i] || globMatch(p, rows[i].path);
        scopes[i] = scopes[i] && keep[i];
    }
    for (std::size_t v = 0; v < vars.size(); ++v)
        vars[v] = vars[v] && keep[setup_.vars[v].scope];
}

void Dump::header(const std::vector<bool>& scopes, const std::vector<bool>& vars) {
    chunk_ += "$date\n\t" + setup_.date + "\n$end\n";
    chunk_ += "$version\n\t" + setup_.version + "\n";
    if (!call_.empty()) chunk_ += "\t" + call_ + "\n";
    chunk_ += "$end\n$timescale\n\t" + setup_.timescale + "\n$end\n";
    // The scopes chosen, those with a variable chosen, and the scopes
    // around them.
    const std::vector<DumpScope>& rows = setup_.scopes;
    std::vector<bool> needed = scopes;
    for (std::size_t v = 0; v < vars.size(); ++v)
        if (vars[v]) needed[setup_.vars[v].scope] = true;
    for (std::size_t i = rows.size(); i-- > 0;)
        if (needed[i] && rows[i].parent != kNoRow) needed[rows[i].parent] = true;
    std::vector<std::uint32_t> open;
    std::size_t v = 0;
    for (std::uint32_t i = 0; i < rows.size(); ++i) {
        if (!needed[i]) continue;
        while (!open.empty() && open.back() != rows[i].parent) {
            chunk_ += "$upscope $end\n";
            open.pop_back();
        }
        chunk_ += "$scope ";
        chunk_ += kindName(rows[i].kind);
        chunk_ += " " + rows[i].name + " $end\n";
        open.push_back(i);
        while (v < setup_.vars.size() && setup_.vars[v].scope < i)
            ++v;
        for (; v < setup_.vars.size() && setup_.vars[v].scope == i; ++v) {
            if (!vars[v]) continue;
            const DumpVar& d = setup_.vars[v];
            // A slot has one code, whatever names it (WAVEFORM.md 2).
            if (codeOf_[d.slot] == kNoRow) {
                codeOf_[d.slot] = static_cast<std::uint32_t>(codes_.size());
                Code c;
                c.id = codeText(codes_.size());
                c.slot = d.slot;
                c.type = d.type;
                c.width = d.width;
                c.shortReal = d.shortReal;
                c.last = last_.size();
                last_.resize(last_.size() + store_.layout(d.slot).elemWords);
                codes_.push_back(std::move(c));
            }
            chunk_ += "$var ";
            chunk_ += typeName(d.type);
            chunk_ +=
                " " + std::to_string(d.width) + " " + codes_[codeOf_[d.slot]].id + " " + d.name;
            if (!d.range.empty()) chunk_ += " " + d.range;
            chunk_ += " $end\n";
        }
    }
    for (; !open.empty(); open.pop_back())
        chunk_ += "$upscope $end\n";
    chunk_ += "$enddefinitions $end\n";
}

void Dump::stamp(std::uint64_t time) {
    if (stamped_ == time) return;
    stamped_ = time;
    chunk_ += "#" + std::to_string(time) + "\n";
}

bool Dump::take(Code& c) {
    const SlotLayout& l = store_.layout(c.slot);
    const std::uint64_t* now = store_.element(c.slot, 0);
    std::uint64_t* last = last_.data() + c.last;
    bool same = true;
    for (std::uint32_t i = 0; i < l.elemWords; ++i) {
        same = same && last[i] == now[i];
        last[i] = now[i];
    }
    return !same;
}

void Dump::value(const Code& c, bool unknown) {
    const SlotLayout& l = store_.layout(c.slot);
    const std::uint64_t* w = store_.element(c.slot, 0);
    if (isReal(c.type)) {
        const double d =
            c.shortReal
                ? static_cast<double>(std::bit_cast<float>(static_cast<std::uint32_t>(w[0])))
                : std::bit_cast<double>(w[0]);
        char text[40];
        std::snprintf(text, sizeof text, "%.16g", d);
        chunk_ += 'r';
        chunk_ += text;
        chunk_ += ' ' + c.id + '\n';
        return;
    }
    // The bits, most significant first (21.7.2.2).
    const bool four = l.domain == Domain::Four;
    std::string bits(c.width, '0');
    for (std::uint32_t i = 0; i < c.width; ++i) {
        const bool a = bitAt(w, four ? 2 : 1, i);
        const bool b = four && bitAt(w + 1, 2, i);
        bits[c.width - 1 - i] = unknown ? 'x' : b ? (a ? 'x' : 'z') : (a ? '1' : '0');
    }
    if (c.width == 1) {
        chunk_ += bits + c.id + '\n';
        return;
    }
    // The shortest form: a leading bit goes when left-extending the rest
    // gives it back, 0 before 0 or 1, x before x, z before z (Tables 21-8
    // and 21-9).
    std::size_t k = 0;
    while (k + 1 < bits.size()) {
        const char h = bits[k];
        const char n = bits[k + 1];
        if (!((h == '0' && (n == '0' || n == '1')) || (h == 'x' && n == 'x') ||
              (h == 'z' && n == 'z')))
            break;
        ++k;
    }
    chunk_ += 'b';
    chunk_.append(bits, k);
    chunk_ += ' ' + c.id + '\n';
}

void Dump::section(const char* name, bool unknown) {
    chunk_ += name;
    chunk_ += '\n';
    for (Code& c : codes_) {
        // Events have no value; reals have no x (WAVEFORM.md 4).
        if (c.type == DumpType::Event || (unknown && isReal(c.type))) continue;
        if (!unknown) take(c);
        value(c, unknown);
    }
    chunk_ += "$end\n";
}

void Dump::writeChanges(const Scheduler& s) {
    const std::uint64_t t = s.time();
    for (Code& c : codes_) {
        if (c.type == DumpType::Event) {
            // Triggered in this time slot, and not written as it happened.
            const std::uint64_t h = *store_.element(c.slot, 0);
            if (h == 0 || !s.triggered(static_cast<EventId>(h - 1)) || c.eventAt == t) continue;
            stamp(t);
            chunk_ += '1' + c.id + '\n';
            c.eventAt = t;
            continue;
        }
        if (!take(c)) continue;
        stamp(t);
        value(c, false);
    }
}

void Dump::emit() {
    if (chunk_.empty()) return;
    if (file_ != nullptr && state_ != State::Done) {
        std::fwrite(chunk_.data(), 1, chunk_.size(), file_);
        bytes_ += chunk_.size();
        // $dumplimit: the file has reached the size (WAVEFORM.md 4).
        if (limit_ != 0 && bytes_ >= limit_) {
            const std::string note = "$comment the dump limit of " + std::to_string(limit_) +
                                     " bytes was reached $end\n";
            std::fwrite(note.data(), 1, note.size(), file_);
            state_ = State::Done;
        }
    }
    chunk_.clear();
}

void Dump::close() {
    if (file_ != nullptr) std::fclose(file_);
    file_ = nullptr;
}

}  // namespace kirimasim::kernel
