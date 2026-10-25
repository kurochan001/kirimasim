// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/verify.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace kirimasim::sir {
namespace {

struct Body {
    Owner owner;
    const std::vector<FrameVar>* frame = nullptr;
    const std::vector<BasicBlock>* blocks = nullptr;
    const ActivationInfo* activation = nullptr;
    const Process* process = nullptr;
    const Subroutine* subroutine = nullptr;
};

bool hasMetadata(const std::vector<FrameVar>& frame, const ActivationInfo& info) {
    return !info.scopes.empty() || info.root != kNone || !info.extents.empty() ||
           info.inheritCaptures || std::any_of(frame.begin(), frame.end(), [](const auto& f) {
               return f.home != FrameHome::Ordinary || f.activationScope != kNone ||
                      f.captureSource != kNone || f.readonly || f.refStatic;
           });
}

bool hasMetadata(const Body& body) { return hasMetadata(*body.frame, *body.activation); }

bool sameShape(const FrameVar& a, const FrameVar& b) {
    return a.type == b.type && a.target == b.target && a.declType == b.declType &&
           a.refStatic == b.refStatic && a.dims.size() == b.dims.size() &&
           std::equal(
               a.dims.begin(), a.dims.end(), b.dims.begin(),
               [](const auto& x, const auto& y) { return x.left == y.left && x.right == y.right; });
}

bool storageType(Type t) {
    if (isIntegral(t)) return t.width != 0;
    switch (t.kind) {
        case TypeKind::Real:
        case TypeKind::ShortReal:
        case TypeKind::String:
        case TypeKind::Chandle:
        case TypeKind::DynArray:
        case TypeKind::Queue: return true;
        default: return false;
    }
}

bool dimensionsFit(const FrameVar& field) {
    std::uint64_t elements = 1;
    for (const auto& dim : field.dims) {
        const auto left = static_cast<std::uint64_t>(dim.left);
        const auto right = static_cast<std::uint64_t>(dim.right);
        const auto count = (dim.left >= dim.right ? left - right : right - left) + 1;
        if (count == 0 || count > UINT32_MAX / elements) return false;
        elements *= count;
    }
    return true;
}

struct State {
    std::vector<ActivationScopeId> scopes;
    std::vector<std::pair<std::uint32_t, std::size_t>> controls;
    friend bool operator==(const State&, const State&) = default;
};

class Checker {
public:
    explicit Checker(const Design& design) : design_(design) {}
    std::vector<VerifyError> run() {
        for (std::uint32_t i = 0; i < design_.processes.size(); ++i)
            check(of(Owner::of(ProcessId{i})));
        for (std::uint32_t i = 0; i < design_.subroutines.size(); ++i)
            check(of(Owner::of(SubroutineId{i})));
        std::vector<unsigned char> colors(design_.processes.size());
        for (std::uint32_t i = 0; i < design_.processes.size(); ++i) {
            std::vector<std::uint32_t> path;
            auto at = i;
            while (at < colors.size() && colors[at] == 0) {
                colors[at] = 1;
                path.push_back(at);
                const auto parent = design_.processes[at].parent;
                at = parent.kind == Owner::Kind::Process ? parent.id : kNone;
            }
            if (at < colors.size() && colors[at] == 1)
                error(of(Owner::of(ProcessId{i})), "fork parent cycle");
            for (auto entry : path)
                colors[entry] = 2;
        }
        return std::move(errors_);
    }

private:
    const Design& design_;
    std::vector<VerifyError> errors_;
    Body of(Owner owner) const {
        if (owner.kind == Owner::Kind::Process && owner.id < design_.processes.size()) {
            const auto& p = design_.processes[owner.id];
            return {owner, &p.frame, &p.blocks, &p.activation, &p, nullptr};
        }
        if (owner.kind == Owner::Kind::Subroutine && owner.id < design_.subroutines.size()) {
            const auto& s = design_.subroutines[owner.id];
            return {owner, &s.frame, &s.blocks, &s.activation, nullptr, &s};
        }
        return {};
    }
    static std::string where(const Body& body) {
        return std::string(body.process ? "process @" : "subroutine @") +
               std::to_string(body.owner.id);
    }
    void error(const Body& body, std::string message, SourceLoc loc = {}) {
        errors_.push_back({where(body), std::move(message), loc});
    }
    void metadata(const Body& body) {
        const auto& info = *body.activation;
        const auto& frame = *body.frame;
        if (info.root != kNone && (info.root.index() >= info.scopes.size() ||
                                   info.scopes[info.root.index()].parent != kNone))
            error(body, "automatic root scope is invalid");
        std::vector<unsigned> membership(frame.size());
        std::vector<unsigned char> colors(info.scopes.size());
        for (std::uint32_t i = 0; i < info.scopes.size(); ++i) {
            const auto& scope = info.scopes[i];
            if (scope.parent != kNone && scope.parent.index() >= info.scopes.size())
                error(body, "automatic scope parent is out of range");
            for (auto f : scope.fields) {
                if (f.index() >= frame.size()) {
                    error(body, "automatic scope field is out of range");
                    continue;
                }
                ++membership[f.index()];
                if (frame[f.index()].home != FrameHome::Scoped ||
                    frame[f.index()].activationScope.index() != i)
                    error(body, "automatic scope field has a different home");
            }
            std::vector<std::uint32_t> path;
            std::uint32_t at = i;
            while (at < info.scopes.size() && colors[at] == 0) {
                colors[at] = 1;
                path.push_back(at);
                at = info.scopes[at].parent.index();
            }
            if (at < info.scopes.size() && colors[at] == 1)
                error(body, "automatic scope parent cycle");
            for (auto entry : path)
                colors[entry] = 2;
        }
        const Body parent = body.process ? of(body.process->parent) : Body{};
        if ((!info.extents.empty() || info.inheritCaptures) && parent.frame == nullptr)
            error(body, "capture extents need a fork parent");
        std::vector<ActivationScopeId> extents;
        for (auto scope : info.extents) {
            if (parent.frame == nullptr || scope.index() >= parent.activation->scopes.size()) {
                error(body, "captured extent is out of range in its parent");
            } else {
                const auto wanted = extents.empty() ? ActivationScopeId{kNone} : extents.back();
                if (parent.activation->scopes[scope.index()].parent != wanted ||
                    (extents.empty() && parent.activation->root != kNone &&
                     scope != parent.activation->root))
                    error(body, "captured extents are not a complete lexical chain");
            }
            if (std::find(extents.begin(), extents.end(), scope) != extents.end())
                error(body, "captured extent is duplicated");
            extents.push_back(scope);
        }
        for (std::uint32_t i = 0; i < frame.size(); ++i) {
            const auto& f = frame[i];
            if (f.refStatic && f.type.kind != TypeKind::Ref)
                error(body, "ref-static requires a reference binding");
            switch (f.home) {
                case FrameHome::Ordinary:
                    if (f.activationScope != kNone || f.captureSource != kNone ||
                        membership[i] != 0 || (f.readonly && f.type.kind != TypeKind::Ref))
                        error(body, "ordinary frame row has automatic-field metadata");
                    break;
                case FrameHome::Scoped:
                    if (f.activationScope.index() >= info.scopes.size() || membership[i] != 1 ||
                        f.captureSource != kNone)
                        error(body, "scoped frame row needs exactly one matching field");
                    if (!storageType(f.type) || !dimensionsFit(f) || f.readonly || f.refStatic)
                        error(body, "automatic field needs a supported typed storage layout");
                    break;
                case FrameHome::Capture: {
                    if (f.activationScope != kNone || membership[i] != 0 ||
                        parent.frame == nullptr ||
                        f.captureSource.index() >= parent.frame->size()) {
                        error(body, "capture source is invalid in its fork parent");
                        break;
                    }
                    const auto& source = (*parent.frame)[f.captureSource.index()];
                    if (!sameShape(f, source) || (source.readonly && !f.readonly))
                        error(body, "capture type, shape or access rights differ from its source");
                    if (source.home == FrameHome::Scoped) {
                        if (std::find(info.extents.begin(), info.extents.end(),
                                      source.activationScope) == info.extents.end())
                            error(body, "captured field lacks its parent extent");
                    } else if (source.home == FrameHome::Capture) {
                        if (!info.inheritCaptures)
                            error(body, "forwarded capture lacks inherited extents");
                    } else if (source.home != FrameHome::Ordinary ||
                               source.type.kind != TypeKind::Ref) {
                        error(body, "a value capture requires promoted shared storage");
                    }
                    break;
                }
                default: error(body, "unknown frame storage home"); break;
            }
        }
        if (body.subroutine) {
            for (const auto& param : body.subroutine->params) {
                const bool ref = param.dir == Direction::Ref || param.dir == Direction::ConstRef;
                if (param.refStatic && !ref)
                    error(body, "ref-static qualifier on a value parameter");
                if (param.frameVar.index() < frame.size()) {
                    const auto& f = frame[param.frameVar.index()];
                    if (f.home == FrameHome::Scoped && f.activationScope != info.root)
                        error(body, "scoped parameter must be in the call root before copy-in");
                }
                if (ref && param.frameVar.index() < frame.size()) {
                    const auto& f = frame[param.frameVar.index()];
                    if (f.refStatic != param.refStatic ||
                        (param.dir == Direction::ConstRef && !f.readonly && hasMetadata(body)))
                        error(body, "reference parameter qualifiers differ from its binding");
                }
            }
        }
    }
    void helper(const Body& body, SubroutineId id, const State& state, SourceLoc loc) {
        if (id.index() >= design_.subroutines.size()) return;
        const auto& sub = design_.subroutines[id.index()];
        if (!readsWaiterFrame(sub.kind) || sub.frameOf.kind != body.owner.kind ||
            sub.frameOf.id != body.owner.id)
            return;
        for (const auto& block : sub.blocks)
            for (const auto& in : block.instrs)
                for (const auto& operand : in.operands) {
                    if (operand.kind != Operand::Kind::Frame || operand.id >= body.frame->size())
                        continue;
                    const auto& frame = (*body.frame)[operand.id];
                    if (frame.home == FrameHome::Scoped &&
                        std::find(state.scopes.begin(), state.scopes.end(),
                                  frame.activationScope) == state.scopes.end())
                        error(body, "waiter-frame helper accesses an inactive automatic scope",
                              loc);
                }
    }
    void fork(const Body& body, const Instr& in, const State& state) {
        const bool joined =
            std::any_of(in.operands.begin(), in.operands.end(), [](const Operand& o) {
                return o.kind == Operand::Kind::Text && o.text == "join";
            });
        for (const auto& operand : in.operands) {
            if (operand.kind != Operand::Kind::Process || operand.id >= design_.processes.size())
                continue;
            const auto& child = design_.processes[operand.id];
            if (child.parent.kind != body.owner.kind || child.parent.id != body.owner.id) continue;
            if (child.activation.extents != state.scopes)
                error(body, "fork must retain all active lexical extents in order", in.loc);
            if (body.process &&
                (!body.activation->extents.empty() || body.activation->inheritCaptures) &&
                !child.activation.inheritCaptures)
                error(body, "nested fork must forward inherited lexical extents", in.loc);
            for (const auto& capture : child.frame) {
                if (capture.home != FrameHome::Capture ||
                    capture.captureSource.index() >= body.frame->size())
                    continue;
                const auto& source = (*body.frame)[capture.captureSource.index()];
                if (source.type.kind == TypeKind::Ref && !source.refStatic && !joined)
                    error(body, "a borrowed ref cannot escape through join_any or join_none",
                          in.loc);
            }
        }
    }
    void readonlyWrites(const Body& body, const std::vector<FrameVar>& frame) {
        for (const auto& block : *body.blocks)
            for (const auto& in : block.instrs)
                if (!in.operands.empty() && in.operands[0].kind == Operand::Kind::Frame &&
                    in.operands[0].id < frame.size() && frame[in.operands[0].id].readonly &&
                    (in.op == Op::Fstore || in.op == Op::StoreElem || in.op == Op::StoreBits ||
                     in.op == Op::StoreRef || in.op == Op::StoreRefElem ||
                     in.op == Op::StoreRefBits))
                    error(body, "write through a readonly automatic binding", in.loc);
    }
    void flow(const Body& body) {
        const auto& blocks = *body.blocks;
        if (blocks.empty()) return;
        const auto& info = *body.activation;
        std::vector<std::optional<State>> incoming(blocks.size());
        State initial;
        if (info.root != kNone && info.root.index() < info.scopes.size())
            initial.scopes.push_back(info.root);
        if (body.process)
            for (const auto& trigger : body.process->triggers)
                if (trigger.guard != kNone) helper(body, trigger.guard, initial, body.process->loc);
        incoming[0] = std::move(initial);
        std::vector<std::uint32_t> work{0};
        const auto edge = [&](const Operand& operand, const State& state, SourceLoc loc) {
            if (operand.id >= blocks.size()) return;
            if (!incoming[operand.id]) {
                incoming[operand.id] = state;
                work.push_back(operand.id);
            } else if (*incoming[operand.id] != state) {
                error(body, "automatic ownership differs across CFG edges", loc);
            }
        };
        while (!work.empty()) {
            const auto block = work.back();
            work.pop_back();
            State state = *incoming[block];
            for (const auto& in : blocks[block].instrs) {
                for (const auto& o : in.operands) {
                    if (o.kind == Operand::Kind::Subroutine)
                        helper(body, o.asSubroutine(), state, in.loc);
                    if (o.kind != Operand::Kind::Frame || o.id >= body.frame->size()) continue;
                    const auto& f = (*body.frame)[o.id];
                    if (f.home == FrameHome::Scoped &&
                        std::find(state.scopes.begin(), state.scopes.end(), f.activationScope) ==
                            state.scopes.end())
                        error(body, "frame access has no executing automatic scope", in.loc);
                }
                if (in.op == Op::ActivationEnter || in.op == Op::ActivationLeave) {
                    if (in.operands.size() != 1 || in.operands[0].kind != Operand::Kind::Scope ||
                        in.operands[0].id >= info.scopes.size()) {
                        error(body, "automatic instruction needs a valid local scope", in.loc);
                        continue;
                    }
                    const ActivationScopeId scope{in.operands[0].id};
                    const auto parent =
                        state.scopes.empty() ? ActivationScopeId{kNone} : state.scopes.back();
                    if (in.op == Op::ActivationEnter) {
                        if (scope == info.root ||
                            std::find(state.scopes.begin(), state.scopes.end(), scope) !=
                                state.scopes.end() ||
                            info.scopes[scope.index()].parent != parent)
                            error(body, "automatic scope entry has the wrong lexical parent",
                                  in.loc);
                        else
                            state.scopes.push_back(scope);
                    } else if (scope == info.root || parent != scope) {
                        error(body, "automatic scope leave is not the innermost block", in.loc);
                    } else
                        state.scopes.pop_back();
                } else if (in.op == Op::ScopeEnter && in.operands.size() == 2 &&
                           in.operands[0].kind == Operand::Kind::Scope &&
                           in.operands[1].kind == Operand::Kind::Block) {
                    edge(in.operands[1], state, in.loc);
                    state.controls.emplace_back(in.operands[0].id, state.scopes.size());
                } else if ((in.op == Op::ScopeLeave || in.op == Op::DisableScope) &&
                           in.operands.size() == 1 && in.operands[0].kind == Operand::Kind::Scope) {
                    for (std::size_t i = state.controls.size(); i-- > 0;)
                        if (state.controls[i].first == in.operands[0].id) {
                            state.scopes.resize(state.controls[i].second);
                            state.controls.resize(i);
                            break;
                        }
                } else if (in.op == Op::Fork)
                    fork(body, in, state);
                if (isTerminator(in.op))
                    for (const auto& o : in.operands)
                        if (o.kind == Operand::Kind::Block) edge(o, state, in.loc);
            }
        }
    }
    void check(Body body) {
        if (body.frame == nullptr) return;
        if (body.subroutine && readsWaiterFrame(body.subroutine->kind)) {
            if (hasMetadata(body)) error(body, "waiter-frame helpers cannot own automatic scopes");
            const auto waiter = of(body.subroutine->frameOf);
            if (waiter.frame != nullptr) readonlyWrites(body, *waiter.frame);
            for (const auto& block : *body.blocks)
                for (const auto& in : block.instrs)
                    if (in.op == Op::ActivationEnter || in.op == Op::ActivationLeave)
                        error(body, "waiter-frame helper cannot change scope ownership", in.loc);
            return;
        }
        metadata(body);
        readonlyWrites(body, *body.frame);
        bool ownership = hasMetadata(body);
        for (const auto& block : *body.blocks)
            for (const auto& in : block.instrs) {
                ownership =
                    ownership || in.op == Op::ActivationEnter || in.op == Op::ActivationLeave;
                if (in.op == Op::Fork)
                    for (const auto& o : in.operands)
                        if (o.kind == Operand::Kind::Process && o.id < design_.processes.size())
                            ownership = ownership || hasMetadata(of(Owner::of(o.asProcess())));
            }
        if (ownership) flow(body);
    }
};

}  // namespace

std::vector<VerifyError> verifyActivationScopes(const Design& design) {
    return Checker(design).run();
}

bool hasActivationMetadata(const Design& design) {
    for (const auto& process : design.processes)
        if (hasMetadata(process.frame, process.activation)) return true;
    for (const auto& subroutine : design.subroutines)
        if (hasMetadata(subroutine.frame, subroutine.activation)) return true;
    return false;
}

}  // namespace kirimasim::sir
