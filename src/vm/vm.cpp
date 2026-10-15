#include "haremasim/vm/vm.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "haremasim/sir/types.h"

namespace haremasim::vm {

namespace {

/// The text of a $display or $write format that has no arguments: only %%
/// stands for %. The front end rejects other specifiers for now.
std::string formatLiteral(std::string_view fmt) {
    std::string out;
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        out += fmt[i];
        if (fmt[i] == '%' && i + 1 < fmt.size() && fmt[i + 1] == '%') ++i;
    }
    return out;
}

std::uint64_t constantValue(const sir::Design& d, const sir::Operand& o) {
    if (o.kind != sir::Operand::Kind::Const || o.id >= d.constants.size()) return 0;
    const sir::Constant& c = d.constants[o.id];
    return c.aWords.empty() ? 0 : c.aWords[0];
}

/// System tasks the VM knows. Names are resolved to these when the design
/// is loaded, so nothing compares names while the design runs (SIR_SPEC.md
/// 1).
enum class SysTask : std::uint8_t { None, Display, Write, Finish, Unknown };

SysTask resolveSysTask(std::string_view name) {
    if (name == "$display") return SysTask::Display;
    if (name == "$write") return SysTask::Write;
    if (name == "$finish") return SysTask::Finish;
    return SysTask::Unknown;
}

class Interpreter {
public:
    Interpreter(const sir::Design& d, kernel::Context& ctx) : d_(d), sched_(ctx.scheduler()) {
        // sysTasks_[process][block][instruction]
        sysTasks_.resize(d.processes.size());
        for (std::size_t p = 0; p < d.processes.size(); ++p) {
            const auto& blocks = d.processes[p].blocks;
            sysTasks_[p].resize(blocks.size());
            for (std::size_t b = 0; b < blocks.size(); ++b) {
                for (const sir::Instr& in : blocks[b].instrs) {
                    const bool sys = in.op == sir::Op::Syscall && !in.operands.empty();
                    sysTasks_[p][b].push_back(sys ? resolveSysTask(in.operands[0].text)
                                                  : SysTask::None);
                }
            }
        }
    }

    RunResult run() {
        for (std::uint32_t p = 0; p < d_.processes.size(); ++p)
            if (d_.processes[p].kind == sir::ProcessKind::Initial) sched_.scheduleActive(p);
        std::uint32_t p = 0;
        while (sched_.popActive(p)) {
            if (!runProcess(p)) return {kernel::EndReason::Internal, error_};
        }
        return {sched_.reason(), {}};
    }

private:
    const sir::Design& d_;
    kernel::Scheduler& sched_;
    std::string error_;
    std::vector<std::vector<std::vector<SysTask>>> sysTasks_;

    bool fail(const sir::Process& p, const std::string& what) {
        error_ = "process \"" + p.name + "\": " + what;
        return false;
    }

    /// Runs a process from its entry block until it returns or the
    /// simulation ends. False on an instruction the VM cannot run yet.
    bool runProcess(std::uint32_t pid) {
        const sir::Process& p = d_.processes[pid];
        sir::BlockId b = 0;
        for (;;) {
            const auto& instrs = p.blocks[b].instrs;
            for (std::size_t k = 0; k < instrs.size(); ++k) {
                const sir::Instr& in = instrs[k];
                switch (in.op) {
                    case sir::Op::Syscall:
                        if (!syscall(p, in, sysTasks_[pid][b][k])) return false;
                        if (sched_.ending()) return true;
                        break;
                    case sir::Op::Ret: return true;
                    case sir::Op::Br: b = in.operands[0].id; break;
                    default:
                        return fail(p, std::string("instruction ") + sir::opName(in.op) +
                                           " is not supported by the VM yet");
                }
                if (in.op == sir::Op::Br) break;
            }
        }
    }

    bool syscall(const sir::Process& p, const sir::Instr& in, SysTask task) {
        switch (task) {
            case SysTask::Display:
            case SysTask::Write: {
                std::string text = in.operands.size() > 1 ? formatLiteral(in.operands[1].text) : "";
                if (task == SysTask::Display) text += '\n';
                kernel::writeOut(text);
                return true;
            }
            case SysTask::Finish: {
                const auto level = static_cast<int>(constantValue(d_, in.operands[1]));
                sched_.requestEnd(kernel::EndReason::Finish, level);
                return true;
            }
            case SysTask::None:
            case SysTask::Unknown: break;
        }
        return fail(p, "system task " + in.operands[0].text + " is not supported by the VM yet");
    }
};

}  // namespace

RunResult run(const sir::Design& design, kernel::Context& context) {
    return Interpreter(design, context).run();
}

}  // namespace haremasim::vm
