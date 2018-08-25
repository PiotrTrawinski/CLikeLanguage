#include "Operation.h"
#include "Declaration.h"
#include "ClassDeclaration.h"

using namespace std;

std::pair<FunctionValue*, ClassDeclaration*> findClassConstructor(const CodePosition& position, Scope* scope, const string& name, vector<Value*>& arguments) {
    ClassDeclaration* classDeclaration = nullptr;
    Scope* searchScope = scope; 

    while (searchScope && !classDeclaration) {
        classDeclaration = searchScope->classDeclarationMap.getDeclaration(name);
        searchScope = searchScope->parentScope;
    }
    if (classDeclaration) {
        if (!classDeclaration->interpret()) {
            return {nullptr, classDeclaration};
        }
        if (classDeclaration->body->constructors.empty()) {
            if (arguments.size() == 0) {
                return {classDeclaration->body->inlineConstructors, classDeclaration};
            } else {
                errorMessageBool("only default (0 argument) constructor exists, got "
                    + to_string(arguments.size()) + " arguments", position
                );
                return {nullptr, classDeclaration};
            }
        } else {
            vector<FunctionValue*> viableDeclarations;
            vector<FunctionValue*> perfectMatches;
            for (const auto function : classDeclaration->body->constructors) {
                auto functionType = (FunctionType*)function->type;
                if (functionType && functionType->argumentTypes.size()-1 == arguments.size()) {
                    bool allMatch = true;
                    for (int i = 0; i < arguments.size(); ++i) {
                        if (functionType->argumentTypes[i]->kind == Type::Kind::Reference && !Value::isLvalue(arguments[i])) {
                            allMatch = false;
                        }
                        if (!cmpPtr(functionType->argumentTypes[i]->getEffectiveType(), arguments[i]->type->getEffectiveType())) {
                            allMatch = false;
                        }
                    }
                    if (allMatch) {
                        perfectMatches.push_back(function);
                    } else {
                        viableDeclarations.push_back(function);
                    }
                }
            }
            if (perfectMatches.size() == 1) {
                return {perfectMatches.back(), classDeclaration};
            } else if (perfectMatches.size() > 1) {
                string message = "ambogous constructor call. ";
                message += "Possible constructors at lines: ";
                for (int i = 0; i < perfectMatches.size(); ++i) {
                    message += to_string(perfectMatches[i]->position.lineNumber);
                    if (i != perfectMatches.size() - 1) {
                        message += ", ";
                    }
                }
                errorMessageBool(message, position);
                return {nullptr, classDeclaration};
            } else {
                vector<optional<vector<CastOperation*>>> neededCasts;
                for (auto function : viableDeclarations) {
                    neededCasts.push_back(vector<CastOperation*>());
                    auto argumentTypes = ((FunctionType*)function->type)->argumentTypes;
                    for (int i = 0; i < argumentTypes.size(); ++i) {
                        if (!cmpPtr(argumentTypes[i], arguments[i]->type)) {
                            auto cast = CastOperation::Create(arguments[i]->position, argumentTypes[i]);
                            cast->arguments.push_back(arguments[i]);
                            auto castInterpret = cast->interpret(scope, true);
                            if (castInterpret) {
                                neededCasts.back().value().push_back(cast);
                            } else {
                                neededCasts.back() = nullopt;
                                break;
                            }
                        } else {
                            neededCasts.back().value().push_back(nullptr);
                        }
                    }
                }

                int matchId = -1;
                vector<FunctionValue*> possibleFunctions;
                for (int i = 0; i < neededCasts.size(); ++i) {
                    if (neededCasts[i]) {
                        matchId = i;
                        possibleFunctions.push_back(viableDeclarations[i]);
                    }
                }

                if (matchId == -1) {
                    errorMessageBool("no fitting constructor to call", position);
                    return {nullptr, classDeclaration};
                } 
                if (possibleFunctions.size() > 1) {
                    string message = "ambogous constructor call. ";
                    message += "Possible constructor at lines: ";
                    for (int i = 0; i < possibleFunctions.size(); ++i) {
                        message += to_string(possibleFunctions[i]->position.lineNumber);
                        if (i != possibleFunctions.size() - 1) {
                            message += ", ";
                        }
                    }
                    errorMessageBool(message, position);
                    return {nullptr, classDeclaration};
                }

                for (int i = 0; i < arguments.size(); ++i) {
                    CastOperation* cast = neededCasts[matchId].value()[i];
                    if (cast) {
                        auto castInterpret = cast->interpret(scope);
                        if (castInterpret.value()) {
                            arguments[i] = castInterpret.value();
                        } else {
                            arguments[i] = cast;
                        }
                    }
                }
                return {viableDeclarations[matchId], classDeclaration};
            }
        }
    } else {
        return {nullptr, nullptr};
    }
}

/*
    Operation
*/
Operation::Operation(const CodePosition& position, Kind kind) : 
    Value(position, Value::ValueKind::Operation),
    kind(kind)
{}
vector<unique_ptr<Operation>> Operation::objects;
Operation* Operation::Create(const CodePosition& position, Kind kind) {
    objects.emplace_back(make_unique<Operation>(position, kind));
    return objects.back().get();
}
optional<Value*> Operation::expandAssignOperation(Kind kind, Scope* scope) {
    auto assign = Operation::Create(position, Operation::Kind::Assign);
    auto operation = Operation::Create(position, kind);
    operation->arguments = {arguments[0], arguments[1]};
    assign->arguments = {arguments[0], operation};

    auto assignInterpret = assign->interpret(scope);
    if (!assignInterpret) return nullopt;
    if (assignInterpret.value()) return assignInterpret.value();
    return assign;
}
bool Operation::interpretAllArguments(Scope* scope) {
    for (auto& val : arguments) {
        auto valInterpret = val->interpret(scope);
        if (!valInterpret) {
            return false;
        }
        if (valInterpret.value()) {
            val = valInterpret.value();
        }
        if (!val->type) {
        }
        if (!val->type->interpret(scope)) {
            return false;
        }
    }
    return true;
}

