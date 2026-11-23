// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/codegen/model.h"
#include "frame_loops.h"
#include "kirimasim/analysis/islands.h"
#include "kirimasim/analysis/values.h"
#include "kirimasim/sir/constant_index.h"
#include "kirimasim/sir/frame_layout.h"
#include "kirimasim/sir/process_policy.h"

#include <algorithm>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string_view>
#include <tuple>

namespace kirimasim::codegen {
namespace {
using sir::Op;
using sir::Operand;

bool scalar(sir::Type type) { return sir::isIntegral(type) && type.width <= 64; }

// Bound compile-time enumeration and accept only literal level symbols.
bool smallLevelUdp(const sir::TypeEntry& t) {
    return t.kind == sir::TypeEntryKind::Udp && !t.sequential && t.inputs >= 1 && t.inputs <= 3 &&
           !t.rows.empty() && t.rows.size() <= 64 &&
           std::ranges::all_of(t.rows, [&](const auto& r) {
               return r.inputs.size() == t.inputs &&
                      r.inputs.find_first_not_of("01xb?") == std::string::npos && r.state == 0 &&
                      (r.output == '0' || r.output == '1' || r.output == 'x');
           });
}

class Body {
public:
    Body(const sir::Design& design, const sir::Process& process,
         const std::vector<std::uint32_t>& slots, bool bounds, const std::vector<bool>& known,
         std::vector<sir::SlotRange>* guard, bool approximateTwo)
        : d_(design), p_(process), slots_(slots), bounds_(bounds), types_(p_.numTemps),
          known_(known), guard_(guard), approximateTwo_(approximateTwo) {
        for (const auto& block : p_.blocks) {
            for (const auto& param : block.params)
                types_[param.temp.index()] = param.type;
            for (const auto& in : block.instrs)
                if (in.result != sir::kNone) types_[in.result.index()] = in.type;
        }
    }

    std::string emit(sir::ProcessId id, std::uint64_t maxSteps, BodyEmission* record,
                     bool allowFrameLoops = false) {
        if (record) record->attempted = true;
        const auto reject = [&](std::string_view reason, std::string_view detail = {}) {
            if (record) {
                record->rejection = reason;
                if (!detail.empty()) {
                    record->rejection += ' ';
                    record->rejection += detail;
                }
            }
            return std::string{};
        };
        constexpr std::size_t partSize = 192;
        std::optional<FrameLoops> loops;
        if (!p_.frame.empty()) {
            // Keep branch checkpoints and the frame in one generated function.
            if (allowFrameLoops && std::ranges::all_of(p_.blocks, [](const auto& b) {
                    return b.instrs.size() <= partSize;
                }))
                loops = frameLoops(d_, id);
            if (!loops) return reject("process-frame");
        }
        if (p_.blocks.empty()) return reject("no-body");
        if (!acyclic(loops)) return reject("cyclic-control-flow");
        if (!planPackedReads() && !planWideStorage()) return reject("temp-storage");
        planRepeatedReads();
        std::uint64_t steps = loops ? loops->repeatedSteps : 0;
        for (const auto& block : p_.blocks)
            steps += block.instrs.size();
        for (const auto& block : p_.blocks)
            for (const auto& in : block.instrs)
                if (in.op == Op::Call) {
                    const auto& sub = d_.subroutines[in.operands.front().id];
                    if (d_.foreignReflection || !sir::returnedScalarInput(sub))
                        return reject("call", sub.name);
                    const auto n = sub.blocks.front().instrs.size();
                    if (n > UINT64_MAX - steps) return reject("step-overflow");
                    steps += n;
                }
        if (maxSteps && steps > maxSteps) return reject("step-budget");
        // Bound the compiler's per-function optimization work on deep but
        // valid straight-line expressions. Each part consumes the same SSA
        // array and writes through the same kernel table in program order.
        const auto name = [&](std::size_t b, std::size_t i) {
            return "part" + std::to_string(id.index()) + "_" + std::to_string(b) + "_" +
                   std::to_string(i);
        };
        inPart_ = true;
        for (std::size_t b = 0; b < p_.blocks.size(); ++b) {
            const auto& ins = p_.blocks[b].instrs;
            if (ins.size() <= partSize) continue;
            for (std::size_t first = 0; first < ins.size() - 1; first += partSize) {
                out_ << "static void " << name(b, first)
                     << "(kirima_context* ctx,kirima_model_activation& f,W4* t"
                     << (wideWords_ ? ",uint64_t* w" : "") << ") {\n";
                for (auto i = first; i < std::min(first + partSize, ins.size() - 1); ++i)
                    if (!instruction(ins[i])) return reject("instruction", sir::opName(ins[i].op));
                out_ << "}\n";
            }
        }
        inPart_ = false;
        out_ << "static kirima_status body" << id.index()
             << "(kirima_context* ctx, void* raw) {\n  auto& f = "
                "*static_cast<kirima_model_activation*>(raw);\n";
        if (loops) {
            out_ << "  auto* locals = static_cast<W4*>(f.payload);\n"
                    "  const auto* process = static_cast<const kirima_process_api*>("
                    "f.kernel->query_service(KIRIMA_SERVICE_PROCESS,KIRIMA_PROCESS_VERSION));\n";
        }
        if (wideWords_) out_ << "  uint64_t w[" << wideWords_ << "];\n";
        if (!types_.empty()) out_ << "  W4 t[" << types_.size() << "];\n";
        for (std::size_t b = 0; b < p_.blocks.size(); ++b) {
            const auto& ins = p_.blocks[b].instrs;
            out_ << "b" << b << ":\n";
            if (ins.size() > partSize) {
                for (std::size_t first = 0; first < ins.size() - 1; first += partSize)
                    out_ << "  " << name(b, first) << "(ctx,f,"
                         << (types_.empty() ? "nullptr" : "t") << (wideWords_ ? ",w" : "")
                         << "); if (f.kernel->ending(ctx)) return KIRIMA_STOP;\n";
                if (!instruction(ins.back()))
                    return reject("instruction", sir::opName(ins.back().op));
            } else {
                for (const auto& in : ins)
                    if (!instruction(in)) return reject("instruction", sir::opName(in.op));
            }
        }
        out_ << "}\n";
        return out_.str();
    }

private:
    const sir::Design& d_;
    const sir::Process& p_;
    const std::vector<std::uint32_t>& slots_;
    bool bounds_;
    bool inPart_ = false;
    std::vector<sir::Type> types_;
    const std::vector<bool>& known_;
    std::vector<sir::SlotRange>* guard_;
    bool approximateTwo_;
    bool fourResult(sir::Type type) const { return !approximateTwo_ && sir::isFourState(type); }
    std::ostringstream out_;

