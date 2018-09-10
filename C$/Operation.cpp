#include "Operation.h"
#include "Declaration.h"
#include "ClassDeclaration.h"

using namespace std;

std::pair<FunctionValue*, ClassDeclaration*> findClassConstructor(const CodePosition& position, Scope* scope, const string& name, vector<Value*>& arguments) {
    ClassDeclaration* classDeclaration = nullptr;
    Scope* searchScope = scope; 

    classDeclaration = searchScope->classDeclarationMap.getDeclaration(name);
    while (searchScope->parentScope && !classDeclaration) {
        searchScope = searchScope->parentScope;
        classDeclaration = searchScope->classDeclarationMap.getDeclaration(name);
    }
    if (classDeclaration) {
        if (!classDeclaration->interpret()) {
            return {nullptr, classDeclaration};
        }
        auto classType = ClassType::Create(classDeclaration->name);
        classType->interpret(searchScope, false);

        if (classDeclaration->body->constructors.empty()) {
            if (arguments.size() == 0) {
                return {classDeclaration->body->inlineConstructors, classDeclaration};
            } 
            else if (arguments.size() == 1 && cmpPtr(arguments[0]->type->getEffectiveType(), (Type*)classType)) {
                return {classDeclaration->body->copyConstructor, classDeclaration};
            }
            else {
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
                vector<optional<vector<ConstructorOperation*>>> neededCtors;
                for (auto function : viableDeclarations) {
                    neededCtors.push_back(vector<ConstructorOperation*>());
                    auto argumentTypes = ((FunctionType*)function->type)->argumentTypes;
                    for (int i = 0; i < argumentTypes.size(); ++i) {
                        if (!cmpPtr(argumentTypes[i], arguments[i]->type)) {
                            auto ctor = ConstructorOperation::Create(arguments[i]->position, argumentTypes[i], {arguments[i]});
                            auto ctorInterpret = ctor->interpret(scope, true);
                            if (ctorInterpret) {
                                neededCtors.back().value().push_back(ctor);
                            } else {
                                neededCtors.back() = nullopt;
                                break;
                            }
                        } else {
                            neededCtors.back().value().push_back(nullptr);
                        }
                    }
                }

                int matchId = -1;
                vector<FunctionValue*> possibleFunctions;
                for (int i = 0; i < neededCtors.size(); ++i) {
                    if (neededCtors[i]) {
                        matchId = i;
                        possibleFunctions.push_back(viableDeclarations[i]);
                    }
                }

                if (matchId == -1) {
                    if (arguments.size() == 1 && cmpPtr(arguments[0]->type->getEffectiveType(), (Type*)classType)) {
                        return {classDeclaration->body->copyConstructor, classDeclaration};
                    } else {
                        errorMessageBool("no fitting constructor to call", position);
                        return {nullptr, classDeclaration};
                    }
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
                    auto ctor = neededCtors[matchId].value()[i];
                    if (ctor) {
                        auto ctorInterpret = ctor->interpret(scope);
                        if (ctorInterpret.value()) {
                            arguments[i] = ctorInterpret.value();
                        } else {
                            arguments[i] = ctor;
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
    auto assign = AssignOperation::Create(position);
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
    
    if (kind != Kind::Dot && !interpretAllArguments(scope)) {
        return nullopt;
    }

    Type* effectiveType1 = nullptr;
    Type* effectiveType2 = nullptr;

    if (kind != Kind::Dot) {
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
            if ((((RawPointerType*)effectiveType1)->underlyingType)->kind == Type::Kind::Class) {
                classType = (ClassType*)((RawPointerType*)effectiveType1)->underlyingType;
            }    
            break;
        case Type::Kind::OwnerPointer:
            if ((((OwnerPointerType*)effectiveType1)->underlyingType)->kind == Type::Kind::Class) {
                classType = (ClassType*)((OwnerPointerType*)effectiveType1)->underlyingType;
            }
            break;
        case Type::Kind::MaybeError:
            if ((((MaybeErrorType*)effectiveType1)->underlyingType)->kind == Type::Kind::Class) {
                classType = (ClassType*)((MaybeErrorType*)effectiveType1)->underlyingType;
            }
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
        Value* bool1 = ConstructorOperation::Create(position, Type::Create(Type::Kind::Bool), {arguments[0]});
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
        Value* bool1 = ConstructorOperation::Create(position, Type::Create(Type::Kind::Bool), {arguments[0]});
        auto bool1Interpret = bool1->interpret(scope);
        if (!bool1Interpret) return nullopt;
        if (bool1Interpret.value()) bool1 = bool1Interpret.value();

        Value* bool2 = ConstructorOperation::Create(position, Type::Create(Type::Kind::Bool), {arguments[1]});
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
        Value* bool1 = ConstructorOperation::Create(position, Type::Create(Type::Kind::Bool), {arguments[0]});
        auto bool1Interpret = bool1->interpret(scope);
        if (!bool1Interpret) return nullopt;
        if (bool1Interpret.value()) bool1 = bool1Interpret.value();

        Value* bool2 = ConstructorOperation::Create(position, Type::Create(Type::Kind::Bool), {arguments[1]});
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
        case Type::Kind::MaybeError:
            classType = (ClassType*)((MaybeErrorType*)effectiveType0)->underlyingType;
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
        } else if (effectiveType0->kind == Type::Kind::MaybeError) {
            arg0 = ((MaybeErrorType*)effectiveType0)->llvmGepValue(llvmObj, arguments[0]->getReferenceLlvm(llvmObj));
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
        case Type::Kind::MaybeError: {
            auto arg = arguments[0]->getReferenceLlvm(llvmObj);
            vector<llvm::Value*> indexList;
            indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
            indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvmObj->context), 0));
            auto gep = llvm::GetElementPtrInst::Create(
                ((llvm::PointerType*)arg->getType())->getElementType(), arg, indexList, "", llvmObj->block
            );
            return gep;
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
            return new llvm::LoadInst(getReferenceLlvm(llvmObj), "", llvmObj->block);
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
        arguments[0]->type->createDestructorLlvm(llvmObj, arguments[0]->getReferenceLlvm(llvmObj));
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
    if (wasInterpreted) {
        return nullptr;
    }
    wasInterpreted = true;

    if (!interpretAllArguments(scope)) {
        return nullopt;
    }
    if (!type->interpret(scope) || !argType->interpret(scope)) {
        errorMessageBool("cannot cast to unknown type " + DeclarationMap::toString(type), position);
        return nullopt;
    }
    if (arguments[0]->valueKind == Value::ValueKind::Operation) {
        containsErrorResolve = ((Operation*)arguments[0])->containsErrorResolve;
    }

    auto effectiveType = arguments[0]->type->getEffectiveType();

    if (cmpPtr(type, effectiveType)) {
        return arguments[0];
    } 
    else if (type->sizeInBytes() == effectiveType->sizeInBytes()) {
        auto k1 = type->kind;
        auto k2 = effectiveType->kind;
        if (isLvalue(arguments[0])) {
            argIsLValue = true;
            return nullptr;
        } else if ((k1 == Type::Kind::RawPointer || k1 == Type::Kind::OwnerPointer 
            || k1 == Type::Kind::Bool || k1 == Type::Kind::Integer || k1 == Type::Kind::Float) 
            && 
            (k2 == Type::Kind::RawPointer || k2 == Type::Kind::OwnerPointer 
            || k2 == Type::Kind::Bool || k2 == Type::Kind::Integer || k2 == Type::Kind::Float)) 
        {
            return nullptr;
        } else {
            return errorMessageOpt("cannot cast " + 
                DeclarationMap::toString(arguments[0]->type) + 
                " to " + DeclarationMap::toString(type) + 
                " (this cast requires l-value argument)", position
            );
        }
    } else {
        return errorMessageOpt("cannot cast " + 
            DeclarationMap::toString(arguments[0]->type) + 
            " to " + DeclarationMap::toString(type) + 
            " (sizeof(" + DeclarationMap::toString(arguments[0]->type) + ") !=" +
            " sizeof(" + DeclarationMap::toString(type) + "))", position
        );
    }
    
    
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
    if (argIsLValue) {
        return new llvm::LoadInst(getReferenceLlvm(llvmObj), "", llvmObj->block);
    }
    auto effType = arguments[0]->type->getEffectiveType();
    auto k1 = type->kind;
    auto k2 = effType->kind;
    if (k1 == Type::Kind::RawPointer || k1 == Type::Kind::OwnerPointer) {
        if (k2 == Type::Kind::RawPointer || k2 == Type::Kind::OwnerPointer) {
            return new llvm::BitCastInst(arguments[0]->createLlvm(llvmObj), type->createLlvm(llvmObj), "", llvmObj->block);
        } else if (k2 == Type::Kind::Bool || k2 == Type::Kind::Integer) {
            return new llvm::IntToPtrInst(arguments[0]->createLlvm(llvmObj), type->createLlvm(llvmObj), "", llvmObj->block);
        } else if (k2 == Type::Kind::Float) {
            llvm::Value* floatToInt = new llvm::BitCastInst(arguments[0]->createLlvm(llvmObj), IntegerType::Create(IntegerType::Size::I64)->createLlvm(llvmObj), "", llvmObj->block);
            return new llvm::IntToPtrInst(floatToInt, type->createLlvm(llvmObj), "", llvmObj->block);
        } else {
            internalError("unexpected type during cast llvm creating", arguments[0]->position);
            return nullptr;
        }
    } else if (k1 == Type::Kind::Bool || k1 == Type::Kind::Integer || k1 == Type::Kind::Float) {
        if (k2 == Type::Kind::RawPointer || k2 == Type::Kind::OwnerPointer) {
            if (k1 == Type::Kind::Integer) {
                return new llvm::PtrToIntInst(arguments[0]->createLlvm(llvmObj), type->createLlvm(llvmObj), "", llvmObj->block);
            } else if (k1 == Type::Kind::Float) {
                auto ptrToInt = new llvm::PtrToIntInst(arguments[0]->createLlvm(llvmObj), IntegerType::Create(IntegerType::Size::I64)->createLlvm(llvmObj), "", llvmObj->block);
                return new llvm::BitCastInst(ptrToInt, type->createLlvm(llvmObj), "", llvmObj->block);
            }
        } else if (k2 == Type::Kind::Bool || k2 == Type::Kind::Integer || k2 == Type::Kind::Float) {
            return new llvm::BitCastInst(arguments[0]->createLlvm(llvmObj), type->createLlvm(llvmObj), "", llvmObj->block);
        } else {
            internalError("unexpected type during cast llvm creating", arguments[0]->position);
            return nullptr;
        }
    } else {
        internalError("unexpected type during cast llvm creating", arguments[0]->position);
    }
}
llvm::Value* CastOperation::getReferenceLlvm(LlvmObject* llvmObj) {
    return new llvm::BitCastInst(arguments[0]->getReferenceLlvm(llvmObj), RawPointerType::Create(type)->createLlvm(llvmObj), "", llvmObj->block);
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

    switch (arguments[0]->type->getEffectiveType()->kind) {
        case Type::Kind::StaticArray:
            type = ReferenceType::Create(((StaticArrayType*)arguments[0]->type->getEffectiveType())->elementType);
            break;
        case Type::Kind::DynamicArray:
            type = ReferenceType::Create(((DynamicArrayType*)arguments[0]->type->getEffectiveType())->elementType);
            break;
        case Type::Kind::ArrayView:
            type = ReferenceType::Create(((ArrayViewType*)arguments[0]->type->getEffectiveType())->elementType);
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
    auto effType = arguments[0]->type->getEffectiveType();
    auto arg = arguments[0]->getReferenceLlvm(llvmObj);
    vector<llvm::Value*> indexList;
    if (effType->kind == Type::Kind::StaticArray) {
        indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
    } else if (effType->kind == Type::Kind::DynamicArray) {
        arg = new llvm::LoadInst(((DynamicArrayType*)effType)->llvmGepData(llvmObj, arg), "", llvmObj->block);
    }
    indexList.push_back(index->createLlvm(llvmObj));
    return llvm::GetElementPtrInst::Create(((llvm::PointerType*)arg->getType())->getElementType(), arg, indexList, "", llvmObj->block);
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
        vector<optional<vector<ConstructorOperation*>>> neededCtors;
        for (Declaration* declaration : viableDeclarations) {
            neededCtors.push_back(vector<ConstructorOperation*>());
            auto argumentTypes = ((FunctionType*)declaration->variable->type)->argumentTypes;
            for (int i = 0; i < argumentTypes.size(); ++i) {
                if (!cmpPtr(argumentTypes[i], arguments[i]->type)) {
                    auto ctor = ConstructorOperation::Create(arguments[i]->position, argumentTypes[i], {arguments[i]});
                    auto castInterpret = ctor->interpret(scope, true);
                    if (castInterpret) {
                        neededCtors.back().value().push_back(ctor);
                    } else {
                        neededCtors.back() = nullopt;
                        break;
                    }
                } else {
                    neededCtors.back().value().push_back(nullptr);
                }
            }
        }

        int matchId = -1;
        vector<Declaration*> possibleDeclarations;
        for (int i = 0; i < neededCtors.size(); ++i) {
            if (neededCtors[i]) {
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
            auto ctor = neededCtors[matchId].value()[i];
            if (ctor) {
                auto ctorInterpret = ctor->interpret(scope);
                if (ctorInterpret.value()) {
                    arguments[i] = ctorInterpret.value();
                } else {
                    arguments[i] = ctor;
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

    // first check if is it class constructor. if yes -> its ConstructorOperation
    if (function->valueKind == Value::ValueKind::Variable) {
        auto functionName = ((Variable*)function)->name;
        Scope* searchScope = scope; 
        auto classDeclaration = searchScope->classDeclarationMap.getDeclaration(functionName);
        while (searchScope->parentScope && !classDeclaration) {
            searchScope = searchScope->parentScope;
            classDeclaration = searchScope->classDeclarationMap.getDeclaration(functionName);
        }
        if (classDeclaration) {
            auto constructorType = ClassType::Create(classDeclaration->name);
            constructorType->declaration = classDeclaration;
            auto op = ConstructorOperation::Create(position, constructorType, arguments);
            auto opInterpret = op->interpret(scope);
            if (!opInterpret) return nullopt;
            if (opInterpret.value()) return opInterpret.value();
            else return op;
        }
    }

    if (!interpretAllArguments(scope)) {
        return nullopt;
    }

    // first check if is it class constructor
    /*if (function->valueKind == Value::ValueKind::Variable) {
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
    }*/

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
                auto ctor = ConstructorOperation::Create(position, functionType->argumentTypes[i], {arguments[i]});
                auto ctorInterpret = ctor->interpret(scope);
                if (!ctorInterpret) return nullopt;
                if (ctorInterpret.value()) arguments[i] = ctorInterpret.value();
                else arguments[i] = ctor;
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
    } else if (((FunctionType*)function->type)->returnType->kind == Type::Kind::Class) {
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

    if (functionType->returnType->kind == Type::Kind::Class) {
        llvmClassStackTemp = functionType->returnType->allocaLlvm(llvmObj);
        new llvm::StoreInst(
            llvm::CallInst::Create(function->createLlvm(llvmObj), args, "", llvmObj->block), 
            llvmClassStackTemp, 
            llvmObj->block
        );
        return llvmClassStackTemp;
    }

    llvmValue = llvm::CallInst::Create(function->createLlvm(llvmObj), args, "", llvmObj->block);
    return llvmValue;
}
void FunctionCallOperation::createDestructorLlvm(LlvmObject* llvmObj) {
    if (llvmClassStackTemp) {
        type->createDestructorLlvm(llvmObj, llvmClassStackTemp);
    } else {
        type->createDestructorLlvm(llvmObj, llvmValue);
    }
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
                    auto ctor = ConstructorOperation::Create(position, returnType, {arguments[0]});
                    arguments[0] = ctor;
                    auto ctorInterpret = arguments[0]->interpret(scope);
                    if (!ctorInterpret) return nullopt;
                    if (ctorInterpret.value()) arguments[0] = ctorInterpret.value();
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



void createLlvmForEachLoop(LlvmObject* llvmObj, llvm::Value* sizeValue, function<void(llvm::Value*)> bodyFunction) {
    auto i64Type = llvm::Type::getInt64Ty(llvmObj->context);

    auto loopStartBlock     = llvm::BasicBlock::Create(llvmObj->context, "loopStart",     llvmObj->function);
    auto loopConditionBlock = llvm::BasicBlock::Create(llvmObj->context, "loopCondition", llvmObj->function);
    auto loopBodyBlock      = llvm::BasicBlock::Create(llvmObj->context, "loop",          llvmObj->function);
    auto afterLoopBlock     = llvm::BasicBlock::Create(llvmObj->context, "afterLoop",     llvmObj->function);

    // start block
    llvm::BranchInst::Create(loopStartBlock, llvmObj->block);
    llvmObj->block = loopStartBlock;
    auto index = new llvm::AllocaInst(i64Type, 0, "", llvmObj->block);
    new llvm::StoreInst(llvm::ConstantInt::get(i64Type, -1), index, llvmObj->block);
    llvm::BranchInst::Create(loopConditionBlock, loopStartBlock);

    // body block
    llvmObj->block = loopBodyBlock;
    bodyFunction(index);

    // after block
    llvm::BranchInst::Create(loopConditionBlock, llvmObj->block);

    // condition block
    llvmObj->block = loopConditionBlock;
    auto indexAfterIncrement = llvm::BinaryOperator::CreateAdd(
        new llvm::LoadInst(index, "", llvmObj->block), 
        llvm::ConstantInt::get(i64Type, 1), "", llvmObj->block
    );
    new llvm::StoreInst(indexAfterIncrement, index, llvmObj->block);
    llvm::BranchInst::Create(
        loopBodyBlock, 
        afterLoopBlock, 
        new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLT, indexAfterIncrement, sizeValue, ""), 
        loopConditionBlock
    );

    llvmObj->block = afterLoopBlock;
}
void createLlvmForEachLoop(LlvmObject* llvmObj, llvm::Value* start, llvm::Value* end, function<void(llvm::Value*)> bodyFunction) {
    auto i64Type = llvm::Type::getInt64Ty(llvmObj->context);

    auto loopStartBlock     = llvm::BasicBlock::Create(llvmObj->context, "loopStart",     llvmObj->function);
    auto loopStepBlock      = llvm::BasicBlock::Create(llvmObj->context, "loopStep",      llvmObj->function);
    auto loopConditionBlock = llvm::BasicBlock::Create(llvmObj->context, "loopCondition", llvmObj->function);
    auto loopBodyBlock      = llvm::BasicBlock::Create(llvmObj->context, "loop",          llvmObj->function);
    auto afterLoopBlock     = llvm::BasicBlock::Create(llvmObj->context, "afterLoop",     llvmObj->function);

    // start block
    llvm::BranchInst::Create(loopStartBlock, llvmObj->block);
    llvmObj->block = loopStartBlock;
    auto index = new llvm::AllocaInst(i64Type, 0, "", llvmObj->block);
    new llvm::StoreInst(start, index, llvmObj->block);
    llvm::BranchInst::Create(loopConditionBlock, loopStartBlock);

    // body block
    llvmObj->block = loopBodyBlock;
    bodyFunction(index);

    // after block
    llvm::BranchInst::Create(loopStepBlock, llvmObj->block);

    // step block
    llvmObj->block = loopStepBlock;
    auto indexAfterIncrement = llvm::BinaryOperator::CreateAdd(
        new llvm::LoadInst(index, "", llvmObj->block), 
        llvm::ConstantInt::get(i64Type, 1), "", llvmObj->block
    );
    new llvm::StoreInst(indexAfterIncrement, index, llvmObj->block);
    llvm::BranchInst::Create(loopConditionBlock, loopStepBlock);

    // condition block
    llvmObj->block = loopConditionBlock;
    llvm::BranchInst::Create(
        loopBodyBlock, 
        afterLoopBlock, 
        new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLT, new llvm::LoadInst(index, "", llvmObj->block), end, ""),
        loopConditionBlock
    );

    llvmObj->block = afterLoopBlock;
}
void createLlvmConditional(LlvmObject* llvmObj, llvm::Value* condition, function<void()> ifTrueFunction, function<void()> ifFalseFunction) {
    auto ifTrueBlock = llvm::BasicBlock::Create(llvmObj->context, "ifTrue", llvmObj->function);
    auto oldBlock = llvmObj->block;

    llvmObj->block = ifTrueBlock;
    ifTrueFunction();
    auto afterIfTrueBlock = llvmObj->block;

    auto ifFalseBlock = llvm::BasicBlock::Create(llvmObj->context, "ifFalse", llvmObj->function);
    llvmObj->block = oldBlock;
    llvm::BranchInst::Create(ifTrueBlock, ifFalseBlock, condition, llvmObj->block);

    llvmObj->block = ifFalseBlock;
    ifFalseFunction();
    auto afterConditionalBlock = llvm::BasicBlock::Create(llvmObj->context, "afterConditional", llvmObj->function);
    
    llvm::BranchInst::Create(afterConditionalBlock, afterIfTrueBlock);
    llvm::BranchInst::Create(afterConditionalBlock, llvmObj->block);
    llvmObj->block = afterConditionalBlock;
}
void createLlvmConditional(LlvmObject* llvmObj, llvm::Value* condition, function<void()> ifTrueFunction) {
    auto ifTrueBlock = llvm::BasicBlock::Create(llvmObj->context, "ifTrue", llvmObj->function);
    auto oldBlock = llvmObj->block;
    llvmObj->block = ifTrueBlock;
    ifTrueFunction();
    auto afterIfTrueBlock = llvm::BasicBlock::Create(llvmObj->context, "afterConditional", llvmObj->function);
    llvm::BranchInst::Create(afterIfTrueBlock, llvmObj->block);
    llvmObj->block = oldBlock;
    llvm::BranchInst::Create(ifTrueBlock, afterIfTrueBlock, condition, llvmObj->block);
    llvmObj->block = afterIfTrueBlock;
}

ConstructorOperation::ConstructorOperation(const CodePosition& position, Type* constructorType, std::vector<Value*> arguments, bool isHeapAllocation, bool isExplicit) : 
    Operation(position, Operation::Kind::Constructor),
    constructorType(constructorType),
    isHeapAllocation(isHeapAllocation),
    isExplicit(isExplicit)
{
    this->arguments = arguments;
    if (isHeapAllocation) {
        type = OwnerPointerType::Create(constructorType);
    } else {
        type = constructorType;
    }
}
vector<unique_ptr<ConstructorOperation>> ConstructorOperation::objects;
ConstructorOperation* ConstructorOperation::Create(const CodePosition& position, Type* constructorType, std::vector<Value*> arguments, bool isHeapAllocation, bool isExplicit) {
    objects.emplace_back(make_unique<ConstructorOperation>(position, constructorType, arguments, isHeapAllocation, isExplicit));
    return objects.back().get();
}
optional<Value*> ConstructorOperation::interpret(Scope* scope) {
    return interpret(scope, false, false);
}
optional<Value*> ConstructorOperation::interpret(Scope* scope, bool onlyTry, bool parentIsAssignment) {
    if (wasInterpreted) return nullptr;
    if (!onlyTry) wasInterpreted = true;
    if (!type->interpret(scope)) return nullopt;
    if (!interpretAllArguments(scope)) return nullopt;

    if (isHeapAllocation) {
        typesize = constructorType->typesize(scope);
    } 
    if (arguments.size() == 1 && cmpPtr(constructorType, arguments[0]->type->getEffectiveType())) {
        if (isHeapAllocation) {
            arguments[0]->wasCaptured = true;
            return nullptr;
        } else {
            return arguments[0];
        }
    } 

    auto constructorInterpret = constructorType->interpretConstructor(position, scope, arguments, onlyTry, parentIsAssignment || isHeapAllocation, isExplicit);
    if (!constructorInterpret) return nullopt;
    if (constructorInterpret.value().value) {
        if (isHeapAllocation || constructorType->kind == Type::Kind::StaticArray) {
            arguments = {constructorInterpret.value().value};
        } else {
            return constructorInterpret.value().value;
        }
    }
    classConstructor = constructorInterpret.value().classConstructor;

    if (type->needsDestruction()) scope->valuesToDestroyBuffer.push_back(this);
    return nullptr;
}
void ConstructorOperation::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef) {
    if (isHeapAllocation) {
        llvmValue = new llvm::BitCastInst(
            llvm::CallInst::Create(llvmObj->mallocFunction, typesize->createLlvm(llvmObj), "", llvmObj->block), 
            type->createLlvm(llvmObj), 
            "", 
            llvmObj->block
        );
        constructorType->createLlvmConstructor(llvmObj, llvmValue, arguments, classConstructor);
        type->createLlvmMoveConstructor(llvmObj, leftLlvmRef, llvmValue);
    } else {
        type->createLlvmConstructor(llvmObj, leftLlvmRef, arguments, classConstructor);
    }
}
void ConstructorOperation::createLlvmAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef) {
    if (isHeapAllocation) {
        llvmValue = new llvm::BitCastInst(
            llvm::CallInst::Create(llvmObj->mallocFunction, typesize->createLlvm(llvmObj), "", llvmObj->block), 
            type->createLlvm(llvmObj), 
            "", 
            llvmObj->block
        );
        constructorType->createLlvmConstructor(llvmObj, llvmValue, arguments, classConstructor);
        type->createLlvmMoveAssignment(llvmObj, leftLlvmRef, llvmValue);
    } else {
        type->createLlvmAssignment(llvmObj, leftLlvmRef, arguments, classConstructor);
    }
}
llvm::Value* ConstructorOperation::getReferenceLlvm(LlvmObject* llvmObj) {
    llvmValue = type->createLlvmReference(llvmObj, arguments, classConstructor);
    return llvmValue;
}
llvm::Value* ConstructorOperation::createLlvm(LlvmObject* llvmObj) {
    if (isHeapAllocation) {
        llvmValue = new llvm::BitCastInst(
            llvm::CallInst::Create(llvmObj->mallocFunction, typesize->createLlvm(llvmObj), "", llvmObj->block), 
            type->createLlvm(llvmObj), 
            "", 
            llvmObj->block
        );
        constructorType->createLlvmConstructor(llvmObj, llvmValue, arguments, classConstructor);
        return llvmValue;
    } else {
        auto createValue = type->createLlvmValue(llvmObj, arguments, classConstructor);
        llvmValue = createValue.first;
        return createValue.second;
    }
}
void ConstructorOperation::createDestructorLlvm(LlvmObject* llvmObj) {
    type->createDestructorLlvm(llvmObj, llvmValue);
}


AssignOperation::AssignOperation(const CodePosition& position) : 
    Operation(position, Operation::Kind::Assign)
{}
vector<unique_ptr<AssignOperation>> AssignOperation::objects;
AssignOperation* AssignOperation::Create(const CodePosition& position) {
    objects.emplace_back(make_unique<AssignOperation>(position));
    return objects.back().get();
}
optional<Value*> AssignOperation::interpret(Scope* scope) {
    if (wasInterpreted) {
        return nullptr;
    }
    wasInterpreted = true;

    if (arguments[0]->valueKind == Value::ValueKind::Variable) {
        auto variable = (Variable*)arguments[0];
        if (!variable->interpretTypeAndDeclaration(scope)) {
            return nullopt;
        }
    }

    optional<Value*> arg1Interpret = nullopt;
    if (arguments[1]->valueKind == Value::ValueKind::Operation && ((Operation*)arguments[1])->kind == Operation::Kind::Constructor) {
        arg1Interpret = ((ConstructorOperation*)arguments[1])->interpret(scope, false, true);
    } else {
        arg1Interpret = arguments[1]->interpret(scope);
    }
    if (!arg1Interpret) return nullopt;
    if (arg1Interpret.value()) arguments[1] = arg1Interpret.value();
    if (!arguments[1]->type->interpret(scope)) return nullopt;

    if (arguments[1]->valueKind == Value::ValueKind::Operation) {
        containsErrorResolve = ((Operation*)arguments[1])->containsErrorResolve;
    }

    if (!containsErrorResolve && arguments[0]->valueKind == Value::ValueKind::Variable) {
        auto variable = (Variable*)arguments[0];
        if (variable->declaration->scope->owner != Scope::Owner::Class) {
            if (!scope->declarationsInitState.at(variable->declaration)) {
                isConstruction = true;
                scope->declarationsInitState.at(variable->declaration) = true;
            }
            scope->maybeUninitializedDeclarations.erase(variable->declaration);
        }
    }

    auto arg0Interpret = arguments[0]->interpret(scope);
    if (!arg0Interpret) return nullopt;
    if (arg0Interpret.value()) arguments[0] = arg0Interpret.value();
    if (!arguments[0]->type->interpret(scope)) return nullopt;

    if (!isLvalue(arguments[0])) {
        return errorMessageOpt("left argument of an assignment must be an l-value", position);
    }

    arguments[1] = ConstructorOperation::Create(arguments[1]->position, arguments[0]->type->getEffectiveType(), {arguments[1]});
    auto constructorInterpret = ((ConstructorOperation*)arguments[1])->interpret(scope, false, true);
    if (!constructorInterpret) return nullopt;
    if (constructorInterpret.value()) arguments[1] = constructorInterpret.value();
    arguments[1]->wasCaptured = true;

    type = arguments[0]->type;

    return nullptr;
}
bool AssignOperation::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const AssignOperation&>(value);
        return this->isConstruction == other.isConstruction
            && Operation::operator==(other);
    }
    else {
        return false;
    }
}
llvm::Value* AssignOperation::getReferenceLlvm(LlvmObject* llvmObj) {
    auto arg0Reference = arguments[0]->getReferenceLlvm(llvmObj);

    if (arguments[1]->valueKind == Value::ValueKind::Operation && ((Operation*)arguments[1])->kind == Operation::Kind::Constructor) {
        auto constructorOp = (ConstructorOperation*)arguments[1];
        if (isConstruction) {
            constructorOp->createLlvmConstructor(llvmObj, arg0Reference);
        } else {
            constructorOp->createLlvmAssignment(llvmObj, arg0Reference);
        }
    }
    else if (isConstruction) {
        if (isLvalue(arguments[1])) {
            if (arguments[0]->type->kind == Type::Kind::Class || arguments[0]->type->kind == Type::Kind::StaticArray || arguments[0]->type->kind == Type::Kind::DynamicArray || arguments[0]->type->kind == Type::Kind::MaybeError) {
                arguments[0]->type->createLlvmCopyConstructor(llvmObj, arg0Reference, arguments[1]->getReferenceLlvm(llvmObj));
            } else {
                arguments[0]->type->createLlvmCopyConstructor(llvmObj, arg0Reference, arguments[1]->createLlvm(llvmObj));
            }
        } else {
            if (arguments[0]->type->kind == Type::Kind::Class) {
                arguments[0]->type->createLlvmMoveConstructor(llvmObj, arg0Reference, arguments[1]->getReferenceLlvm(llvmObj));
            } else {
                arguments[0]->type->createLlvmMoveConstructor(llvmObj, arg0Reference, arguments[1]->createLlvm(llvmObj));
            }
        }
    } else {
        if (isLvalue(arguments[1])) {
            if (arguments[0]->type->kind == Type::Kind::Class || arguments[0]->type->kind == Type::Kind::StaticArray || arguments[0]->type->kind == Type::Kind::DynamicArray || arguments[0]->type->kind == Type::Kind::MaybeError) {
                arguments[0]->type->createLlvmCopyAssignment(llvmObj, arg0Reference, arguments[1]->getReferenceLlvm(llvmObj));
            } else {
                arguments[0]->type->createLlvmCopyAssignment(llvmObj, arg0Reference, arguments[1]->createLlvm(llvmObj));
            }
        } else {
            if (arguments[0]->type->kind == Type::Kind::Class) {
                arguments[0]->type->createLlvmMoveAssignment(llvmObj, arg0Reference, arguments[1]->getReferenceLlvm(llvmObj));
            } else {
                arguments[0]->type->createLlvmMoveAssignment(llvmObj, arg0Reference, arguments[1]->createLlvm(llvmObj));
            }
        }
    }

    return arg0Reference;
}
llvm::Value* AssignOperation::createLlvm(LlvmObject* llvmObj) {
    return new llvm::LoadInst(getReferenceLlvm(llvmObj), "", llvmObj->block);
}