optional<Value*> Operation::interpret(Scope* scope) {
    if (wasInterpreted) {
        return nullptr;
    }
    
    if (kind != Kind::Assign && kind != Kind::Dot && !interpretAllArguments(scope)) {
        return nullopt;
    }

    Type* effectiveType1 = nullptr;
    Type* effectiveType2 = nullptr;

    if (kind != Kind::Assign && kind != Kind::Dot) {
        if (arguments.size() >= 1) {
            effectiveType1 = arguments[0]->type->getEffectiveType();
        }
        if (arguments.size() >= 2) {
            effectiveType2 = arguments[1]->type->getEffectiveType();
        }
        for (auto argument : arguments) {
            if (argument->valueKind == Value::ValueKind::Operation) {
                containsErrorResolve |= ((Operation*)argument)->containsErrorResolve;
            }
        }
    }

    auto typeCopy = type;
    type = nullptr;

    switch (kind) {
    case Kind::Dot: {
        auto arg0Interpret = arguments[0]->interpret(scope);
        if (!arg0Interpret) return nullopt;
        if (arg0Interpret.value()) arguments[0] = arg0Interpret.value();
        
        effectiveType1 = arguments[0]->type->getEffectiveType();
        ClassType* classType = nullptr;
        switch (effectiveType1->kind) {
        case Type::Kind::Class: {
            classType = (ClassType*)effectiveType1;
            break;
        }
        case Type::Kind::RawPointer:
            classType = (ClassType*)((RawPointerType*)effectiveType1)->underlyingType;
            break;
        case Type::Kind::OwnerPointer:
            classType = (ClassType*)((OwnerPointerType*)effectiveType1)->underlyingType;
            break;
        }

        if (!classType) {
            return errorMessageOpt("can use '.' operation on class or pointer to class type only, got "
                + DeclarationMap::toString(arguments[0]->type), position
            );
        }
        if (arguments[1]->valueKind != Value::ValueKind::Variable) {
            return errorMessageOpt("right side of '.' operation needs to be a variable name", position);
        }
        
        Variable* field = (Variable*)arguments[1];
        auto& declarations = classType->declaration->body->declarations;
        vector<Declaration*> viableDeclarations;
        for (auto& declaration : declarations) {
            if (declaration->variable->name == field->name) {
                viableDeclarations.push_back(declaration);
            }
        }
        
        if (viableDeclarations.size() == 0) {
            return errorMessageOpt("class " + DeclarationMap::toString(effectiveType1)
                + " has no field named " + field->name, position
            );
        }
        Declaration* declaration = viableDeclarations.back();
        if (declaration->variable->isConstexpr && declaration->value->valueKind != Value::ValueKind::FunctionValue) {
            return declaration->value;
        }

        arguments[1] = declaration->variable;
        ((Variable*)arguments[1])->declaration = declaration;
        type = declaration->variable->type;

        break;
    }
    case Kind::Address: {
        if (!isLvalue(arguments[0])) {
            return errorMessageOpt("You can only take address of an l-value", position);
        }
        type = RawPointerType::Create(arguments[0]->type);
        break;
    }
    case Kind::GetValue: {
        switch (effectiveType1->kind) {
            case Type::Kind::MaybeError:
                type = ((MaybeErrorType*)arguments[0]->type)->underlyingType;
                break;
            case Type::Kind::OwnerPointer:
                type = ((OwnerPointerType*)arguments[0]->type)->underlyingType;
                break;
            case Type::Kind::RawPointer:
                type = ((RawPointerType*)arguments[0]->type)->underlyingType;
                break;
            default:
                break;
        }
        break;
    }
    case Kind::Deallocation:
        break;
    case Kind::Destroy:{
        if (!isLvalue(arguments[0])) {
            return errorMessageOpt("cannot destroy non-lvalue", position);
        }
        type = Type::Create(Type::Kind::Void);
        break;
    }
    case Kind::Typesize:
        return arguments[0]->type->typesize(scope);
    case Kind::Minus: {
        if (effectiveType1->kind == Type::Kind::Integer) {
            type = IntegerType::Create(IntegerType::Size::I64);
        } else if (effectiveType1->kind == Type::Kind::Float) {
            type = effectiveType1;
        }
        if (arguments[0]->valueKind == Value::ValueKind::Integer) {
            return IntegerValue::Create(position, -(int64_t)((IntegerValue*)arguments[0])->value);
        } else if (arguments[0]->valueKind == Value::ValueKind::Float) {
            return FloatValue::Create(position, -((FloatValue*)arguments[0])->value);
        } else if (arguments[0]->valueKind == Value::ValueKind::Char) {
            return IntegerValue::Create(position, -(int64_t)((CharValue*)arguments[0])->value);
        }
        break;
    }
    case Kind::Mul: {
        auto value = tryEvaluate2ArgArithmetic([](auto val1, auto val2){
            return val1 * val2;
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::Div: {
        auto value = tryEvaluate2ArgArithmetic([](auto val1, auto val2){
            return val1 / val2;
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::Mod: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 % val2;
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::Add: {
        auto value = tryEvaluate2ArgArithmetic([](auto val1, auto val2){
            return val1 + val2;
        }, scope);
        if (value) return value;
        if (type) break;

        auto type1 = arguments[0]->type->getEffectiveType();
        auto type2 = arguments[1]->type->getEffectiveType();
        if (arguments[0]->type->kind == Type::Kind::String && arguments[1]->type->kind == Type::Kind::String) {
            if (arguments[0]->isConstexpr && arguments[1]->isConstexpr) {
                return StringValue::Create(
                    position, 
                    ((StringValue*)arguments[0])->value + ((StringValue*)arguments[1])->value
                );
            }
            type = arguments[0]->type;
        }
        else if (arguments[0]->valueKind == Value::ValueKind::StaticArray && arguments[1]->valueKind == Value::ValueKind::StaticArray) {
            auto staticArray1 = (StaticArrayValue*)arguments[0];
            auto staticArray2 = (StaticArrayValue*)arguments[1];
            auto arrayType1 = (StaticArrayType*)type1;
            auto arrayType2 = (StaticArrayType*)type2;
            if (arrayType1->elementType == arrayType2->elementType) {
                if (arguments[0]->isConstexpr && arguments[1]->isConstexpr) {
                    auto arrayValue = StaticArrayValue::Create(position);
                    arrayValue->type = StaticArrayType::Create(
                        arrayType1->elementType, 
                        arrayType1->sizeAsInt + arrayType2->sizeAsInt
                    );
                    for (auto& element : staticArray1->values) {
                        arrayValue->values.push_back(element);
                    }
                    for (auto& element : staticArray2->values) {
                        arrayValue->values.push_back(element);
                    }
                    return arrayValue;
                }
                type = type1;
            }
        } 
        else if (type1->kind == Type::Kind::DynamicArray && type2->kind == Type::Kind::DynamicArray) {
            auto dynamicArrayType1 = (DynamicArrayType*)type1;
            auto dynamicArrayType2 = (DynamicArrayType*)type2;
            if (dynamicArrayType1->elementType == dynamicArrayType2->elementType) {
                type = type1;
            }
        } 
        else if (type1->kind == Type::Kind::ArrayView && type2->kind == Type::Kind::ArrayView) {
            auto arrayViewType1 = (ArrayViewType*)type1;
            auto arrayViewType2 = (ArrayViewType*)type2;
            if (arrayViewType1->elementType == arrayViewType2->elementType) {
                type = DynamicArrayType::Create(arrayViewType1->elementType);
            }
        } 
        else if (type1->kind == Type::Kind::DynamicArray && type2->kind == Type::Kind::StaticArray) {
            auto arrayType1 = (DynamicArrayType*)type1;
            auto arrayType2 = (StaticArrayType*)type2;
            if (arrayType1->elementType == arrayType2->elementType) {
                type = type1;
            }
        }
        else if (type1->kind == Type::Kind::StaticArray && type2->kind == Type::Kind::DynamicArray) {
            auto arrayType1 = (StaticArrayType*)type1;
            auto arrayType2 = (DynamicArrayType*)type2;
            if (arrayType1->elementType == arrayType2->elementType) {
                type = type2;
            }
        }
        else if (type1->kind == Type::Kind::DynamicArray && type2->kind == Type::Kind::ArrayView) {
            auto arrayType1 = (DynamicArrayType*)type1;
            auto arrayType2 = (ArrayViewType*)type2;
            if (arrayType1->elementType == arrayType2->elementType) {
                type = type1;
            }
        }
        else if (type1->kind == Type::Kind::ArrayView && type2->kind == Type::Kind::DynamicArray) {
            auto arrayType1 = (ArrayViewType*)type1;
            auto arrayType2 = (DynamicArrayType*)type2;
            if (arrayType1->elementType == arrayType2->elementType) {
                type = type2;
            }
        }
        else if (type1->kind == Type::Kind::ArrayView && type2->kind == Type::Kind::StaticArray) {
            auto arrayType1 = (ArrayViewType*)type1;
            auto arrayType2 = (StaticArrayType*)type2;
            if (arrayType1->elementType == arrayType2->elementType) {
                type = DynamicArrayType::Create(arrayType1->elementType);
            }
        }
        else if (type1->kind == Type::Kind::StaticArray && type2->kind == Type::Kind::ArrayView) {
            auto arrayType1 = (StaticArrayType*)type1;
            auto arrayType2 = (StaticArrayType*)type2;
            if (arrayType1->elementType == arrayType2->elementType) {
                type = DynamicArrayType::Create(arrayType1->elementType);
            }
        }
        break;
    }
    case Kind::Sub: {
        auto value = tryEvaluate2ArgArithmetic([](auto val1, auto val2){
            return val1 - val2;
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::Shl: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 << val2;
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::Shr: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 >> val2;
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::Sal: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 << val2;
        }, scope);
        if (value) return value;
        break;
    } 
    case Kind::Sar: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 >> val2;
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::Gt: {
        auto value = tryEvaluate2ArgArithmeticBoolTest([](auto val1, auto val2)->bool{
            return val1 > val2;
        });
        if (value) return value;
        break;
    }
    case Kind::Lt: {
        auto value = tryEvaluate2ArgArithmeticBoolTest([](auto val1, auto val2)->bool{
            return val1 < val2;
        });
        if (value) return value;
        break;
    }
    case Kind::Gte: {
        auto value = tryEvaluate2ArgArithmeticBoolTest([](auto val1, auto val2)->bool{
            return val1 >= val2;
        });
        if (value) return value;
        break;
    }
    case Kind::Lte: {
        auto value = tryEvaluate2ArgArithmeticBoolTest([](auto val1, auto val2)->bool{
            return val1 <= val2;
        });
        if (value) return value;
        break;
    }
    case Kind::Eq: {
        auto value = tryEvaluate2ArgArithmeticBoolTest([](auto val1, auto val2)->bool{
            return val1 == val2;
        });
        if (value) return value;
        if (arguments[0]->type != arguments[1]->type) {
            break;
        }
        if (arguments[0]->isConstexpr && arguments[1]->isConstexpr) {
            if (arguments[0] == arguments[1]) {
                return BoolValue::Create(position, true);
            } else {
                return BoolValue::Create(position, false);
            }
        }
        type = Type::Create(Type::Kind::Bool);
        break;
    }
    case Kind::Neq:{
        auto value = tryEvaluate2ArgArithmeticBoolTest([](auto val1, auto val2)->bool{
            return val1 != val2;
        });
        if (value) return value;
        if (arguments[0]->type != arguments[1]->type) {
            break;
        }
        if (arguments[0]->isConstexpr && arguments[1]->isConstexpr) {
            if (arguments[0] != arguments[1]) {
                return BoolValue::Create(position, true);
            } else {
                return BoolValue::Create(position, false);
            }
        }
        type = Type::Create(Type::Kind::Bool);
        break;
    }
    case Kind::LogicalNot: {
        Value* bool1 = CastOperation::Create(position, Type::Create(Type::Kind::Bool));
        ((Operation*)bool1)->arguments.push_back(arguments[0]);
        auto bool1Interpret = bool1->interpret(scope);
        if (!bool1Interpret) return nullopt;
        if (bool1Interpret.value()) bool1 = bool1Interpret.value();

        if (bool1->isConstexpr) {
            return BoolValue::Create(
                position,
                !((BoolValue*)bool1)->value
            );
        }
        type = Type::Create(Type::Kind::Bool);

        break;   
    }
    case Kind::LogicalAnd: {
        Value* bool1 = CastOperation::Create(position, Type::Create(Type::Kind::Bool));
        ((Operation*)bool1)->arguments.push_back(arguments[0]);
        auto bool1Interpret = bool1->interpret(scope);
        if (!bool1Interpret) return nullopt;
        if (bool1Interpret.value()) bool1 = bool1Interpret.value();

        Value* bool2 = CastOperation::Create(position, Type::Create(Type::Kind::Bool));
        ((Operation*)bool2)->arguments.push_back(arguments[0]);
        auto bool2Interpret = bool2->interpret(scope);
        if (!bool2Interpret) return nullopt;
        if (bool2Interpret.value()) bool2 = bool2Interpret.value();

        if (bool1->isConstexpr && bool2->isConstexpr) {
            return BoolValue::Create(
                position,
                ((BoolValue*)bool1)->value && ((BoolValue*)bool2)->value
            );
        }
        type = Type::Create(Type::Kind::Bool);

        break;   
    }
    case Kind::LogicalOr: {
        Value* bool1 = CastOperation::Create(position, Type::Create(Type::Kind::Bool));
        ((Operation*)bool1)->arguments.push_back(arguments[0]);
        auto bool1Interpret = bool1->interpret(scope);
        if (!bool1Interpret) return nullopt;
        if (bool1Interpret.value()) bool1 = bool1Interpret.value();

        Value* bool2 = CastOperation::Create(position, Type::Create(Type::Kind::Bool));
        ((Operation*)bool2)->arguments.push_back(arguments[0]);
        auto bool2Interpret = bool2->interpret(scope);
        if (!bool2Interpret) return nullopt;
        if (bool2Interpret.value()) bool2 = bool2Interpret.value();

        if (bool1->isConstexpr && bool2->isConstexpr) {
            return BoolValue::Create(
                position,
                ((BoolValue*)bool1)->value || ((BoolValue*)bool2)->value
            );
        }
        type = Type::Create(Type::Kind::Bool);

        break;   
    }
    case Kind::BitNeg: {
        if (arguments[0]->type->kind == Type::Kind::Integer) {
            if (arguments[0]->isConstexpr) {
                if (arguments[0]->valueKind == Value::ValueKind::Integer) {
                    auto value = IntegerValue::Create(
                        position,
                        ~((IntegerValue*)arguments[0])->value
                    );
                    value->type = arguments[0]->type;
                    return value;
                }
                if (arguments[0]->valueKind == Value::ValueKind::Char) {
                    auto value = CharValue::Create(
                        position,
                        ~((CharValue*)arguments[0])->value
                    );
                    return value;
                }
            }
            type = arguments[0]->type;
        }
        break;
    }
    case Kind::BitAnd: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 & val2;
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::BitXor: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 ^ val2;
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::BitOr: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 | val2;
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::Assign:{
        if (arguments[0]->valueKind == Value::ValueKind::Variable) {
            auto variable = (Variable*)arguments[0];
            if (!variable->interpretTypeAndDeclaration(scope)) {
                return nullopt;
            }
        }

        auto arg1Interpret = arguments[1]->interpret(scope);
        if (!arg1Interpret) return nullopt;
        if (arg1Interpret.value()) arguments[1] = arg1Interpret.value();
        if (!arguments[1]->type->interpret(scope)) return nullopt;
        arguments[1]->wasCaptured = true;

        if (arguments[1]->valueKind == Value::ValueKind::Operation) {
            containsErrorResolve = ((Operation*)arguments[1])->containsErrorResolve;
        }

        if (!containsErrorResolve && arguments[0]->valueKind == Value::ValueKind::Variable) {
            auto variable = (Variable*)arguments[0];
            if (variable->declaration->scope->owner != Scope::Owner::Class) {
                scope->maybeUninitializedDeclarations.erase(variable->declaration);
                scope->declarationsInitState.at(variable->declaration) = true;
            }
        }

        auto arg0Interpret = arguments[0]->interpret(scope);
        if (!arg0Interpret) return nullopt;
        if (arg0Interpret.value()) arguments[0] = arg0Interpret.value();
        if (!arguments[0]->type->interpret(scope)) return nullopt;



        if (!isLvalue(arguments[0])) {
            return errorMessageOpt("left argument of an assignment must be an l-value", position);
        }

        CastOperation* cast = CastOperation::Create(arguments[1]->position, arguments[0]->type->getEffectiveType());
        
        /*if (arguments[0]->type->kind == Type::Kind::Reference) {
            cast->argType = ((ReferenceType*)arguments[0]->type)->underlyingType;
            cast->type = cast->argType;
        }*/
        cast->arguments.push_back(arguments[1]);
        arguments[1] = cast;

        auto castInterpret = arguments[1]->interpret(scope);
        if (!castInterpret) return nullopt;
        if (castInterpret.value()) arguments[1] = castInterpret.value();
        type = arguments[0]->type;

        break;
    }
    case Kind::AddAssign:
        return expandAssignOperation(Operation::Kind::Add, scope);
    case Kind::SubAssign:
        return expandAssignOperation(Operation::Kind::Sub, scope);
    case Kind::MulAssign:
        return expandAssignOperation(Operation::Kind::Mul, scope);
    case Kind::DivAssign:
        return expandAssignOperation(Operation::Kind::Div, scope);
    case Kind::ModAssign:
        return expandAssignOperation(Operation::Kind::Mod, scope);
    case Kind::ShlAssign:
        return expandAssignOperation(Operation::Kind::Shl, scope);
    case Kind::ShrAssign:
        return expandAssignOperation(Operation::Kind::Shr, scope);
    case Kind::SalAssign:
        return expandAssignOperation(Operation::Kind::Sal, scope);
    case Kind::SarAssign:
        return expandAssignOperation(Operation::Kind::Sar, scope);
    case Kind::BitNegAssign:
        return expandAssignOperation(Operation::Kind::BitNeg, scope);
    case Kind::BitOrAssign:
        return expandAssignOperation(Operation::Kind::BitOr, scope);
    case Kind::BitXorAssign:
        return expandAssignOperation(Operation::Kind::BitXor, scope);
    default: 
        break;
    }

    if (!type) {
        string message = "incorrect use of operation '" + kindToString(kind) + "'. ";
        if (arguments.size() == 0) {

        }
        else if (arguments.size() == 1) {
            message += "type is: ";
            message += DeclarationMap::toString(arguments[0]->type);
        } else {
            message += "types are: ";
            message += DeclarationMap::toString(arguments[0]->type);
            message += "; ";
            message += DeclarationMap::toString(arguments[1]->type);
        }

        return errorMessageOpt(message, position);
    }

    wasInterpreted = true;
    return nullptr;
}

string Operation::kindToString(Kind kind) {
    switch (kind) {
    case Kind::Destroy: return "destroy";
    case Kind::Dot: return ". (dot)";
    case Kind::FunctionCall: return "function call";
    case Kind::ArrayIndex: return "[x] (array index)";
    case Kind::ArraySubArray: return "[x:y] (sub-array)";
    case Kind::Address: return "@ (address)";
    case Kind::GetValue: return "$ (valueOf)";
    case Kind::Allocation: return "alloc";
    case Kind::Deallocation: return "dealloc";
    case Kind::Cast: return "[T]() (cast)";
    case Kind::BitNeg: return "~ (bit negation)";
    case Kind::LogicalNot: return "! (logical not)";
    case Kind::Minus: return "- (unary minus)";
    case Kind::Mul: return "* (multiply)";
    case Kind::Div: return "/ (divide)";
    case Kind::Mod: return "% (modulo)";
    case Kind::Add: return "+ (add)";
    case Kind::Sub: return "- (substract)";
    case Kind::Shl: return "<< (shift left)";
    case Kind::Shr: return ">> (shift right)";
    case Kind::Sal: return "<<< (logical shift left)";
    case Kind::Sar: return ">>> (logical shift right)";
    case Kind::Gt: return "> (greater then)";
    case Kind::Lt: return "< (less then)";
    case Kind::Gte: return ">= (greater then or equal)";
    case Kind::Lte: return "<= (less then or equal)";
    case Kind::Eq: return "== (equal)";
    case Kind::Neq: return "!= (not equal)";
    case Kind::BitAnd: return "& (bit and)";
    case Kind::BitXor: return "^ (bit xor)";
    case Kind::BitOr: return "| (bit or)";
    case Kind::LogicalAnd: return "&& (logical and)";
    case Kind::LogicalOr: return "|| (logical or)";
    case Kind::Assign: return "= (assign)";
    case Kind::AddAssign: return "+= (add-assign)";
    case Kind::SubAssign: return "-= (sub-assign)";
    case Kind::MulAssign: return "*= (mul-assign)";
    case Kind::DivAssign: return "/= (div-assign)";
    case Kind::ModAssign: return "%= (mod-assign)";
    case Kind::ShlAssign: return "<<= (shl-assign)";
    case Kind::ShrAssign: return ">>= (shr-assign)";
    case Kind::SalAssign: return "<<<= (sal-assign)";
    case Kind::SarAssign: return ">>>= (sar-assign)";
    case Kind::BitNegAssign: return "~= (neg-assign)";
    case Kind::BitOrAssign: return "|= (or-assign)";
    case Kind::BitXorAssign: return "^= (xor-assign)";
    default: return "unknown";
    }
}
int Operation::priority(Kind kind) {
    switch (kind) {
    case Kind::Dot:
    case Kind::FunctionCall:
    case Kind::ArrayIndex:
    case Kind::ArraySubArray:
    case Kind::ErrorResolve:
        return 1;
    case Kind::Address:
    case Kind::GetValue:
    case Kind::Allocation:
    case Kind::Deallocation:
    case Kind::Destroy:
    case Kind::Cast:
    case Kind::BitNeg:
    case Kind::LogicalNot:
    case Kind::Minus:
        return 2;
    case Kind::Mul:
    case Kind::Div:
    case Kind::Mod:
        return 3;
    case Kind::Add:
    case Kind::Sub:
        return 4;
    case Kind::Shl:
    case Kind::Shr:
    case Kind::Sal:
    case Kind::Sar:
        return 5;
    case Kind::Gt:
    case Kind::Lt:
    case Kind::Gte:
    case Kind::Lte:
        return 6;
    case Kind::Eq:
    case Kind::Neq:
        return 7;
    case Kind::BitAnd:
        return 8;
    case Kind::BitXor:
        return 9;
    case Kind::BitOr:
        return 10;
    case Kind::LogicalAnd:
        return 11;
    case Kind::LogicalOr:
        return 12;
    case Kind::Assign:
    case Kind::AddAssign:
    case Kind::SubAssign:
    case Kind::MulAssign:
    case Kind::DivAssign:
    case Kind::ModAssign:
    case Kind::ShlAssign:
    case Kind::ShrAssign:
    case Kind::SalAssign:
    case Kind::SarAssign:
    case Kind::BitNegAssign:
    case Kind::BitOrAssign:
    case Kind::BitXorAssign:
        return 13;
    default: 
        return 14;
    }
}
bool Operation::isLeftAssociative(Kind kind) {
    switch (kind) {
    case Kind::Dot:
    case Kind::FunctionCall:
    case Kind::ArrayIndex:
    case Kind::ArraySubArray:
    case Kind::ErrorResolve:
    case Kind::Mul:
    case Kind::Div:
    case Kind::Mod:
    case Kind::Add:
    case Kind::Sub:
    case Kind::Shl:
    case Kind::Shr:
    case Kind::Sal:
    case Kind::Sar:
    case Kind::Gt:
    case Kind::Lt:
    case Kind::Gte:
    case Kind::Lte:
    case Kind::Eq:
    case Kind::Neq:
    case Kind::BitAnd:
    case Kind::BitXor:
    case Kind::BitOr:
    case Kind::LogicalAnd:
    case Kind::LogicalOr:
        return true;
    default: 
        return false;
    }
}
int Operation::numberOfArguments(Kind kind) {
    switch (kind) {
    case Kind::FunctionCall:
    case Kind::Allocation:
        return 0;
    case Kind::Cast:
    case Kind::ArrayIndex:
    case Kind::ArraySubArray:
    case Kind::Address:
    case Kind::GetValue:
    case Kind::Deallocation:
    case Kind::Destroy:
    case Kind::BitNeg:
    case Kind::LogicalNot:
    case Kind::Minus:
    case Kind::ErrorResolve:
        return 1;
    case Kind::Dot:
    case Kind::Mul:
    case Kind::Div:
    case Kind::Mod:
    case Kind::Add:
    case Kind::Sub:
    case Kind::Shl:
    case Kind::Shr:
    case Kind::Sal:
    case Kind::Sar:
    case Kind::Gt:
    case Kind::Lt:
    case Kind::Gte:
    case Kind::Lte:
    case Kind::Eq:
    case Kind::Neq:
    case Kind::BitAnd:
    case Kind::BitXor:
    case Kind::BitOr:
    case Kind::LogicalAnd:
    case Kind::LogicalOr:
    case Kind::Assign:
    case Kind::AddAssign:
    case Kind::SubAssign:
    case Kind::MulAssign:
    case Kind::DivAssign:
    case Kind::ModAssign:
    case Kind::ShlAssign:
    case Kind::ShrAssign:
    case Kind::SalAssign:
    case Kind::SarAssign:
    case Kind::BitNegAssign:
    case Kind::BitOrAssign:
    case Kind::BitXorAssign:
        return 2;
    default: 
        return 0;
    }
}
int Operation::getPriority() {
    return priority(kind);
}
bool Operation::getIsLeftAssociative() {
    return isLeftAssociative(kind);
}
int Operation::getNumberOfArguments() {
    return numberOfArguments(kind);
}

bool Operation::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const Operation&>(value);
        return this->arguments == other.arguments
            && this->kind == other.kind
            && Value::operator==(other);
    }
    else {
        return false;
    }
}

/*unique_ptr<Value> Operation::copy() {
    auto value = make_unique<Operation>(position, kind);
    value->type = type->copy();
    value->isConstexpr = isConstexpr;
    for (auto& argument : arguments) {
        value->arguments.push_back(argument->copy());
    }
    return value;
}*/
llvm::Value* Operation::getReferenceLlvm(LlvmObject* llvmObj) {
    switch (kind) {
    case Kind::Dot: {
        auto effectiveType0 = arguments[0]->type->getEffectiveType();
        ClassType* classType = nullptr;
        switch (effectiveType0->kind) {
        case Type::Kind::Class: {
            classType = (ClassType*)effectiveType0;
            break;
        }
        case Type::Kind::RawPointer:
            classType = (ClassType*)((RawPointerType*)effectiveType0)->underlyingType;
            break;
        case Type::Kind::OwnerPointer:
            classType = (ClassType*)((OwnerPointerType*)effectiveType0)->underlyingType;
            break;
        }
        auto accessedDeclaration = ((Variable*)arguments[1])->declaration;
        auto& declarations = classType->declaration->body->declarations;
        int declarationIndex = 0;
        bool found = false;
        for (auto declaration : declarations) {
            if (declaration->variable->isConstexpr) {
                continue;
            }
            if (declaration == accessedDeclaration) {
                found = true;
                break;
            }
            declarationIndex += 1;
        }
        if (!found) {
            internalError("couldn't find index of field during llvm creating", position);
        }

        llvm::Value* arg0;
        if (effectiveType0->kind == Type::Kind::RawPointer || effectiveType0->kind == Type::Kind::OwnerPointer) {
            arg0 = arguments[0]->createLlvm(llvmObj);
        } else {
            arg0 = arguments[0]->getReferenceLlvm(llvmObj);
        }

        vector<llvm::Value*> indexList;
        indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
        indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvmObj->context), declarationIndex));
        return llvm::GetElementPtrInst::Create(
            ((llvm::PointerType*)arg0->getType())->getElementType(),
            arg0,
            indexList,
            "",
            llvmObj->block
        );
    }
    case Kind::GetValue: {
        switch (arguments[0]->type->kind) {
        case Type::Kind::RawPointer: {
            return arguments[0]->createLlvm(llvmObj);
        }
        case Type::Kind::OwnerPointer: {
            return arguments[0]->createLlvm(llvmObj);
        }
        default:
            return nullptr;
        }
    }
    default: return nullptr;
    }
}
llvm::Value* Operation::createLlvm(LlvmObject* llvmObj) {
    switch (kind) {
    case Kind::Dot: {
        auto accessedDeclaration = ((Variable*)arguments[1])->declaration;
        if (accessedDeclaration->value && accessedDeclaration->variable->isConstexpr) {
            // is const member function
            return accessedDeclaration->value->createLlvm(llvmObj);
        }
        return new llvm::LoadInst(getReferenceLlvm(llvmObj), "", llvmObj->block);
    }
    case Kind::Address: {
        return arguments[0]->getReferenceLlvm(llvmObj);
    }
    case Kind::GetValue: {
        switch (arguments[0]->type->kind) {
        case Type::Kind::MaybeError: {
            auto arg = arguments[0]->getReferenceLlvm(llvmObj);
            vector<llvm::Value*> indexList;
            indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
            indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvmObj->context), 0));
            auto gep = llvm::GetElementPtrInst::Create(
                ((llvm::PointerType*)arg->getType())->getElementType(), arg, indexList, "", llvmObj->block
            );
            return new llvm::LoadInst(gep, "", llvmObj->block);
        }
        case Type::Kind::RawPointer: {
            return new llvm::LoadInst(arguments[0]->createLlvm(llvmObj), "", llvmObj->block);
        }
        case Type::Kind::OwnerPointer: {
            return new llvm::LoadInst(arguments[0]->createLlvm(llvmObj), "", llvmObj->block);
        }
        default:
            return nullptr;
        }
        break;
    }
    case Kind::Destroy: {
        llvm::Value* value;
        if (arguments[0]->type->kind == Type::Kind::Class) {
            value = arguments[0]->getReferenceLlvm(llvmObj);
        } else {
            value = arguments[0]->createLlvm(llvmObj);
        }
        arguments[0]->type->createDestructorLlvm(llvmObj, value);
        return nullptr;
    }
    case Kind::Minus: {
        auto arg = arguments[0]->createLlvm(llvmObj);
        auto zero = llvm::ConstantInt::get(type->createLlvm(llvmObj), 0);
        if (type->kind == Type::Kind::Integer) {
            return llvm::BinaryOperator::CreateSub(zero, arg, "", llvmObj->block);
        } else {
            return llvm::BinaryOperator::CreateFSub(zero, arg, "", llvmObj->block);
        } 
    }
    case Kind::Mul: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (type->kind == Type::Kind::Integer) {
            return llvm::BinaryOperator::CreateMul(arg1, arg2, "", llvmObj->block);
        } else {
            return llvm::BinaryOperator::CreateFMul(arg1, arg2, "", llvmObj->block);
        } 
    }
    case Kind::Div: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (type->kind == Type::Kind::Integer) {
            if (((IntegerType*)type)->isSigned()) {
                return llvm::BinaryOperator::CreateSDiv(arg1, arg2, "", llvmObj->block);
            } else {
                return llvm::BinaryOperator::CreateUDiv(arg1, arg2, "", llvmObj->block);
            }
        } else {
            return llvm::BinaryOperator::CreateFDiv(arg1, arg2, "", llvmObj->block);
        } 
    }
    case Kind::Mod: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (type->kind == Type::Kind::Integer) {
            if (((IntegerType*)type)->isSigned()) {
                return llvm::BinaryOperator::CreateSRem(arg1, arg2, "", llvmObj->block);
            } else {
                return llvm::BinaryOperator::CreateURem(arg1, arg2, "", llvmObj->block);
            }
        } else {
            return llvm::BinaryOperator::CreateFRem(arg1, arg2, "", llvmObj->block);
        } 
    }
    case Kind::Add: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (type->kind == Type::Kind::Integer) {
            return llvm::BinaryOperator::CreateAdd(arg1, arg2, "", llvmObj->block);
        } else {
            return llvm::BinaryOperator::CreateFAdd(arg1, arg2, "", llvmObj->block);
        } 
    }
    case Kind::Sub: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (type->kind == Type::Kind::Integer) {
            return llvm::BinaryOperator::CreateSub(arg1, arg2, "", llvmObj->block);
        } else {
            return llvm::BinaryOperator::CreateFSub(arg1, arg2, "", llvmObj->block);
        } 
    }
    case Kind::Shl: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        return llvm::BinaryOperator::CreateShl(arg1, arg2, "", llvmObj->block);
    }
    case Kind::Shr: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        return llvm::BinaryOperator::CreateLShr(arg1, arg2, "", llvmObj->block);
    }
    case Kind::Sal: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        return llvm::BinaryOperator::CreateShl(arg1, arg2, "", llvmObj->block);
    }
    case Kind::Sar: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        return llvm::BinaryOperator::CreateAShr(arg1, arg2, "", llvmObj->block);
    }
    case Kind::Gt: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (arguments[0]->type->kind == Type::Kind::Integer) {
            if (((IntegerType*)arguments[0]->type)->isSigned()) {
                return new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SGT, arg1, arg2, "");
            } else {
                return new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_UGT, arg1, arg2, "");
            }
            
        } else {
            return new llvm::FCmpInst(*llvmObj->block, llvm::FCmpInst::FCMP_UGT, arg1, arg2, "");
        }
    }
    case Kind::Lt: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (arguments[0]->type->kind == Type::Kind::Integer) {
            if (((IntegerType*)arguments[0]->type)->isSigned()) {
                return new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLT, arg1, arg2, "");
            } else {
                return new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_ULT, arg1, arg2, "");
            }

        } else {
            return new llvm::FCmpInst(*llvmObj->block, llvm::FCmpInst::FCMP_ULT, arg1, arg2, "");
        }
    }
    case Kind::Gte: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (arguments[0]->type->kind == Type::Kind::Integer) {
            if (((IntegerType*)arguments[0]->type)->isSigned()) {
                return new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SGE, arg1, arg2, "");
            } else {
                return new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_UGE, arg1, arg2, "");
            }

        } else {
            return new llvm::FCmpInst(*llvmObj->block, llvm::FCmpInst::FCMP_UGE, arg1, arg2, "");
        }
    }
    case Kind::Lte: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (arguments[0]->type->kind == Type::Kind::Integer) {
            if (((IntegerType*)arguments[0]->type)->isSigned()) {
                return new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLE, arg1, arg2, "");
            } else {
                return new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_ULE, arg1, arg2, "");
            }

        } else {
            return new llvm::FCmpInst(*llvmObj->block, llvm::FCmpInst::FCMP_ULE, arg1, arg2, "");
        }
    }
    case Kind::Eq: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (arguments[0]->type->kind == Type::Kind::Integer) {
            return new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, arg1, arg2, "");
        } else {
            return new llvm::FCmpInst(*llvmObj->block, llvm::FCmpInst::FCMP_UEQ, arg1, arg2, "");
        }
    }
    case Kind::Neq: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (arguments[0]->type->kind == Type::Kind::Integer) {
            return new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_NE, arg1, arg2, "");
        } else if (arguments[0]->type->kind == Type::Kind::Float) {
            return new llvm::FCmpInst(*llvmObj->block, llvm::FCmpInst::FCMP_UNE, arg1, arg2, "");
        } else {
            internalError("!= operation expects integer and float types only in llvm creating, got " + DeclarationMap::toString(arguments[0]->type), position);
        }
    }
    case Kind::LogicalNot: {
        auto arg = arguments[0]->createLlvm(llvmObj);
        return llvm::BinaryOperator::CreateNot(arg, "", llvmObj->block);
    }
    case Kind::LogicalAnd: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        return llvm::BinaryOperator::CreateAnd(arg1, arg2, "", llvmObj->block);
    }
    case Kind::LogicalOr: {  
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        return llvm::BinaryOperator::CreateOr(arg1, arg2, "", llvmObj->block);
    }
    case Kind::BitNeg: {
        auto arg = arguments[0]->createLlvm(llvmObj);
        if (type->kind == Type::Kind::Integer) {
            llvm::BinaryOperator::CreateNeg(arg, "", llvmObj->block);
        } else {
            llvm::BinaryOperator::CreateFNeg(arg, "", llvmObj->block);
        }
    }
    case Kind::BitAnd: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        return llvm::BinaryOperator::CreateAnd(arg1, arg2, "", llvmObj->block);
    }
    case Kind::BitXor: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        return llvm::BinaryOperator::CreateXor(arg1, arg2, "", llvmObj->block);
    }
    case Kind::BitOr: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        return llvm::BinaryOperator::CreateOr(arg1, arg2, "", llvmObj->block);
    }
    case Kind::Assign: {
        auto arg0Reference = arguments[0]->getReferenceLlvm(llvmObj);
        new llvm::StoreInst(arguments[1]->createLlvm(llvmObj), arg0Reference, llvmObj->block);
        return new llvm::LoadInst(arg0Reference, "", llvmObj->block);
    }
    default:
        internalError("unexpected operation when creating llvm", position);
    }
    return nullptr;
}


