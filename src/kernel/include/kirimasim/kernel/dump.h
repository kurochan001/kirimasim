// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Waveforms: the 4-state VCD file of 21.7.1 and 21.7.2, as WAVEFORM.md
// sets it.
//
// The tier gives the scopes and variables a dump can name (the dump table,
// SIR_SPEC.md 2) before the run starts. The dump tasks say what to dump and
// when; the scheduler calls endSlot() at the end of every time slot, and,
// with --wave-glitches, changed() and triggered() as values change. Nothing
// is looked up by name while the design runs.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "kirimasim/kernel/store.h"

namespace kirimasim::kernel {

class Output;
class Scheduler;

/// The kind of a scope, as the VCD names it (WAVEFORM.md 2).
enum class DumpScopeKind : std::uint8_t { Module, Begin, Fork, Task, Function };

/// The VCD type of a variable (WAVEFORM.md 2, 21.7.2.3).
enum class DumpType : std::uint8_t {
    Reg,
    Integer,
    Time,
    Real,
    Realtime,
    Event,
    Wire,
    Tri,
    Wand,
    Triand,
    Wor,
    Trior,
    Tri0,
    Tri1,
    Supply0,
    Supply1,
};

/// A scope a dump can name. The rows are in depth-first order, each after
/// the scope around it.
struct DumpScope {
    std::string name;  ///< Its own name, as the VCD writes it.
    std::string path;  ///< Its hierarchical name, for --wave-scope.
    DumpScopeKind kind = DumpScopeKind::Module;
    std::uint32_t parent = UINT32_MAX;  ///< UINT32_MAX for a top module.
};

/// A variable a dump can name. The rows of a scope come in the order of
/// declaration; names that are one net share its slot, and its code.
struct DumpVar {
    std::uint32_t scope = 0;
    std::string name;
    std::string range;  ///< "[msb:lsb]", or empty.
    DumpType type = DumpType::Reg;
    std::uint32_t width = 1;  ///< The size the VCD gives.
    SlotId slot{0};
    bool shortReal = false;  ///< The slot holds a shortreal: a float in its low 32 bits.
};

/// What the run tells the dump before it starts (WAVEFORM.md 3, 5).
struct DumpSetup {
    std::vector<DumpScope> scopes;
    std::vector<DumpVar> vars;
    std::string date;       ///< When the run started: "YYYY-MM-DD hh:mm:ss UTC".
    std::string version;    ///< "kirimasim <version>".
    std::string timescale;  ///< The time precision, such as "1 ns".
    /// --wave: the file, which dumps everything from time 0; empty for none.
    std::string waveFile;
    std::vector<std::string> scopePatterns;  ///< --wave-scope
    bool glitches = false;                   ///< --wave-glitches
};

/// What became of a call of $dumpfile or $dumpvars: done, or ignored as
/// too late or under --wave (the warnings of UNSPECIFIED_BEHAVIOR.md D3).
enum class DumpCall : std::uint8_t { Done, Late, CommandLine };

/// The dump of one Context.
class Dump {
public:
    Dump(Output& output, Store& store) : output_(output), store_(store) {}
    ~Dump();
    Dump(const Dump&) = delete;
    Dump& operator=(const Dump&) = delete;

    /// Given once, before the run starts.
    void setup(DumpSetup s);

    /// $dumpfile: `call` is the call as written in the source.
    DumpCall file(std::string name, std::string call);
    /// $dumpvars at `time`: `levels` (0: all) of the scope rows and the
    /// variable rows given, or of every top module when `all` (no scope or
    /// variable was named).
    DumpCall vars(std::uint64_t time, std::uint32_t levels, std::span<const std::uint32_t> scopes,
                  std::span<const std::uint32_t> vars, bool all);
    /// $dumpoff, $dumpon and $dumpall: at the end of this time slot.
    void off() { control(Control::Off); }
    void on() { control(Control::On); }
    void all() { control(Control::All); }
    /// $dumplimit: the size of the file at which dumping stops.
    void limit(std::uint64_t bytes) { limit_ = bytes; }
    /// $dumpflush.
    void flush();