    // The serialized order need not be a topological order. Acyclic
    // control flow lets total instruction count bound every path.
    bool acyclic(const std::optional<FrameLoops>& loops) const {
        if (p_.blocks.size() == 1 && p_.blocks.front().instrs.back().op == Op::Ret) return true;
        std::vector<std::uint32_t> incoming(p_.blocks.size());
        const auto edges = [&](std::uint32_t b, const auto& visit) {
            if (loops && loops->latch[b]) return;
            for (const auto& o : p_.blocks[b].instrs.back().operands)
                if (o.kind == Operand::Kind::Block) visit(o.id);
        };
        for (std::uint32_t b = 0; b < incoming.size(); ++b)
            edges(b, [&](auto dest) { ++incoming[dest]; });
        std::vector<std::uint32_t> ready;
        for (std::uint32_t b = 0; b < incoming.size(); ++b)
            if (!incoming[b]) ready.push_back(b);
        for (std::size_t i = 0; i < ready.size(); ++i)
            edges(ready[i], [&](auto dest) {
                if (!--incoming[dest]) ready.push_back(dest);
            });
        return ready.size() == p_.blocks.size();
    }

    struct Projection {
        std::uint32_t dest, lsb, width;
        bool two;
    };
    std::vector<std::vector<Projection>> packedReads_;
    std::vector<std::uint32_t> readAlias_;

    void planRepeatedReads() {
        // Within one basic block a plain load has no side effects. A kernel
        // write can invoke observers, so it invalidates even unrelated reads.
        readAlias_.resize(types_.size());
        std::iota(readAlias_.begin(), readAlias_.end(), 0U);
        for (const auto& block : p_.blocks) {
            using Key = std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                   sir::TypeKind, std::uint32_t>;
            std::map<Key, std::uint32_t> reads;
            for (const auto& in : block.instrs) {
                if (in.op == Op::Store || in.op == Op::StoreElem || in.op == Op::StoreBits ||
                    in.op == Op::Nba || in.op == Op::NbaElem || in.op == Op::NbaBits) {
                    reads.clear();
                    continue;
                }
                if (in.op != Op::Load && in.op != Op::LoadElem) continue;
                if (!scalar(in.type) && packedReads_.empty()) continue;
                const auto& source = in.operands.front();
                if (source.kind != Operand::Kind::Slot) continue;
                const auto& slot = d_.slot(source.asSlot());
                const auto index = element(in, slot);
                if (!index) continue;
                const auto [it, inserted] =
                    reads.try_emplace({slots_[source.id], *index, source.lsb, source.width,
                                       in.type.kind, in.type.width},
                                      in.result.index());
                if (inserted) continue;
                readAlias_[in.result.index()] = it->second;
                if (!scalar(in.type)) {
                    auto& fields = packedReads_[it->second];
                    const auto& later = packedReads_[in.result.index()];
                    fields.insert(fields.end(), later.begin(), later.end());
                }
            }
        }
    }

    std::vector<std::size_t> wideOffsets_;
    std::size_t wideWords_ = 0;

    bool planWideStorage() {
        packedReads_.clear();
        wideOffsets_.resize(types_.size());
        for (std::size_t i = 0; i < types_.size(); ++i) {
            const auto t = types_[i];
            if (t.kind == sir::TypeKind::Void || scalar(t)) continue;
            if (!sir::isIntegral(t)) return false;
            wideOffsets_[i] = wideWords_;
            wideWords_ += ((std::size_t{t.width} + 63) / 64) * 2;
            // Large snapshots need a negotiated frame, not unbounded stack.
            if (wideWords_ > 8192) return false;
        }
        return wideWords_ && std::ranges::all_of(p_.blocks, [](const auto& block) {
                   return std::ranges::all_of(block.params,
                                              [](const auto& param) { return scalar(param.type); });
               });
    }

    std::string wideTemp(std::uint32_t id) const {
        return "(w+" + std::to_string(wideOffsets_[id]) + ")";
    }

    std::string packedOperand(const Operand& operand, std::string_view name) {
        if (operand.kind == Operand::Kind::Temp) return wideTemp(operand.id);
        const auto& constant = d_.constant(operand.asConst());
        out_ << "    static constexpr uint64_t " << name << "[]={";
        const auto words = (std::size_t{constant.type.width} + 63) / 64;
        for (std::size_t i = 0; i < words; ++i) {
            const auto a = i < constant.aWords.size() ? constant.aWords[i] : 0;
            const auto b = i < constant.bWords.size() ? constant.bWords[i] : 0;
            out_ << (approximateTwo_ ? a & ~b : a) << "ULL," << (approximateTwo_ ? 0 : b) << "ULL,";
        }
        out_ << "};\n";
        return std::string{name};
    }