/*
    CastOperation
*/
CastOperation::CastOperation(const CodePosition& position, Type* argType) : 
    Operation(position, Operation::Kind::Cast),
    argType(argType)
{
    type = argType;
}
vector<unique_ptr<CastOperation>> CastOperation::objects;
CastOperation* CastOperation::Create(const CodePosition& position, Type* argType) {
    objects.emplace_back(make_unique<CastOperation>(position, argType));
    return objects.back().get();
}
optional<Value*> CastOperation::interpret(Scope* scope) {
    return interpret(scope, false);
}
optional<Value*> CastOperation::interpret(Scope* scope, bool onlyTry) {
    if (wasInterpreted) {
        return nullptr;
    }
    if (!interpretAllArguments(scope)) {
        return nullopt;
    }
    if (!type->interpret(scope) || !argType->interpret(scope)) {
        if (!onlyTry) errorMessageBool("cannot cast to unknown type " + DeclarationMap::toString(type), position);
        return nullopt;
    }
    if (arguments[0]->valueKind == Value::ValueKind::Operation) {
        containsErrorResolve = ((Operation*)arguments[0])->containsErrorResolve;
    }

    auto effectiveType = arguments[0]->type->getEffectiveType();

    if (cmpPtr(arguments[0]->type, type)) {
        return arguments[0];
    } 
    else if (type->kind == Type::Kind::Reference) {
        if (!isLvalue(arguments[0])){
            if (!onlyTry) errorMessageBool("cannot cast non-lvalue to reference type", position);
            return nullopt;
        }
        if (cmpPtr(effectiveType, type->getEffectiveType())) {
            return arguments[0];
        }
    }
    else if (cmpPtr(effectiveType, type)) {
        return arguments[0];
    }
    else if (type->kind == Type::Kind::Bool) {
        if (arguments[0]->isConstexpr) {
            if (arguments[0]->valueKind == Value::ValueKind::Char) {
                return BoolValue::Create(position, ((CharValue*)arguments[0])->value != 0);
            }
            if (arguments[0]->valueKind == Value::ValueKind::Integer) {
                return BoolValue::Create(position, ((IntegerValue*)arguments[0])->value != 0);
            }
            if (arguments[0]->valueKind == Value::ValueKind::Float) {
                return BoolValue::Create(position, ((FloatValue*)arguments[0])->value != 0);
            }
            if (arguments[0]->valueKind == Value::ValueKind::String) {
                return BoolValue::Create(position, ((StringValue*)arguments[0])->value.size() != 0);
            }
            if (arguments[0]->valueKind == Value::ValueKind::StaticArray) {
                return BoolValue::Create(position, ((StaticArrayValue*)arguments[0])->values.size() != 0);
            }
        }
        if (effectiveType->kind != Type::Kind::Class) {
            if (!onlyTry) wasInterpreted = true;
            if (arguments[0]->type->kind == Type::Kind::Integer) {
                auto op = Operation::Create(position, Operation::Kind::Neq);
                op->arguments.push_back(arguments[0]);
                auto integerValue = IntegerValue::Create(position, 0);
                integerValue->type = IntegerType::Create(((IntegerType*)arguments[0]->type)->size);
                op->arguments.push_back(integerValue);
                auto opInterpret = op->interpret(scope);
                if (!opInterpret) return nullopt;
                if (opInterpret.value()) return opInterpret.value();
                else return op;
            } 
            if (arguments[0]->type->kind == Type::Kind::Float) {
                auto op = Operation::Create(position, Operation::Kind::Neq);
                op->arguments.push_back(arguments[0]);
                auto integerValue = FloatValue::Create(position, 0);
                integerValue->type = FloatType::Create(((FloatType*)arguments[0]->type)->size);
                op->arguments.push_back(integerValue);
                auto opInterpret = op->interpret(scope);
                if (!opInterpret) return nullopt;
                if (opInterpret.value()) return opInterpret.value();
                else return op;
            } 
            return nullptr;
        }
    }
    else if (type->kind == Type::Kind::Integer) {
        if (effectiveType->kind == Type::Kind::Integer) {
            if (arguments[0]->isConstexpr) {
                if (arguments[0]->valueKind == Value::ValueKind::Char) {
                    auto intValue = IntegerValue::Create(position, ((CharValue*)arguments[0])->value);
                    intValue->type = type;
                    return intValue;
                }
                arguments[0]->type = type;
                return arguments[0];
            }
            if (!onlyTry) wasInterpreted = true;
            return nullptr;
        }
        if (effectiveType->kind == Type::Kind::Float) {
            if (arguments[0]->isConstexpr) {
                auto intValue = IntegerValue::Create(position, ((FloatValue*)arguments[0])->value);
                intValue->type = type;
                return intValue;
            }
            if (!onlyTry) wasInterpreted = true;
            return nullptr;
        }
        if (effectiveType->kind == Type::Kind::Bool) {
            if (arguments[0]->isConstexpr) {
                int value = ((BoolValue*)arguments[0])->value ? 1 : 0;
                auto intValue = IntegerValue::Create(position, value);
                intValue->type = type;
                return intValue;
            }
            if (!onlyTry) wasInterpreted = true;
            return nullptr;
        }
    }
    else if (type->kind == Type::Kind::Float) {
        if (effectiveType->kind == Type::Kind::Integer) {
            if (arguments[0]->isConstexpr) {
                Value* floatValue;
                if (arguments[0]->valueKind == Value::ValueKind::Integer) {
                    floatValue = FloatValue::Create(position, ((IntegerValue*)arguments[0])->value);
                } else {
                    floatValue = FloatValue::Create(position, ((CharValue*)arguments[0])->value);
                }
                floatValue->type = type;
                return floatValue;
            }
            if (!onlyTry) wasInterpreted = true;
            return nullptr;
        }
        if (effectiveType->kind == Type::Kind::Float) {
            if (arguments[0]->isConstexpr) {
                arguments[0]->type = type;
                return arguments[0];
            }
            if (!onlyTry) wasInterpreted = true;
            return nullptr;
        }
        if (effectiveType->kind == Type::Kind::Bool) {
            if (arguments[0]->isConstexpr) {
                double value = ((BoolValue*)arguments[0])->value ? 1 : 0;
                auto floatValue = FloatValue::Create(position, value);
                floatValue->type = type;
                return floatValue;
            }
            if (!onlyTry) wasInterpreted = true;
            return nullptr;
        }
    }
    else if (type->kind == Type::Kind::RawPointer && effectiveType->kind == Type::Kind::RawPointer) {
        if (!onlyTry) wasInterpreted = true;
        return nullptr;
    } else if (type->kind == Type::Kind::MaybeError) {
        if (effectiveType->kind == Type::Kind::MaybeError) {
            auto maybeErrorType = (MaybeErrorType*)effectiveType;
            if (maybeErrorType->underlyingType->kind == Type::Kind::Void
                || ((MaybeErrorType*)type)->underlyingType->kind == Type::Kind::Void) {
                if (!onlyTry) wasInterpreted = true;
                return nullptr;
            }
        } else {
            auto cast = CastOperation::Create(position, ((MaybeErrorType*)type)->underlyingType);
            cast->arguments.push_back(arguments[0]);
            auto castInterpret = cast->interpret(scope);
            if (!castInterpret) return nullopt;
            else if (castInterpret.value()) arguments[0] = castInterpret.value();
            else arguments[0] = cast;
            if (!onlyTry) wasInterpreted = true;
            return nullptr;
        }
    }

    if (!onlyTry) {
        return errorMessageOpt("cannot cast " + 
            DeclarationMap::toString(arguments[0]->type) + 
            " to " + DeclarationMap::toString(type), position
        );
    }
    
    return nullopt;
}
bool CastOperation::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const CastOperation&>(value);
        return cmpPtr(this->argType, other.argType)
            && Operation::operator==(other);
    }
    else {
        return false;
    }
}
/*unique_ptr<Value> CastOperation::copy() {
    auto value = make_unique<CastOperation>(position, argType->copy());
    value->type = type->copy();
    value->isConstexpr = isConstexpr;
    for (auto& argument : arguments) {
        value->arguments.push_back(argument->copy());
    }
    return value;
}*/
llvm::Value* CastOperation::createLlvm(LlvmObject* llvmObj) {
    auto arg = arguments[0]->createLlvm(llvmObj);
    auto arg0Type = arguments[0]->type->getEffectiveType();
    if (type->kind == Type::Kind::Integer) {
        auto integerType = (IntegerType*)type;
        if (arg0Type->kind == Type::Kind::Integer) {
            auto sizeCast = integerType->sizeInBytes();
            auto sizeArg = ((IntegerType*)arg0Type)->sizeInBytes();
            if (sizeCast == sizeArg) {
                return arg;
            } else if (sizeCast > sizeArg) {
                return new llvm::SExtInst(arg, integerType->createLlvm(llvmObj), "", llvmObj->block);
            } else {
                return new llvm::TruncInst(arg, integerType->createLlvm(llvmObj), "", llvmObj->block);
            }
        }
        else if (arg0Type->kind == Type::Kind::Float) {
            if (integerType->isSigned()) {
                return new llvm::FPToSIInst(arg, type->createLlvm(llvmObj), "", llvmObj->block);
            } else {
                return new llvm::FPToUIInst(arg, type->createLlvm(llvmObj), "", llvmObj->block);
            }
        }
        else {
            internalError("only integer and float types can be casted to integer in llvm stage", position);
        }
    } else if (type->kind == Type::Kind::Float) {
        auto floatType = (FloatType*)type;
        if (arg0Type->kind == Type::Kind::Integer) {
            if (((IntegerType*)arg0Type)->isSigned()) {
                return new llvm::SIToFPInst(arg, type->createLlvm(llvmObj), "", llvmObj->block);
            } else {
                return new llvm::UIToFPInst(arg, type->createLlvm(llvmObj), "", llvmObj->block);
            }
        } 
        else if (arg0Type->kind == Type::Kind::Float) {
            if (floatType->size == FloatType::Size::F32) {
                return new llvm::FPTruncInst(arg, type->createLlvm(llvmObj), "", llvmObj->block);
            } else {
                return new llvm::FPExtInst(arg, type->createLlvm(llvmObj), "", llvmObj->block);
            }
        }
        else {
            internalError("only integer and float types can be casted to float in llvm stage", position);
        }
    } else if (type->kind == Type::Kind::MaybeError) {
        return new llvm::LoadInst(getReferenceLlvm(llvmObj), "", llvmObj->block);
    } else if (type->kind == Type::Kind::RawPointer) {
        if (arg0Type->kind == Type::Kind::RawPointer) {
            return new llvm::BitCastInst(arg, type->createLlvm(llvmObj), "", llvmObj->block);
        } else {
            internalError("only pointer can be casted to pointer in llvm stage", position);
        }
    }
    else {
        internalError("can only cast to integer, float, maybeError and raw pointer types in llvm stage", position);
    }
    return nullptr;
}
llvm::Value* CastOperation::getReferenceLlvm(LlvmObject* llvmObj) {
    //auto arg = arguments[0]->createLlvm(llvmObj);
    if (type->kind == Type::Kind::MaybeError) {
        if (arguments[0]->type->kind == Type::Kind::MaybeError) {
            auto maybeErrorType = (MaybeErrorType*)arguments[0]->type;
            if (maybeErrorType->underlyingType->kind == Type::Kind::Void) {
                // ?T = ?void
                auto var = type->allocaLlvm(llvmObj);
                vector<llvm::Value*> indexList;
                indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
                indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvmObj->context), 1));
                auto gep = llvm::GetElementPtrInst::Create(
                    ((llvm::PointerType*)var->getType())->getElementType(), var, indexList, "", llvmObj->block
                );
                new llvm::StoreInst(arguments[0]->createLlvm(llvmObj), gep, llvmObj->block);
                return var;
            }
            else if (((MaybeErrorType*)type)->underlyingType->kind == Type::Kind::Void) {
                // ?void = ?T
                auto argRef = arguments[0]->getReferenceLlvm(llvmObj);
                vector<llvm::Value*> indexList;
                indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
                indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvmObj->context), 1));
                auto gep = llvm::GetElementPtrInst::Create(
                    ((llvm::PointerType*)argRef->getType())->getElementType(), argRef, indexList, "", llvmObj->block
                );
                return gep;
            }
        } else {
            // ?T = T
            auto var = type->allocaLlvm(llvmObj);
            vector<llvm::Value*> indexList;
            indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
            indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvmObj->context), 0));
            auto gep = llvm::GetElementPtrInst::Create(
                ((llvm::PointerType*)var->getType())->getElementType(), var, indexList, "", llvmObj->block
            );
            new llvm::StoreInst(arguments[0]->createLlvm(llvmObj), gep, llvmObj->block);
            return var;
        }
    } else {
        internalError("could not get reference to object after casting", position);
    }
}