    /// Whether endSlot() has anything to do.
    [[nodiscard]] bool busy() const { return state_ != State::None && state_ != State::Done; }
    /// Whether changes are written as they happen (--wave-glitches).
    [[nodiscard]] bool glitches() const { return glitches_ && state_ == State::On; }
    /// The end of a time slot (WAVEFORM.md 4): the start, the changes, and
    /// $dumpoff, $dumpon and $dumpall.
    void endSlot(const Scheduler& s);
    /// With --wave-glitches: slot s has changed; the event whose handle is
    /// 1 + e (SIR_SPEC.md 3.2.1) was triggered.
    void changed(SlotId s, std::uint64_t time);
    void triggered(std::uint32_t e, std::uint64_t time);
    /// The end of the run, however it ended: what the last time slot left,
    /// then the file is closed (WAVEFORM.md 4).
    void finish(const Scheduler& s);

private:
    /// None: no $dumpvars yet. Starting: one ran in this time slot. On and
    /// Off: dumping. Done: the limit was reached, the file could not be
    /// written, or the run ended.
    enum class State : std::uint8_t { None, Starting, On, Off, Done };
    enum class Control : std::uint8_t { Off, On, All };
    struct Request {
        std::uint32_t levels = 0;
        std::vector<std::uint32_t> scopes;
        std::vector<std::uint32_t> vars;
        bool all = false;  ///< Every top module.
    };
    /// A variable written to the file: one per slot.
    struct Code {
        std::string id;
        SlotId slot{0};
        DumpType type = DumpType::Reg;
        std::uint32_t width = 1;
        bool shortReal = false;
        std::uint64_t last = 0;              ///< Offset of its last value in last_.
        std::uint64_t eventAt = UINT64_MAX;  ///< The time it was last written as triggered.
    };

    Output& output_;
    Store& store_;
    DumpSetup setup_;
    State state_ = State::None;
    bool glitches_ = false;
    bool commandLine_ = false;
    std::string name_ = "dump.vcd";
    std::string call_;
    std::uint64_t startTime_ = 0;
    std::vector<Request> requests_;
    std::vector<Control> controls_;
    std::uint64_t limit_ = 0;
    std::uint64_t bytes_ = 0;
    std::FILE* file_ = nullptr;
    std::vector<Code> codes_;
    std::vector<std::uint32_t> codeOf_;  ///< By slot: its row in codes_, or UINT32_MAX.
    std::vector<std::uint64_t> last_;
    std::uint64_t stamped_ = UINT64_MAX;  ///< The time of the last time line.
    std::string chunk_;

    void control(Control c);
    /// Opens the file, chooses the variables, and writes the header and
    /// the $dumpvars section.
    void start(const Scheduler& s);
    /// The scopes and variables the requests and --wave-scope choose.
    void choose(std::vector<bool>& scopes, std::vector<bool>& vars) const;
    void header(const std::vector<bool>& scopes, const std::vector<bool>& vars);
    /// Appends the time line of `time` to the chunk, once.
    void stamp(std::uint64_t time);
    /// Appends c's value line; `unknown`: every bit x ($dumpoff).
    void value(const Code& c, bool unknown);
    /// Whether code c's value differs from its last one, which it becomes.
    bool take(Code& c);
    /// Appends the value line of every code but events to a section.
    void section(const char* name, bool unknown);
    void writeChanges(const Scheduler& s);
    /// Writes the chunk to the file, unless the limit was reached.
    void emit();
    void close();
};

/// Whether `text` matches `pattern`, where * matches any characters and ?
/// one (--wave-scope, WAVEFORM.md 5).
bool globMatch(std::string_view pattern, std::string_view text);

}  // namespace kirimasim::kernel