    bool wideLogic(const sir::Instr& in) {
        std::string_view operation;
        switch (in.op) {
            case Op::And: operation = "and4(x,y)"; break;
            case Op::Or: operation = "or4(x,y)"; break;
            case Op::Xor: operation = "xor4(x,y)"; break;
            case Op::Xnor: operation = "xnor4(x,y,n)"; break;
            case Op::Not: operation = "not4(x,n)"; break;
            case Op::To2:
            case Op::To4: operation = "x"; break;
            case Op::Eq:
            case Op::Ne: operation = "eq4(x,y)"; break;
            case Op::Ceq:
            case Op::Cne: operation = "ceq4(x,y)"; break;
            case Op::Weq:
            case Op::Wne: operation = "weq4(x,y)"; break;
            default: return false;
        }
        const auto width = type(in.operands.front()).width;
        const bool equality = scalar(in.type);
        if (width <= 64 || (equality ? in.type.width != 1 : in.type.width != width)) return false;
        for (const auto& op : in.operands)
            if ((op.kind != Operand::Kind::Temp && op.kind != Operand::Kind::Const) ||
                type(op).width != width)
                return false;
        const auto words = (std::size_t{width} + 63) / 64;
        if (words > 4096) return false;
        out_ << "  {\n";
        const auto left = packedOperand(in.operands[0], "left");
        const auto right = in.operands.size() == 2 ? packedOperand(in.operands[1], "right") : "";
        if (equality) out_ << "    W4 result=kW4One;\n";
        out_ << "    for (unsigned i=0;i<" << words * 2 << ";i+=2) { ";
        if (in.op == Op::Not || in.op == Op::Xnor)
            out_ << "const unsigned n=i==" << (words - 1) * 2 << "?" << ((width - 1) % 64 + 1)
                 << ":64; ";
        out_ << "const W4 x{" << left << "[i]," << left << "[i+1]}; ";
        if (!right.empty()) out_ << "const W4 y{" << right << "[i]," << right << "[i+1]}; ";
        if (equality) {
            out_ << "result=and4(result," << operation << "); }\n";
            const bool negate = in.op == Op::Ne || in.op == Op::Cne || in.op == Op::Wne;
            if (negate) out_ << "    result=negate1(result);\n";
            out_ << "    t[" << in.result.index()
                 << "]=" << (fourResult(in.type) ? "result" : "to2(result)") << "; }\n";
        } else {
            const auto dest = wideTemp(in.result.index());
            out_ << "const W4 v=" << (fourResult(in.type) ? "" : "to2(") << operation
                 << (fourResult(in.type) ? "" : ")") << "; " << dest << "[i]=v.a; " << dest
                 << "[i+1]=v.b; } }\n";
        }
        return true;
    }

    bool wideSelect(const sir::Instr& in) {
        if (bounds_ || in.operands[0].kind != Operand::Kind::Temp || !scalar(type(in.operands[1])))
            return false;
        const auto& source = in.operands[0];
        const auto& index = in.operands[1];
        const auto xw = type(source).width, width = in.type.width;
        if (scalar(type(source))) return false;
        if (width <= 64) {
            out_ << "  t[" << in.result.index() << "] = " << (fourResult(in.type) ? "" : "to2(")
                 << "packed_select(" << wideTemp(source.id) << "," << xw << "," << value(index)
                 << "," << type(index).width << "," << sir::isSigned(type(index)) << "," << width
                 << ")" << (fourResult(in.type) ? "" : ")") << ";\n";
            return true;
        }
        if (index.kind != Operand::Kind::Const) return false;
        const auto pos = sir::constantIndex(d_.constant(index.asConst()));
        if (!pos || *pos < 0 || width > xw || *pos > xw - width) return false;
        out_ << "  packed_load(" << wideTemp(in.result.index()) << "," << wideTemp(source.id)
             << ",2," << *pos << "," << width << ",true," << !fourResult(in.type) << ");\n";
        return true;
    }
    bool planPackedReads() {
        if (std::ranges::all_of(types_,
                                [](auto t) { return t.kind == sir::TypeKind::Void || scalar(t); }))
            return true;
        packedReads_.resize(types_.size());
        for (const auto& block : p_.blocks) {
            for (const auto& param : block.params)
                if (!scalar(param.type)) return false;
            for (const auto& in : block.instrs) {
                if (in.type.kind != sir::TypeKind::Void && !scalar(in.type) &&
                    (!sir::isIntegral(in.type) || (in.op != Op::Load && in.op != Op::LoadElem)))
                    return false;
                for (std::size_t i = 0; i < in.operands.size(); ++i) {
                    const auto& op = in.operands[i];
                    if (op.kind != Operand::Kind::Temp || scalar(type(op))) continue;
                    if (i != 0 || !scalar(in.type) ||
                        (in.op != Op::Bitsel && in.op != Op::Partsel && in.op != Op::IdxPartsel) ||
                        in.operands[1].kind != Operand::Kind::Const)
                        return false;
                    const auto pos = sir::constantIndex(d_.constant(in.operands[1].asConst()));
                    const auto bits = type(op).width;
                    if (!pos || *pos < 0 || in.type.width > bits || *pos > bits - in.type.width)
                        return false;
                    packedReads_[op.id].push_back({in.result.index(),
                                                   static_cast<std::uint32_t>(*pos), in.type.width,
                                                   !fourResult(in.type)});
                }
            }
        }
        return true;
    }

    void loadField(std::uint32_t dest, const Operand& target, std::uint32_t elem, std::uint32_t lsb,
                   std::uint32_t width, bool two, std::string_view data) {
        const bool four = sir::isFourState(d_.slot(target.asSlot()).type);
        if (guard_ && four)
            guard_->push_back(sir::physicalRange(d_, {target.asSlot(), elem, 1, lsb, width}));
        out_ << "  t[" << dest << "] = " << (two && four ? "to2(" : "") << "load(" << data << ","
             << lsb << "," << width << "," << (four ? 2 : 1) << "," << (!guard_ && four) << ")"
             << (two && four ? ")" : "") << ";\n";
    }