/*
    ArrayIndexOperation
*/
ArrayIndexOperation::ArrayIndexOperation(const CodePosition& position, Value* index) : 
    Operation(position, Operation::Kind::ArrayIndex),
    index(index)
{}
vector<unique_ptr<ArrayIndexOperation>> ArrayIndexOperation::objects;
ArrayIndexOperation* ArrayIndexOperation::Create(const CodePosition& position, Value* index) {
    objects.emplace_back(make_unique<ArrayIndexOperation>(position, index));
    return objects.back().get();
}
optional<Value*> ArrayIndexOperation::interpret(Scope* scope) {
    if (!interpretAllArguments(scope)) {
        return nullopt;
    }
    auto indexInterpret = index->interpret(scope);
    if (!indexInterpret) return nullopt;
    if (indexInterpret.value()) index = indexInterpret.value();

    if (index->type->kind != Type::Kind::Integer) {
        return errorMessageOpt("array index must be an integer value, got " 
            + DeclarationMap::toString(index->type), position
        );
    }

    if (index->isConstexpr && arguments[0]->isConstexpr && arguments[0]->valueKind == Value::ValueKind::StaticArray) {
        auto& staticArrayValues = ((StaticArrayValue*)arguments[0])->values;
        if (index->valueKind == Value::ValueKind::Integer) {
            auto indexValue = ((IntegerValue*)index)->value;
            switch (((IntegerType*)index->type)->size) {
            case IntegerType::Size::I8:
                return evaluateConstexprIntegerIndex<int8_t>(staticArrayValues, indexValue);
            case IntegerType::Size::I16:
                return evaluateConstexprIntegerIndex<int16_t>(staticArrayValues, indexValue);
            case IntegerType::Size::I32:
                return evaluateConstexprIntegerIndex<int32_t>(staticArrayValues, indexValue);
            case IntegerType::Size::I64:
                return evaluateConstexprIntegerIndex<int64_t>(staticArrayValues, indexValue);
            case IntegerType::Size::U8:
                return evaluateConstexprIntegerIndex<uint8_t>(staticArrayValues, indexValue);
            case IntegerType::Size::U16:
                return evaluateConstexprIntegerIndex<uint16_t>(staticArrayValues, indexValue);
            case IntegerType::Size::U32:
                return evaluateConstexprIntegerIndex<uint32_t>(staticArrayValues, indexValue);
            case IntegerType::Size::U64:
                return evaluateConstexprIntegerIndex<uint64_t>(staticArrayValues, indexValue);
            }
        }
        else if (index->valueKind == Value::ValueKind::Char) {
            auto indexValue = ((CharValue*)index)->value;
            if (indexValue >= staticArrayValues.size()) {
                return errorMessageOpt("array index outside the bounds of an array", position);
            }
            return staticArrayValues[indexValue];
        } else {
            internalError("expected constexpr integer or char in array index", position);
        }
    }

    switch (arguments[0]->type->kind) {
        case Type::Kind::StaticArray:
            type = ReferenceType::Create(((StaticArrayType*)arguments[0]->type)->elementType);
            break;
        case Type::Kind::DynamicArray:
            type = ReferenceType::Create(((DynamicArrayType*)arguments[0]->type)->elementType);
            break;
        case Type::Kind::ArrayView:
            type = ReferenceType::Create(((ArrayViewType*)arguments[0]->type)->elementType);
            break;
        default:
            break;
    }
    if (!type) {
        return errorMessageOpt("cannot index value of type " + DeclarationMap::toString(arguments[0]->type), position);
    }

    return nullptr;
}
bool ArrayIndexOperation::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const ArrayIndexOperation&>(value);
        return cmpPtr(this->index, other.index)
            && Operation::operator==(other);
    }
    else {
        return false;
    }
}
/*unique_ptr<Value> ArrayIndexOperation::copy() {
    auto value = make_unique<ArrayIndexOperation>(position, index->copy());
    value->type = type->copy();
    value->isConstexpr = isConstexpr;
    for (auto& argument : arguments) {
        value->arguments.push_back(argument->copy());
    }
    return value;
}*/
llvm::Value* ArrayIndexOperation::getReferenceLlvm(LlvmObject* llvmObj) {
    auto arg = arguments[0]->getReferenceLlvm(llvmObj);
    vector<llvm::Value*> indexList;
    if (arguments[0]->type->kind == Type::Kind::StaticArray) {
        if (((StaticArrayType*)arguments[0]->type)->sizeAsInt != -1) {
            indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
        }
    }
    indexList.push_back(index->createLlvm(llvmObj));
    return llvm::GetElementPtrInst::Create(
        ((llvm::PointerType*)arg->getType())->getElementType(),
        arg,
        indexList,
        "",
        llvmObj->block
    );
}
llvm::Value* ArrayIndexOperation::createLlvm(LlvmObject* llvmObj) {
    return new llvm::LoadInst(getReferenceLlvm(llvmObj), "", llvmObj->block);
}


