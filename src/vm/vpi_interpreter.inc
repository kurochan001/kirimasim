// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Private Interpreter methods. All stored bindings are numeric capabilities.
static kernel::VpiType foreignType(sir::Type type) {
    return {type.kind == sir::TypeKind::Real ? 64U : type.width, sir::isFourState(type),
            sir::isSigned(type), type.kind == sir::TypeKind::Real};
}
static bool readForeignBinding(const kernel::VpiBinding& binding, std::span<W4> out) {
    auto& vm = *static_cast<Interpreter*>(binding.owner);
    return vm.refPlace({binding.first, binding.second}, 0, binding.rootWidth)
               .read(out, binding.slot.lsb, binding.slot.width) == kernel::ActivationError::None;
}
static bool writeForeignBinding(const kernel::VpiBinding& binding, std::span<const W4> value) {
    auto& vm = *static_cast<Interpreter*>(binding.owner);
    return vm.refPlace({binding.first, binding.second}, 0, binding.rootWidth)
               .write(value, binding.slot.lsb, binding.slot.width) == kernel::ActivationError::None;
}
std::vector<kernel::VpiArgument> foreignArguments(const sir::Instr& in) {
    const auto& ops = in.operands;
    const auto site = static_cast<std::uint32_t>(value(ops[3]).a);
    std::vector<kernel::VpiArgument> arguments;
    arguments.reserve((ops.size() - 7) / 5);
    for (std::size_t i = 7; i < ops.size(); i += 5) {
        const auto type = typeOf(ops[i]);
        kernel::VpiArgument arg;
        arg.value.type = foreignType(type);
        arg.value.words = chunks(ops[i]);
        arg.name = ops[i + 3].text;
        if (ops[i + 1].kind != Operand::Kind::Text) {
            const auto r = value(ops[i + 1]), position = value(ops[i + 2]);
            if (position.b == 0 && position.a <= UINT32_MAX) {
                auto& b = arg.binding;
                b.writable = ops[i + 4].text == "variable" && !readonlyRef(r);
                b.net = ops[i + 4].text == "net";
                b.slot.lsb = static_cast<std::uint32_t>(position.a);
                b.slot.width = arg.value.type.width;
                if (!isFrameRef(r) && !isHeapRef(r)) {
                    const auto source = refSlot(r);
                    arg.name = d_.slot(source).name;
                    b.isSlot = true;
                    b.slot.slot = kernelSlot(source);
                    b.slot.elem = refElem(r);
                    b.rootWidth = foreignType(d_.slot(source).type).width;
                } else if (isFrameRef(r)) {
                    b.owner = this;
                    b.first = r.a;
                    b.second = r.b;
                    b.read = &readForeignBinding;
                    b.write = &writeForeignBinding;
                    b.rootWidth = site < vpiWidths_.size() && (i - 7) / 5 < vpiWidths_[site].size()
                                      ? vpiWidths_[site][(i - 7) / 5]
                                      : type.width;
                }
                if (b.rootWidth == 0 || b.slot.lsb > b.rootWidth ||
                    b.slot.width > b.rootWidth - b.slot.lsb)
                    b = {};
            }
        }
        arguments.push_back(std::move(arg));
    }
    return arguments;
}
bool prepareVpi() {
    if (!sir::verify(d_).empty()) return false;
    for (const auto& instance : d_.instances) {
        if (std::any_of(
                d_.foreignObjects.begin(), d_.foreignObjects.end(), [&](const auto& declaration) {
                    return declaration.slot == sir::kNone && declaration.name == instance.scopeName;
                }))
            continue;
        kernel::VpiObjectInfo info;
        info.kind = vpiModule;
        info.name = instance.scopeName;
        info.precision = d_.timePrecision;
        if (instance.parent != sir::kNone)
            info.scopeName = info.parentName = d_.instances[instance.parent.index()].scopeName;
        vpi_.object(std::move(info));
    }
    for (const auto& declaration : d_.foreignObjects) {
        if (declaration.slot == sir::kNone) {
            kernel::VpiObjectInfo info;
            info.name = declaration.name;
            info.leafName = declaration.leafName;
            info.scopeName = info.parentName = declaration.scope;
            info.unit = declaration.scale.unit;
            info.precision = declaration.scale.precision;
            info.line = declaration.location.line;
            info.file = declaration.location.file < d_.sources.size()
                            ? d_.sources[declaration.location.file]
                            : std::string{};
            switch (declaration.scopeKind) {
                case sir::ForeignScopeKind::Module: info.kind = vpiModule; break;
                case sir::ForeignScopeKind::Begin: info.kind = vpiNamedBegin; break;
                case sir::ForeignScopeKind::Fork: info.kind = vpiNamedFork; break;
                case sir::ForeignScopeKind::Task: info.kind = vpiTask; break;
                case sir::ForeignScopeKind::Function: info.kind = vpiFunction; break;
                case sir::ForeignScopeKind::Generate: info.kind = vpiGenScope; break;
                case sir::ForeignScopeKind::None: return false;
            }
            vpi_.object(std::move(info));
            continue;
        }
        const auto& slot = d_.slot(declaration.slot);
        kernel::VpiObjectInfo info;
        info.name = declaration.name;
        info.leafName = declaration.leafName;
        info.scopeName = info.parentName = declaration.scope;
        info.type = foreignType(slot.type);
        info.kind = !slot.dims.empty()                ? vpiMemory
                    : slot.kind != sir::SlotKind::Var ? vpiNet
                    : info.type.real                  ? vpiRealVar
                    : declaration.integer             ? vpiIntegerVar
                                                      : vpiReg;
        info.file = declaration.location.file < d_.sources.size()
                        ? d_.sources[declaration.location.file]
                        : std::string{};
        info.line = declaration.location.line;
        info.unit = declaration.scale.unit;
        info.precision = declaration.scale.precision;
        info.left = declaration.left;
        info.right = declaration.right;
        info.binding.isSlot = true;
        info.binding.writable = slot.kind == sir::SlotKind::Var;
        info.binding.net = slot.kind != sir::SlotKind::Var;
        info.binding.slot = {kernelSlot(declaration.slot), 0, 0, slot.type.width};
        info.binding.rootWidth = slot.type.width;
        for (const auto& dim : slot.dims)
            info.dimensions.emplace_back(dim.left, dim.right);
        vpi_.object(std::move(info));
    }
    const auto routines = vpi_.routines();
    for (std::uint32_t b = 0; b < info_.size(); ++b) {
        const BodyId body{b};
        std::map<std::uint32_t, const sir::Instr*> definitions;
        for (const auto& block : bodyBlocks(body))
            for (const auto& in : block.instrs)
                if (in.result != sir::kNone) definitions.emplace(in.result.index(), &in);
        for (const auto& block : bodyBlocks(body))
            for (const auto& in : block.instrs) {
                if (in.op != Op::Syscall || in.operands.empty() ||
                    in.operands[0].text != "$vpi.call")
                    continue;
                const auto& ops = in.operands;
                const auto routine = static_cast<std::uint32_t>(value(ops[1]).a),
                           site = static_cast<std::uint32_t>(value(ops[3]).a);
                if (routine >= routines.size() || routines[routine].name != ops[2].text) {
                    vpi_.reportError("VPI routine registration differs from the design manifest");
                    return false;
                }
                std::vector<kernel::VpiArgument> arguments;
                if (vpiWidths_.size() <= site) vpiWidths_.resize(std::size_t{site} + 1);
                for (std::size_t i = 7; i < ops.size(); i += 5) {
                    kernel::VpiArgument arg;
                    const auto type = typeIn(body, ops[i]);
                    arg.value.type = foreignType(type);
                    arg.value.words.resize(kernel::wide::chunks(type.width));
                    arg.name = ops[i + 3].text;
                    arg.available = false;
                    if (ops[i].kind == Operand::Kind::Const) {
                        arg.value.words = chunks(ops[i]);
                        arg.available = true;
                    }
                    std::uint32_t rootWidth = type.width;
                    if (ops[i + 1].kind == Operand::Kind::Temp) {
                        const auto found = definitions.find(ops[i + 1].id);
                        if (found != definitions.end() && found->second->op == Op::RefMake) {
                            const auto& target = found->second->operands[0];
                            if (target.kind == Operand::Kind::Frame) {
                                const auto& frame = bodyFrame(body)[target.id];
                                rootWidth = frame.type.kind == sir::TypeKind::Ref
                                                ? foreignType(frame.target).width
                                                : foreignType(frame.type).width;
                            } else if (target.kind == Operand::Kind::Slot) {
                                rootWidth = d_.slot(target.asSlot()).type.width;
                                if ((found->second->operands.size() == 1 ||
                                     (found->second->operands.size() == 2 &&
                                      found->second->operands.back().kind ==
                                          Operand::Kind::Text)) &&
                                    ops[i + 2].kind == Operand::Kind::Const) {
                                    arg.binding.isSlot = true;
                                    arg.binding.slot = {
                                        kernelSlot(target.asSlot()), 0,
                                        static_cast<std::uint32_t>(value(ops[i + 2]).a),
                                        type.width};
                                    arg.binding.net = ops[i + 4].text == "net";
                                    arg.binding.rootWidth = rootWidth;
                                }
                            }
                        }
                    }
                    vpiWidths_[site].push_back(rootWidth);
                    arguments.push_back(std::move(arg));
                }
                const auto unit = static_cast<std::int32_t>(value(ops[5]).a),
                           precision = static_cast<std::int32_t>(value(ops[6]).a);
                if (!vpi_.compileCall(site, routine, ops[4].text, arguments, foreignType(in.type),
                                      unit, precision,
                                      in.loc.file < d_.sources.size() ? d_.sources[in.loc.file]
                                                                      : std::string{},
                                      in.loc.line))
                    return false;
            }
    }
    return true;
}