    sir::Type type(const Operand& o) const {
        if (o.kind == Operand::Kind::Temp) return types_[o.id];
        if (o.kind == Operand::Kind::Const) return d_.constant(o.asConst()).type;
        return {};
    }
    std::string value(const Operand& o) const {
        if (o.kind == Operand::Kind::Temp) return "t[" + std::to_string(readAlias_[o.id]) + "]";
        const auto& c = d_.constant(o.asConst());
        const auto a = c.aWords.empty() ? 0 : c.aWords[0];
        const auto b = c.bWords.empty() ? 0 : c.bWords[0];
        return "W4{" + std::to_string(approximateTwo_ ? a & ~b : a) + "ULL," +
               std::to_string(approximateTwo_ ? 0 : b) + "ULL}";
    }
    bool jump(const sir::Instr& in, std::size_t at) {
        const auto& edge = in.operands[at];
        const auto& params = p_.blocks[edge.id].params;
        out_ << "{\n";
        for (std::size_t i = 0; i < params.size(); ++i)
            out_ << "    const W4 a" << i << " = " << value(in.operands[at + 1 + i]) << ";\n";
        for (std::size_t i = 0; i < params.size(); ++i)
            out_ << "    t[" << params[i].temp.index() << "] = a" << i << ";\n";
        out_ << "    goto b" << edge.id << ";\n  }\n";
        return true;
    }
    std::optional<std::uint32_t> element(const sir::Instr& in, const sir::Slot& slot) const {
        if (in.op == Op::Store || in.op == Op::Load || in.op == Op::Nba)
            return in.operands[0].firstElem;
        return sir::constantElement(d_, slot.dims,
                                    std::span{in.operands}.subspan(1, slot.dims.size()));
    }
    bool indexedStorage(const sir::Instr& in, const sir::Slot& slot) {
        if (bounds_ || guard_ || !sir::isIntegral(slot.type)) return false;
        const auto& ops = in.operands;
        const auto& target = ops.front();
        const auto id = slots_[target.id];
        const auto width = target.width ? target.width : slot.type.width;
        const bool read = in.op == Op::LoadElem;
        const bool bits = in.op == Op::StoreBits || in.op == Op::NbaBits;
        const bool nba = in.op == Op::NbaElem || in.op == Op::NbaBits;
        if ((!bits && width > 64) || (!read && !scalar(type(ops.back())))) return false;
        out_ << "  { std::optional<uint32_t> e=0; ";
        for (std::size_t i = 0; i < slot.dims.size(); ++i) {
            const auto& index = ops[i + 1];
            if (!scalar(type(index))) return false;
            const auto dim = slot.dims[i];
            out_ << "e=element_at(e," << value(index) << "," << type(index).width << ","
                 << sir::isSigned(type(index)) << ",static_cast<int64_t>("
                 << static_cast<std::uint64_t>(dim.left) << "ULL),static_cast<int64_t>("
                 << static_cast<std::uint64_t>(dim.right) << "ULL)); ";
        }
        if (read) {
            const bool four = sir::isFourState(slot.type);
            out_ << "t[" << in.result.index() << "]=" << (fourResult(in.type) ? "" : "to2(")
                 << "e ? load(ctx,f," << id << ",*e," << target.lsb << "," << width << ","
                 << (four ? 2 : 1) << "," << four << ") : allX(" << in.type.width << ")"
                 << (fourResult(in.type) ? "" : ")") << "; }\n";
            return true;
        }
        const auto& rhs = ops.back();
        out_ << "if(e) { W4 v=" << value(rhs) << "; ";
        std::string offset = std::to_string(target.lsb), span = std::to_string(target.width);
        if (bits) {
            const auto& index = ops[1 + slot.dims.size()];
            if (!scalar(type(index))) return false;
            out_ << "auto pos=position4(" << value(index) << "," << type(index).width << ","
                 << sir::isSigned(type(index)) << "); if(pos) { auto at=inside(*pos,"
                 << type(rhs).width << "," << slot.type.width << "," << target.lsb << ","
                 << target.width
                 << "); if(at) { v={(v.a>>at->skip)&mask(at->n),"
                    "(v.b>>at->skip)&mask(at->n)}; ";
            offset = "at->lo";
            span = "at->n";
        }
        if (!sir::isFourState(slot.type) || approximateTwo_) out_ << "v=to2(v); ";
        out_ << "const uint64_t words[]={v.a,v.b}; f.kernel->"
             << (nba ? "schedule_nba(ctx,f.process," : "write(ctx,") << id << ",*e," << offset
             << "," << span << ",words" << (nba ? ",0" : "") << "); "
             << (inPart_ ? "if(f.kernel->ending(ctx)) return;"
                         : "if(f.kernel->ending(ctx)) return KIRIMA_STOP;")
             << (bits ? " } }" : "") << " } }\n";
        return true;
    }