/*
    ArraySubArrayOperation
*/
ArraySubArrayOperation::ArraySubArrayOperation(const CodePosition& position, Value* firstIndex, Value* secondIndex) : 
    Operation(position, Operation::Kind::ArraySubArray),
    firstIndex(firstIndex),
    secondIndex(secondIndex)
{}
vector<unique_ptr<ArraySubArrayOperation>> ArraySubArrayOperation::objects;
ArraySubArrayOperation* ArraySubArrayOperation::Create(const CodePosition& position, Value* firstIndex, Value* secondIndex) {
    objects.emplace_back(make_unique<ArraySubArrayOperation>(position, firstIndex, secondIndex));
    return objects.back().get();
}
optional<Value*> ArraySubArrayOperation::interpret(Scope* scope) {
    return nullptr;
}
bool ArraySubArrayOperation::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const ArraySubArrayOperation&>(value);
        return cmpPtr(this->firstIndex, other.firstIndex)
            && cmpPtr(this->secondIndex, other.secondIndex)
            && Operation::operator==(other);
    }
    else {
        return false;
    }
}
/*unique_ptr<Value> ArraySubArrayOperation::copy() {
    auto value = make_unique<ArraySubArrayOperation>(position, firstIndex->copy(), secondIndex->copy());
    value->type = type->copy();
    value->isConstexpr = isConstexpr;
    for (auto& argument : arguments) {
        value->arguments.push_back(argument->copy());
    }
    return value;
}*/


