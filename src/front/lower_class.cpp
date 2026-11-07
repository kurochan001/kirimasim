// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Approved class contract 1/3/5: canonical descriptors and ordinary lowering.
#include "lowerer.h"

#include "kirimasim/sir/classes.h"

#include <type_traits>

#include "slang/ast/ASTVisitor.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/expressions/SelectExpressions.h"
#include "slang/ast/symbols/SubroutineSymbols.h"

namespace kirimasim::front {
namespace {
using sir::Op;
using sir::Operand;
struct ClassPresence : ast::ASTVisitor<ClassPresence, ast::VisitFlags::Symbols> {
    bool found = false;
    void handle(const ast::ClassType& type) { found = found || type.getSyntax() != nullptr; }
    void handle(const ast::GenericClassDefSymbol& type) {
        found = found || type.getSyntax() != nullptr;
    }
};
const ast::ClassType* declaringClass(const ast::Symbol& symbol) {
    const auto* parent = symbol.getParentScope();
    return parent && parent->asSymbol().kind == ast::SymbolKind::ClassType
               ? &parent->asSymbol().as<ast::ClassType>()
               : nullptr;
}
const ast::SubroutineSymbol* methodSymbol(const ast::Symbol& symbol) {
    if (symbol.kind == ast::SymbolKind::Subroutine) return &symbol.as<ast::SubroutineSymbol>();
    if (symbol.kind == ast::SymbolKind::MethodPrototype)
        return symbol.as<ast::MethodPrototypeSymbol>().getSubroutine();
    return nullptr;
}
}  // namespace

sir::ClassId Lowerer::classOf(const ast::Type& type) {
    const auto& canonical = type.getCanonicalType().as<ast::ClassType>();
    if (auto it = classIds_.find(&canonical); it != classIds_.end()) return it->second;
    checkStack(canonical.location);
    if (!canonical.getSyntax())
        unsupported(canonical.location, "type '" + canonical.toString() + "'");
    const sir::ClassId id{static_cast<std::uint32_t>(classTypes_.size())};
    classIds_.emplace(&canonical, id);
    classTypes_.push_back(&canonical);
    design_.classes.emplace_back();
    design_.classes[id.index()].name = canonical.toString();
    design_.classes[id.index()].abstract = canonical.isAbstract;
    design_.classes[id.index()].interfaceClass = canonical.isInterface;
    design_.classes[id.index()].final = canonical.isFinal;
    if (const auto* base = canonical.getBaseClass()) {
        const auto parent = classOf(*base);
        design_.classes[id.index()].base = parent;
    }
    for (const auto* interface : canonical.getDeclaredInterfaces()) {
        const auto parent = classOf(*interface);
        design_.classes[id.index()].interfaces.push_back(parent);
    }
    for (const auto& member : canonical.members()) {
        if (member.kind != ast::SymbolKind::ClassProperty) continue;
        const auto& property = member.as<ast::ClassPropertySymbol>();
        if (property.lifetime == ast::VariableLifetime::Static) continue;
        std::vector<sir::ArrayDim> dims;
        const auto& element = arrayElement(property.getType(), dims);
        if (hasEventLeaves(element)) unsupported(property.location, "managed event class property");
        const auto value = typeOf(element, property.location);
        const auto index = static_cast<std::uint32_t>(design_.classes[id.index()].fields.size());
        classFields_.emplace(&property, sir::ClassFieldId{id, index});
        design_.classes[id.index()].fields.push_back(
            {std::string(property.name), value, std::move(dims), sir::kNone,
             property.flags.has(ast::VariableFlags::Const), property.getInitializer() != nullptr});
    }
    return id;
}

void Lowerer::discoverClasses(const ast::RootSymbol& root) {
    ClassPresence presence;
    root.visit(presence);
    if (!presence.found) return;
    std::set<const ast::SubroutineSymbol*> scanned, requested;

    const auto discoverType = [&](const ast::Type& type, auto& self) -> void {
        const auto& t = type.getCanonicalType();
        if (t.isClass()) {
            classOf(t);
            return;
        }
        if (t.isUnpackedArray()) self(t.getArrayElementType()->getCanonicalType(), self);
        if (t.kind == ast::SymbolKind::UnpackedStructType)
            for (const auto& field : t.as<ast::UnpackedStructType>().members())
                if (field.kind == ast::SymbolKind::Field)
                    self(field.template as<ast::FieldSymbol>().getType(), self);
    };
    auto visitor = ast::makeVisitor([&](auto& v, const auto& node) {
        using T = std::remove_cvref_t<decltype(node)>;
        if constexpr (std::is_same_v<T, ast::ClassType>)
            return;
        else {
            if constexpr (std::is_base_of_v<ast::Expression, T>) {
                checkStack(node.sourceRange.start());
                discoverType(*node.type, discoverType);
            }
            if constexpr (std::is_same_v<T, ast::SubroutineSymbol>) {
                if (!requested.contains(&node)) {
                    auto staticStorage = ast::makeVisitor(
                        [&](auto&, const ast::VariableSymbol& variable) {
                            if (variable.lifetime == ast::VariableLifetime::Static)
                                variable.visit(v);
                        },
                        [&](auto&, const ast::Expression&) {});
                    node.getBody().visit(staticStorage);
                    return;
                }
                if (!scanned.insert(&node).second) return;
                discoverType(node.getReturnType(), discoverType);
            } else if constexpr (std::is_base_of_v<ast::VariableSymbol, T>)
                discoverType(node.getType(), discoverType);

            v.visitDefault(node);
            if constexpr (std::is_same_v<T, ast::CallExpression>) {
                if (!node.isSystemCall()) {
                    const auto* callee = std::get<0>(node.subroutine);
                    requested.insert(callee);
                    callee->visit(v);
                }
            }
        }
    });
    try {
        root.visit(visitor);
        for (std::size_t i = 0; i < classTypes_.size(); ++i) {
            const auto* type = classTypes_[i];
            if (const auto* base = type->getBaseConstructorCall()) base->visit(visitor);
            for (const auto& member : type->members()) {
                if (const auto* method = methodSymbol(member)) {
                    if (method->flags.has(ast::MethodFlags::BuiltIn)) continue;
                    if (method->isVirtual() || method->flags.has(ast::MethodFlags::Constructor))
                        requested.insert(method);
                    if (requested.contains(method)) method->visit(visitor);
                    if (method->isVirtual()) virtualSlotOf(*method);
                } else if (member.kind == ast::SymbolKind::ClassProperty) {
                    if (const auto* init = member.as<ast::ClassPropertySymbol>().getInitializer())
                        init->visit(visitor);
                }
            }
        }
    } catch (const Unsupported&) {
        return;
    }
}

sir::VirtualSlotId Lowerer::virtualSlotOf(const ast::SubroutineSymbol& method) {
    const auto* root = &method;
    while (root->getOverride())
        root = root->getOverride();
    if (auto it = virtualSlots_.find(root); it != virtualSlots_.end()) return it->second;
    const auto* owner = declaringClass(*root);
    if (!owner) unsupported(method.location, "virtual method outside a class");
    const auto declaring = classOf(*owner);
    sir::VirtualSlot slot;
    slot.name = std::string(root->name);
    slot.declaring = declaring;
    slot.kind = root->subroutineKind == ast::SubroutineKind::Task ? sir::SubroutineKind::Task
                                                                  : sir::SubroutineKind::Function;
    slot.result = root->getReturnType().isVoid() ? sir::voidType()
                                                 : typeOf(root->getReturnType(), root->location);
    slot.mayWait = slot.kind == sir::SubroutineKind::Task;
    for (const auto* argument : root->getArguments()) {
        sir::ClassParameter parameter;
        switch (argument->direction) {
            case ast::ArgumentDirection::In: parameter.dir = sir::Direction::Input; break;
            case ast::ArgumentDirection::Out: parameter.dir = sir::Direction::Output; break;
            case ast::ArgumentDirection::InOut: parameter.dir = sir::Direction::Inout; break;
            case ast::ArgumentDirection::Ref:
                parameter.dir = argument->flags.has(ast::VariableFlags::Const)
                                    ? sir::Direction::ConstRef
                                    : sir::Direction::Ref;
                break;
        }
        parameter.type =
            typeOf(arrayElement(argument->getType(), parameter.dims), argument->location);
        parameter.refStatic = argument->flags.has(ast::VariableFlags::RefStatic);
        slot.parameters.push_back(std::move(parameter));
    }
    const sir::VirtualSlotId id{static_cast<std::uint32_t>(design_.virtualSlots.size())};
    virtualSlots_.emplace(root, id);
    virtualMethods_.push_back(root);
    design_.virtualSlots.push_back(std::move(slot));
    return id;
}

void Lowerer::lowerClasses() {
    for (std::size_t i = 0; i < classTypes_.size(); ++i) {
        const auto* type = classTypes_[i];
        design_.classes[i].scope = declaringInstance(*type).value_or(instance_);
        for (const auto& member : type->members()) {
            if (member.kind != ast::SymbolKind::ClassProperty) continue;
            const auto& property = member.as<ast::ClassPropertySymbol>();
            if (property.lifetime != ast::VariableLifetime::Static) continue;
            try {
                const auto slot = structElement(property.getType())
                                      ? structSlots(property, property.location)
                                      : slotOf(property, property.location);
                design_.classes[i].statics.push_back({std::string(property.name), slot});
            } catch (const Unsupported&) {
                continue;
            }
        }
        if (type->isInterface) continue;
        try {
            if (const auto* constructor = type->getConstructor();
                constructor && declaringClass(*constructor) == type) {
                const auto id = subroutineOf(*constructor, constructor->location);
                design_.classes[i].constructor = id;
            } else
                implicitConstructor(sir::ClassId{static_cast<std::uint32_t>(i)});
        } catch (const Unsupported&) {
            continue;
        }
    }
    // Slots are global; a class implements each applicable declaration by
    // slang's compile-time member binding, including inherited bodies.
    for (std::size_t c = 0; c < classTypes_.size(); ++c) {
        design_.classes[c].virtualMethods.resize(design_.virtualSlots.size(),
                                                 sir::SubroutineId{sir::kNone});
        for (std::size_t v = 0; v < virtualMethods_.size(); ++v) {
            const auto* declaration = virtualMethods_[v];
            const auto* declaring = declaringClass(*declaration);
            if (!declaring || !declaring->isAssignmentCompatible(*classTypes_[c])) continue;
            const auto* symbol = classTypes_[c]->find(declaration->name);
            const auto* method = symbol ? methodSymbol(*symbol) : nullptr;
            if (!method || method->flags.has(ast::MethodFlags::Pure)) continue;
            try {
                const auto id = subroutineOf(*method, method->location);
                design_.classes[c].virtualMethods[v] = id;
                design_.subroutine(id).mayWait = design_.virtualSlots[v].mayWait;
            } catch (const Unsupported&) {
                continue;
            }
        }
    }
}

Val Lowerer::classReceiver(slang::SourceLocation sl) {
    if (b_.classReceiverSymbol) {
        if (const auto found = findFrameVar(*b_.classReceiverSymbol))
            return emit(Op::Fload, frameVar(*found).type, {Operand::frame(*found)}, sl);
    }
    if (!b_.isSub || design_.subroutine(b_.sub).classOwner == sir::kNone ||
        design_.subroutine(b_.sub).classRole == sir::ClassRole::StaticMethod)
        unsupported(sl, "instance class property outside a method");
    const auto& parameter = design_.subroutine(b_.sub).params[0];
    return emit(Op::Fload, parameter.type, {Operand::frame(parameter.frameVar)}, sl);
}

void Lowerer::implicitConstructor(sir::ClassId id) {
    if (design_.classes[id.index()].constructor != sir::kNone) return;
    const auto* type = classTypes_[id.index()];
    const auto savedInstance = instance_;
    instance_ = declaringInstance(*type).value_or(instance_);
    const sir::SubroutineId method{static_cast<std::uint32_t>(design_.subroutines.size())};
    sir::Subroutine constructor;
    constructor.name = type->getHierarchicalPath() + ".new";
    constructor.instance = instance_;
    constructor.automatic = true;
    constructor.classOwner = id;
    constructor.classRole = sir::ClassRole::Constructor;
    constructor.loc = loc(type->location);
    constructor.params = {
        {"this", sir::Direction::Input, sir::classType(id), sir::kNone, sir::FrameVarId{0}}};
    constructor.frame = {{"this", sir::classType(id)}};
    constructor.blocks.resize(1);
    design_.subroutines.push_back(std::move(constructor));
    design_.classes[id.index()].constructor = method;
    Body saved = std::move(b_);
    const auto* eval = evalSymbol_;
    b_ = {};
    b_.isSub = true;
    b_.sub = method;
    b_.scale = timeScaleOf(*type);
    evalSymbol_ = type;
    try {
        beginClassConstructor(*type, type->location);
        lowerReturn(nullptr, type->location);
        finishLexical();
    } catch (...) {
        b_ = std::move(saved);
        evalSymbol_ = eval;
        instance_ = savedInstance;
        throw;
    }
    b_ = std::move(saved);
    evalSymbol_ = eval;
    instance_ = savedInstance;
}

void Lowerer::lowerBaseConstruction(const ast::ClassType& type, const ast::Type& base,
                                    slang::SourceLocation sl) {
    const auto parent = classOf(base);
    const ast::Expression* written = type.getBaseConstructorCall();
    if (written != nullptr && written->kind == ast::ExpressionKind::NewClass)
        written = written->as<ast::NewClassExpression>().constructorCall();
    if (written != nullptr) {
        lowerCallTo(written->as<ast::CallExpression>(), nullptr, parent, true);
        return;
    }
    // No arguments are written: the call is implied (8.7, 8.17). It is
    // super.new(default) when this constructor has the default keyword, which
    // passes on the arguments that keyword expanded to, and otherwise
    // super.new(), where each argument takes its default value (13.5.3).
    const auto* constructor = base.getCanonicalType().as<ast::ClassType>().getConstructor();
    if (constructor == nullptr || constructor->getArguments().empty()) {
        emitVoid(Op::ObjectBaseInit, {}, sl);
        return;
    }
    const auto* own = type.getConstructor();
    const bool forwarded = own != nullptr && own->flags.has(ast::MethodFlags::DefaultedSuperArg);
    const slang::SourceRange range{sl, sl};
    std::deque<ast::NamedValueExpression> names;
    std::vector<const ast::Expression*> arguments;
    for (const ast::FormalArgumentSymbol* formal : constructor->getArguments()) {
        if (!forwarded) {
            arguments.push_back(formal->getDefaultValue());
            continue;
        }
        const auto expanded =
            std::ranges::find(own->getArguments(), formal->name, &ast::FormalArgumentSymbol::name);
        if (expanded == own->getArguments().end())
            unsupported(sl, "default of this base constructor");
        arguments.push_back(&names.emplace_back(**expanded, range));
    }
    const ast::CallExpression call(constructor, constructor->getReturnType(), nullptr, arguments,
                                   ast::LookupLocation::max, range);
    lowerCallTo(call, nullptr, parent, true);
}

void Lowerer::beginClassConstructor(const ast::ClassType& type, slang::SourceLocation sl) {
    if (type.thisVar) {
        b_.frameVars[type.thisVar] = sir::FrameVarId{0};
        if (!b_.subSymbol) {
            b_.classReceiverSymbol = type.thisVar;
            ownFrame(*type.thisVar, sir::FrameVarId{0});
        }
    }
    if (const auto* base = type.getBaseClass()) lowerBaseConstruction(type, *base, sl);
    b_.classBaseEmitted = true;
    emitVoid(Op::ObjectStageBegin, {}, sl);
    const auto owner = classOf(type);
    for (const auto& member : type.members()) {
        if (member.kind != ast::SymbolKind::ClassProperty) continue;
        const auto& field = member.as<ast::ClassPropertySymbol>();
        if (field.lifetime == ast::VariableLifetime::Static || !field.getInitializer()) continue;
        const auto id = classFields_.at(&field);
        std::vector<Operand> operands{Operand::classField(id)};
        const auto& metadata = design_.classes[owner.index()].fields[id.index];
        if (!metadata.dims.empty()) {
            const auto storage = tempStorage(field.getType(), field.location);
            Stores stores;
            assignAggregate(storage, field.getType(), *field.getInitializer(), stores,
                            field.location);
            flushStores(stores, false, field.location);
            std::vector<Place> places;
            storagePlaces(storage, field.getType(), places, field.location);
            for (const auto& place : places)
                operands.push_back(loadPlace(place, field.location).op);
        } else
            operands.push_back(
                coerce(lowerExpr(*field.getInitializer()), metadata.type, field.location).op);
        emitVoid(Op::ObjectInit, std::move(operands), field.location);
    }
    emitVoid(Op::ObjectStageBody, {}, sl);
}

Val Lowerer::lowerClassNew(const ast::NewClassExpression& expression) {
    const auto sl = expression.sourceRange.start();
    if (expression.isSuperClass) {
        if (!b_.classBaseEmitted)
            unsupported(sl, "base construction outside a constructor prologue");
        return {Operand::temp(sir::TempId{0}), sir::voidType()};
    }
    const auto id = classOf(*expression.type);
    if (const auto* call = expression.constructorCall())
        return lowerCallTo(call->as<ast::CallExpression>(), nullptr, id);
    implicitConstructor(id);
    return emit(Op::ObjectNew, sir::classType(id), {}, sl);
}

Val Lowerer::lowerClassReference(const ast::Expression& expression) {
    const auto saved = b_.classReferenceDepth;
    ++b_.classReferenceDepth;
    try {
        auto value = lowerExpr(expression);
        b_.classReferenceDepth = saved;
        return value;
    } catch (...) {
        b_.classReferenceDepth = saved;
        throw;
    }
}

namespace {

/// What an expression names when it is a property, through the elements of
/// the property's unpacked arrays; property stays null otherwise.
struct PropertyAccess {
    const ast::ClassPropertySymbol* property = nullptr;
    const ast::Expression* object = nullptr;
    std::vector<const ast::Expression*> indices;
};

PropertyAccess propertyAccess(const ast::Expression& expression) {
    PropertyAccess access;
    const ast::Expression* root = &expression;
    while (root->kind == ast::ExpressionKind::ElementSelect &&
           root->as<ast::ElementSelectExpression>()
               .value()
               .type->getCanonicalType()
               .isUnpackedArray()) {
        access.indices.push_back(&root->as<ast::ElementSelectExpression>().selector());
        root = &root->as<ast::ElementSelectExpression>().value();
    }
    if (root->kind == ast::ExpressionKind::MemberAccess) {
        const auto& member = root->as<ast::MemberAccessExpression>();
        if (member.member.kind == ast::SymbolKind::ClassProperty) {
            access.property = &member.member.as<ast::ClassPropertySymbol>();
            access.object = &member.value();
        }
    } else if (const auto* symbol = namedSymbol(*root);
               symbol && symbol->kind == ast::SymbolKind::ClassProperty)
        access.property = &symbol->as<ast::ClassPropertySymbol>();
    return access;
}

}  // namespace

bool Lowerer::isObjectProperty(const ast::Expression& e) {
    const auto* property = propertyAccess(e).property;
    return property != nullptr && property->lifetime != ast::VariableLifetime::Static;
}

std::optional<Lowerer::Place> Lowerer::classPlace(const ast::Expression& expression,
                                                  slang::SourceLocation sl) {
    const auto [property, object, indices] = propertyAccess(expression);
    if (!property) return {};
    if (property->lifetime == ast::VariableLifetime::Static) {
        if (!object || isAggregate(*expression.type)) return {};
        const auto storage = storageOf(expression, sl);
        if (!storage) return {};
        return storagePlace(*storage);
    }
    const auto id = classFields_.at(property);
    Place place;
    place.home = Place::Home::Object;
    place.kind = Place::Kind::Element;
    place.container = sir::classType(id.declaring);
    place.leaf = id.index;
    place.receiver = object ? lowerClassReference(*object) : classReceiver(sl);
    place.frame = object ? sir::FrameVarId{sir::kNone} : sir::FrameVarId{0};
    if (object && b_.isSub) {
        const auto* symbol = namedSymbol(*object);
        if (symbol && findFrameVar(*symbol) == sir::FrameVarId{0}) place.frame = sir::FrameVarId{0};
    }
    const auto& field = design_.classes[id.declaring.index()].fields[id.index];
    if (indices.size() != field.dims.size()) unsupported(sl, "whole class array property");
    for (auto it = indices.rbegin(); it != indices.rend(); ++it)
        place.indices.push_back(lowerClassReference(**it).op);
    place.type = field.type;
    return place;
}
Val Lowerer::lowerClassCast(const ast::CallExpression& call, bool required) {
    // Both forms capture the target before the source (class contract 5).
    // The required cast reports failure before any destination store.
    const auto sl = call.sourceRange.start();
    const auto& destination = *outputPlace(call.arguments()[0]);
    const auto target = referencePlace(placeOf(destination, sl), false, sl);
    const auto source = lowerExpr(*call.arguments()[1]);
    const auto type = typeOf(*destination.type, sl);
    if (required) {
        const auto cast = emit(sir::Op::ObjectCastRequired, type, {source.op}, sl);
        emitVoid(sir::Op::StoreRef, {target.op, cast.op}, sl);
        return cast;
    }
    const auto check = emit(sir::Op::ObjectCastCheck, sir::u2(1),
                            {sir::Operand::classRow(type.object), source.op}, sl);
    const auto success = newBlock(), done = newBlock();
    terminate(sir::Op::BrIf, {check.op, sir::Operand::block(success), sir::Operand::block(done)},
              sl);
    setBlock(success);
    const auto cast = emit(sir::Op::ObjectCast, type, {source.op}, sl);
    emitVoid(sir::Op::StoreRef, {target.op, cast.op}, sl);
    terminate(sir::Op::Br, {sir::Operand::block(done)}, sl);
    setBlock(done);
    return coerce(check, typeOf(*call.type, sl), sl);
}

void Lowerer::checkClassExecutionSupport() {
    if (classTypes_.empty() || numErrors_ != 0) return;
    const auto at = classTypes_.front()->location;
    if (initValues_) {
        report(at, "source initialization overrides with class objects");
        return;
    }
    if (!sir::classExecutionShapeSupported(design_))
        report(at, "dependency observation or pattern formatting with class objects");
}
}  // namespace kirimasim::front
