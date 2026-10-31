// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/classes.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <utility>

namespace kirimasim::sir {
namespace {
std::uint64_t dimensions(std::span<const ArrayDim> dims) {
    std::uint64_t count = 1;
    for (const auto dim : dims) {
        const auto lo = static_cast<std::uint64_t>(std::min(dim.left, dim.right));
        const auto hi = static_cast<std::uint64_t>(std::max(dim.left, dim.right));
        const auto distance = hi - lo;
        if (distance == UINT64_MAX || count > UINT64_MAX / (distance + 1)) return 0;
        count *= distance + 1;
    }
    return count;
}
bool sameDimensions(std::span<const ArrayDim> a, std::span<const ArrayDim> b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (dimensions(a.subspan(i, 1)) != dimensions(b.subspan(i, 1))) return false;
    return true;
}
bool valueType(const Design& d, Type type) {
    if (type.readOnlyRef) return false;
    if (isIntegral(type)) return type.width != 0 && type.detail == kNone && type.object == kNone;
    if (type.width != 0) return false;
    if (isClass(type)) return type.detail == kNone && type.object.index() < d.classes.size();
    if (type.object != kNone) return false;
    if (isContainer(type)) {
        if (type.detail.index() >= d.types.size()) return false;
        const auto kind = d.types[type.detail.index()].kind;
        return kind ==
               (type.kind == TypeKind::Queue ? TypeEntryKind::Queue : TypeEntryKind::DynArray);
    }
    if (type.detail != kNone) return false;
    return type.kind == TypeKind::Real || type.kind == TypeKind::ShortReal ||
           type.kind == TypeKind::String || type.kind == TypeKind::Event ||
           type.kind == TypeKind::Chandle;
}
bool resultType(const Design& d, Type type) { return type == voidType() || valueType(d, type); }
bool declaredElement(const Design& d, Type type, TypeId declared) {
    if (declared == kNone) return true;
    if (declared.index() >= d.types.size()) return false;
    const auto& row = d.types[declared.index()];
    switch (row.kind) {
        case TypeEntryKind::Enum:
        case TypeEntryKind::PackedStruct:
        case TypeEntryKind::PackedUnion: return row.value == type;
        case TypeEntryKind::DynArray: return type == dynarrayType(declared);
        case TypeEntryKind::Queue: return type == queueType(declared);
        default: return false;
    }
}
bool receiver(const Subroutine& sub) {
    if (sub.params.empty()) return false;
    const auto& parameter = sub.params.front();
    if (parameter.dir != Direction::Input || parameter.type != classType(sub.classOwner) ||
        parameter.slot != kNone || parameter.frameVar.index() >= sub.frame.size())
        return false;
    const auto& field = sub.frame[parameter.frameVar.index()];
    return field.type == parameter.type && field.dims.empty();
}
const std::vector<ArrayDim>* parameterDimensions(const Design& d, const Subroutine& sub,
                                                 const Param& parameter) {
    if (parameter.frameVar.index() < sub.frame.size())
        return &sub.frame[parameter.frameVar.index()].dims;
    if (parameter.slot.index() < d.slots.size()) return &d.slots[parameter.slot.index()].dims;
    return nullptr;
}

// The capability itself is not an SSA value. This flow proof restricts its
// creation, phase transitions and normal completion to its own invocation.
void construction(const Design& d, ClassValidation& result, const Subroutine& sub,
                  std::size_t number) {
    if (sub.classOwner.index() >= d.classes.size() || sub.blocks.empty()) return;
    const auto& type = d.classes[sub.classOwner.index()];
    enum : unsigned { BeforeBase = 1, Ready = 2, Initializing = 4, Body = 8, Done = 16 };
    struct Flow {
        unsigned phase = 0;
        std::vector<unsigned char> initialized;
    };
    std::vector<Flow> inputs(sub.blocks.size());
    inputs[0] = {type.base == kNone ? Ready : BeforeBase,
                 std::vector<unsigned char>(type.fields.size(), 0)};
    std::vector<std::size_t> work{0};
    std::set<std::pair<std::string, std::string>> reported;
    while (!work.empty()) {
        const auto block = work.back();
        work.pop_back();
        auto flow = inputs[block];
        const auto& instructions = sub.blocks[block].instrs;
        for (std::size_t i = 0; i < instructions.size(); ++i) {
            const auto& in = instructions[i];
            const auto bad = [&](const std::string& message) {
                const auto where = "subroutine " + std::to_string(number) + " bb" +
                                   std::to_string(block) + " instruction " + std::to_string(i);
                if (reported.emplace(where, message).second)
                    result.errors.push_back({where, message, in.loc});
            };
            switch (in.op) {
                case Op::ObjectBaseInit:
                    if (flow.phase != BeforeBase)
                        bad("base construction must run once before the own stage");
                    flow.phase = Ready;
                    break;
                case Op::ObjectStageBegin:
                    if (flow.phase != Ready)
                        bad("constructor stage must begin after its direct base");
                    flow.phase = Initializing;
                    std::fill(flow.initialized.begin(), flow.initialized.end(), 0);
                    break;
                case Op::ObjectInit:
                    if (flow.phase != Initializing && flow.phase != Body)
                        bad("constructor write needs an active own stage");
                    if (!in.operands.empty() && in.operands[0].kind == Operand::Kind::ClassField &&
                        in.operands[0].id == sub.classOwner.index() &&
                        in.operands[0].member < type.fields.size()) {
                        const auto index = in.operands[0].member;
                        const auto& field = type.fields[index];
                        if (field.constant &&
                            ((flow.phase == Initializing && !field.hasInitializer) ||
                             (flow.phase == Body && field.hasInitializer)))
                            bad("const write is outside its permitted constructor phase");
                        if (flow.phase == Body && !field.constant)
                            bad("ordinary constructor body writes do not use initialization "
                                "privilege");
                        if (flow.phase == Initializing) {
                            if (flow.initialized[index] & 1)
                                bad("declaration initialization may execute more than once");
                            flow.initialized[index] = 3;
                        }
                    }
                    break;
                case Op::ObjectStageBody:
                    if (flow.phase != Initializing)
                        bad("constructor body must follow own initialization");
                    for (std::size_t field = 0; field < type.fields.size(); ++field)
                        if (type.fields[field].hasInitializer && !(flow.initialized[field] & 2))
                            bad("constructor body may precede a required field initializer");
                    flow.phase = Body;
                    break;
                case Op::ObjectStageEnd:
                    if (flow.phase != Body)
                        bad("constructor stage completion needs its body phase");
                    flow.phase = Done;
                    break;
                case Op::Ret:
                    if (flow.phase != Done)
                        bad("constructor returns before completing its own stage");
                    break;
                default: break;
            }
        }
        if (instructions.empty()) continue;
        for (const auto& operand : instructions.back().operands) {
            if (operand.kind != Operand::Kind::Block || operand.id >= inputs.size()) continue;
            auto& next = inputs[operand.id];
            if (next.phase == 0) {
                next = flow;
                work.push_back(operand.id);
                continue;
            }
            bool changed = (next.phase | flow.phase) != next.phase;
            next.phase |= flow.phase;
            for (std::size_t field = 0; field < next.initialized.size(); ++field) {
                const auto old = next.initialized[field];
                next.initialized[field] = static_cast<unsigned char>(
                    ((old | flow.initialized[field]) & 1) | ((old & flow.initialized[field]) & 2));
                changed = changed || old != next.initialized[field];
            }
            if (changed) work.push_back(operand.id);
        }
    }
}
}  // namespace

bool ClassValidation::assignable(ClassId dynamic, ClassId view) const {
    if (dynamic.index() >= ancestors.size() || view.index() >= ancestors.size()) return false;
    const auto& bases = ancestors[dynamic.index()];
    return std::binary_search(bases.begin(), bases.end(), view);
}
std::span<const FrameVarId> ClassValidation::receivers(Owner owner) const {
    if (owner.kind == Owner::Kind::Process && owner.id < processReceivers.size())
        return processReceivers[owner.id];
    if (owner.kind == Owner::Kind::Subroutine && owner.id < subroutineReceivers.size())
        return subroutineReceivers[owner.id];
    return {};
}

ClassValidation validateClasses(const Design& d) {
    ClassValidation result;
    const auto bad = [&](const std::string& where, const std::string& message, SourceLoc loc = {}) {
        result.errors.push_back({where, message, loc});
    };
    if (d.classes.size() >= kNone || d.virtualSlots.size() >= kNone) {
        bad("classes", "class or virtual slot count exceeds the ID range");
        return result;
    }
    result.ancestors.resize(d.classes.size());
    const auto valid = [&](ClassId id) { return id.index() < d.classes.size(); };
    std::set<SlotId> staticSlots;
    for (std::size_t i = 0; i < d.classes.size(); ++i) {
        const auto& type = d.classes[i];
        const auto where = "class !c" + std::to_string(i);
        if (type.name.empty()) bad(where, "class needs a name", type.loc);
        if (type.scope.index() >= d.instances.size())
            bad(where, "declaring scope is out of range", type.loc);
        if (type.base != kNone &&
            (!valid(type.base) ||
             d.classes[type.base.index()].interfaceClass != type.interfaceClass ||
             d.classes[type.base.index()].final))
            bad(where, "invalid or final base class", type.loc);
        std::set<ClassId> interfaces;
        for (const auto parent : type.interfaces)
            if (!valid(parent) || !d.classes[parent.index()].interfaceClass ||
                d.classes[parent.index()].final || !interfaces.insert(parent).second)
                bad(where, "invalid or duplicate interface class", type.loc);
        if (type.weakTarget != kNone && !valid(type.weakTarget))
            bad(where, "weak target class is out of range", type.loc);
        if (type.interfaceClass && (!type.fields.empty() || type.constructor != kNone))
            bad(where, "interface class has instance storage or a constructor", type.loc);
        if (type.fields.size() >= kNone || type.statics.size() >= kNone)
            bad(where, "field count exceeds the ID range", type.loc);
        std::set<std::string> names;
        for (const auto& field : type.fields) {
            if (field.name.empty() || !names.insert(field.name).second)
                bad(where, "invalid or duplicate own field name", type.loc);
            if (!valueType(d, field.type))
                bad(where, "field needs an ordinary typed value", type.loc);
            const auto count = elementCount(field);
            const auto chunks = isIntegral(field.type) ? integralWords(field.type.width) : 1;
            const auto limit =
                static_cast<std::uint64_t>(std::numeric_limits<std::ptrdiff_t>::max()) / 16;
            if (count == 0 || chunks == 0 || count > limit / std::max(chunks, 1U))
                bad(where, "field layout word count overflows", type.loc);
            if (!declaredElement(d, field.type, field.declType))
                bad(where, "field declared element type differs from its value layout", type.loc);
        }
        for (const auto& field : type.statics) {
            if (field.name.empty() || !names.insert(field.name).second)
                bad(where, "invalid or duplicate own static name", type.loc);
            if (field.slot.index() >= d.slots.size()) {
                bad(where, "static slot is out of range", type.loc);
                continue;
            }
            const auto& slot = d.slots[field.slot.index()];
            if (slot.kind != SlotKind::Var || slot.instance != type.scope ||
                slot.subroutine != kNone || !staticSlots.insert(field.slot).second)
                bad(where, "static field must own one declaring-scope variable slot", type.loc);
        }
        if (!type.interfaceClass) {
            if (type.constructor.index() >= d.subroutines.size())
                bad(where, "class needs its constructor wrapper", type.loc);
            else {
                const auto& ctor = d.subroutines[type.constructor.index()];
                if (ctor.classRole != ClassRole::Constructor ||
                    ctor.classOwner != ClassId{static_cast<std::uint32_t>(i)})
                    bad(where, "constructor wrapper has the wrong owner or role", type.loc);
                if (type.weakTarget != kNone && ctor.params.size() != 1)
                    bad(where, "weak allocation wrapper takes only its implicit receiver",
                        type.loc);
            }
        }
        if (type.virtualMethods.size() != d.virtualSlots.size())
            bad(where, "virtual table has the wrong number of design slots", type.loc);
    }
    struct Visit {
        ClassId type;
        bool exit;
    };
    std::vector<unsigned char> color(d.classes.size(), 0);
    std::vector<Visit> work;
    std::vector<ClassId> order;
    bool acyclic = true;
    order.reserve(d.classes.size());
    for (std::uint32_t i = 0; i < d.classes.size(); ++i) {
        work.push_back({ClassId{i}, false});
        while (!work.empty()) {
            const auto visit = work.back();
            work.pop_back();
            auto& state = color[visit.type.index()];
            if (visit.exit) {
                state = 2;
                order.push_back(visit.type);
                continue;
            }
            if (state == 2) continue;
            if (state == 1) {
                acyclic = false;
                bad("class !c" + std::to_string(visit.type.index()), "inheritance cycle");
                continue;
            }
            state = 1;
            work.push_back({visit.type, true});
            const auto& type = d.classes[visit.type.index()];
            for (const auto parent : type.interfaces)
                if (valid(parent)) work.push_back({parent, false});
            if (valid(type.base)) work.push_back({type.base, false});
        }
    }
    for (const auto id : order) {
        auto& ancestors = result.ancestors[id.index()];
        const auto inherit = [&](ClassId parent) {
            if (!valid(parent)) return;
            const auto& source = result.ancestors[parent.index()];
            ancestors.insert(ancestors.end(), source.begin(), source.end());
        };
        const auto& type = d.classes[id.index()];
        if (acyclic) {
            inherit(type.base);
            for (const auto parent : type.interfaces)
                inherit(parent);
        }
        ancestors.push_back(id);
        std::sort(ancestors.begin(), ancestors.end());
        ancestors.erase(std::unique(ancestors.begin(), ancestors.end()), ancestors.end());
    }
    for (std::size_t i = 0; i < d.subroutines.size(); ++i) {
        const auto& sub = d.subroutines[i];
        const auto where = "subroutine " + std::to_string(i);
        if (sub.classRole == ClassRole::None) {
            if (sub.classOwner != kNone)
                bad(where, "ordinary subroutine has a class owner", sub.loc);
            continue;
        }
        if (sub.classRole != ClassRole::Constructor && sub.classRole != ClassRole::Method &&
            sub.classRole != ClassRole::StaticMethod)
            bad(where, "unknown class subroutine role", sub.loc);
        if (!valid(sub.classOwner)) {
            bad(where, "class subroutine owner is out of range", sub.loc);
            continue;
        }
        if (sub.classRole == ClassRole::Method && d.classes[sub.classOwner.index()].interfaceClass)
            bad(where, "interface methods use virtual signatures rather than executable bodies",
                sub.loc);
        if (!sub.automatic || sub.frameOf.kind != Owner::Kind::None ||
            (sub.kind != SubroutineKind::Function && sub.kind != SubroutineKind::Task))
            bad(where, "class methods need their own automatic invocation", sub.loc);
        if (sub.classRole != ClassRole::StaticMethod && !receiver(sub))
            bad(where, "instance method needs a typed input receiver", sub.loc);
        if (sub.classRole == ClassRole::Constructor) {
            if (sub.kind != SubroutineKind::Function || sub.mayWait ||
                sub.returnType != voidType() ||
                d.classes[sub.classOwner.index()].constructor !=
                    SubroutineId{static_cast<std::uint32_t>(i)})
                bad(where, "invalid constructor wrapper", sub.loc);
            construction(d, result, sub, i);
        }
    }
    for (std::size_t i = 0; i < d.virtualSlots.size(); ++i) {
        const auto& slot = d.virtualSlots[i];
        const auto where = "virtual slot !v" + std::to_string(i);
        if (slot.name.empty() || !valid(slot.declaring))
            bad(where, "virtual slot needs a declaring class and name");
        if ((slot.kind != SubroutineKind::Function && slot.kind != SubroutineKind::Task) ||
            (slot.kind != SubroutineKind::Task && slot.mayWait) || !resultType(d, slot.result) ||
            (slot.kind == SubroutineKind::Task && slot.result != voidType()))
            bad(where, "invalid virtual result or execution kind");
        if (slot.parameters.size() >= kNone)
            bad(where, "virtual parameter count exceeds the ID range");
        for (const auto& parameter : slot.parameters)
            if (!valueType(d, parameter.type) || dimensions(parameter.dims) == 0 ||
                static_cast<unsigned>(parameter.dir) > static_cast<unsigned>(Direction::ConstRef) ||
                (parameter.refStatic && parameter.dir != Direction::Ref &&
                 parameter.dir != Direction::ConstRef) ||
                !declaredElement(d, parameter.type, parameter.declType))
                bad(where, "invalid virtual parameter type or shape");
    }
    for (std::size_t i = 0; i < d.classes.size(); ++i) {
        const auto& type = d.classes[i];
        const auto where = "class !c" + std::to_string(i);
        for (std::size_t slot = 0;
             slot < std::min(type.virtualMethods.size(), d.virtualSlots.size()); ++slot) {
            const auto implementation = type.virtualMethods[slot];
            const auto& signature = d.virtualSlots[slot];
            const bool required =
                result.assignable(ClassId{static_cast<std::uint32_t>(i)}, signature.declaring);
            if (!required && implementation != kNone)
                bad(where, "unrelated virtual slot has an implementation", type.loc);
            if (implementation == kNone) {
                if (required && !type.abstract && !type.interfaceClass)
                    bad(where, "concrete class leaves a virtual method abstract", type.loc);
                continue;
            }
            if (implementation.index() >= d.subroutines.size()) {
                bad(where, "virtual implementation is out of range", type.loc);
                continue;
            }
            const auto& method = d.subroutines[implementation.index()];
            bool compatibleReturn = equivalentValueTypes(d, method.returnType, signature.result);
            if (isClass(method.returnType) && isClass(signature.result))
                compatibleReturn =
                    result.assignable(method.returnType.object, signature.result.object);
            bool compatible =
                !type.interfaceClass && method.classRole == ClassRole::Method && receiver(method) &&
                result.assignable(ClassId{static_cast<std::uint32_t>(i)}, method.classOwner) &&
                method.kind == signature.kind && method.mayWait == signature.mayWait &&
                compatibleReturn && method.params.size() == signature.parameters.size() + 1;
            for (std::size_t p = 0; compatible && p < signature.parameters.size(); ++p) {
                const auto& formal = method.params[p + 1];
                const auto& want = signature.parameters[p];
                const auto* dims = parameterDimensions(d, method, formal);
                compatible = formal.dir == want.dir && formal.refStatic == want.refStatic &&
                             equivalentValueTypes(d, formal.type, want.type) && dims &&
                             sameDimensions(*dims, want.dims);
            }
            if (!compatible)
                bad(where, "virtual override signature or owner is incompatible", type.loc);
            if (valid(type.base)) {
                const auto& base = d.classes[type.base.index()].virtualMethods;
                if (slot < base.size() && base[slot] != kNone && implementation != base[slot] &&
                    method.classOwner != ClassId{static_cast<std::uint32_t>(i)})
                    bad(where, "virtual selection must inherit or override the base implementation",
                        type.loc);
            }
        }
    }
    if (!d.classes.empty()) {
        result.processReceivers.resize(d.processes.size());
        result.subroutineReceivers.resize(d.subroutines.size());
        for (std::size_t i = 0; i < d.subroutines.size(); ++i) {
            const auto& sub = d.subroutines[i];
            if ((sub.classRole == ClassRole::Constructor || sub.classRole == ClassRole::Method) &&
                valid(sub.classOwner) && receiver(sub))
                result.subroutineReceivers[i].push_back(sub.params[0].frameVar);
        }
        // Guard aliases share a frame; captured bindings select a row of
        // their immediate parent. Propagate rights to a fixed point, without
        // recursion or lookup of source names. Malformed cycles cannot loop.
        bool changed = true;
        while (changed) {
            changed = false;
            const auto add = [&](std::vector<FrameVarId>& fields, FrameVarId field) {
                const auto at = std::lower_bound(fields.begin(), fields.end(), field);
                if (at != fields.end() && *at == field) return;
                fields.insert(at, field);
                changed = true;
            };
            for (std::size_t i = 0; i < d.subroutines.size(); ++i) {
                const auto& sub = d.subroutines[i];
                if (!readsWaiterFrame(sub.kind) ||
                    (sub.frameOf.kind == Owner::Kind::Subroutine && sub.frameOf.id == i))
                    continue;
                const auto parent = result.receivers(sub.frameOf);
                for (const auto field : parent)
                    add(result.subroutineReceivers[i], field);
            }
            for (std::size_t i = 0; i < d.processes.size(); ++i) {
                const auto& process = d.processes[i];
                for (std::uint32_t f = 0; f < process.frame.size(); ++f) {
                    const auto& field = process.frame[f];
                    const auto parent = result.receivers(process.parent);
                    if (field.home == FrameHome::Capture &&
                        std::binary_search(parent.begin(), parent.end(), field.captureSource))
                        add(result.processReceivers[i], FrameVarId{f});
                }
            }
        }
    }
    return result;
}

std::vector<VerifyError> verifyObjectInstruction(const Design& d, const ClassValidation& classes,
                                                 const Subroutine* caller, const Instr& in,
                                                 std::span<const Type> types) {
    std::vector<VerifyError> errors;
    const auto bad = [&](const char* message) {
        errors.push_back({opName(in.op), message, in.loc});
    };
    const auto& ops = in.operands;
    if (types.size() != ops.size()) {
        bad("operand type table has the wrong size");
        return errors;
    }
    const auto canonical = [](const Operand& o) {
        return o.edge == Edge::Any && o.firstElem == 0 && o.numElems == 0 && o.lsb == 0 &&
               o.width == 0 && o.count == 0 && o.text.empty();
    };
    const auto raw = [&](std::size_t at) {
        if (at >= ops.size()) return false;
        const auto& o = ops[at];
        return (o.kind == Operand::Kind::Temp || o.kind == Operand::Kind::Const) && canonical(o) &&
               o.member == 0;
    };
    const auto classValue = [&](std::size_t at) {
        return raw(at) && isClass(types[at]) && valueType(d, types[at]);
    };
    const auto literalNull = [&](std::size_t at) {
        if (!raw(at) || ops[at].kind != Operand::Kind::Const || ops[at].id >= d.constants.size())
            return false;
        const auto& constant = d.constants[ops[at].id];
        return constant.type == Type{TypeKind::Chandle, 0} && constant.aWords.size() == 1 &&
               constant.aWords[0] == 0 && constant.bWords.empty() && constant.bytes.empty();
    };
    const auto resultClass = [&] { return isClass(in.type) && valueType(d, in.type); };
    const auto ownConstructor = [&] {
        return caller && caller->classRole == ClassRole::Constructor &&
               caller->classOwner.index() < d.classes.size();
    };
    const auto metadata = [&](std::size_t at, Operand::Kind kind) {
        return at < ops.size() && ops[at].kind == kind && canonical(ops[at]) &&
               (kind == Operand::Kind::ClassField || ops[at].member == 0);
    };
    const auto arguments = [&](std::span<const Param> params, std::size_t first, std::size_t end,
                               const Subroutine& owner) {
        if (end < first || end - first != params.size()) {
            bad("argument count differs from the method signature");
            return;
        }
        for (std::size_t i = 0; i < params.size(); ++i) {
            const auto& param = params[i];
            const auto at = first + i;
            const auto* dims = parameterDimensions(d, owner, param);
            if (!dims) {
                bad("method parameter has no storage shape");
                continue;
            }
            const bool ref = param.dir == Direction::Ref || param.dir == Direction::ConstRef;
            auto expected = param.type;
            if (ref)
                expected = dims->empty() && isContainer(param.type) ? refType(param.type.detail)
                           : isClass(param.type)
                               ? classRefType(param.type.object, param.dir == Direction::ConstRef)
                               : refType();
            const bool place =
                ops[at].kind == Operand::Kind::Slot || ops[at].kind == Operand::Kind::Frame;
            if (ref                                                 ? !raw(at)
                : (!dims->empty() || param.dir != Direction::Input) ? !place
                                                                    : !raw(at))
                bad("method argument has the wrong value or storage form");
            const bool readonlyPromotion = isClass(param.type) &&
                                           param.dir == Direction::ConstRef &&
                                           types[at] == classRefType(param.type.object);
            if (types[at] != expected && !readonlyPromotion)
                bad("method argument type differs from its parameter");
        }
    };
    for (std::size_t i = 0; i < ops.size(); ++i) {
        const auto kind = ops[i].kind;
        if (kind == Operand::Kind::Class && (in.op != Op::ObjectCastCheck || i != 0))
            bad("class IDs are only metadata of object.cast.check");
        if (kind == Operand::Kind::ClassField &&
            ((in.op != Op::ObjectRead && in.op != Op::ObjectWrite && in.op != Op::ObjectInit) ||
             i != 0))
            bad("field IDs are only metadata of property operations");
        if (kind == Operand::Kind::VirtualSlot &&
            ((in.op != Op::ObjectCall && in.op != Op::ObjectCallTask) || i != 0))
            bad("virtual slot IDs are only metadata of virtual calls");
    }
    switch (in.op) {
        case Op::ObjectNew:
        case Op::ObjectWeakNew: {
            if (!resultClass()) {
                bad("allocation needs a class result");
                break;
            }
            const auto& type = d.classes[in.type.object.index()];
            if (type.abstract || type.interfaceClass)
                bad("cannot allocate an abstract or interface class");
            if (in.op == Op::ObjectWeakNew) {
                if (type.weakTarget == kNone || ops.size() != 1 ||
                    (!literalNull(0) &&
                     (!classValue(0) || !classes.assignable(types[0].object, type.weakTarget))))
                    bad("weak construction needs one compatible target or literal null");
            } else {
                if (type.weakTarget != kNone)
                    bad("weak reference construction uses object.weak.new");
                if (type.constructor.index() < d.subroutines.size()) {
                    const auto& ctor = d.subroutines[type.constructor.index()];
                    if (ctor.params.empty())
                        bad("constructor has no receiver");
                    else
                        arguments(std::span(ctor.params).subspan(1), 0, ops.size(), ctor);
                } else
                    bad("allocation has no constructor wrapper");
            }
            break;
        }
        case Op::ObjectCopy:
            if (ops.size() != 1 || !classValue(0) || in.type != types[0])
                bad("copy preserves the static class view");
            break;
        case Op::ObjectView:
            if (ops.size() != 1 || !classValue(0) || !resultClass() ||
                !classes.assignable(types[0].object, in.type.object))
                bad("view conversion must be assignment-compatible");
            break;
        case Op::ObjectCastCheck:
            if (ops.size() != 2 || !metadata(0, Operand::Kind::Class) ||
                ops[0].id >= d.classes.size() || (!classValue(1) && !literalNull(1)) ||
                in.type != u2(1))
                bad("cast check needs a destination class, a class value or literal null and i2<1> "
                    "result");
            break;
        case Op::ObjectCast:
            if (ops.size() != 1 || (!classValue(0) && !literalNull(0)) || !resultClass())
                bad("cast needs a typed class result and a class value or literal null");
            break;
        case Op::ObjectId:
            if (ops.size() != 1 || !classValue(0) || in.type != u2(64))
                bad("object ID needs a class value and i2<64> result");
            break;
        case Op::ObjectSeed:
            if (ops.size() != 2 || !classValue(0) || !raw(1) || types[1] != u2(32))
                bad("object seed needs a class value and i2<32> seed");
            break;
        case Op::ObjectRead:
        case Op::ObjectWrite:
        case Op::ObjectInit: {
            if (!metadata(0, Operand::Kind::ClassField) || ops[0].id >= d.classes.size() ||
                ops[0].member >= d.classes[ops[0].id].fields.size()) {
                bad("property field is out of range");
                break;
            }
            const auto& field = d.classes[ops[0].id].fields[ops[0].member];
            if (in.op == Op::ObjectInit) {
                if (!ownConstructor() || caller->classOwner.index() != ops[0].id)
                    bad("privileged field initialization belongs only to its own constructor");
                const auto count = elementCount(field);
                if (count == 0 || count != ops.size() - 1)
                    bad("initializer needs every field element in declaration order");
                else
                    for (std::size_t i = 1; i < ops.size(); ++i)
                        if (!raw(i) || types[i] != field.type)
                            bad("field initializer element has the wrong type");
            } else {
                const auto count = 2 + field.dims.size() + (in.op == Op::ObjectWrite ? 1 : 0);
                if (ops.size() != count || !classValue(1) ||
                    !classes.assignable(types[1].object, ClassId{ops[0].id}))
                    bad("property access needs a compatible receiver and one index per dimension");
                else {
                    for (std::size_t i = 0; i < field.dims.size(); ++i)
                        if (!raw(i + 2) || !isIntegral(types[i + 2]))
                            bad("property index must be an integral value");
                    if (in.op == Op::ObjectRead && in.type != field.type)
                        bad("property read has the wrong result type");
                    if (in.op == Op::ObjectWrite &&
                        (!raw(ops.size() - 1) || types.back() != field.type))
                        bad("property write has the wrong value type");
                }
                if (in.op == Op::ObjectWrite && field.constant)
                    bad("ordinary writes cannot change const properties");
            }
            break;
        }
        case Op::ObjectStageBegin:
        case Op::ObjectStageBody:
        case Op::ObjectStageEnd:
            if (!ops.empty() || !ownConstructor())
                bad("constructor phase operation needs its own constructor invocation");
            break;
        case Op::ObjectBaseInit:
            if (!ownConstructor() ||
                d.classes[caller->classOwner.index()].base.index() >= d.classes.size())
                bad("base initialization needs a constructor with a direct base");
            else {
                const auto& base = d.classes[d.classes[caller->classOwner.index()].base.index()];
                if (base.constructor.index() >= d.subroutines.size())
                    bad("direct base has no constructor");
                else {
                    const auto& ctor = d.subroutines[base.constructor.index()];
                    if (ctor.params.empty())
                        bad("base constructor has no receiver");
                    else
                        arguments(std::span(ctor.params).subspan(1), 0, ops.size(), ctor);
                }
            }
            break;
        case Op::ObjectCall:
        case Op::ObjectCallTask:
        case Op::ObjectCallDirect:
        case Op::ObjectCallTaskDirect: {
            const bool task = in.op == Op::ObjectCallTask || in.op == Op::ObjectCallTaskDirect;
            const bool direct = in.op == Op::ObjectCallDirect || in.op == Op::ObjectCallTaskDirect;
            if (ops.size() < (task ? 3U : 2U) || !classValue(1)) {
                bad("method call needs metadata and a typed receiver");
                break;
            }
            if (task && (!metadata(ops.size() - 1, Operand::Kind::Block)))
                bad("waiting method needs a continuation block");
            const auto end = ops.size() - (task ? 1 : 0);
            if (direct) {
                if (!metadata(0, Operand::Kind::Subroutine) || ops[0].id >= d.subroutines.size()) {
                    bad("direct method is out of range");
                    break;
                }
                const auto& method = d.subroutines[ops[0].id];
                if (method.classRole != ClassRole::Method ||
                    !classes.assignable(types[1].object, method.classOwner))
                    bad("direct call needs an instance method of its receiver view");
                if ((task && method.kind != SubroutineKind::Task) || (!task && method.mayWait))
                    bad("method call has the wrong suspension form");
                if (in.type != (task ? voidType() : method.returnType))
                    bad("method call has the wrong result type");
                if (method.params.empty())
                    bad("method has no receiver");
                else
                    arguments(std::span(method.params).subspan(1), 2, end, method);
            } else {
                if (!metadata(0, Operand::Kind::VirtualSlot) ||
                    ops[0].id >= d.virtualSlots.size()) {
                    bad("virtual slot is out of range");
                    break;
                }
                const auto& slot = d.virtualSlots[ops[0].id];
                if (!classes.assignable(types[1].object, slot.declaring))
                    bad("virtual slot is not part of the receiver view");
                if ((task && slot.kind != SubroutineKind::Task) || (!task && slot.mayWait))
                    bad("virtual call has the wrong suspension form");
                if (in.type != (task ? voidType() : slot.result))
                    bad("virtual call has the wrong result type");
                if (end - 2 != slot.parameters.size())
                    bad("virtual argument count differs from its signature");
                else
                    for (std::size_t i = 0; i < slot.parameters.size(); ++i) {
                        const auto& param = slot.parameters[i];
                        const bool ref =
                            param.dir == Direction::Ref || param.dir == Direction::ConstRef;
                        auto expected = param.type;
                        if (ref)
                            expected = param.dims.empty() && isContainer(param.type)
                                           ? refType(param.type.detail)
                                       : isClass(param.type)
                                           ? classRefType(param.type.object,
                                                          param.dir == Direction::ConstRef)
                                           : refType();
                        const bool place = ops[i + 2].kind == Operand::Kind::Slot ||
                                           ops[i + 2].kind == Operand::Kind::Frame;
                        const bool wantPlace = !param.dims.empty() || param.dir != Direction::Input;
                        const bool readonlyPromotion =
                            isClass(param.type) && param.dir == Direction::ConstRef &&
                            types[i + 2] == classRefType(param.type.object);
                        if ((ref         ? !raw(i + 2)
                             : wantPlace ? !place
                                         : !raw(i + 2)) ||
                            (types[i + 2] != expected && !readonlyPromotion))
                            bad("virtual argument has the wrong type or storage form");
                    }
            }
            break;
        }
        case Op::ObjectWeakLock:
        case Op::ObjectWeakClear:
            if (ops.size() != 1 || !classValue(0) ||
                d.classes[types[0].object.index()].weakTarget == kNone)
                bad("weak operation needs a weak reference value");
            else if (in.op == Op::ObjectWeakLock &&
                     in.type != classType(d.classes[types[0].object.index()].weakTarget))
                bad("weak promotion must return its declared target view");
            break;
        default: bad("unknown object operation"); break;
    }
    return errors;
}

Subroutine virtualSignature(const VirtualSlot& slot) {
    Subroutine sub;
    sub.classOwner = slot.declaring;
    sub.classRole = ClassRole::Method;
    sub.kind = slot.kind;
    sub.returnType = slot.result;
    sub.mayWait = slot.mayWait;
    sub.automatic = true;
    sub.params.push_back(
        {"this", Direction::Input, classType(slot.declaring), kNone, FrameVarId{0}});
    sub.frame.push_back({"this", classType(slot.declaring)});
    for (const auto& param : slot.parameters) {
        const FrameVarId id{static_cast<std::uint32_t>(sub.frame.size())};
        sub.params.push_back({"", param.dir, param.type, kNone, id, param.refStatic});
        FrameVar field;
        field.type = param.type;
        field.dims = param.dims;
        field.declType = param.declType;
        if (param.dir == Direction::Ref || param.dir == Direction::ConstRef) {
            field.type = param.dims.empty() && isContainer(param.type) ? refType(param.type.detail)
                         : isClass(param.type)
                             ? classRefType(param.type.object, param.dir == Direction::ConstRef)
                             : refType();
            field.target = param.type;
        }
        sub.frame.push_back(std::move(field));
    }
    return sub;
}

bool classReferenceShape(const Design& d, std::span<const FrameVar> frame,
                         std::span<const BasicBlock> blocks, const Operand& reference,
                         std::span<const ArrayDim> expected) {
    if (dimensions(expected) == 0) return false;
    std::vector<const Operand*> work{&reference};
    std::set<TempId> seen;
    bool origin = false;
    while (!work.empty()) {
        const auto* value = work.back();
        work.pop_back();
        if (value->kind != Operand::Kind::Temp || value->id == kNone) return false;
        if (!seen.insert(value->asTemp()).second) continue;
        bool found = false;
        for (std::size_t b = 0; b < blocks.size(); ++b) {
            for (std::size_t p = 0; p < blocks[b].params.size(); ++p) {
                if (blocks[b].params[p].temp != value->asTemp()) continue;
                found = true;
                if (blocks[b].params[p].type.kind != TypeKind::Ref ||
                    blocks[b].params[p].type.object == kNone)
                    return false;
                bool incoming = false;
                for (const auto& predecessor : blocks) {
                    if (predecessor.instrs.empty()) continue;
                    const auto& ops = predecessor.instrs.back().operands;
                    for (std::size_t at = 0; at < ops.size(); ++at)
                        if (ops[at].kind == Operand::Kind::Block && ops[at].id == b) {
                            if (p >= ops[at].count || p + 1 >= ops.size() - at) return false;
                            work.push_back(&ops[at + 1 + p]);
                            incoming = true;
                        }
                }
                if (!incoming) return false;
            }
            for (const auto& in : blocks[b].instrs) {
                if (in.result == kNone || in.result != value->asTemp()) continue;
                found = true;
                if (in.type.kind != TypeKind::Ref || in.type.object == kNone || in.operands.empty())
                    return false;
                const auto& base = in.operands[0];
                const std::vector<ArrayDim>* dims = nullptr;
                if (base.kind == Operand::Kind::Frame && base.id < frame.size())
                    dims = &frame[base.id].dims;
                if (base.kind == Operand::Kind::Slot && base.id < d.slots.size())
                    dims = &d.slots[base.id].dims;
                if (!dims || dimensions(*dims) == 0) return false;
                if (in.op == Op::Fload) {
                    if (base.kind != Operand::Kind::Frame ||
                        frame[base.id].type.kind != TypeKind::Ref ||
                        frame[base.id].type.object == kNone || !sameDimensions(*dims, expected))
                        return false;
                } else if (in.op == Op::RefMake) {
                    const auto indices = in.operands.size() - 1;
                    if (indices == 0) {
                        if (!sameDimensions(*dims, expected)) return false;
                    } else if (indices != dims->size() || !expected.empty())
                        return false;
                } else
                    return false;
                origin = true;
            }
        }
        if (!found) return false;
    }
    return origin;
}

bool hasClassMetadata(const Design& d) {
    if (!d.classes.empty() || !d.virtualSlots.empty()) return true;
    const auto has = [](Type type) {
        return isClass(type) || type.object != kNone || type.readOnlyRef;
    };
    for (const auto& slot : d.slots)
        if (has(slot.type)) return true;
    for (const auto& value : d.constants)
        if (has(value.type)) return true;
    for (const auto& type : d.types)
        if (has(type.value)) return true;
    for (const auto& pattern : d.patterns)
        if (has(pattern.value)) return true;
    for (const auto& layout : d.nbaLayouts)
        for (const auto value : layout.values)
            if (has(value)) return true;
    const auto body = [&](const auto& item) {
        for (const auto& field : item.frame)
            if (has(field.type) || has(field.target)) return true;
        for (const auto& block : item.blocks) {
            for (const auto& param : block.params)
                if (has(param.type)) return true;
            for (const auto& in : block.instrs) {
                if (isObjectOp(in.op) || has(in.type)) return true;
                for (const auto& operand : in.operands)
                    if (operand.kind == Operand::Kind::Class ||
                        operand.kind == Operand::Kind::ClassField ||
                        operand.kind == Operand::Kind::VirtualSlot)
                        return true;
            }
        }
        return false;
    };
    for (const auto& sub : d.subroutines) {
        if (sub.classRole != ClassRole::None || sub.classOwner != kNone || has(sub.returnType) ||
            body(sub))
            return true;
        for (const auto& param : sub.params)
            if (has(param.type)) return true;
    }
    for (const auto& process : d.processes)
        if (body(process)) return true;
    return false;
}

}  // namespace kirimasim::sir