/*
    FunctionCallOperation
*/
FunctionCallOperation::FunctionCallOperation(const CodePosition& position) : 
    Operation(position, Operation::Kind::FunctionCall)
{}
vector<unique_ptr<FunctionCallOperation>> FunctionCallOperation::objects;
FunctionCallOperation* FunctionCallOperation::Create(const CodePosition& position) {
    objects.emplace_back(make_unique<FunctionCallOperation>(position));
    return objects.back().get();
}
FunctionCallOperation::FindFunctionStatus FunctionCallOperation::findFunction(Scope* scope, Scope* searchScope, string functionName) {
    const auto& declarations = searchScope->declarationMap.getDeclarations(functionName);
    vector<Declaration*> viableDeclarations;
    vector<Declaration*> perfectMatches;
    for (const auto declaration : declarations) {
        auto functionType = (FunctionType*)declaration->variable->type;
        if (functionType && functionType->kind == Type::Kind::TemplateFunction) {
            continue;
        }
        if (functionType && functionType->argumentTypes.size() == arguments.size()) {
            bool allMatch = true;
            for (int i = 0; i < functionType->argumentTypes.size(); ++i) {
                if (functionType->argumentTypes[i]->kind == Type::Kind::Reference && !isLvalue(arguments[i])) {
                    allMatch = false;
                }
                if (!cmpPtr(functionType->argumentTypes[i]->getEffectiveType(), arguments[i]->type->getEffectiveType())) {
                    allMatch = false;
                }
            }
            if (allMatch) {
                perfectMatches.push_back(declaration);
            } else {
                viableDeclarations.push_back(declaration);
            }
        }
    }
    if (perfectMatches.size() == 1) {
        function = perfectMatches.back()->value;
        idName = searchScope->declarationMap.getIdName(perfectMatches.back());
        type = ((FunctionType*)perfectMatches.back()->variable->type)->returnType;
    } else if (perfectMatches.size() > 1) {
        string message = "ambogous function call. ";
        message += "Possible functions at lines: ";
        for (int i = 0; i < perfectMatches.size(); ++i) {
            message += to_string(perfectMatches[i]->position.lineNumber);
            if (i != perfectMatches.size() - 1) {
                message += ", ";
            }
        }
        errorMessageBool(message, position);
        return FindFunctionStatus::Error;
    } else {
        vector<optional<vector<CastOperation*>>> neededCasts;
        for (Declaration* declaration : viableDeclarations) {
            neededCasts.push_back(vector<CastOperation*>());
            auto argumentTypes = ((FunctionType*)declaration->variable->type)->argumentTypes;
            for (int i = 0; i < argumentTypes.size(); ++i) {
                if (!cmpPtr(argumentTypes[i], arguments[i]->type)) {
                    auto cast = CastOperation::Create(arguments[i]->position, argumentTypes[i]);
                    cast->arguments.push_back(arguments[i]);
                    auto castInterpret = cast->interpret(scope, true);
                    if (castInterpret) {
                        neededCasts.back().value().push_back(cast);
                    } else {
                        neededCasts.back() = nullopt;
                        break;
                    }
                } else {
                    neededCasts.back().value().push_back(nullptr);
                }
            }
        }

        int matchId = -1;
        vector<Declaration*> possibleDeclarations;
        for (int i = 0; i < neededCasts.size(); ++i) {
            if (neededCasts[i]) {
                matchId = i;
                possibleDeclarations.push_back(viableDeclarations[i]);
            }
        }

        if (matchId == -1) {
            return FindFunctionStatus::Fail;
        } 
        if (possibleDeclarations.size() > 1) {
            string message = "ambogous function call. ";
            message += "Possible functions at lines: ";
            for (int i = 0; i < possibleDeclarations.size(); ++i) {
                message += to_string(possibleDeclarations[i]->position.lineNumber);
                if (i != possibleDeclarations.size() - 1) {
                    message += ", ";
                }
            }
            errorMessageBool(message, position);
            return FindFunctionStatus::Error;
        }

        for (int i = 0; i < arguments.size(); ++i) {
            CastOperation* cast = neededCasts[matchId].value()[i];
            if (cast) {
                auto castInterpret = cast->interpret(scope);
                if (castInterpret.value()) {
                    arguments[i] = castInterpret.value();
                } else {
                    arguments[i] = cast;
                }
            }
        }
        function = viableDeclarations[matchId]->value;
        type = ((FunctionType*)possibleDeclarations[matchId]->variable->type)->returnType;
        idName = searchScope->declarationMap.getIdName(viableDeclarations[matchId]);
    }
    return FindFunctionStatus::Success;
    /*int insertIndex = viableDeclarations.size()-1;
    for (const auto declaration : declarations) {
    auto functionType = (TemplateFunctionType*)declaration->variable->type;
    if (functionType && functionType->argumentTypes.size() == arguments.size()) {
    //bool hasDecoratedType = false;

    viableDeclarations.push_back(declaration);
    }
    }*/
}
optional<Value*> FunctionCallOperation::interpret(Scope* scope) {
    if (wasInterpreted) {
        return nullptr;
    }
    wasInterpreted = true;
    if (!interpretAllArguments(scope)) {
        return nullopt;
    }

    // first check if is it class constructor
    if (function->valueKind == Value::ValueKind::Variable) {
        auto [constructor, classDeclaration] = findClassConstructor(position, scope, ((Variable*)function)->name, arguments);
        if (classDeclaration) {
            if (constructor) {
                auto op = ConstructorOperation::Create(position, constructor, classDeclaration, arguments);
                op->interpret(scope);
                return op;
            } else {
                return nullopt;
            }
        }
    }

    auto functionInterpret = function->interpret(scope);
    if (!functionInterpret) return nullopt;
    if (functionInterpret.value()) function = functionInterpret.value();
    if (function->valueKind == Value::ValueKind::Variable && function->isConstexpr) {
        string functionName = ((Variable*)function)->name;
        FindFunctionStatus status;
        Scope* actualScope = scope;
        do {
            status = findFunction(scope, actualScope, functionName);
            if (status == FindFunctionStatus::Error) {
                return nullopt;
            }
            actualScope = actualScope->parentScope;
        } while (status != FindFunctionStatus::Success && actualScope);
        if (status != FindFunctionStatus::Success) {
            return errorMessageOpt("no fitting function to call", position);
        }
    } else if (function->valueKind == Value::ValueKind::Operation
        && (((Operation*)function)->kind == Operation::Kind::Dot)
        && ((Variable*)((Operation*)function)->arguments[1])->isConstexpr) {
        auto dotOperation = (Operation*)function;
        auto var = (Variable*)dotOperation->arguments[1];
        auto arg0Type = dotOperation->arguments[0]->type;
        ClassScope* classScope = nullptr;

        if (arg0Type->kind == Type::Kind::Class) {
            classScope = ((ClassType*)arg0Type)->declaration->body;
            auto thisArgument = Operation::Create(position, Operation::Kind::Address);
            thisArgument->arguments.push_back(dotOperation->arguments[0]);
            if (!thisArgument->interpret(scope)) {
                return nullopt;
            }
            arguments.push_back(thisArgument);
        } else {
            classScope = ((ClassType*)((RawPointerType*)arg0Type)->underlyingType)->declaration->body;
            arguments.push_back(dotOperation->arguments[0]);
        }

        switch (findFunction(scope, classScope, var->name)) {
        case FindFunctionStatus::Error:   return nullopt;
        case FindFunctionStatus::Fail:    return errorMessageOpt("no fitting class function to call", position);
        case FindFunctionStatus::Success: break;
        }
    }
    else if (function->type->kind == Type::Kind::Function) {
        auto functionType = (FunctionType*)function->type;
        if (functionType->argumentTypes.size() != arguments.size()) {
            return errorMessageOpt("expected " + to_string(functionType->argumentTypes.size()) 
                + " arguments, got "+ to_string(arguments.size()), position
            );
        }
        for (int i = 0; i < functionType->argumentTypes.size(); ++i) {
            if (!cmpPtr(functionType->argumentTypes[i], arguments[i]->type)) {
                auto cast = CastOperation::Create(position, functionType->argumentTypes[i]);
                cast->arguments.push_back(arguments[i]);
                auto castInterpret = cast->interpret(scope);
                if (!castInterpret) return nullopt;
                if (castInterpret.value()) arguments[i] = castInterpret.value();
                else arguments[i] = cast;
            }
        }

        type = functionType->returnType;
    } else {
        return errorMessageOpt("function call on non function value", position);
    }

    if (type->needsDestruction()) {
        scope->valuesToDestroyBuffer.push_back(this);
    }
    return nullptr;
}
bool FunctionCallOperation::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const FunctionCallOperation&>(value);
        return cmpPtr(this->function, other.function)
            && this->idName == other.idName
            && Operation::operator==(other);
    }
    else {
        return false;
    }
}
llvm::Value* FunctionCallOperation::createLlvm(LlvmObject* llvmObj) {
    if (((FunctionType*)function->type)->returnType->kind == Type::Kind::Reference) {
        return new llvm::LoadInst(getReferenceLlvm(llvmObj), "", llvmObj->block);
    } else {
        return getReferenceLlvm(llvmObj);
    }
}
llvm::Value* FunctionCallOperation::getReferenceLlvm(LlvmObject* llvmObj) {
    vector<llvm::Value*> args;
    auto functionType = (FunctionType*)function->type;
    for (int i = 0; i < arguments.size(); ++i) {
        if (functionType->argumentTypes[i]->kind == Type::Kind::Reference) {
            args.push_back(arguments[i]->getReferenceLlvm(llvmObj));
        } else {
            args.push_back(arguments[i]->createLlvm(llvmObj));
        }
    }
    llvmValue = llvm::CallInst::Create(function->createLlvm(llvmObj), args, "", llvmObj->block);
    return llvmValue;
}
void FunctionCallOperation::createDestructorLlvm(LlvmObject* llvmObj) {
    type->createDestructorLlvm(llvmObj, llvmValue);
}