    bool storage(const sir::Instr& in) {
        const auto& target = in.operands[0];
        if (target.kind != Operand::Kind::Slot) return false;
        const auto& slot = d_.slot(target.asSlot());
        const auto width = target.width ? target.width : slot.type.width;
        if (!sir::isIntegral(slot.type)) return false;
        if (in.op == Op::StoreBits || in.op == Op::NbaBits) return indexedStorage(in, slot);
        const auto elem = element(in, slot);
        if (!elem) return indexedStorage(in, slot);
        const std::string where = std::to_string(slots_[target.id]) + "," + std::to_string(*elem);
        if (in.op == Op::Load || in.op == Op::LoadElem) {
            const std::string source = "ctx,f," + where;
            if (width <= 64)
                loadField(in.result.index(), target, *elem, target.lsb, width, !fourResult(in.type),
                          source);
            else {
                if (wideWords_) {
                    if (in.type.width != width) return false;
                    const bool four = sir::isFourState(slot.type);
                    if (guard_ && four)
                        for (std::uint32_t bit = 0; bit < width; bit += 64)
                            guard_->push_back(
                                sir::physicalRange(d_, {target.asSlot(), *elem, 1, target.lsb + bit,
                                                        std::min<std::uint32_t>(64, width - bit)}));
                    out_ << "  packed_load(" << wideTemp(in.result.index())
                         << ",f.kernel->slot_words(ctx," << where << ")," << (four ? 2 : 1) << ","
                         << target.lsb << "," << width << "," << (four && !guard_) << ","
                         << !fourResult(in.type) << ");\n";
                    return true;
                }
                if (packedReads_.empty() || scalar(in.type)) return false;
                // Capture at the load, before an intervening store or NBA.
                const auto& fields = packedReads_[in.result.index()];
                if (fields.empty()) return true;
                if (fields.size() > 1)
                    out_ << "  { const auto* words = f.kernel->slot_words(ctx," << where << ");\n";
                for (const auto& projection : fields)
                    loadField(projection.dest, target, *elem, target.lsb + projection.lsb,
                              projection.width, projection.two || !fourResult(in.type),
                              fields.size() > 1 ? "words" : source);
                if (fields.size() > 1) out_ << "  }\n";
            }
            return true;
        }
        const bool nba = in.op == Op::Nba || in.op == Op::NbaElem;
        const auto checkEnd = inPart_ ? "if (f.kernel->ending(ctx)) return;"
                                      : "if (f.kernel->ending(ctx)) return KIRIMA_STOP;";
        const auto& rhs = in.operands.back();
        if (width > 64) {
            if (!wideWords_ || rhs.kind != Operand::Kind::Temp || type(rhs).width != width)
                return false;
            auto words = wideTemp(rhs.id);
            out_ << "  { ";
            if (!sir::isFourState(slot.type)) {
                const auto n = (std::size_t{width} + 63) / 64;
                out_ << "uint64_t known[" << n << "]; for (unsigned i=0;i<" << n
                     << ";++i) known[i]=" << words << "[2*i]&~" << words << "[2*i+1]; ";
                words = "known";
            }
            out_ << "f.kernel->" << (nba ? "schedule_nba(ctx,f.process," : "write(ctx,") << where
                 << "," << target.lsb << "," << target.width << "," << words << (nba ? ",0" : "")
                 << "); " << checkEnd << " }\n";
            return true;
        }
        const auto word = value(rhs);
        out_ << "  { const W4 v=" << (sir::isFourState(slot.type) ? word : "to2(" + word + ")")
             << "; const uint64_t words[]={v.a,v.b}; f.kernel->"
             << (nba ? "schedule_nba(ctx,f.process," : "write(ctx,") << where << "," << target.lsb
             << "," << target.width << ",words" << (nba ? ",0" : "") << "); " << checkEnd << " }\n";
        return true;
    }
    std::string knownExpression(const sir::Instr& in) const {
        // A two-state destination alone does not justify integer arithmetic:
        // both inputs must have no X/Z before the operation.
        std::string_view op;
        switch (in.op) {
            case Op::Add: op = "+"; break;
            case Op::Sub: op = "-"; break;
            case Op::Mul: op = "*"; break;
            case Op::And: op = "&"; break;
            case Op::Or: op = "|"; break;
            case Op::Xor: op = "^"; break;
            default: return {};
        }
        const auto known = [&](const Operand& o) {
            if (guard_ || approximateTwo_) return true;
            if (o.kind == Operand::Kind::Temp) return static_cast<bool>(known_[o.id]);
            const auto& c = d_.constant(o.asConst());
            return std::ranges::all_of(c.bWords, [](auto w) { return w == 0; });
        };
        if (!std::ranges::all_of(in.operands, known)) return {};
        std::ostringstream expr;
        expr << "W4{(" << value(in.operands[0]) << ".a " << op << ' ' << value(in.operands[1])
             << ".a) & mask(" << in.type.width << "),0}";
        return expr.str();
    }
    std::string expression(const sir::Instr& in) const {
        const auto& ops = in.operands;
        const auto v = [&](std::size_t i) { return value(ops[i]); };
        const auto w = std::to_string(in.type.width);
        const auto inputWidth = std::to_string(type(ops[0]).width);
        const auto signedInput = sir::isSigned(type(ops[0])) ? "true" : "false";
        const auto two = [&](const std::string& name) {
            return name + "(" + v(0) + "," + v(1) + ")";
        };
        const auto arithmetic = [&](const std::string& name) {
            return name + "(" + v(0) + "," + v(1) + "," + w + ")";
        };
        const auto compare = [&](const std::string& name) {
            return name + "(" + v(0) + "," + v(1) + "," + inputWidth + "," + signedInput + ")";
        };
        switch (in.op) {
            case Op::Add: return arithmetic("add4");
            case Op::Sub: return arithmetic("sub4");
            case Op::Mul: return arithmetic("mul4");
            case Op::Div: return compare("div4");
            case Op::Mod: return compare("mod4");
            case Op::Gt: return compare("gt4");
            case Op::Ge: return compare("ge4");
            case Op::Lt: return compare("lt4");
            case Op::Le: return compare("le4");
            case Op::Eq: return two("eq4");
            case Op::Ne: return two("ne4");
            case Op::Ceq: return two("ceq4");
            case Op::Cne: return two("cne4");
            case Op::Weq: return two("weq4");
            case Op::Wne: return two("wne4");
            case Op::And: return two("and4");
            case Op::Or: return two("or4");
            case Op::Xor: return two("xor4");
            case Op::Xnor: return arithmetic("xnor4");
            case Op::Shl: return arithmetic("shl4");
            case Op::Shr: return arithmetic("shr4");
            case Op::Ashr: return compare("ashr4");
            case Op::Lor: return two("lor4");
            case Op::Land: return two("land4");
            case Op::Neg: return "neg4(" + v(0) + "," + w + ")";
            case Op::Not: return "not4(" + v(0) + "," + w + ")";
            case Op::Lnot: return "lnot4(" + v(0) + ")";
            case Op::Rand: return "redAnd4(" + v(0) + "," + inputWidth + ")";
            case Op::Rnand: return "redNand4(" + v(0) + "," + inputWidth + ")";
            case Op::Ror: return "redOr4(" + v(0) + ")";
            case Op::Rnor: return "redNor4(" + v(0) + ")";
            case Op::Rxor: return "redXor4(" + v(0) + ")";
            case Op::Rxnor: return "redXnor4(" + v(0) + ")";
            case Op::Truth: return "truth4(" + v(0) + ")";
            case Op::Sext: return "sext4(" + v(0) + "," + inputWidth + "," + w + ")";
            case Op::Trunc: return "trunc4(" + v(0) + "," + w + ")";
            case Op::Zext:
            case Op::To2:
            case Op::To4: return v(0);
            case Op::Select: return "select4(" + v(0) + "," + v(1) + "," + v(2) + "," + w + ")";
            case Op::Concat: {
                std::ostringstream result;
                for (std::size_t i = 0; i < ops.size(); ++i)
                    result << "concat4(";
                result << "W4{0,0}";
                for (const auto& op : ops)
                    result << ',' << value(op) << ',' << type(op).width << ')';
                return result.str();
            }
            case Op::Repl:
                return "repl4(" + v(1) + "," + std::to_string(type(ops[1]).width) + "," + v(0) +
                       ".a)";
            case Op::Partsel:
            case Op::Bitsel:
            case Op::IdxPartsel:
                if (bounds_) return {};
                return "select_bits(" + v(0) + "," + inputWidth + "," + v(1) + "," +
                       std::to_string(type(ops[1]).width) + "," +
                       (sir::isSigned(type(ops[1])) ? "true" : "false") + "," + w + ")";
            default: return {};
        }
    }
    bool closedOverKnownInputs(const sir::Instr& in) const {
        for (const auto& o : in.operands)
            if (o.kind == Operand::Kind::Const &&
                std::ranges::any_of(d_.constant(o.asConst()).bWords,
                                    [](auto word) { return word != 0; }))
                return false;
        if (in.op == Op::Div || in.op == Op::Mod) {
            const auto& divisor = in.operands[1];
            return divisor.kind == Operand::Kind::Const &&
                   std::ranges::any_of(d_.constant(divisor.asConst()).aWords,
                                       [](auto word) { return word != 0; });
        }
        if (in.op == Op::Bitsel || in.op == Op::IdxPartsel || in.op == Op::Partsel) {
            const auto& selector = in.operands[1];
            if (selector.kind != Operand::Kind::Const) return false;
            const auto pos = sir::constantIndex(d_.constant(selector.asConst()));
            const auto width = type(in.operands[0]).width;
            return pos && *pos >= 0 && in.type.width <= width && *pos <= width - in.type.width;
        }
        return true;
    }
    bool instruction(const sir::Instr& in) {
        if (in.result != sir::kNone && readAlias_[in.result.index()] != in.result.index())
            return true;
        if (guard_ && !closedOverKnownInputs(in)) return false;
        if (wideWords_) {
            const bool wideOperand = std::ranges::any_of(in.operands, [&](const auto& o) {
                return (o.kind == Operand::Kind::Temp || o.kind == Operand::Kind::Const) &&
                       !scalar(type(o));
            });
            if (wideOperand || (in.result != sir::kNone && !scalar(in.type))) {
                switch (in.op) {
                    case Op::Bitsel:
                    case Op::Partsel:
                    case Op::IdxPartsel: return wideSelect(in);
                    case Op::Load:
                    case Op::LoadElem:
                    case Op::Store:
                    case Op::StoreElem:
                    case Op::Nba:
                    case Op::NbaElem:
                    case Op::NbaBits:
                    case Op::StoreBits: return storage(in);
                    default: return wideLogic(in);
                }
            }
        }
        if (!packedReads_.empty() &&
            (in.op == Op::Bitsel || in.op == Op::Partsel || in.op == Op::IdxPartsel) &&
            in.operands[0].kind == Operand::Kind::Temp && !scalar(type(in.operands[0])))
            return true;
        for (const auto& o : in.operands)
            if ((o.kind == Operand::Kind::Temp || o.kind == Operand::Kind::Const) &&
                !scalar(type(o)))
                return false;
        if (!p_.frame.empty() && (in.op == Op::Br || in.op == Op::BrIf))
            out_
                << "  { uint32_t taken; if (process->checkpoint(ctx,KIRIMA_TAKE_INTERRUPT,&taken)"
                   " != KIRIMA_PROCESS_OK) return KIRIMA_ERROR; if (taken) return KIRIMA_STOP; }\n";
        switch (in.op) {
            case Op::Fload:
                out_ << "  t[" << in.result.index() << "] = locals[" << in.operands[0].id << "];\n";
                return true;
            case Op::Fstore:
                out_ << "  locals[" << in.operands[0].id << "] = " << value(in.operands[1])
                     << ";\n";
                return true;
            case Op::Udp: {
                const auto id = in.operands.front().id;
                if (!smallLevelUdp(d_.types[id])) return false;
                out_ << "  t[" << in.result.index() << "] = " << (approximateTwo_ ? "to2(" : "")
                     << "udp_value(udp" << id << "[";
                for (std::size_t i = 1; i < in.operands.size(); ++i) {
                    if (i > 1) out_ << " | ";
                    out_ << "(udp_digit(" << value(in.operands[i]) << ") << " << 2 * (i - 1) << ")";
                }
                out_ << "])" << (fourResult(in.type) ? "" : ")") << ";\n";
                return true;
            }
            case Op::Call: {
                const auto& sub = d_.subroutines[in.operands.front().id];
                const auto index = sir::returnedScalarInput(sub);
                if (!index || type(in.operands[*index + 1]) != sub.returnType) return false;
                out_ << "  t[" << in.result.index() << "] = " << value(in.operands[*index + 1])
                     << ";\n";
                return true;
            }
            case Op::Ret: out_ << "  return KIRIMA_DONE;\n"; return true;
            case Op::Br: return jump(in, 0);
            case Op::BrIf:
                out_ << "  if ((" << value(in.operands[0]) << ".a & ~" << value(in.operands[0])
                     << ".b & 1u) != 0) ";
                if (!jump(in, 1)) return false;
                return jump(in, 2 + in.operands[1].count);
            case Op::Load:
            case Op::LoadElem:
            case Op::Store:
            case Op::StoreElem:
            case Op::Nba:
            case Op::NbaElem:
            case Op::NbaBits:
            case Op::StoreBits: return storage(in);
            default: break;
        }
        if (in.result == sir::kNone || in.operands.empty() ||
            !std::ranges::all_of(in.operands, [](const auto& o) {
                return o.kind == Operand::Kind::Temp || o.kind == Operand::Kind::Const;
            }))
            return false;
        auto rhs = knownExpression(in);
        if (rhs.empty()) rhs = expression(in);
        if (rhs.empty()) return false;
        out_ << "  t[" << in.result.index()
             << "] = " << (fourResult(in.type) ? rhs : "to2(" + rhs + ")") << ";\n";
        return true;
    }
};
Model makeModel(const sir::VerifiedDesign& verified, const analysis::TableData& table,
                std::uint64_t maxSteps, bool checkBounds, std::vector<KnownIsland>* guards,
                EmissionOptions options) {
    Model result;
    if (!analysis::matchesIslandAnalysis(verified, table)) return result;
    const auto& d = verified.design();
    if (options.reportEmissions) result.emissions.resize(d.processes.size());
    const auto record = [&](sir::ProcessId id) -> BodyEmission* {
        return options.reportEmissions ? &result.emissions[id.index()] : nullptr;
    };
    std::vector<std::uint32_t> slots;
    slots.reserve(d.slots.size());
    std::uint32_t count = 0;
    for (const auto& s : d.slots)
        slots.push_back(s.viewOf == sir::kNone ? count++ : slots[s.viewOf.index()]);
    std::vector<std::vector<bool>> known(d.processes.size());
    for (const auto& row : table.members)
        known[row.process.index()].resize(d.process(row.process).numTemps);
    std::vector<sir::ProcessId> ordinary;
    if (options.includeOrdinary && !guards) {
        for (std::uint32_t p = 0; p < d.processes.size(); ++p) {
            const auto& proc = d.processes[p];
            if (proc.kind == sir::ProcessKind::CombAt && !proc.triggers.empty() &&
                proc.implicitContainers.empty() && proc.strengthTriggers.empty() &&
                sir::processPolicy(d, proc).start == sir::ProcessStart::ArmNow &&
                std::ranges::none_of(proc.triggers,
                                     [](const auto& t) { return t.guard != sir::kNone; })) {
                ordinary.emplace_back(p);
                known[p].resize(proc.numTemps);
            }
        }
    }
    if (!guards)
        for (const auto& fact : analysis::analyzeValues(verified, false)) {
            const auto& site = fact.site;
            if (site.owner != sir::Owner::Kind::Process || known[site.row].empty() ||
                fact.noX == analysis::NoXProof::Unknown)
                continue;
            const auto& block = d.processes[site.row].blocks[site.block];
            if (site.kind == analysis::SiteKind::Result)
                known[site.row][block.instrs[site.instruction].result.index()] = true;
            else if (site.kind == analysis::SiteKind::BlockParameter)
                known[site.row][block.params[site.index].temp.index()] = true;
        }
    std::vector<std::string> bodies;
    bodies.reserve(table.members.size());
    std::vector<bool> supported;
    for (const auto& row : table.members) {
        if (supported.size() <= row.island) supported.resize(row.island + 1, true);
        if (guards && guards->size() <= row.island) guards->resize(row.island + 1);
        auto* guard = guards ? &(*guards)[row.island] : nullptr;
        if (guard) guard->island = row.island;
        bodies.push_back(Body(d, d.process(row.process), slots, checkBounds,
                              known[row.process.index()], guard ? &guard->inputs : nullptr,
                              options.approximateTwoState)
                             .emit(row.process, maxSteps, record(row.process),
                                   options.allowFrameLoops && !guard));
        if (bodies.back().empty()) supported[row.island] = false;
    }
    for (std::uint32_t i = 0; i < supported.size(); ++i)
        if (!supported[i]) result.unsupportedIslands.push_back(i);
    if (guards) {
        std::erase_if(*guards, [&](const auto& guard) { return !supported[guard.island]; });
        for (auto& guard : *guards) {
            const auto key = [](const sir::SlotRange& r) {
                return std::tie(r.slot, r.firstElem, r.lsb, r.width);
            };
            std::ranges::sort(guard.inputs, {}, key);
            guard.inputs.erase(std::unique(guard.inputs.begin(), guard.inputs.end()),
                               guard.inputs.end());
        }
    }
    std::vector<std::string> emitted;
    for (std::size_t i = 0; i < table.members.size(); ++i)
        if (supported[table.members[i].island]) {
            result.bodies.push_back(table.members[i].process);
            emitted.push_back(std::move(bodies[i]));
        }
    for (auto id : ordinary) {
        auto body = Body(d, d.process(id), slots, checkBounds, known[id.index()], nullptr,
                         options.approximateTwoState)
                        .emit(id, maxSteps, record(id));
        if (!body.empty()) {
            result.bodies.push_back(id);
            emitted.push_back(std::move(body));
        }
    }
    if (result.bodies.empty()) return result;
    std::set<std::uint32_t> udpTypes;
    for (const auto id : result.bodies)
        for (const auto& block : d.process(id).blocks)
            for (const auto& ins : block.instrs)
                if (ins.op == Op::Udp) udpTypes.insert(ins.operands.front().id);
    std::ostringstream out;
    const bool hasFrames =
        std::ranges::any_of(result.bodies, [&](auto id) { return !d.process(id).frame.empty(); });
    if (hasFrames) out << "#include \"kirimasim/kernel/process_abi.h\"\n";
    if (!udpTypes.empty()) out << "#include \"kirimasim/kernel/udp.h\"\n";
    out << R"(#include "kirimasim/kernel/model_abi.h"
#include "kirimasim/kernel/bits.h"
namespace {
using namespace kirimasim::kernel;
W4 load(const uint64_t* p,unsigned lsb,unsigned width,unsigned stride,bool readUnknown) {
    return {readField(p,stride,lsb,width),readUnknown ? readField(p+1,stride,lsb,width) : 0};
}
W4 load(kirima_context* ctx,kirima_model_activation& f,unsigned slot,unsigned elem,
        unsigned lsb,unsigned width,unsigned stride,bool readUnknown) {
    return load(f.kernel->slot_words(ctx,slot,elem),lsb,width,stride,readUnknown);
}
std::optional<uint32_t> element_at(std::optional<uint32_t> prior,W4 index,unsigned width,
                                   bool sign,int64_t left,int64_t right) {
    if (!prior) return {};
    const auto pos=position4(index,width,sign);
    if (!pos || *pos < (left<right ? left : right) || *pos > (left<right ? right : left)) return {};
    const uint64_t first=static_cast<uint64_t>(left), last=static_cast<uint64_t>(right);
    const uint64_t at=static_cast<uint64_t>(*pos);
    const auto length=(left<right ? last-first : first-last)+1;
    const auto offset=left<right ? at-first : first-at;
    return static_cast<uint32_t>(*prior*length+offset);
}
void packed_load(uint64_t* dst,const uint64_t* src,unsigned stride,unsigned lsb,unsigned width,
                 bool four,bool two) {
    for (unsigned bit=0;bit<width;bit+=64) {
        const unsigned n = width-bit<64 ? width-bit : 64;
        W4 x{readField(src,stride,lsb+bit,n),four ? readField(src+1,stride,lsb+bit,n) : 0};
        if (two) x=to2(x);
        dst[bit/64*2]=x.a;
        dst[bit/64*2+1]=x.b;
    }
}
W4 packed_select(const uint64_t* src,unsigned xw,W4 index,unsigned iw,bool sign,unsigned width) {
    const auto pos=position4(index,iw,sign);
    const auto at=pos ? inside(*pos,width,xw) : std::nullopt;
    if (!at) return allX(width);
    const auto outside=mask(width) & ~(mask(at->n)<<at->skip);
    return {outside | (readField(src,2,at->lo,at->n)<<at->skip),
            outside | (readField(src+1,2,at->lo,at->n)<<at->skip)};
}
W4 select_bits(W4 v,unsigned width,W4 index,unsigned indexWidth,bool sign,unsigned n) {
    const auto pos = position4(index,indexWidth,sign);
    return pos ? bits4(v,width,*pos,n) : allX(n);
}
)";
    if (!udpTypes.empty()) {
        out << "unsigned udp_digit(W4 v) { return (v.a & 1U) | ((v.b & 1U) << 1U); }\n"
               "W4 udp_value(unsigned code) { return code == kUdpX ? W4{1,1} : W4{code,0}; }\n";
        for (auto id : udpTypes) {
            const auto& t = d.types[id];
            out << "constexpr char udp_rows" << id << "[][" << t.inputs + 2 << "] = {";
            for (const auto& row : t.rows)
                out << '\"' << row.inputs << row.output << "\",";
            out << "};\nconstexpr auto udp" << id << " = udpLevelLookup<" << t.inputs
                << ">(udp_rows" << id << ");\n";
        }
    }
    for (const auto& body : emitted)
        out << body;

    std::vector<sir::FramePayloadSize> payloads;
    for (auto id : result.bodies) {
        const auto& frame = d.process(id).frame;
        if (!frame.empty()) out << "constexpr kirima_model_field fields" << id.index() << "[]={\n";
        const auto writeField = [](void* raw, const sir::FrameVar&,
                                   const sir::FrameFieldLayout& field) {
            auto& os = *static_cast<std::ostringstream*>(raw);
            os << '{' << field.offset.value_or(UINT64_MAX) << "ULL," << field.logicalOffset
               << "ULL," << field.size << "ULL," << field.elements << "ULL," << field.id.index()
               << ',' << field.chunksPerElement << "},\n";
        };
        const auto size = sir::framePayloadLayout(frame, writeField, &out);
        if (!size) return {};
        payloads.push_back(*size);
        if (!frame.empty()) out << "};\n";
    }
    out << "constexpr kirima_model_body bodies[] = {\n";
    for (std::size_t i = 0; i < result.bodies.size(); ++i) {
        const auto id = result.bodies[i];
        const auto count = d.process(id).frame.size();
        out << "  {{KIRIMA_MODEL_PROCESS," << id.index() << ',' << count << ','
            << sir::kFrameCellAlignment << ',' << payloads[i].bytes << "ULL,"
            << payloads[i].logicalBytes << "ULL,"
            << (count ? "fields" + std::to_string(id.index()) : "nullptr") << "},body" << id.index()
            << ",nullptr},\n";
    }
    out << "};\n";
    if (hasFrames)
        out << "constexpr kirima_service_requirement services[] = {"
               "{KIRIMA_SERVICE_PROCESS,KIRIMA_PROCESS_VERSION}};\n";
    out << "constexpr kirima_module_table "
           "model{KIRIMA_MODULE_DESCRIPTOR,sizeof(kirima_module_table),"
           "KIRIMA_ABI_MAJOR,KIRIMA_ABI_MINOR,{1,KIRIMA_MODEL_LAYOUT_64_V1,{";
    for (auto byte : table.key.design)
        out << static_cast<unsigned>(byte) << ",";
    out << "}}," << result.bodies.size() << (hasFrames ? ",1,bodies,services" : ",0,bodies,nullptr")
        << "};\n}\n"
           "extern \"C\" KIRIMA_MODULE_EXPORT const kirima_module_table* "
           "kirima_get_module_table(uint32_t major,uint32_t minor) {\n"
           "  return major == model.abi_major && minor >= model.minimum_minor ? &model : "
           "nullptr;\n}\n";
    result.source = out.str();
    return result;
}
}  // namespace

Model emitModel(const sir::VerifiedDesign& verified, const analysis::TableData& table,
                std::uint64_t maxSteps, bool checkBounds, EmissionOptions options) {
    return makeModel(verified, table, maxSteps, checkBounds, nullptr, options);
}
KnownModel emitKnownModel(const sir::VerifiedDesign& verified, const analysis::TableData& table,
                          std::uint64_t maxSteps, bool checkBounds) {
    const auto& d = verified.design();
    // Foreign calls and synchronous guard/watch/path callbacks may write a
    // value between two ordinary instructions in the same island evaluation.
    if (d.foreignReflection || !d.foreignObjects.empty() || !d.strengthNodes.empty() ||
        !d.modulePaths.empty() ||
        std::ranges::any_of(d.processes, [](const auto& p) { return p.netResolver; }) ||
        std::ranges::any_of(d.subroutines, [](const auto& sub) {
            return sir::readsWaiterFrame(sub.kind) ||
                   sub.kind == sir::SubroutineKind::PathCondition;
        }))
        return {};
    KnownModel result;
    result.model = makeModel(verified, table, maxSteps, checkBounds, &result.guards, {});
    return result;
}
}  // namespace kirimasim::codegen