/*unique_ptr<Value> FunctionCallOperation::copy() {
    auto value = make_unique<FunctionCallOperation>(position);
    value->type = type->copy();
    value->isConstexpr = isConstexpr;
    for (auto& argument : arguments) {
        value->arguments.push_back(argument->copy());
    }
    auto functionCopy = function.copy();
    value->function = move(*(Variable*)functionCopy.get());
    return value;
}*/


/*
    TemplateFunctionCallOperation
*/
TemplateFunctionCallOperation::TemplateFunctionCallOperation(const CodePosition& position) : 
    FunctionCallOperation(position)
{
    kind = Operation::Kind::TemplateFunctionCall;
}
vector<unique_ptr<TemplateFunctionCallOperation>> TemplateFunctionCallOperation::objects;
TemplateFunctionCallOperation* TemplateFunctionCallOperation::Create(const CodePosition& position) {
    objects.emplace_back(make_unique<TemplateFunctionCallOperation>(position));
    return objects.back().get();
}
optional<Value*> TemplateFunctionCallOperation::interpret(Scope* scope) {
    return nullptr;
}
bool TemplateFunctionCallOperation::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const TemplateFunctionCallOperation&>(value);
        return this->templateTypes == other.templateTypes
            && FunctionCallOperation::operator==(other);
    }
    else {
        return false;
    }
}
/*unique_ptr<Value> TemplateFunctionCallOperation::copy() {
    auto value = make_unique<TemplateFunctionCallOperation>(position);
    value->type = type->copy();
    value->isConstexpr = isConstexpr;
    for (auto& argument : arguments) {
        value->arguments.push_back(argument->copy());
    }
    auto functionCopy = function.copy();
    value->function = move(*(Variable*)functionCopy.get());
    for (auto& templateType : templateTypes) {
        value->templateTypes.push_back(templateType->copy());
    }
    return value;
}*/


/*
    FlowOperation
*/
FlowOperation::FlowOperation(const CodePosition& position, Kind kind) : 
    Operation(position, kind)
{}
vector<unique_ptr<FlowOperation>> FlowOperation::objects;
FlowOperation* FlowOperation::Create(const CodePosition& position, Kind kind) {
    objects.emplace_back(make_unique<FlowOperation>(position, kind));
    return objects.back().get();
}
optional<Value*> FlowOperation::interpret(Scope* scope) {
    if (wasInterpreted) {
        return nullptr;
    }
    if (!interpretAllArguments(scope)) {
        return nullopt;
    }

    switch (kind) {
    case Kind::Remove: {
        type = Type::Create(Type::Kind::Void);
        if (arguments.size() == 0 || arguments[0]->valueKind != Value::ValueKind::Variable) {
            return errorMessageOpt("remove for-loop statement requires variable name", position);
        }
        string varName = ((Variable*)arguments[0])->name;
        scopePtr = scope;
        while (scopePtr) {
            for (int i = scopePtr->declarationsOrder.size() - 1; i >= 0; --i) {
                auto declaration = scopePtr->declarationsOrder[i];
                if (declaration->variable->type->kind == Type::Kind::Class
                    || declaration->variable->type->kind == Type::Kind::OwnerPointer) 
                {
                    if (scope->declarationsInitState.at(declaration)) {
                        variablesToDestroy.push_back(declaration);
                        if (scope->maybeUninitializedDeclarations.find(declaration) !=
                            scope->maybeUninitializedDeclarations.end()) {
                            warningMessage("destruction of maybe uninitialized variable " + declaration->variable->name + " on remove statement ", position);
                        }
                    }
                }
            }
            if (scopePtr->owner == Scope::Owner::For) {
                auto forScope = (ForScope*)scopePtr;
                if (holds_alternative<ForEachData>(forScope->data)) {
                    auto& forIterData = get<ForEachData>(forScope->data);
                    if (forIterData.it->name == varName) {
                        break;
                    } else {
                        scopePtr = scopePtr->parentScope;
                    }
                } else {
                    scopePtr = scopePtr->parentScope;
                }
            }
            else {
                scopePtr = scopePtr->parentScope;
            }
        }
        if (!scopePtr) {
            return errorMessageOpt("cannot tie remove statment with any loop", position);
        }
        break;
    }
    case Kind::Continue: {
        type = Type::Create(Type::Kind::Void);
        scopePtr = scope;
        if (arguments.size() == 1 && arguments[0]->valueKind != Value::ValueKind::Variable) {
            return errorMessageOpt("continue loop statement needs no value or variable name", position);
        }
        while (scopePtr) {
            for (int i = scopePtr->declarationsOrder.size() - 1; i >= 0; --i) {
                auto declaration = scopePtr->declarationsOrder[i];
                if (declaration->variable->type->kind == Type::Kind::Class
                    || declaration->variable->type->kind == Type::Kind::OwnerPointer) 
                {
                    if (scope->declarationsInitState.at(declaration)) {
                        variablesToDestroy.push_back(declaration);
                        if (scope->maybeUninitializedDeclarations.find(declaration) !=
                            scope->maybeUninitializedDeclarations.end()) {
                            warningMessage("destruction of maybe uninitialized variable " + declaration->variable->name + " on continue statement ", position);
                        }
                    }
                }
            }
            if (arguments.size() == 0 && scopePtr->owner == Scope::Owner::While) {
                break;
            } 
            else if (scopePtr->owner == Scope::Owner::For) {
                auto forScope = (ForScope*)scopePtr;
                if (arguments.size() == 0) {
                    break;
                } else {
                    string varName = ((Variable*)arguments[0])->name;
                    if (holds_alternative<ForIterData>(forScope->data)) {
                        auto& forIterData = get<ForIterData>(forScope->data);
                        if (forIterData.iterVariable->name == varName) {
                            break;
                        } else {
                            scopePtr = scopePtr->parentScope;
                        }
                    } else if (holds_alternative<ForEachData>(forScope->data)) {
                        auto& forIterData = get<ForEachData>(forScope->data);
                        if (forIterData.it->name == varName) {
                            break;
                        } else {
                            scopePtr = scopePtr->parentScope;
                        }
                    }
                }
            }
            else {
                scopePtr = scopePtr->parentScope;
            }
        }
        if (!scopePtr) {
            return errorMessageOpt("cannot tie continue statment with any loop", position);
        }
        break;
    }
    case Kind::Break: {
        type = Type::Create(Type::Kind::Void);
        scopePtr = scope;
        if (arguments.size() == 1 && arguments[0]->valueKind != Value::ValueKind::Variable) {
            return errorMessageOpt("break loop statement needs no value or variable name", position);
        }
        while (scopePtr) {
            for (int i = scopePtr->declarationsOrder.size() - 1; i >= 0; --i) {
                auto declaration = scopePtr->declarationsOrder[i];
                if (declaration->variable->type->kind == Type::Kind::Class
                    || declaration->variable->type->kind == Type::Kind::OwnerPointer) 
                {
                    if (scope->declarationsInitState.at(declaration)) {
                        variablesToDestroy.push_back(declaration);
                        if (scope->maybeUninitializedDeclarations.find(declaration) !=
                            scope->maybeUninitializedDeclarations.end()) {
                            warningMessage("destruction of maybe uninitialized variable " + declaration->variable->name + " on break statement ", position);
                        }
                    }
                }
            }
            if (arguments.size() == 0 && scopePtr->owner == Scope::Owner::While) {
                break;
            } 
            else if (scopePtr->owner == Scope::Owner::For) {
                auto forScope = (ForScope*)scopePtr;
                if (arguments.size() == 0) {
                    break;
                } else {
                    string varName = ((Variable*)arguments[0])->name;
                    if (holds_alternative<ForIterData>(forScope->data)) {
                        auto& forIterData = get<ForIterData>(forScope->data);
                        if (forIterData.iterVariable->name == varName) {
                            break;
                        } else {
                            scopePtr = scopePtr->parentScope;
                        }
                    } else if (holds_alternative<ForEachData>(forScope->data)) {
                        auto& forIterData = get<ForEachData>(forScope->data);
                        if (forIterData.it->name == varName) {
                            break;
                        } else {
                            scopePtr = scopePtr->parentScope;
                        }
                    }
                }
            }
            else {
                scopePtr = scopePtr->parentScope;
            }
        }
        if (!scopePtr) {
            return errorMessageOpt("cannot tie break statment with any loop", position);
        }
        break;
    }
    case Kind::Return: {
        scope->hasReturnStatement = true;
        type = Type::Create(Type::Kind::Void);
        scopePtr = scope;
        while (scopePtr) {
            for (int i = scopePtr->declarationsOrder.size() - 1; i >= 0; --i) {
                auto declaration = scopePtr->declarationsOrder[i];
                if (declaration->variable->type->kind == Type::Kind::Class
                    || declaration->variable->type->kind == Type::Kind::OwnerPointer) 
                {
                    if (scope->declarationsInitState.at(declaration)) {
                        variablesToDestroy.push_back(declaration);
                        if (scope->maybeUninitializedDeclarations.find(declaration) !=
                            scope->maybeUninitializedDeclarations.end()) {
                            warningMessage("destruction of maybe uninitialized variable " + declaration->variable->name + " on return statement ", position);
                        }
                    }
                }
            }
            if (scopePtr->owner == Scope::Owner::Function) {
                auto functionValue = ((FunctionScope*)scopePtr)->function;
                auto returnType = ((FunctionType*)functionValue->type)->returnType;

                if (arguments.size() == 0) {
                    if (returnType->kind != Type::Kind::Void) {
                        return errorMessageOpt("expected return value of type " + DeclarationMap::toString(returnType)
                            + " got nothing", position);
                    } 
                } else {
                    CastOperation* cast = CastOperation::Create(position, returnType);
                    cast->arguments.push_back(arguments[0]);
                    arguments[0] = cast;
                    auto castInterpret = arguments[0]->interpret(scope);
                    if (!castInterpret) return nullopt;
                    if (castInterpret.value()) arguments[0] = castInterpret.value();
                    arguments[0]->type = returnType;
                }
                break;
            } else {
                scopePtr = scopePtr->parentScope;
            }
        }
        if (!scopePtr) {
            internalError("return statement found outside function", position);
        }
        if (!arguments.empty() && arguments[0]->type->needsDestruction()) {
            if (scope->valuesToDestroyBuffer.empty()) {
                internalError("return statement captures destructable value, but no value in the scope buffer", position);
            }
            scope->valuesToDestroyBuffer.pop_back();
        }
        break;
    }
    }

    return nullptr;
}
bool FlowOperation::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const FlowOperation&>(value);
        return this->scopePtr == other.scopePtr
            && Operation::operator==(other);
    }
    else {
        return false;
    }
}
llvm::Value* FlowOperation::createLlvm(LlvmObject* llvmObj) {
    if (kind == Kind::Return) {
        if (arguments.size() == 0) {
            return llvm::ReturnInst::Create(llvmObj->context, (llvm::Value*)nullptr, llvmObj->block);
        }
        else {
            if (arguments[0]->type->kind == Type::Kind::Reference) {
                return llvm::ReturnInst::Create(llvmObj->context, arguments[0]->getReferenceLlvm(llvmObj), llvmObj->block);
            } else {
                return llvm::ReturnInst::Create(llvmObj->context, arguments[0]->createLlvm(llvmObj), llvmObj->block);
            }
        }
    }
    return nullptr;
}


ErrorResolveOperation::ErrorResolveOperation(const CodePosition& position) : 
    Operation(position, Operation::Kind::ErrorResolve)
{
    containsErrorResolve = true;
}
vector<unique_ptr<ErrorResolveOperation>> ErrorResolveOperation::objects;
ErrorResolveOperation* ErrorResolveOperation::Create(const CodePosition& position) {
    objects.emplace_back(make_unique<ErrorResolveOperation>(position));
    return objects.back().get();
}
optional<Value*> ErrorResolveOperation::interpret(Scope* scope) {
    if (wasInterpreted) {
        return nullptr;
    }
    wasInterpreted = true;

    if (!interpretAllArguments(scope)) {
        return nullopt;
    }

    if (arguments[0]->type->kind != Type::Kind::MaybeError) {
        return errorMessageOpt("error-resolve operation can only be used on '?' (maybe error) types. got "
            + DeclarationMap::toString(arguments[0]->type), position);
    }
    type = ((MaybeErrorType*)arguments[0]->type)->underlyingType;

    if (onErrorScope) {
        scope->onErrorScopeToInterpret = onErrorScope;
    }
    if (onSuccessScope) {
        scope->onSuccessScopeToInterpret = onSuccessScope;
    }

    return nullptr;
}
bool ErrorResolveOperation::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const ErrorResolveOperation&>(value);
        return this->onErrorScope == other.onErrorScope
            && this->onSuccessScope == other.onSuccessScope
            && Operation::operator==(other);
    }
    else {
        return false;
    }
}
llvm::Value* ErrorResolveOperation::createLlvm(LlvmObject* llvmObj) {
    return nullptr;
}


SizeofOperation::SizeofOperation(const CodePosition& position) : 
    Operation(position, Operation::Kind::Sizeof)
{}
vector<unique_ptr<SizeofOperation>> SizeofOperation::objects;
SizeofOperation* SizeofOperation::Create(const CodePosition& position) {
    objects.emplace_back(make_unique<SizeofOperation>(position));
    return objects.back().get();
}
optional<Value*> SizeofOperation::interpret(Scope* scope) {
    if (wasInterpreted) {
        return nullptr;
    }
    wasInterpreted = true;
    type = IntegerType::Create(IntegerType::Size::I64);
    if (!argType->interpret(scope)) {
        return errorMessageOpt("non-type expression given to sizeof operation", position);
    }
    return argType->typesize(scope);
}
bool SizeofOperation::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const SizeofOperation&>(value);
        return cmpPtr(this->argType, other.argType)
            && Operation::operator==(other);
    }
    else {
        return false;
    }
}



ConstructorOperation::ConstructorOperation(const CodePosition& position, FunctionValue* constructor, ClassDeclaration* classDeclaration, vector<Value*> arguments) : 
    Operation(position, Operation::Kind::Constructor),
    constructor(constructor),
    classDeclaration(classDeclaration)
{
    this->arguments = arguments;
    auto thisType = ClassType::Create(classDeclaration->name);
    thisType->declaration = classDeclaration;
    type = thisType;
}
vector<unique_ptr<ConstructorOperation>> ConstructorOperation::objects;
ConstructorOperation* ConstructorOperation::Create(const CodePosition& position, FunctionValue* constructor, ClassDeclaration* classDeclaration, vector<Value*> arguments) {
    objects.emplace_back(make_unique<ConstructorOperation>(position, constructor, classDeclaration, arguments));
    return objects.back().get();
}
optional<Value*> ConstructorOperation::interpret(Scope* scope) {
    if (type->needsDestruction()) {
        scope->valuesToDestroyBuffer.push_back(this);
    }
    return nullptr;
}
bool ConstructorOperation::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const ConstructorOperation&>(value);
        return this->constructor == other.constructor
            && Operation::operator==(other);
    }
    else {
        return false;
    }
}
llvm::Value* ConstructorOperation::getReferenceLlvm(LlvmObject* llvmObj) {
    llvmValue = type->allocaLlvm(llvmObj);

    vector<llvm::Value*> args;
    auto functionType = (FunctionType*)constructor->type;
    for (int i = 0; i < arguments.size(); ++i) {
        if (functionType->argumentTypes[i]->kind == Type::Kind::Reference) {
            args.push_back(arguments[i]->getReferenceLlvm(llvmObj));
        } else {
            args.push_back(arguments[i]->createLlvm(llvmObj));
        }
    }
    args.push_back(llvmValue);

    llvm::CallInst::Create(constructor->createLlvm(llvmObj), args, "", llvmObj->block);

    return llvmValue;
}
llvm::Value* ConstructorOperation::createLlvm(LlvmObject* llvmObj) {
    return new llvm::LoadInst(getReferenceLlvm(llvmObj), "", llvmObj->block);
}
void ConstructorOperation::createDestructorLlvm(LlvmObject* llvmObj) {
    type->createDestructorLlvm(llvmObj, llvmValue);
}


AllocationOperation::AllocationOperation(const CodePosition& position) : 
    Operation(position, Operation::Kind::Allocation)
{}
vector<unique_ptr<AllocationOperation>> AllocationOperation::objects;
AllocationOperation* AllocationOperation::Create(const CodePosition& position) {
    objects.emplace_back(make_unique<AllocationOperation>(position));
    return objects.back().get();
}
optional<Value*> AllocationOperation::interpret(Scope* scope) {
    if (!type->interpret(scope, false)) {
        return nullopt;
    }
    auto underlyingType = ((OwnerPointerType*)type)->underlyingType;
    typesize = underlyingType->typesize(scope);

    if (underlyingType->kind == Type::Kind::Class) {
        auto [constructor, classDeclaration] = findClassConstructor(position, scope, ((ClassType*)underlyingType)->name, arguments);
        this->constructor = constructor;
        this->classDeclaration = classDeclaration;
        if (!constructor) {
            return nullopt;
        }
    }
    
    if (type->needsDestruction()) {
        scope->valuesToDestroyBuffer.push_back(this);
    }
    return nullptr;
}
bool AllocationOperation::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const AllocationOperation&>(value);
        return this->typesize == other.typesize
            && Operation::operator==(other);
    }
    else {
        return false;
    }
}
llvm::Value* AllocationOperation::getReferenceLlvm(LlvmObject* llvmObj) {
    return nullptr;
}
llvm::Value* AllocationOperation::createLlvm(LlvmObject* llvmObj) {
    llvmValue = new llvm::BitCastInst(
        llvm::CallInst::Create(llvmObj->mallocFunction, typesize->createLlvm(llvmObj), "", llvmObj->block), 
        type->createLlvm(llvmObj), 
        "", 
        llvmObj->block
    );

    if (constructor) {
        vector<llvm::Value*> args;
        auto functionType = (FunctionType*)constructor->type;
        for (int i = 0; i < arguments.size(); ++i) {
            if (functionType->argumentTypes[i]->kind == Type::Kind::Reference) {
                args.push_back(arguments[i]->getReferenceLlvm(llvmObj));
            } else {
                args.push_back(arguments[i]->createLlvm(llvmObj));
            }
        }
        args.push_back(llvmValue);

        llvm::CallInst::Create(constructor->createLlvm(llvmObj), args, "", llvmObj->block);
    }

    return llvmValue;
}
void AllocationOperation::createDestructorLlvm(LlvmObject* llvmObj) {
    type->createDestructorLlvm(llvmObj, llvmValue);
}