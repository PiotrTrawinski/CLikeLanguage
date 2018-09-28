#include "Operation.h"
#include "Declaration.h"
#include "ClassDeclaration.h"

using namespace std;


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
void Operation::templateCopy(Operation* operation, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    operation->kind = kind;
    for (auto arg : arguments) {
        operation->arguments.push_back((Value*)arg->templateCopy(parentScope, templateToType));
    }
    Value::templateCopy(operation, parentScope, templateToType);
}
Statement* Operation::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto operation = Create(position, kind);
    templateCopy(operation, parentScope, templateToType);
    return operation;
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

optional<Value*> Operation::createCustomFunctionOperation(Scope* scope) {
    auto functionCall = FunctionCallOperation::Create(position);
    functionCall->arguments = arguments;
    functionCall->function = Variable::Create(position, kindToFunctionName(kind));
    auto functionCallInterpret = functionCall->interpret(scope);
    if (!functionCallInterpret) return nullopt;
    if (functionCallInterpret.value()) return functionCallInterpret.value();
    else return functionCall;
}
optional<Value*> Operation::interpret(Scope* scope) {
    if (wasInterpreted) {
        return nullptr;
    }
    
    if (!interpretAllArguments(scope)) {
        return nullopt;
    }

    Type* effectiveType1 = nullptr;
    Type* effectiveType2 = nullptr;

    if (arguments.size() >= 1) {
        effectiveType1 = arguments[0]->type->getEffectiveType();
    }
    if (arguments.size() >= 2) {
        effectiveType2 = arguments[1]->type->getEffectiveType();
    }
    for (auto argument : arguments) {
        if (argument->valueKind == Value::ValueKind::Operation) {
            if (((Operation*)argument)->containedErrorResolve) {
                containedErrorResolve = ((Operation*)argument)->containedErrorResolve;
            }
        }
    }

    auto typeCopy = type;
    type = nullptr;

    switch (kind) {
    case Kind::Address: {
        if (isLvalue(arguments[0])) {
            if (arguments[0]->type->kind == Type::Kind::Reference) {
                type = RawPointerType::Create(((ReferenceType*)arguments[0]->type)->underlyingType);
            } else {
                type = RawPointerType::Create(arguments[0]->type);
            }
        }
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
    case Kind::Deallocation: {
        if (arguments[0]->type->getEffectiveType()->kind == Type::Kind::OwnerPointer) {
            if (!isLvalue(arguments[0])) {
                return errorMessageOpt("cannot dealloc non-lvalue owner pointer", position);
            }
            if (arguments[0]->valueKind == Value::ValueKind::Variable) {
                auto variable = (Variable*)arguments[0];
                scope->maybeUninitializedDeclarations.insert(variable->declaration);
                if (scope->declarationsInitState.at(variable->declaration)) {
                    scope->declarationsInitState[variable->declaration] = false;
                } else {
                    return arguments[0];
                }
            }
        } else if (arguments[0]->type->getEffectiveType()->kind != Type::Kind::RawPointer) {
            return errorMessageOpt("cannot dealloc non-pointer value", position);
        }

        type = Type::Create(Type::Kind::Void);
        break;
    }
    case Kind::Destroy:{
        if (!isLvalue(arguments[0])) {
            return errorMessageOpt("cannot destroy non-lvalue", position);
        }
        if (arguments[0]->valueKind == Value::ValueKind::Variable) {
            auto variable = (Variable*)arguments[0];
            scope->maybeUninitializedDeclarations.insert(variable->declaration);
            if (scope->declarationsInitState.at(variable->declaration)) {
                scope->declarationsInitState[variable->declaration] = false;
            } else {
                return arguments[0];
            }
        }
        type = Type::Create(Type::Kind::Void);
        break;
    }
    case Kind::Move: {
        if (!isLvalue(arguments[0])) {
            return arguments[0];
        }
        if (arguments[0]->valueKind == Value::ValueKind::Variable) {
            auto variable = (Variable*)arguments[0];
            scope->maybeUninitializedDeclarations.insert(variable->declaration);
            scope->declarationsInitState[variable->declaration] = false;
        }
        type = arguments[0]->type->getEffectiveType();
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
        if (arguments[0]->valueKind == Value::ValueKind::StaticArray && arguments[1]->valueKind == Value::ValueKind::StaticArray) {
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
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::Lt: {
        auto value = tryEvaluate2ArgArithmeticBoolTest([](auto val1, auto val2)->bool{
            return val1 < val2;
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::Gte: {
        auto value = tryEvaluate2ArgArithmeticBoolTest([](auto val1, auto val2)->bool{
            return val1 >= val2;
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::Lte: {
        auto value = tryEvaluate2ArgArithmeticBoolTest([](auto val1, auto val2)->bool{
            return val1 <= val2;
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::Eq: {
        auto value = tryEvaluate2ArgArithmeticBoolTest([](auto val1, auto val2)->bool{
            return val1 == val2;
        }, scope);
        if (value) return value;
        break;
    }
    case Kind::Neq:{
        auto value = tryEvaluate2ArgArithmeticBoolTest([](auto val1, auto val2)->bool{
            return val1 != val2;
        }, scope);
        if (value) return value;
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
        arguments[0] = bool1;
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
        arguments[0] = bool1;
        arguments[1] = bool2;
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
        arguments[0] = bool1;
        arguments[1] = bool2;
        type = Type::Create(Type::Kind::Bool);

        break;   
    }
    case Kind::LogicalXor: {
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
                ((BoolValue*)bool1)->value ^ ((BoolValue*)bool2)->value
            );
        }
        arguments[0] = bool1;
        arguments[1] = bool2;
        type = Type::Create(Type::Kind::Bool);

        break;   
    }
    case Kind::LongCircuitLogicalAnd: {
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
        arguments[0] = bool1;
        arguments[1] = bool2;
        type = Type::Create(Type::Kind::Bool);

        break;   
    }
    case Kind::LongCircuitLogicalOr: {
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
        arguments[0] = bool1;
        arguments[1] = bool2;
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
        return createCustomFunctionOperation(scope);
    }

    wasInterpreted = true;
    return nullptr;
}

string Operation::kindToString(Kind kind) {
    switch (kind) {
    case Kind::Destroy: return "destroy";
    case Kind::Move: return "move";
    case Kind::Dot: return ". (dot)";
    case Kind::FunctionCall: return "function call";
    case Kind::ArrayIndex: return "[] (array index)";
    case Kind::ArraySubArray: return "[:] (sub-array)";
    case Kind::Address: return "@ (address)";
    case Kind::GetValue: return "$ (valueOf)";
    case Kind::Deallocation: return "dealloc";
    case Kind::Cast: return "<T> (cast)";
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
    case Kind::LogicalXor: return "^^ (logical xor)";
    case Kind::LongCircuitLogicalAnd: return "&&& (long circuit logical and)";
    case Kind::LongCircuitLogicalOr: return "||| (long circuit logical or)";
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
optional<Operation::Kind> Operation::stringToKind(const std::string& str) {
    if (str == "[]")       return Kind::ArrayIndex;
    else if (str == "[:]") return Kind::ArraySubArray;
    else if (str == "@")   return Kind::Address;
    else if (str == "$")   return Kind::GetValue;
    else if (str == "~")   return Kind::BitNeg;
    else if (str == "!")   return Kind::LogicalNot;
    else if (str == "*")   return Kind::Mul;
    else if (str == "/")   return Kind::Div;
    else if (str == "%")   return Kind::Mod;
    else if (str == "+")   return Kind::Add;
    else if (str == "-")   return Kind::Sub;
    else if (str == "<<")  return Kind::Shl;
    else if (str == ">>")  return Kind::Shr;
    else if (str == "<<<") return Kind::Sal;
    else if (str == ">>>") return Kind::Sar;
    else if (str == ">")   return Kind::Gt;
    else if (str == "<")   return Kind::Lt;
    else if (str == ">=")  return Kind::Gte;
    else if (str == "<=")  return Kind::Lte;
    else if (str == "==")  return Kind::Eq;
    else if (str == "!=")  return Kind::Neq;
    else if (str == "&")   return Kind::BitAnd;
    else if (str == "^")   return Kind::BitXor;
    else if (str == "|")   return Kind::BitOr;
    else if (str == "&&")  return Kind::LogicalAnd;
    else if (str == "||")  return Kind::LogicalOr;
    else if (str == "^^")  return Kind::LogicalXor;
    else if (str == "&&&")  return Kind::LongCircuitLogicalAnd;
    else if (str == "|||")  return Kind::LongCircuitLogicalOr;
    else if (str == "+=")  return Kind::AddAssign;
    else if (str == "-=")  return Kind::SubAssign;
    else if (str == "*=")  return Kind::MulAssign;
    else if (str == "/=")  return Kind::DivAssign;
    else if (str == "%=")  return Kind::ModAssign;
    else if (str == "<<=")  return Kind::ShlAssign;
    else if (str == ">>=")  return Kind::ShrAssign;
    else if (str == "<<<=")  return Kind::SalAssign;
    else if (str == ">>>=")  return Kind::SarAssign;
    else if (str == "~=")  return Kind::BitNegAssign;
    else if (str == "|=")  return Kind::BitOrAssign;
    else if (str == "^=")  return Kind::BitXorAssign;
    else return nullopt;
}
string Operation::kindToFunctionName(Kind kind) {
    switch (kind) {
    case Kind::ArrayIndex:    return "__operator_ArrayIndex__";
    case Kind::ArraySubArray: return "__operator_ArraySubArray__";
    case Kind::Address:       return "__operator_Address__";
    case Kind::GetValue:      return "__operator_GetValue__";
    case Kind::BitNeg:        return "__operator_BitNeg__";
    case Kind::LogicalNot:    return"__operator_LogicalNot__";
    case Kind::Minus:         return "__operator_Minus__";
    case Kind::Mul:           return "__operator_Mul__";
    case Kind::Div:           return "__operator_Div__";
    case Kind::Mod:           return "__operator_Mod__";
    case Kind::Add:           return "__operator_Add__";
    case Kind::Sub:           return "__operator_Sub__";
    case Kind::Shl:           return "__operator_Shl__";
    case Kind::Shr:           return "__operator_Shr__";
    case Kind::Sal:           return "__operator_Sal__";
    case Kind::Sar:           return "__operator_Sar__";
    case Kind::Gt:            return "__operator_Gt__";
    case Kind::Lt:            return "__operator_Lt__";
    case Kind::Gte:           return "__operator_Gte__";
    case Kind::Lte:           return "__operator_Lte__";
    case Kind::Eq:            return "__operator_Eq__";
    case Kind::Neq:           return "__operator_Neq__";
    case Kind::BitAnd:        return "__operator_BitAnd__";
    case Kind::BitXor:        return "__operator_BitXor__";
    case Kind::BitOr:         return "__operator_BitOr__";
    case Kind::LogicalAnd:    return "__operator_LogicalAnd__";
    case Kind::LogicalOr:     return "__operator_LogicalOr__";
    case Kind::LogicalXor:     return "__operator_LogicalXor__";
    case Kind::LongCircuitLogicalAnd: return "__operator_LongCircuitLogicalAnd__";
    case Kind::LongCircuitLogicalOr:  return "__operator_LongCircuitLogicalOr__";
    case Kind::AddAssign:     return "__operator_AddAssign__";
    case Kind::SubAssign:     return "__operator_SubAssign__";
    case Kind::MulAssign:     return "__operator_MulAssign__";
    case Kind::DivAssign:     return "__operator_DivAssign__";
    case Kind::ModAssign:     return "__operator_ModAssign__";
    case Kind::ShlAssign:     return "__operator_ShlAssign__";
    case Kind::ShrAssign:     return "__operator_ShrAssign__";
    case Kind::SalAssign:     return "__operator_SalAssign__";
    case Kind::SarAssign:     return "__operator_SarAssign__";
    case Kind::BitNegAssign:  return "__operator_BitNegAssign__";
    case Kind::BitOrAssign:   return "__operator_BitOrAssign__";
    case Kind::BitXorAssign:  return "__operator_BitXorAssign__";
    default:                  return "__operator_unknown__";
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
    case Kind::Move:
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
    case Kind::LongCircuitLogicalAnd:
        return 11;
    case Kind::LogicalXor:
        return 12;
    case Kind::LogicalOr:
    case Kind::LongCircuitLogicalOr:
        return 13;
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
        return 14;
    default: 
        return 15;
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
    case Kind::LogicalXor:
    case Kind::LongCircuitLogicalAnd:
    case Kind::LongCircuitLogicalOr:
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
    case Kind::Move:
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
    case Kind::LogicalXor:
    case Kind::LongCircuitLogicalAnd:
    case Kind::LongCircuitLogicalOr:
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
void Operation::createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj) {
    switch (kind) {
    case Kind::Address:
        arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        break;
    case Kind::GetValue:
        switch (arguments[0]->type->getEffectiveType()->kind) {
        case Type::Kind::MaybeError:
            arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        case Type::Kind::RawPointer:
        case Type::Kind::OwnerPointer:
            arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
            arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        }
        break;
    case Kind::Deallocation:
        arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        break;
    case Kind::Destroy:
        arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        break;
    case Kind::Move:
        arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        break;
    case Kind::Minus:
    case Kind::LogicalNot:
    case Kind::BitNeg:
        arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        break;
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
    case Kind::LongCircuitLogicalAnd:
    case Kind::LongCircuitLogicalOr:
    case Kind::LogicalXor:
    case Kind::BitAnd:
    case Kind::BitXor:
    case Kind::BitOr:
        arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        arguments[1]->createAllocaLlvmIfNeededForValue(llvmObj);
        break;
    case Kind::LogicalAnd:
    case Kind::LogicalOr:
        arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        arguments[1]->createAllocaLlvmIfNeededForValue(llvmObj);
        shortCircuitLlvmVar = new llvm::AllocaInst(llvm::Type::getInt1Ty(llvmObj->context), 0, "", llvmObj->block);
        break;
    default:
        break;
    }
}
void Operation::createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj) {
    switch (kind) {
    case Kind::GetValue:
        switch (arguments[0]->type->getEffectiveType()->kind) {
        case Type::Kind::RawPointer: 
            arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
            break;
        case Type::Kind::OwnerPointer:
            arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
            break;
        case Type::Kind::MaybeError: 
            arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
            break;
        }
        break;
    case Kind::Move:
        arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        break;
    }
}
llvm::Value* Operation::getReferenceLlvm(LlvmObject* llvmObj) {
    switch (kind) {
    case Kind::GetValue: {
        switch (arguments[0]->type->getEffectiveType()->kind) {
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
    case Kind::Move: {
        return arguments[0]->getReferenceLlvm(llvmObj);
    }
    default: return nullptr;
    }
}
llvm::Value* Operation::createLlvm(LlvmObject* llvmObj) {
    switch (kind) {
    case Kind::Address: {
        return arguments[0]->getReferenceLlvm(llvmObj);
    }
    case Kind::GetValue: {
        switch (arguments[0]->type->getEffectiveType()->kind) {
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
    case Kind::Deallocation: {
        llvm::CallInst::Create(
            llvmObj->freeFunction, 
            new llvm::BitCastInst(arguments[0]->createLlvm(llvmObj), llvm::Type::getInt8PtrTy(llvmObj->context), "", llvmObj->block), 
            "", 
            llvmObj->block
        );
        break;
    }
    case Kind::Destroy: {
        arguments[0]->type->createLlvmDestructorRef(llvmObj, arguments[0]->getReferenceLlvm(llvmObj));
        return nullptr;
    }
    case Kind::Move: {
        return arguments[0]->createLlvm(llvmObj);
    }
    case Kind::Minus: {
        auto arg = arguments[0]->createLlvm(llvmObj);
        if (type->getEffectiveType()->kind == Type::Kind::Integer) {
            return llvm::BinaryOperator::CreateNeg(arg, "", llvmObj->block);
        } else {
            return llvm::BinaryOperator::CreateFNeg(arg, "", llvmObj->block);
        } 
    }
    case Kind::Mul: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (type->getEffectiveType()->kind == Type::Kind::Integer) {
            return llvm::BinaryOperator::CreateMul(arg1, arg2, "", llvmObj->block);
        } else {
            return llvm::BinaryOperator::CreateFMul(arg1, arg2, "", llvmObj->block);
        } 
    }
    case Kind::Div: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (type->getEffectiveType()->kind == Type::Kind::Integer) {
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
        if (type->getEffectiveType()->kind == Type::Kind::Integer) {
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
        if (type->getEffectiveType()->kind == Type::Kind::Integer) {
            return llvm::BinaryOperator::CreateAdd(arg1, arg2, "", llvmObj->block);
        } else {
            return llvm::BinaryOperator::CreateFAdd(arg1, arg2, "", llvmObj->block);
        } 
    }
    case Kind::Sub: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (type->getEffectiveType()->kind == Type::Kind::Integer) {
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
        if (arguments[0]->type->getEffectiveType()->kind == Type::Kind::Integer) {
            if (((IntegerType*)arguments[0]->type)->isSigned()) {
                return new llvm::SExtInst(
                    new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SGT, arg1, arg2, ""),
                    Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                    "", llvmObj->block
                );
            } else {
                return new llvm::SExtInst(
                    new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_UGT, arg1, arg2, ""),
                    Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                    "", llvmObj->block
                );
            }
            
        } else {
            return new llvm::SExtInst(
                new llvm::FCmpInst(*llvmObj->block, llvm::FCmpInst::FCMP_UGT, arg1, arg2, ""),
                Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                "", llvmObj->block
            );
        }
    }
    case Kind::Lt: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (arguments[0]->type->getEffectiveType()->kind == Type::Kind::Integer) {
            if (((IntegerType*)arguments[0]->type)->isSigned()) {
                return new llvm::SExtInst(
                    new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLT, arg1, arg2, ""),
                    Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                    "", llvmObj->block
                );
            } else {
                return new llvm::SExtInst(
                    new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_ULT, arg1, arg2, ""),
                    Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                    "", llvmObj->block
                );
            }

        } else {
            return new llvm::SExtInst(
                new llvm::FCmpInst(*llvmObj->block, llvm::FCmpInst::FCMP_ULT, arg1, arg2, ""),
                Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                "", llvmObj->block
            );
        }
    }
    case Kind::Gte: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (arguments[0]->type->getEffectiveType()->kind == Type::Kind::Integer) {
            if (((IntegerType*)arguments[0]->type)->isSigned()) {
                return new llvm::SExtInst(
                    new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SGE, arg1, arg2, ""),
                    Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                    "", llvmObj->block
                );
            } else {
                return new llvm::SExtInst(
                    new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_UGE, arg1, arg2, ""),
                    Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                    "", llvmObj->block
                );
            }

        } else {
            return new llvm::SExtInst(
                new llvm::FCmpInst(*llvmObj->block, llvm::FCmpInst::FCMP_UGE, arg1, arg2, ""),
                Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                "", llvmObj->block
            );
        }
    }
    case Kind::Lte: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (arguments[0]->type->getEffectiveType()->kind == Type::Kind::Integer) {
            if (((IntegerType*)arguments[0]->type)->isSigned()) {
                return new llvm::SExtInst(
                    new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLE, arg1, arg2, ""),
                    Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                    "", llvmObj->block
                );
            } else {
                return new llvm::SExtInst(
                    new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_ULE, arg1, arg2, ""),
                    Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                    "", llvmObj->block
                );
            }

        } else {
            return new llvm::SExtInst(
                new llvm::FCmpInst(*llvmObj->block, llvm::FCmpInst::FCMP_ULE, arg1, arg2, ""),
                Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                "", llvmObj->block
            );
        }
    }
    case Kind::Eq: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (arguments[0]->type->getEffectiveType()->kind == Type::Kind::Integer) {
            return new llvm::SExtInst(
                new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, arg1, arg2, ""),
                Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                "", llvmObj->block
            );
        } else {
            return new llvm::SExtInst(
                new llvm::FCmpInst(*llvmObj->block, llvm::FCmpInst::FCMP_UEQ, arg1, arg2, ""),
                Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                "", llvmObj->block
            );
        }
    }
    case Kind::Neq: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        if (arguments[0]->type->getEffectiveType()->kind == Type::Kind::Integer) {
            return new llvm::SExtInst(
                new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_NE, arg1, arg2, ""),
                Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                "", llvmObj->block
            );
        } else if (arguments[0]->type->getEffectiveType()->kind == Type::Kind::Float) {
            return new llvm::SExtInst(
                new llvm::FCmpInst(*llvmObj->block, llvm::FCmpInst::FCMP_UNE, arg1, arg2, ""),
                Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
                "", llvmObj->block
            );
        } else {
            internalError("!= operation expects integer and float types only in llvm creating, got " + DeclarationMap::toString(arguments[0]->type), position);
        }
    }
    case Kind::LogicalNot: {
        auto arg = arguments[0]->createLlvm(llvmObj);
        return new llvm::SExtInst(
            new llvm::TruncInst(llvm::BinaryOperator::CreateNot(arg, "", llvmObj->block), llvm::Type::getInt1Ty(llvmObj->context), "", llvmObj->block),
            Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
            "", llvmObj->block
        );
    }
    case Kind::LogicalAnd: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg1Result = new llvm::TruncInst(arg1, llvm::Type::getInt1Ty(llvmObj->context), "", llvmObj->block);
        createLlvmConditional(llvmObj, arg1Result, 
            [&](){
                auto arg2 = arguments[1]->createLlvm(llvmObj);
                auto arg2Result = new llvm::TruncInst(arg2, llvm::Type::getInt1Ty(llvmObj->context), "", llvmObj->block);
                new llvm::StoreInst(arg2Result, shortCircuitLlvmVar, llvmObj->block);
            }, 
            [&]() {
                new llvm::StoreInst(arg1Result, shortCircuitLlvmVar, llvmObj->block);
            }
        );
        return new llvm::SExtInst(
            new llvm::LoadInst(shortCircuitLlvmVar, "", llvmObj->block),
            Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
            "", llvmObj->block
        );
    }
    case Kind::LogicalOr: {  
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg1Result = new llvm::TruncInst(arg1, llvm::Type::getInt1Ty(llvmObj->context), "", llvmObj->block);
        createLlvmConditional(llvmObj, arg1Result, 
            [&](){
                new llvm::StoreInst(arg1Result, shortCircuitLlvmVar, llvmObj->block);
            }, 
            [&]() {
                auto arg2 = arguments[1]->createLlvm(llvmObj);
                auto arg2Result = new llvm::TruncInst(arg2, llvm::Type::getInt1Ty(llvmObj->context), "", llvmObj->block);
                new llvm::StoreInst(arg2Result, shortCircuitLlvmVar, llvmObj->block);
            }
        );
        return new llvm::SExtInst(
            new llvm::LoadInst(shortCircuitLlvmVar, "", llvmObj->block),
            Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
            "", llvmObj->block
        );
    }
    case Kind::LogicalXor: {  
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        return new llvm::SExtInst(
            llvm::BinaryOperator::CreateXor(
                new llvm::TruncInst(arg1, llvm::Type::getInt1Ty(llvmObj->context), "", llvmObj->block),
                new llvm::TruncInst(arg2, llvm::Type::getInt1Ty(llvmObj->context), "", llvmObj->block),
                "", llvmObj->block
            ), 
            Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
            "", llvmObj->block
        );
    }
    case Kind::LongCircuitLogicalAnd: {
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        return new llvm::SExtInst(
            new llvm::TruncInst(llvm::BinaryOperator::CreateAnd(arg1, arg2, "", llvmObj->block), llvm::Type::getInt1Ty(llvmObj->context), "", llvmObj->block),
            Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
            "", llvmObj->block
        );
    }
    case Kind::LongCircuitLogicalOr: {  
        auto arg1 = arguments[0]->createLlvm(llvmObj);
        auto arg2 = arguments[1]->createLlvm(llvmObj);
        return new llvm::SExtInst(
            new llvm::TruncInst(llvm::BinaryOperator::CreateOr(arg1, arg2, "", llvmObj->block), llvm::Type::getInt1Ty(llvmObj->context), "", llvmObj->block),
            Type::Create(Type::Kind::Bool)->createLlvm(llvmObj), 
            "", llvmObj->block
        );
    }
    case Kind::BitNeg: {
        auto arg = arguments[0]->createLlvm(llvmObj);
        return llvm::BinaryOperator::CreateNot(arg, "", llvmObj->block);
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
void CastOperation::templateCopy(CastOperation* operation, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    operation->argType = argType->templateCopy(parentScope, templateToType);
    Operation::templateCopy(operation, parentScope, templateToType);
}
Statement* CastOperation::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto operation = Create(position, nullptr);
    templateCopy(operation, parentScope, templateToType);
    return operation;
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
        containedErrorResolve = ((Operation*)arguments[0])->containedErrorResolve;
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
void CastOperation::createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj) {
    if (argIsLValue) {
        arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
    } else {
        arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
    }
}
void CastOperation::createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj) {
    arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
}
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
            } else {
                return nullptr;
            }
        } else if (k2 == Type::Kind::Bool || k2 == Type::Kind::Integer || k2 == Type::Kind::Float) {
            return new llvm::BitCastInst(arguments[0]->createLlvm(llvmObj), type->createLlvm(llvmObj), "", llvmObj->block);
        } else {
            internalError("unexpected type during cast llvm creating", arguments[0]->position);
            return nullptr;
        }
    } else {
        internalError("unexpected type during cast llvm creating", arguments[0]->position);
        return nullptr;
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
void ArrayIndexOperation::templateCopy(ArrayIndexOperation* operation, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    operation->index = (Value*)index->templateCopy(parentScope, templateToType);
    Operation::templateCopy(operation, parentScope, templateToType);
}
Statement* ArrayIndexOperation::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto operation = Create(position, nullptr);
    templateCopy(operation, parentScope, templateToType);
    return operation;
}
optional<Value*> ArrayIndexOperation::interpret(Scope* scope) {
    if (!interpretAllArguments(scope)) {
        return nullopt;
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
    default: {
        auto functionCall = FunctionCallOperation::Create(position);
        functionCall->arguments = arguments;
        auto indexInterpret = index->interpret(scope);
        if (!indexInterpret) return nullopt;
        if (indexInterpret.value()) index = indexInterpret.value();
        functionCall->arguments.push_back(index);
        functionCall->function = Variable::Create(position, kindToFunctionName(kind));
        auto functionCallInterpret = functionCall->interpret(scope);
        if (!functionCallInterpret) return nullopt;
        if (functionCallInterpret.value()) return functionCallInterpret.value();
        else return functionCall;
    }
    }

    index = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {index});
    auto indexInterpret = index->interpret(scope);
    if (!indexInterpret) return nullopt;
    if (indexInterpret.value()) index = indexInterpret.value();

    if (index->isConstexpr && arguments[0]->isConstexpr && arguments[0]->valueKind == Value::ValueKind::StaticArray) {
        auto& staticArrayValues = ((StaticArrayValue*)arguments[0])->values;
        auto indexValue = ((IntegerValue*)index)->value;
        if (indexValue >= staticArrayValues.size()) {
            return errorMessageOpt("array index outside the bounds of an array", position);
        }
        return staticArrayValues[indexValue];
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
void ArrayIndexOperation::createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj) {
    createAllocaLlvmIfNeededForReference(llvmObj);
}
void ArrayIndexOperation::createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj) {
    auto effType = arguments[0]->type->getEffectiveType();
    if (effType->kind == Type::Kind::StaticArray) {
        arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
    } else if (effType->kind == Type::Kind::DynamicArray) {
        if (isLvalue(arguments[0])) {
            arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        } else {
            arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        }
    } else if (effType->kind == Type::Kind::ArrayView) {
        if (isLvalue(arguments[0])) {
            arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        } else {
            arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        }
    }
}
llvm::Value* ArrayIndexOperation::getReferenceLlvm(LlvmObject* llvmObj) {
    auto effType = arguments[0]->type->getEffectiveType();
    llvm::Value* data;
    vector<llvm::Value*> indexList;
    if (effType->kind == Type::Kind::StaticArray) {
        data = arguments[0]->getReferenceLlvm(llvmObj);
        indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
    } else if (effType->kind == Type::Kind::DynamicArray) {
        if (isLvalue(arguments[0])) {
            data = new llvm::LoadInst(((DynamicArrayType*)effType)->llvmGepData(llvmObj, arguments[0]->getReferenceLlvm(llvmObj)), "", llvmObj->block);
        } else {
            data = ((DynamicArrayType*)effType)->llvmExtractData(llvmObj, arguments[0]->createLlvm(llvmObj));
        }
    } else if (effType->kind == Type::Kind::ArrayView) {
        if (isLvalue(arguments[0])) {
            data = new llvm::LoadInst(((ArrayViewType*)effType)->llvmGepData(llvmObj, arguments[0]->getReferenceLlvm(llvmObj)), "", llvmObj->block);
        } else {
            data = ((ArrayViewType*)effType)->llvmExtractData(llvmObj, arguments[0]->createLlvm(llvmObj));
        }
    } else {
        internalError("not array type argument in index operation");
        return nullptr;
    }
    indexList.push_back(index->createLlvm(llvmObj));
    return llvm::GetElementPtrInst::Create(((llvm::PointerType*)data->getType())->getElementType(), data, indexList, "", llvmObj->block);
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
void ArraySubArrayOperation::templateCopy(ArraySubArrayOperation* operation, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    operation->firstIndex = (Value*)firstIndex->templateCopy(parentScope, templateToType);
    operation->secondIndex = (Value*)secondIndex->templateCopy(parentScope, templateToType);
    Operation::templateCopy(operation, parentScope, templateToType);
}
Statement* ArraySubArrayOperation::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto operation = Create(position, nullptr, nullptr);
    templateCopy(operation, parentScope, templateToType);
    return operation;
}
optional<Value*> ArraySubArrayOperation::interpret(Scope* scope) {
    if (!interpretAllArguments(scope)) {
        return nullopt;
    }

    switch (arguments[0]->type->getEffectiveType()->kind) {
    case Type::Kind::StaticArray:
        type = ArrayViewType::Create(((StaticArrayType*)arguments[0]->type->getEffectiveType())->elementType);
        break;
    case Type::Kind::DynamicArray:
        type = ArrayViewType::Create(((DynamicArrayType*)arguments[0]->type->getEffectiveType())->elementType);
        break;
    case Type::Kind::ArrayView:
        type = ArrayViewType::Create(((ArrayViewType*)arguments[0]->type->getEffectiveType())->elementType);
        break;
    default:{
        auto functionCall = FunctionCallOperation::Create(position);
        functionCall->arguments = arguments;
        auto indexInterpret = firstIndex->interpret(scope);
        if (!indexInterpret) return nullopt;
        if (indexInterpret.value()) firstIndex = indexInterpret.value();
        functionCall->arguments.push_back(firstIndex);
        indexInterpret = secondIndex->interpret(scope);
        if (!indexInterpret) return nullopt;
        if (indexInterpret.value()) secondIndex = indexInterpret.value();
        functionCall->arguments.push_back(secondIndex);
        functionCall->function = Variable::Create(position, kindToFunctionName(kind));
        auto functionCallInterpret = functionCall->interpret(scope);
        if (!functionCallInterpret) return nullopt;
        if (functionCallInterpret.value()) return functionCallInterpret.value();
        else return functionCall;
    }
    }

    firstIndex = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {firstIndex});
    auto indexInterpret = firstIndex->interpret(scope);
    if (!indexInterpret) return nullopt;
    if (indexInterpret.value()) firstIndex = indexInterpret.value();
    secondIndex = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {secondIndex});
    indexInterpret = secondIndex->interpret(scope);
    if (!indexInterpret) return nullopt;
    if (indexInterpret.value()) secondIndex = indexInterpret.value();

    if (firstIndex->type->kind != Type::Kind::Integer) {
        return errorMessageOpt("array sub-array first index must be an integer value, got " 
            + DeclarationMap::toString(firstIndex->type), position
        );
    }
    if (secondIndex->type->kind != Type::Kind::Integer) {
        return errorMessageOpt("array sub-array second index must be an integer value, got " 
            + DeclarationMap::toString(secondIndex->type), position
        );
    }

    Value* firstIndexMinus1 = Operation::Create(position, Operation::Kind::Sub);
    ((Operation*)firstIndexMinus1)->arguments = {firstIndex, IntegerValue::Create(position, 1)};
    auto firstIndexMinus1OperInterpret = firstIndexMinus1->interpret(scope);
    if (!firstIndexMinus1OperInterpret) return nullopt;
    if (firstIndexMinus1OperInterpret.value()) firstIndexMinus1 = firstIndexMinus1OperInterpret.value();

    size = Operation::Create(position, Operation::Kind::Sub);
    ((Operation*)size)->arguments = {secondIndex, firstIndexMinus1};
    auto sizeInterpret = size->interpret(scope);
    if (!sizeInterpret) return nullopt;
    if (sizeInterpret.value()) size = sizeInterpret.value();

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
void ArraySubArrayOperation::createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj) {
    auto effType = arguments[0]->type->getEffectiveType();
    if (effType->kind == Type::Kind::StaticArray) {
        arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
    } else if (effType->kind == Type::Kind::DynamicArray) {
        if (isLvalue(arguments[0])) {
            arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        } else {
            arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        }
    } else if (effType->kind == Type::Kind::ArrayView) {
        if (isLvalue(arguments[0])) {
            arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        } else {
            arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        }
    }
}
void ArraySubArrayOperation::createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj) {
    if (!viewRef) {
        viewRef = ((ArrayViewType*)type)->allocaLlvm(llvmObj);
        createAllocaLlvmIfNeededForValue(llvmObj);
    }
}
llvm::Value* ArraySubArrayOperation::getReferenceLlvm(LlvmObject* llvmObj) {
    auto effType = arguments[0]->type->getEffectiveType();
    auto thisViewType = (ArrayViewType*)type;
    auto firstIndexValue = firstIndex->createLlvm(llvmObj);
    auto sizeValue = size->createLlvm(llvmObj);
    new llvm::StoreInst(sizeValue, thisViewType->llvmGepSize(llvmObj, viewRef), llvmObj->block);
    if (effType->kind == Type::Kind::StaticArray) {
        auto staticArrayType = (StaticArrayType*)effType;
        auto arg = arguments[0]->getReferenceLlvm(llvmObj);
        vector<llvm::Value*> indexList;
        indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
        indexList.push_back(firstIndexValue);
        auto firstElementPtr = llvm::GetElementPtrInst::Create(((llvm::PointerType*)arg->getType())->getElementType(), arg, indexList, "", llvmObj->block);
        new llvm::StoreInst(
            new llvm::BitCastInst(firstElementPtr, RawPointerType::Create(thisViewType->elementType)->createLlvm(llvmObj), "", llvmObj->block), 
            thisViewType->llvmGepData(llvmObj, viewRef),
            llvmObj->block
        );
    } else if (effType->kind == Type::Kind::DynamicArray) {
        auto dynamicArrayType = (DynamicArrayType*)effType;
        llvm::Value* data;
        if (Value::isLvalue(arguments[0])) {
            data = dynamicArrayType->llvmGepData(llvmObj, arguments[0]->getReferenceLlvm(llvmObj));
        } else {
            data = dynamicArrayType->llvmExtractData(llvmObj, arguments[0]->createLlvm(llvmObj));
        }
        auto firstElementPtr = llvm::GetElementPtrInst::Create(((llvm::PointerType*)data->getType())->getElementType(), data, firstIndexValue, "", llvmObj->block);
        new llvm::StoreInst(firstElementPtr, thisViewType->llvmGepData(llvmObj, viewRef), llvmObj->block);
    } else if (effType->kind == Type::Kind::ArrayView) {
        auto arrayViewType = (ArrayViewType*)effType;
        llvm::Value* data;
        if (Value::isLvalue(arguments[0])) {
            data = arrayViewType->llvmGepData(llvmObj, arguments[0]->getReferenceLlvm(llvmObj));
        } else {
            data = arrayViewType->llvmExtractData(llvmObj, arguments[0]->createLlvm(llvmObj));
        }
        auto firstElementPtr = llvm::GetElementPtrInst::Create(((llvm::PointerType*)data->getType())->getElementType(), data, firstIndexValue, "", llvmObj->block);
        new llvm::StoreInst(firstElementPtr, thisViewType->llvmGepData(llvmObj, viewRef), llvmObj->block);
    }

    return viewRef;
}
llvm::Value* ArraySubArrayOperation::createLlvm(LlvmObject* llvmObj) {
    auto effType = arguments[0]->type->getEffectiveType();
    auto thisViewType = (ArrayViewType*)type;
    auto firstIndexValue = firstIndex->createLlvm(llvmObj);
    auto sizeValue = size->createLlvm(llvmObj);
    llvm::Value* viewValue = llvm::ConstantStruct::get((llvm::StructType*)thisViewType->createLlvm(llvmObj), {
        llvm::UndefValue::get(IntegerType::Create(IntegerType::Size::I64)->createLlvm(llvmObj)),
        llvm::UndefValue::get(RawPointerType::Create(thisViewType->elementType)->createLlvm(llvmObj))
    });
    viewValue = llvm::InsertValueInst::Create(viewValue, sizeValue, 0, "", llvmObj->block);
    if (effType->kind == Type::Kind::StaticArray) {
        auto staticArrayType = (StaticArrayType*)effType;
        auto arg = arguments[0]->getReferenceLlvm(llvmObj);
        vector<llvm::Value*> indexList;
        indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
        indexList.push_back(firstIndexValue);
        auto firstElementPtr = llvm::GetElementPtrInst::Create(((llvm::PointerType*)arg->getType())->getElementType(), arg, indexList, "", llvmObj->block);
        viewValue = llvm::InsertValueInst::Create(
            viewValue, 
            new llvm::BitCastInst(firstElementPtr, RawPointerType::Create(thisViewType->elementType)->createLlvm(llvmObj), "", llvmObj->block),
            1, "", llvmObj->block
        );
    } else if (effType->kind == Type::Kind::DynamicArray) {
        auto dynamicArrayType = (DynamicArrayType*)effType;
        llvm::Value* data;
        if (Value::isLvalue(arguments[0])) {
            data = new llvm::LoadInst(dynamicArrayType->llvmGepData(llvmObj, arguments[0]->getReferenceLlvm(llvmObj)), "", llvmObj->block);
        } else {
            data = dynamicArrayType->llvmExtractData(llvmObj, arguments[0]->createLlvm(llvmObj));
        }
        auto firstElementPtr = llvm::GetElementPtrInst::Create(((llvm::PointerType*)data->getType())->getElementType(), data, firstIndexValue, "", llvmObj->block);
        viewValue = llvm::InsertValueInst::Create(viewValue, firstElementPtr, 1, "", llvmObj->block);
    } else if (effType->kind == Type::Kind::ArrayView) {
        auto arrayViewType = (ArrayViewType*)effType;
        llvm::Value* data;
        if (Value::isLvalue(arguments[0])) {
            data = new llvm::LoadInst(arrayViewType->llvmGepData(llvmObj, arguments[0]->getReferenceLlvm(llvmObj)), "", llvmObj->block);
        } else {
            data = arrayViewType->llvmExtractData(llvmObj, arguments[0]->createLlvm(llvmObj));
        }
        auto firstElementPtr = llvm::GetElementPtrInst::Create(((llvm::PointerType*)data->getType())->getElementType(), data, firstIndexValue, "", llvmObj->block);
        viewValue = llvm::InsertValueInst::Create(viewValue, firstElementPtr, 1, "", llvmObj->block);
    }

    return viewValue;
}



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
void FunctionCallOperation::templateCopy(FunctionCallOperation* operation, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    operation->function = (Value*)function->templateCopy(parentScope, templateToType);
    Operation::templateCopy(operation, parentScope, templateToType);
}
Statement* FunctionCallOperation::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    if (function->valueKind == ValueKind::Variable) {
        auto functionName = ((Variable*)function)->name;
        auto foundType = templateToType.find(functionName);
        if (foundType != templateToType.end()) {
            return ConstructorOperation::Create(position, foundType->second, arguments, false, true);
        }
    }
    
    auto operation = Create(position);
    templateCopy(operation, parentScope, templateToType);
    return operation;
}
bool FunctionCallOperation::createTemplateFunctionCall(Scope* scope, Declaration* declaration) {
    auto funType = (TemplateFunctionType*)declaration->variable->type;
    auto implementation = funType->getImplementation(scope, declaration);
    if (!implementation->interpret(scope)) {
        return false;
    }
    function = implementation;
    type = ((FunctionType*)implementation->type)->returnType;
    return true;
}
int considerPriorities(const vector<Type*>& funArgTypes0, const vector<Type*>& funArgTypes1) {
    int winner = -1;
    for (int i = 0; i < funArgTypes0.size(); ++i) {
        if (cmpPtr(funArgTypes0[i]->getEffectiveType(), funArgTypes1[i]->getEffectiveType())) {
            if (funArgTypes0[i]->kind == Type::Kind::Reference) {
                if (funArgTypes1[i]->kind != Type::Kind::Reference) {
                    if (winner == 1) {
                        winner = -2; 
                        break;
                    } else {
                        winner = 0;
                    }
                }
            } else {
                if (funArgTypes1[i]->kind == Type::Kind::Reference) {
                    if (winner == 0) {
                        winner = -2;
                        break;
                    } else {
                        winner = 1;
                    }
                }
            }
        }
    }
    return winner;
}
void considerPriorities(vector<Declaration*>& matches, const CodePosition& position) {
    if (matches.size() > 1) {
        int winner = -1;
        do {
            auto& funArgTypes0 = ((FunctionType*)matches[0]->variable->type)->argumentTypes;
            auto& funArgTypes1 = ((FunctionType*)matches[1]->variable->type)->argumentTypes;

            winner = considerPriorities(funArgTypes0, funArgTypes1);

            if (winner == -1) internalError("found 2 perfect functions which differ in more then argument references", position);
            if (winner == 0) {
                matches.erase(matches.begin()+1);
            } else if (winner == 1) {
                matches.erase(matches.begin());
            }
        } while (matches.size() > 1 && winner != -2);
    }
}
void considerPriorities(vector<pair<Declaration*, vector<ConstructorOperation*>>>& matches, const CodePosition& position) {
    if (matches.size() > 1) {
        int winner = -1;
        do {
            auto& funArgTypes0 = ((FunctionType*)matches[0].first->variable->type)->argumentTypes;
            auto& funArgTypes1 = ((FunctionType*)matches[1].first->variable->type)->argumentTypes;

            winner = considerPriorities(funArgTypes0, funArgTypes1);

            if (winner == -1) internalError("found 2 perfect functions which differ in more then argument references", position);
            if (winner == 0) {
                matches.erase(matches.begin()+1);
            } else if (winner == 1) {
                matches.erase(matches.begin());
            }
        } while (matches.size() > 1 && winner != -2);
    }
}
FunctionCallOperation::FindFunctionStatus FunctionCallOperation::findFunction(Scope* scope, Scope* searchScope, string functionName, const vector<Type*>& templatedTypes) {
    const auto& declarations = searchScope->declarationMap.getDeclarations(functionName);
    vector<Declaration*> viableDeclarations;
    vector<Declaration*> perfectMatches;

    if (templatedTypes.empty()) {
        // try to perfect find perfect non-template function
        for (const auto declaration : declarations) {
            auto functionType = (FunctionType*)declaration->variable->type;
            if (functionType->kind == Type::Kind::TemplateFunction) {
                continue;
            }
            if (functionType->argumentTypes.size() == arguments.size()) {
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
        considerPriorities(perfectMatches, position);
        if (perfectMatches.size() == 1) {
            function = perfectMatches.back()->value;
            idName = searchScope->declarationMap.getIdName(perfectMatches.back());
            type = ((FunctionType*)perfectMatches.back()->variable->type)->returnType;
            return FindFunctionStatus::Success;
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
        }
    }

    // no perfect non-template function. try to find perfect template function
    for (const auto declaration : declarations) {
        if (declaration->variable->type->kind != Type::Kind::TemplateFunction) {
            continue;
        }
        auto functionType = (TemplateFunctionType*)declaration->variable->type;
        functionType->implementationTypes.clear();
        functionType->implementationTypes.resize(functionType->templateTypes.size(), nullptr);
        std::vector<Type*> argumentTypes;
        if (functionType->implementationTypes.size() < templatedTypes.size()) {
            continue;
        }
        if (templatedTypes.size() > 0) {
            unordered_map<string, Type*> templateToType;
            for (int i = 0; i < templatedTypes.size(); ++i) {
                templateToType[functionType->templateTypes[i]->name] = templatedTypes[i];
                functionType->implementationTypes[i] = templatedTypes[i];
            }
            for (int i = 0; i < functionType->argumentTypes.size(); ++i) {
                argumentTypes.push_back(functionType->argumentTypes[i]->templateCopy(scope, templateToType));
            }
        } else {
            argumentTypes = functionType->argumentTypes;
        }
        if (argumentTypes.size() == arguments.size()) {
            bool allMatch = true;
            bool fail = false;
            for (int i = 0; i < argumentTypes.size(); ++i) {
                if (argumentTypes[i]->kind == Type::Kind::Reference && !isLvalue(arguments[i])) {
                    allMatch = false;
                }
                auto matchResult = argumentTypes[i]->getEffectiveType()->matchTemplate(functionType, arguments[i]->type->getEffectiveType());
                if (matchResult == MatchTemplateResult::Viable) {
                    allMatch = false;
                } else if (matchResult == MatchTemplateResult::Fail) {
                    allMatch = false;
                    fail = true;
                    break;
                }
            }
            for (auto implementationType : functionType->implementationTypes) {
                if (!implementationType) fail = true;
            }
            if (allMatch) {
                perfectMatches.push_back(declaration);
            } else if (!fail) {
                viableDeclarations.push_back(declaration);
            }
        }
    }
    if (perfectMatches.size() == 1) {
        if (createTemplateFunctionCall(searchScope, perfectMatches.back())) {
            return FindFunctionStatus::Success;
        } else {
            return FindFunctionStatus::Error;
        }
    } else if (perfectMatches.size() > 1) {
        int minTemplateArgsCount = ((TemplateFunctionType*)perfectMatches[0]->variable->type)->templateTypes.size();
        for (int i = 1; i < perfectMatches.size(); ++i) {
            minTemplateArgsCount = min(minTemplateArgsCount, (int)((TemplateFunctionType*)perfectMatches[i]->variable->type)->templateTypes.size());
        }
        perfectMatches.erase(remove_if(perfectMatches.begin(), perfectMatches.end(), [&](Declaration* decl){
            return ((TemplateFunctionType*)decl->variable->type)->templateTypes.size() > minTemplateArgsCount;
        }), perfectMatches.end());
        int argCount = ((TemplateFunctionType*)perfectMatches[0]->variable->type)->argumentTypes.size();
        for (int i = perfectMatches.size() - 1; i >= 1; --i) {
            int winner = 0;
            auto funType1 = ((TemplateFunctionType*)perfectMatches[i]->variable->type);
            auto funType2 = ((TemplateFunctionType*)perfectMatches[i-1]->variable->type);
            for (int j = 0; j < argCount; ++j) {
                auto cmpResult = funType1->argumentTypes[j]->getEffectiveType()->compareTemplateDepth(funType2->argumentTypes[j]->getEffectiveType());
                if (cmpResult == -2) {
                    winner = -2;
                    break;
                } else if (cmpResult == -1) {
                    if (winner == 1) {
                        winner = -2;
                        break;
                    }
                    winner = -1;
                } else if (cmpResult == 1) {
                    if (winner == -1) {
                        winner = -2;
                        break;
                    }
                    winner = 1;
                }
            }
            if (winner == -1) {
                perfectMatches.pop_back();
            }
            else if (winner == 1) {
                perfectMatches.erase(perfectMatches.end()-2);
            }
        }
        considerPriorities(perfectMatches, position);
        if (perfectMatches.size() == 1) {
            if (createTemplateFunctionCall(searchScope, perfectMatches.back())) {
                return FindFunctionStatus::Success;
            } else {
                return FindFunctionStatus::Error;
            }
        } else {
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
        }
    } 

    // no perfect functions
    vector<optional<vector<ConstructorOperation*>>> neededCtors;
    for (Declaration* declaration : viableDeclarations) {
        neededCtors.push_back(vector<ConstructorOperation*>());
        vector<Type*> argumentTypes;
        if (declaration->variable->type->kind == Type::Kind::Function) {
            argumentTypes = ((FunctionType*)declaration->variable->type)->argumentTypes;
        } else {
            auto templateType = (TemplateFunctionType*)declaration->variable->type;
            for (auto argType : templateType->argumentTypes) {
                argumentTypes.push_back(argType->substituteTemplate(templateType));
            }
        }
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

    vector<pair<Declaration*, vector<ConstructorOperation*>>> possibleDeclarations;
    for (int i = 0; i < neededCtors.size(); ++i) {
        if (neededCtors[i]) {
            possibleDeclarations.push_back({viableDeclarations[i], neededCtors[i].value()});
        }
    }

    if (possibleDeclarations.empty()) {
        return FindFunctionStatus::Fail;
    } 
    if (possibleDeclarations.size() > 1) {
        int minTemplateArgsCount = INT_MAX;
        for (int i = 0; i < possibleDeclarations.size(); ++i) {
            if (possibleDeclarations[0].first->variable->type->kind == Type::Kind::Function) {
                minTemplateArgsCount = 0;
                break;
            } else {
                minTemplateArgsCount = min(minTemplateArgsCount, (int)((TemplateFunctionType*)possibleDeclarations[0].first->variable->type)->templateTypes.size());
            }
        }
        possibleDeclarations.erase(remove_if(possibleDeclarations.begin(), possibleDeclarations.end(), [&](pair<Declaration*, vector<ConstructorOperation*>> decl){
            return decl.first->variable->type->kind == Type::Kind::TemplateFunction && ((TemplateFunctionType*)decl.first->variable->type)->templateTypes.size() > minTemplateArgsCount;
        }), possibleDeclarations.end());
    }
    considerPriorities(possibleDeclarations, position);
    if (possibleDeclarations.size() > 1) {
        string message = "ambogous function call. ";
        message += "Possible functions at lines: ";
        for (int i = 0; i < possibleDeclarations.size(); ++i) {
            message += to_string(possibleDeclarations[i].first->position.lineNumber);
            if (i != possibleDeclarations.size() - 1) {
                message += ", ";
            }
        }
        errorMessageBool(message, position);
        return FindFunctionStatus::Error;
    }

    for (int i = 0; i < arguments.size(); ++i) {
        auto ctor = possibleDeclarations[0].second[i];
        if (ctor) {
            auto ctorInterpret = ctor->interpret(scope);
            if (ctorInterpret.value()) {
                arguments[i] = ctorInterpret.value();
            } else {
                arguments[i] = ctor;
            }
        }
    }
    if (possibleDeclarations[0].first->variable->type->kind == Type::Kind::Function) {
        function = possibleDeclarations[0].first->value;
        type = ((FunctionType*)possibleDeclarations[0].first->variable->type)->returnType;
        idName = searchScope->declarationMap.getIdName(possibleDeclarations[0].first);
    } else {
        if (!createTemplateFunctionCall(searchScope, possibleDeclarations[0].first)) {
            return FindFunctionStatus::Error;
        }
    }
    
    return FindFunctionStatus::Success;
}
Type* stringToBasicType(const std::string& str) {
    if (str == "string") return DynamicArrayType::Create(IntegerType::Create(IntegerType::Size::U8));
    if (str == "int")    return IntegerType::Create(IntegerType::Size::I64);
    if (str == "float")  return FloatType::Create(FloatType::Size::F64);
    if (str == "byte")   return IntegerType::Create(IntegerType::Size::U8);
    if (str == "bool")   return Type::Create(Type::Kind::Bool);
    if (str == "void")   return Type::Create(Type::Kind::Void);
    if (str == "i64")    return IntegerType::Create(IntegerType::Size::I64);
    if (str == "i32")    return IntegerType::Create(IntegerType::Size::I32);
    if (str == "i16")    return IntegerType::Create(IntegerType::Size::I16);
    if (str == "i8")     return IntegerType::Create(IntegerType::Size::I8);
    if (str == "u64")    return IntegerType::Create(IntegerType::Size::U64);
    if (str == "u32")    return IntegerType::Create(IntegerType::Size::U32);
    if (str == "u16")    return IntegerType::Create(IntegerType::Size::U16);
    if (str == "u8")     return IntegerType::Create(IntegerType::Size::U8);
    if (str == "f64")    return FloatType::Create(FloatType::Size::F64);
    if (str == "f32")    return FloatType::Create(FloatType::Size::F32);
    return nullptr;
}
optional<Value*> FunctionCallOperation::interpret(Scope* scope) {
    if (wasInterpreted) {
        return nullptr;
    }
    wasInterpreted = true;

    // first check if is it class constructor. if yes -> its ConstructorOperation
    string className = "";
    vector<Type*> templatedArguments;
    if (function->valueKind == Value::ValueKind::Variable) {
        className = ((Variable*)function)->name;
    } else if (function->valueKind == Value::ValueKind::TemplatedVariable) {
        className = ((TemplatedVariable*)function)->name;
        templatedArguments = ((TemplatedVariable*)function)->templatedTypes;
    }
    if (!className.empty()) {
        if (templatedArguments.empty()) {
            auto basicType = stringToBasicType(className);
            if (basicType) {
                auto op = ConstructorOperation::Create(position, basicType, arguments, false, true);
                auto opInterpret = op->interpret(scope);
                if (!opInterpret) return nullopt;
                if (opInterpret.value()) return opInterpret.value();
                else return op;
            }
        }

        Scope* searchScope = scope; 
        auto classDeclaration = searchScope->classDeclarationMap.getDeclaration(className);
        while (searchScope->parentScope && !classDeclaration) {
            searchScope = searchScope->parentScope;
            classDeclaration = searchScope->classDeclarationMap.getDeclaration(className);
        }
        if (classDeclaration) {
            classDeclaration = classDeclaration->get(templatedArguments);
            if (!classDeclaration->interpret({})) return nullopt;
            auto constructorType = ClassType::Create(classDeclaration->name);
            constructorType->declaration = classDeclaration;
            constructorType->templateTypes = templatedArguments;
            auto op = ConstructorOperation::Create(position, constructorType, arguments, false, true);
            auto opInterpret = op->interpret(scope);
            if (!opInterpret) return nullopt;
            if (opInterpret.value()) return opInterpret.value();
            else return op;
        }
    }
    

    if (!interpretAllArguments(scope)) {
        return nullopt;
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
    } 
    else if (function->valueKind == Value::ValueKind::TemplatedVariable) {
        auto templatedVariable = (TemplatedVariable*)function;
        FindFunctionStatus status;
        Scope* actualScope = scope;
        do {
            status = findFunction(scope, actualScope, templatedVariable->name, templatedVariable->templatedTypes);
            if (status == FindFunctionStatus::Error) {
                return nullopt;
            }
            actualScope = actualScope->parentScope;
        } while (status != FindFunctionStatus::Success && actualScope);
        if (status != FindFunctionStatus::Success) {
            return errorMessageOpt("no fitting function to call", position);
        }
    }
    else if (function->valueKind == Value::ValueKind::Operation
        && (((Operation*)function)->kind == Operation::Kind::Dot)
        && ((Variable*)((Operation*)function)->arguments[1])->isConstexpr) {
        auto dotOperation = (DotOperation*)function;
        auto varName = ((Variable*)dotOperation->arguments[1])->name;
        auto arg0Type = dotOperation->arguments[0]->type;

        if (arg0Type->getEffectiveType()->kind == Type::Kind::DynamicArray) {
            auto interpretFunction = arg0Type->getEffectiveType()->interpretFunction(position, scope, varName, arguments);
            if (!interpretFunction) return nullopt;
            type = interpretFunction.value().first;
            classConstructor = interpretFunction.value().second;
            buildInFunctionName = varName;
        } else {
            ClassScope* classScope = nullptr;

            if (arg0Type->kind == Type::Kind::Class) {
                classScope = ((ClassType*)arg0Type)->declaration->body;
                auto thisArgument = Operation::Create(position, Operation::Kind::Address);
                thisArgument->arguments.push_back(dotOperation->arguments[0]);
                if (!thisArgument->interpret(scope)) return nullopt;
                arguments.push_back(thisArgument);
            } else if (arg0Type->kind == Type::Kind::Reference){
                classScope = ((ClassType*)((ReferenceType*)arg0Type)->underlyingType)->declaration->body;
                auto thisArgument = Operation::Create(position, Operation::Kind::Address);
                thisArgument->arguments.push_back(dotOperation->arguments[0]);
                if (!thisArgument->interpret(scope)) return nullopt;
                arguments.push_back(thisArgument);
            } else if (arg0Type->kind == Type::Kind::OwnerPointer){
                classScope = ((ClassType*)((OwnerPointerType*)arg0Type)->underlyingType)->declaration->body;
                auto thisArgument = CastOperation::Create(position, RawPointerType::Create(((OwnerPointerType*)arg0Type)->underlyingType));
                thisArgument->arguments.push_back(dotOperation->arguments[0]);
                if (!thisArgument->interpret(scope)) return nullopt;
                arguments.push_back(thisArgument);
            } else if (arg0Type->kind == Type::Kind::MaybeError){
                classScope = ((ClassType*)((MaybeErrorType*)arg0Type)->underlyingType)->declaration->body;
                auto thisArgument = Operation::Create(position, Operation::Kind::Address);
                auto thisValue = Operation::Create(position, Operation::Kind::GetValue);
                thisValue->arguments.push_back(dotOperation->arguments[0]);
                thisArgument->arguments.push_back(thisValue);
                if (!thisArgument->interpret(scope)) return nullopt;
                arguments.push_back(thisArgument);
            } else if (arg0Type->kind == Type::Kind::RawPointer){
                classScope = ((ClassType*)((RawPointerType*)arg0Type)->underlyingType)->declaration->body;
                arguments.push_back(dotOperation->arguments[0]);
            }

            switch (findFunction(scope, classScope, varName)) {
            case FindFunctionStatus::Error:   return nullopt;
            case FindFunctionStatus::Fail:    return errorMessageOpt("no fitting class function to call", position);
            case FindFunctionStatus::Success: break;
            }
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
void FunctionCallOperation::createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj) {
    if (!buildInFunctionName.empty()) {
        
    } else {
        auto functionType = (FunctionType*)function->type;
        for (int i = 0; i < arguments.size(); ++i) {
            if (functionType->argumentTypes[i]->kind == Type::Kind::Reference) {
                arguments[i]->createAllocaLlvmIfNeededForReference(llvmObj);
                argRefs.push_back(nullptr);
            } else {
                if (isLvalue(arguments[i])) {
                    argRefs.push_back(arguments[i]->type->createRefForLlvmCopy(llvmObj, arguments[i]));
                } else {
                    arguments[i]->createAllocaLlvmIfNeededForValue(llvmObj);
                    argRefs.push_back(nullptr);
                }
            }
        }
        function->createAllocaLlvmIfNeededForValue(llvmObj);

        if (functionType->returnType->kind != Type::Kind::Reference && functionType->returnType->needsReference()) {
            llvmRef = functionType->returnType->allocaLlvm(llvmObj);
        }
    }
}
void FunctionCallOperation::createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj) {
    if (!buildInFunctionName.empty()) {

    } else {
        auto functionType = (FunctionType*)function->type;
        for (int i = 0; i < arguments.size(); ++i) {
            if (functionType->argumentTypes[i]->kind == Type::Kind::Reference) {
                arguments[i]->createAllocaLlvmIfNeededForReference(llvmObj);
                argRefs.push_back(nullptr);
            } else {
                if (isLvalue(arguments[i])) {
                    argRefs.push_back(arguments[i]->type->createRefForLlvmCopy(llvmObj, arguments[i]));
                } else {
                    arguments[i]->createAllocaLlvmIfNeededForValue(llvmObj);
                    argRefs.push_back(nullptr);
                }
            }
        }
        function->createAllocaLlvmIfNeededForValue(llvmObj);

        if (functionType->returnType->kind != Type::Kind::Reference) {
            llvmRef = functionType->returnType->allocaLlvm(llvmObj);
        }
    }
}
llvm::Value* FunctionCallOperation::createLlvmCall(LlvmObject* llvmObj) {
    vector<llvm::Value*> args;
    auto functionType = (FunctionType*)function->type;
    for (int i = 0; i < arguments.size(); ++i) {
        if (functionType->argumentTypes[i]->kind == Type::Kind::Reference) {
            args.push_back(arguments[i]->getReferenceLlvm(llvmObj));
        } else {
            if (isLvalue(arguments[i])) {
                args.push_back(arguments[i]->type->createLlvmCopy(llvmObj, arguments[i], argRefs[i]));
            } else {
                arguments[i]->wasCaptured = true;
                args.push_back(arguments[i]->createLlvm(llvmObj));
            }
        }
    }
    return llvm::CallInst::Create(function->createLlvm(llvmObj), args, "", llvmObj->block);
}
llvm::Value* FunctionCallOperation::createLlvm(LlvmObject* llvmObj) {
    if (!buildInFunctionName.empty()) {
        auto dotOperation = (DotOperation*)function;
        auto arg0EffType = dotOperation->arguments[0]->type->getEffectiveType();
        if (arg0EffType->kind == Type::Kind::DynamicArray) {
            auto result = arg0EffType->createFunctionLlvmValue(buildInFunctionName, llvmObj, dotOperation->arguments[0]->getReferenceLlvm(llvmObj), arguments, classConstructor);
            llvmRef = result.first;
            llvmValue = result.second;
            return llvmValue;
        } else {
            internalError("didn't find matching buildIn dot function, but buildInFunctionName is not empty");
        }
    }
    
    auto callResult = createLlvmCall(llvmObj);
    auto functionType = (FunctionType*)function->type;
    if (functionType->returnType->kind == Type::Kind::Reference) {
        llvmRef = callResult;
        llvmValue = new llvm::LoadInst(callResult, "", llvmObj->block);
    } else if (functionType->returnType->needsReference()) {
        llvmValue = callResult;
        new llvm::StoreInst(llvmValue, llvmRef, llvmObj->block);
        return llvmValue;
    } else {
        llvmValue = callResult;
    }
    return llvmValue;
}
llvm::Value* FunctionCallOperation::getReferenceLlvm(LlvmObject* llvmObj) {
    if (!buildInFunctionName.empty()) {
        auto dotOperation = (DotOperation*)function;
        auto arg0EffType = dotOperation->arguments[0]->type->getEffectiveType();
        if (arg0EffType->kind == Type::Kind::DynamicArray) {
            llvmRef = arg0EffType->createFunctionLlvmReference(buildInFunctionName, llvmObj, dotOperation->arguments[0]->getReferenceLlvm(llvmObj), arguments, classConstructor);
            return llvmRef;
        } else {
            internalError("didn't find matching buildIn dot function, but buildInFunctionName is not empty");
        }
    }

    auto functionType = (FunctionType*)function->type;
    if (functionType->returnType->kind == Type::Kind::Reference) {
        llvmRef = createLlvmCall(llvmObj);
        return llvmRef;
    }
    new llvm::StoreInst(createLlvmCall(llvmObj), llvmRef, llvmObj->block);
    return llvmRef;
}
void FunctionCallOperation::createDestructorLlvm(LlvmObject* llvmObj) {
    if (llvmRef) {
        type->createLlvmDestructorRef(llvmObj, llvmRef);
    } else {
        type->createLlvmDestructorValue(llvmObj, llvmValue);
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
void FlowOperation::templateCopy(FlowOperation* operation, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    Operation::templateCopy(operation, parentScope, templateToType);
}
Statement* FlowOperation::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto operation = Create(position, kind);
    templateCopy(operation, parentScope, templateToType);
    return operation;
}
optional<Value*> FlowOperation::interpret(Scope* scope) {
    if (wasInterpreted) {
        return nullptr;
    }
    if (!interpretAllArguments(scope)) {
        return nullopt;
    }

    if (arguments.size() > 0 && arguments[0]->valueKind == Value::ValueKind::Operation) {
        containedErrorResolve = ((Operation*)arguments[0])->containedErrorResolve;
    }

    scope->hasReturnStatement = true;
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
                auto& declaration = scopePtr->declarationsOrder[i];
                auto& variable = declaration->variable;
                if (variable->type->needsDestruction() && scopePtr->declarationsInitState.at(declaration)) {
                    if (scopePtr->maybeUninitializedDeclarations.find(declaration) != scopePtr->maybeUninitializedDeclarations.end()) {
                        warningMessage("destruction of maybe uninitialized variable " + variable->name + " on remove statement", position);
                    }
                    auto destroyOp = Operation::Create(position, Operation::Kind::Destroy);
                    destroyOp->arguments.push_back(variable);
                    if (!destroyOp->interpret(scope)) return nullopt;
                    variablesDestructors.push_back(destroyOp);
                }
            }
            if (scopePtr->owner == Scope::Owner::For) {
                auto forScope = (ForScope*)scopePtr;
                if (holds_alternative<ForEachData>(forScope->data)) {
                    auto& forIterData = get<ForEachData>(forScope->data);
                    if (forIterData.it->name == varName || forIterData.index->name == varName) {
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
                auto& declaration = scopePtr->declarationsOrder[i];
                auto& variable = declaration->variable;
                if (variable->type->needsDestruction() && scopePtr->declarationsInitState.at(declaration)) {
                    if (scopePtr->maybeUninitializedDeclarations.find(declaration) != scopePtr->maybeUninitializedDeclarations.end()) {
                        warningMessage("destruction of maybe uninitialized variable " + variable->name + " on continue statement", position);
                    }
                    auto destroyOp = Operation::Create(position, Operation::Kind::Destroy);
                    destroyOp->arguments.push_back(variable);
                    if (!destroyOp->interpret(scope)) return nullopt;
                    variablesDestructors.push_back(destroyOp);
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
                        if (forIterData.it->name == varName || forIterData.index->name == varName) {
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
                auto& declaration = scopePtr->declarationsOrder[i];
                auto& variable = declaration->variable;
                if (variable->type->needsDestruction() && scopePtr->declarationsInitState.at(declaration)) {
                    if (scopePtr->maybeUninitializedDeclarations.find(declaration) != scopePtr->maybeUninitializedDeclarations.end()) {
                        warningMessage("destruction of maybe uninitialized variable " + variable->name + " on break statement", position);
                    }
                    auto destroyOp = Operation::Create(position, Operation::Kind::Destroy);
                    destroyOp->arguments.push_back(variable);
                    if (!destroyOp->interpret(scope)) return nullopt;
                    variablesDestructors.push_back(destroyOp);
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
                        if (forIterData.it->name == varName || forIterData.index->name == varName) {
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
        type = Type::Create(Type::Kind::Void);
        scopePtr = scope;
        while (scopePtr) {
            for (int i = scopePtr->declarationsOrder.size() - 1; i >= 0; --i) {
                auto& declaration = scopePtr->declarationsOrder[i];
                auto& variable = declaration->variable;
                if (variable->type->needsDestruction() && scopePtr->declarationsInitState.at(declaration)) {
                    if (scopePtr->maybeUninitializedDeclarations.find(declaration) != scopePtr->maybeUninitializedDeclarations.end()) {
                        warningMessage("destruction of maybe uninitialized variable " + variable->name + " on return statement", position);
                    }
                    auto destroyOp = Operation::Create(position, Operation::Kind::Destroy);
                    destroyOp->arguments.push_back(variable);
                    if (!destroyOp->interpret(scope)) return nullopt;
                    if (arguments.empty() || arguments[0]->valueKind != ValueKind::Variable || ((Variable*)arguments[0])->declaration != variable->declaration) {
                        variablesDestructors.push_back(destroyOp);
                    }
                }
            }
            if (scopePtr->owner == Scope::Owner::Function) {
                auto functionValue = ((FunctionScope*)scopePtr)->function;
                auto returnType = ((FunctionType*)functionValue->type)->returnType;

                if (arguments.size() == 0) {
                    if (returnType->kind == Type::Kind::MaybeError && ((MaybeErrorType*)returnType)->underlyingType->kind == Type::Kind::Void) {
                        isReturnMaybeErrorVoidType = true;
                    } else if (returnType->kind != Type::Kind::Void) {
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
        if (!arguments.empty()) {
            arguments[0]->wasCaptured = true;
        }
        scopePtr = scope;
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
void FlowOperation::createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj) {
    if (kind == Kind::Return && arguments.size() > 0) {
        if (arguments[0]->type->kind == Type::Kind::Reference) {
            arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        } else {
            arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        }
    }
}
void FlowOperation::createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj) {}
llvm::Value* FlowOperation::createLlvm(LlvmObject* llvmObj) {
    for (auto& destructor : variablesDestructors) {
        destructor->createLlvm(llvmObj);
    }
    if (kind == Kind::Return) {
        if (arguments.size() == 0) {
            if (isReturnMaybeErrorVoidType) {
                return llvm::ReturnInst::Create(llvmObj->context, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), llvmObj->block);
            } else {
                return llvm::ReturnInst::Create(llvmObj->context, (llvm::Value*)nullptr, llvmObj->block);
            }
        }
        else {
            llvm::Value* llvmArg;
            if (arguments[0]->type->kind == Type::Kind::Reference) {
                llvmArg = arguments[0]->getReferenceLlvm(llvmObj);
            } else {
                llvmArg = arguments[0]->createLlvm(llvmObj);
            }
            auto iter = ((CodeScope*)scopePtr)->valuesToDestroyAfterStatement.find(this);
            if (iter != ((CodeScope*)scopePtr)->valuesToDestroyAfterStatement.end()) {
                for (auto value : iter->second) {
                    if (!value->wasCaptured) {
                        value->createDestructorLlvm(llvmObj);
                        value->wasCaptured = true;
                    }
                }
            }
            return llvm::ReturnInst::Create(llvmObj->context, llvmArg, llvmObj->block);
        }
    }
    else if (kind == Kind::Break) {
        if (scopePtr->owner == Scope::Owner::For) {
            llvm::BranchInst::Create(((ForScope*)scopePtr)->llvmAfterBlock, llvmObj->block);
        } else {
            llvm::BranchInst::Create(((WhileScope*)scopePtr)->llvmAfterBlock, llvmObj->block);
        }
    }
    else if (kind == Kind::Continue) {
        if (scopePtr->owner == Scope::Owner::For) {
            llvm::BranchInst::Create(((ForScope*)scopePtr)->llvmStepBlock, llvmObj->block);
        } else {
            llvm::BranchInst::Create(((WhileScope*)scopePtr)->llvmConditionBlock, llvmObj->block);
        }
    }
    return nullptr;
}


ErrorResolveOperation::ErrorResolveOperation(const CodePosition& position) : 
    Operation(position, Operation::Kind::ErrorResolve)
{
    containedErrorResolve = this;
}
vector<unique_ptr<ErrorResolveOperation>> ErrorResolveOperation::objects;
ErrorResolveOperation* ErrorResolveOperation::Create(const CodePosition& position) {
    objects.emplace_back(make_unique<ErrorResolveOperation>(position));
    return objects.back().get();
}
void ErrorResolveOperation::templateCopy(ErrorResolveOperation* operation, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    operation->onErrorScope = (CodeScope*)onErrorScope->templateCopy(parentScope, templateToType);
    operation->onSuccessScope = (CodeScope*)onSuccessScope->templateCopy(parentScope, templateToType);
    Operation::templateCopy(operation, parentScope, templateToType);
}
Statement* ErrorResolveOperation::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto operation = Create(position);
    templateCopy(operation, parentScope, templateToType);
    return operation;
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

    arguments[0]->wasCaptured = true;

    if (onErrorScope) {
        scope->onErrorScopeToInterpret = onErrorScope;

        errorValueVariable = Variable::Create(CodePosition(nullptr, 0, 0), "errorvalue");
        errorValueVariable->isConst = false;
        errorValueVariable->type = MaybeErrorType::Create(Type::Create(Type::Kind::Void));
        errorValueVariable->type->interpret(onErrorScope);

        errorValueDeclaration = Declaration::Create(position);
        errorValueDeclaration->scope = onErrorScope;
        errorValueDeclaration->variable = errorValueVariable;
        errorValueDeclaration->status = Declaration::Status::Completed;
        onErrorScope->declarationMap.addVariableDeclaration(errorValueDeclaration);
        onErrorScope->declarationsInitState.insert({errorValueDeclaration, true});
        onErrorScope->declarationsOrder.push_back(errorValueDeclaration);
        errorValueVariable->declaration = errorValueDeclaration;
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
void ErrorResolveOperation::createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj) {
    errorValueDeclaration->createAllocaLlvmIfNeeded(llvmObj);
    if (onErrorScope) onErrorScope->allocaAllDeclarationsLlvm(llvmObj);
    if (onSuccessScope) onSuccessScope->allocaAllDeclarationsLlvm(llvmObj);
    arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
}
void ErrorResolveOperation::createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj) {
    errorValueDeclaration->createAllocaLlvmIfNeeded(llvmObj);
    if (onErrorScope) onErrorScope->allocaAllDeclarationsLlvm(llvmObj);
    if (onSuccessScope) onSuccessScope->allocaAllDeclarationsLlvm(llvmObj);
    arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
}
llvm::Value* ErrorResolveOperation::getReferenceLlvm(LlvmObject* llvmObj) {
    auto maybeErrorType = (MaybeErrorType*)arguments[0]->type;
    auto maybeErrorRef = arguments[0]->getReferenceLlvm(llvmObj);
    if (maybeErrorType->underlyingType->kind == Type::Kind::Void) {
        llvmErrorValue = new llvm::LoadInst(maybeErrorRef);
    } else {
        llvmErrorValue = new llvm::LoadInst(maybeErrorType->llvmGepError(llvmObj, maybeErrorRef), "", llvmObj->block);
    }
    auto errorEq0 = new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, llvmErrorValue, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), "");
    llvmSuccessBlock = llvm::BasicBlock::Create(llvmObj->context, "onsuccess", llvmObj->function);
    llvmErrorBlock   = llvm::BasicBlock::Create(llvmObj->context, "onerror", llvmObj->function);
    llvm::BranchInst::Create(llvmSuccessBlock, llvmErrorBlock, errorEq0, llvmObj->block);
    if (onErrorScope) new llvm::StoreInst(llvmErrorValue, errorValueVariable->getReferenceLlvm(llvmObj), llvmErrorBlock);
    llvmObj->block = llvmSuccessBlock;
    if (maybeErrorType->underlyingType->kind == Type::Kind::Void) {
        return nullptr;
    } else {
        llvmRef = maybeErrorType->llvmGepValue(llvmObj, maybeErrorRef);
        return llvmRef;
    }
}
llvm::Value* ErrorResolveOperation::createLlvm(LlvmObject* llvmObj) {
    auto maybeErrorType = (MaybeErrorType*)arguments[0]->type;
    if (maybeErrorType->needsReference()) {
        return new llvm::LoadInst(getReferenceLlvm(llvmObj), "", llvmObj->block);
    }
    auto maybeErrorVal = arguments[0]->createLlvm(llvmObj);
    if (maybeErrorType->underlyingType->kind == Type::Kind::Void) {
        llvmErrorValue = maybeErrorVal;
    } else {
        llvmErrorValue = maybeErrorType->llvmExtractError(llvmObj, maybeErrorVal);
    }
    auto errorEq0 = new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, llvmErrorValue, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), "");
    llvmSuccessBlock = llvm::BasicBlock::Create(llvmObj->context, "onsuccess", llvmObj->function);
    llvmErrorBlock   = llvm::BasicBlock::Create(llvmObj->context, "onerror", llvmObj->function);
    llvm::BranchInst::Create(llvmSuccessBlock, llvmErrorBlock, errorEq0, llvmObj->block);
    if (onErrorScope) new llvm::StoreInst(llvmErrorValue, errorValueVariable->getReferenceLlvm(llvmObj), llvmErrorBlock);
    llvmObj->block = llvmSuccessBlock;
    if (maybeErrorType->underlyingType->kind == Type::Kind::Void) {
        return nullptr;
    } else {
        llvmValue = maybeErrorType->llvmExtractValue(llvmObj, maybeErrorVal);
        return llvmValue;
    }
}
void ErrorResolveOperation::createLlvmSuccessDestructor(LlvmObject* llvmObj) {
    if (!wasCaptured) {
        if (llvmRef) {
            type->createLlvmDestructorRef(llvmObj, llvmRef);
        } else if (llvmValue) {
            type->createLlvmDestructorRef(llvmObj, llvmValue);
        }
    }
}


SizeofOperation::SizeofOperation(const CodePosition& position) : 
    Operation(position, Operation::Kind::Sizeof)
{}
vector<unique_ptr<SizeofOperation>> SizeofOperation::objects;
SizeofOperation* SizeofOperation::Create(const CodePosition& position) {
    objects.emplace_back(make_unique<SizeofOperation>(position));
    return objects.back().get();
}
void SizeofOperation::templateCopy(SizeofOperation* operation, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    operation->argType = argType->templateCopy(parentScope, templateToType);
    Operation::templateCopy(operation, parentScope, templateToType);
}
Statement* SizeofOperation::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto operation = Create(position);
    templateCopy(operation, parentScope, templateToType);
    return operation;
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
void createLlvmForEachLoop(LlvmObject* llvmObj, llvm::Value* start, llvm::Value* end, bool includeEnd, function<void(llvm::Value*)> bodyFunction) {
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
    if (includeEnd) {
        llvm::BranchInst::Create(
            loopBodyBlock, 
            afterLoopBlock, 
            new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLE, new llvm::LoadInst(index, "", llvmObj->block), end, ""),
            loopConditionBlock
        );
    } else {
        llvm::BranchInst::Create(
            loopBodyBlock, 
            afterLoopBlock, 
            new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLT, new llvm::LoadInst(index, "", llvmObj->block), end, ""),
            loopConditionBlock
        );
    }
    

    llvmObj->block = afterLoopBlock;
}
void createLlvmForEachLoopReversed(LlvmObject* llvmObj, llvm::Value* start, llvm::Value* end, function<void(llvm::Value*)> bodyFunction) {
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
    auto indexAfterIncrement = llvm::BinaryOperator::CreateSub(
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
        new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SGT, new llvm::LoadInst(index, "", llvmObj->block), end, ""),
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
    llvm::BranchInst::Create(ifTrueBlock, afterIfTrueBlock,condition, llvmObj->block);
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
void ConstructorOperation::templateCopy(ConstructorOperation* operation, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    operation->constructorType = constructorType->templateCopy(parentScope, templateToType);
    operation->isHeapAllocation = isHeapAllocation;
    operation->isExplicit = isExplicit;
    Operation::templateCopy(operation, parentScope, templateToType);
}
Statement* ConstructorOperation::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto operation = Create(position, nullptr);
    templateCopy(operation, parentScope, templateToType);
    return operation;
}
optional<Value*> ConstructorOperation::interpret(Scope* scope) {
    return interpret(scope, false, false);
}
optional<Value*> ConstructorOperation::interpret(Scope* scope, bool onlyTry, bool parentIsAssignment) {
    if (wasInterpreted) return nullptr;
    if (!onlyTry) wasInterpreted = true;
    if (!type->interpret(scope)) return nullopt;
    if (!constructorType->interpret(scope)) return nullopt;
    if (!interpretAllArguments(scope)) return nullopt;

    if (!onlyTry && isHeapAllocation) {
        typesize = constructorType->typesize(scope);
    } 
    if (arguments.size() == 1 && cmpPtr(constructorType, arguments[0]->type->getEffectiveType())) {
        if (isHeapAllocation) {
            if (!onlyTry) arguments[0]->wasCaptured = true;
            return nullptr;
        } else {
            return arguments[0];
        }
    } 

    auto constructorInterpret = constructorType->interpretConstructor(position, scope, arguments, onlyTry, parentIsAssignment || isHeapAllocation, isExplicit);
    if (!constructorInterpret) return nullopt;
    if (!onlyTry) {
        if (constructorInterpret.value().argWasCaptured) {
            arguments[0]->wasCaptured = true;
        }
        if (constructorInterpret.value().value) {
            if (isHeapAllocation || constructorType->kind == Type::Kind::StaticArray) {
                arguments = {constructorInterpret.value().value};
            } else {
                return constructorInterpret.value().value;
            }
        }
        classConstructor = constructorInterpret.value().classConstructor;

        if (type->needsDestruction()) scope->valuesToDestroyBuffer.push_back(this);
    }
    return nullptr;
}
void ConstructorOperation::createAllocaLlvmIfNeededForConstructor(LlvmObject* llvmObj) {
    if (isHeapAllocation) {
        constructorType->createAllocaIfNeededForConstructor(llvmObj, arguments, classConstructor);
    } else {
        type->createAllocaIfNeededForConstructor(llvmObj, arguments, classConstructor);
    }
}
void ConstructorOperation::createAllocaLlvmIfNeededForAssignment(LlvmObject* llvmObj) {
    if (isHeapAllocation) {
        constructorType->createAllocaIfNeededForAssignment(llvmObj, arguments, classConstructor);
    } else {
        type->createAllocaIfNeededForAssignment(llvmObj, arguments, classConstructor);
    }
}
void ConstructorOperation::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef) {
    if (isHeapAllocation) {
        auto allocPtr = new llvm::BitCastInst(
            llvm::CallInst::Create(llvmObj->mallocFunction, typesize->createLlvm(llvmObj), "", llvmObj->block), 
            type->createLlvm(llvmObj), 
            "", 
            llvmObj->block
        );
        constructorType->createLlvmConstructor(llvmObj, allocPtr, arguments, classConstructor);
        type->createLlvmMoveConstructor(llvmObj, leftLlvmRef, allocPtr);
    } else {
        type->createLlvmConstructor(llvmObj, leftLlvmRef, arguments, classConstructor);
    }
}
void ConstructorOperation::createLlvmAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef) {
    if (isHeapAllocation) {
        auto allocPtr = new llvm::BitCastInst(
            llvm::CallInst::Create(llvmObj->mallocFunction, typesize->createLlvm(llvmObj), "", llvmObj->block), 
            type->createLlvm(llvmObj), 
            "", 
            llvmObj->block
        );
        constructorType->createLlvmConstructor(llvmObj, allocPtr, arguments, classConstructor);
        type->createLlvmMoveAssignment(llvmObj, leftLlvmRef, allocPtr);
    } else {
        type->createLlvmAssignment(llvmObj, leftLlvmRef, arguments, classConstructor);
    }
}
void ConstructorOperation::createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj) {
    if (!isHeapAllocation) {
        llvmRef = type->createRefForLlvmValue(llvmObj, arguments, classConstructor);
    }
}
void ConstructorOperation::createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj) {
    if (isHeapAllocation) {
        llvmRef = type->allocaLlvm(llvmObj);
        createAllocaLlvmIfNeededForValue(llvmObj);
    } else {
        llvmRef = type->createRefForLlvmReference(llvmObj, arguments, classConstructor);
    }
}
llvm::Value* ConstructorOperation::getReferenceLlvm(LlvmObject* llvmObj) {
    if (isHeapAllocation) {
        new llvm::StoreInst(createLlvm(llvmObj), llvmRef, llvmObj->block);
    } else {
        llvmRef = type->createLlvmReference(llvmObj, arguments, classConstructor, llvmRef);
    }
    return llvmRef;
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
    } else {
        auto createValue = type->createLlvmValue(llvmObj, arguments, classConstructor, llvmRef);
        llvmRef = createValue.first;
        llvmValue = createValue.second;
    }
    return llvmValue;
}
void ConstructorOperation::createDestructorLlvm(LlvmObject* llvmObj) {
    if (llvmRef) {
        type->createLlvmDestructorRef(llvmObj, llvmRef);
    } else {
        type->createLlvmDestructorValue(llvmObj, llvmValue);
    }
}


AssignOperation::AssignOperation(const CodePosition& position, bool forceConstruction) : 
    Operation(position, Operation::Kind::Assign),
    isConstruction(forceConstruction)
{}
vector<unique_ptr<AssignOperation>> AssignOperation::objects;
AssignOperation* AssignOperation::Create(const CodePosition& position, bool forceConstruction) {
    objects.emplace_back(make_unique<AssignOperation>(position, forceConstruction));
    return objects.back().get();
}
void AssignOperation::templateCopy(AssignOperation* operation, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    operation->isConstruction = isConstruction;
    Operation::templateCopy(operation, parentScope, templateToType);
}
Statement* AssignOperation::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto operation = Create(position);
    templateCopy(operation, parentScope, templateToType);
    return operation;
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
        containedErrorResolve = ((Operation*)arguments[1])->containedErrorResolve;
    }

    if (!containedErrorResolve && arguments[0]->valueKind == Value::ValueKind::Variable) {
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
void AssignOperation::createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj) {
    createAllocaLlvmIfNeededForReference(llvmObj);
}
void AssignOperation::createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj) {
    arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);

    if (arguments[1]->valueKind == Value::ValueKind::Operation && ((Operation*)arguments[1])->kind == Operation::Kind::Constructor) {
        auto constructorOp = (ConstructorOperation*)arguments[1];
        if (isConstruction) {
            constructorOp->createAllocaLlvmIfNeededForConstructor(llvmObj);
        } else {
            constructorOp->createAllocaLlvmIfNeededForAssignment(llvmObj);
        }
    }
    else {
        if (isLvalue(arguments[1])) {
            if (arguments[0]->type->kind == Type::Kind::Class || arguments[0]->type->kind == Type::Kind::StaticArray || arguments[0]->type->kind == Type::Kind::DynamicArray || arguments[0]->type->kind == Type::Kind::MaybeError) {
                arguments[1]->createAllocaLlvmIfNeededForReference(llvmObj);
            } else {
                arguments[1]->createAllocaLlvmIfNeededForValue(llvmObj);
            }
        } else {
            if (arguments[0]->type->kind == Type::Kind::Class) {
                arguments[1]->createAllocaLlvmIfNeededForReference(llvmObj);
            } else {
                arguments[1]->createAllocaLlvmIfNeededForValue(llvmObj);
            }
        }
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


DotOperation::DotOperation(const CodePosition& position) : 
    Operation(position, Operation::Kind::Dot)
{}
vector<unique_ptr<DotOperation>> DotOperation::objects;
DotOperation* DotOperation::Create(const CodePosition& position) {
    objects.emplace_back(make_unique<DotOperation>(position));
    return objects.back().get();
}
void DotOperation::templateCopy(DotOperation* operation, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    operation->isBuildInOperation = isBuildInOperation;
    Operation::templateCopy(operation, parentScope, templateToType);
}
Statement* DotOperation::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto operation = Create(position);
    templateCopy(operation, parentScope, templateToType);
    return operation;
}
optional<Value*> DotOperation::interpret(Scope* scope) {
    if (wasInterpreted) {
        return nullptr;
    }
    wasInterpreted = true;

    auto arg0Interpret = arguments[0]->interpret(scope);
    if (!arg0Interpret) return nullopt;
    if (arg0Interpret.value()) arguments[0] = arg0Interpret.value();
    auto effectiveType1 = arguments[0]->type->getEffectiveType();

    if (arguments[1]->valueKind != Value::ValueKind::Variable) {
        return errorMessageOpt("right side of '.' operation needs to be a label", position);
    }
    auto fieldName = ((Variable*)arguments[1])->name;

    if (effectiveType1->kind == Type::Kind::DynamicArray) {
        isBuildInOperation = true;
        if (fieldName == "size" || fieldName == "capacity") {
            type = IntegerType::Create(IntegerType::Size::I64);
            return nullptr;
        } else if (fieldName == "data") {
            type = RawPointerType::Create(((DynamicArrayType*)effectiveType1)->elementType);
            return nullptr;
        } else if (fieldName == "push" || fieldName == "pushArray" || fieldName == "insert" || fieldName == "insertArray"
            || fieldName == "resize" || fieldName == "extend" || fieldName == "shrink" || fieldName == "reserve"
            || fieldName == "pop" || fieldName == "remove" || fieldName == "clear" || fieldName == "last"
            || fieldName == "trim" || fieldName == "slice") 
        {
            arguments[1]->isConstexpr = true;
            type = Type::Create(Type::Kind::Void);
            return nullptr;
        } else {
            return errorMessageOpt("dynamic array has no field named " + fieldName, position);
        }
    } else if (effectiveType1->kind == Type::Kind::ArrayView) {
        isBuildInOperation = true;
        if (fieldName == "size") {
            type = IntegerType::Create(IntegerType::Size::I64);
            return nullptr;
        } else if (fieldName == "data") {
            type = RawPointerType::Create(((ArrayViewType*)effectiveType1)->elementType);
            return nullptr;
        } else {
            return errorMessageOpt("array view has no field named " + fieldName, position);
        }
    } else {
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

        if (!classType->interpret(scope)) {
            return nullopt;
        }
        auto& declarations = classType->declaration->body->declarations;
        vector<Declaration*> viableDeclarations;
        for (auto& declaration : declarations) {
            if (declaration->variable->name == fieldName) {
                viableDeclarations.push_back(declaration);
            }
        }

        if (viableDeclarations.size() == 0) {
            return errorMessageOpt("class " + DeclarationMap::toString(effectiveType1)
                + " has no field named " + fieldName, position
            );
        }
        Declaration* declaration = viableDeclarations.back();
        if (declaration->variable->isConstexpr && declaration->value->valueKind != Value::ValueKind::FunctionValue) {
            return declaration->value;
        }

        arguments[1] = declaration->variable;
        ((Variable*)arguments[1])->declaration = declaration;
        type = declaration->variable->type;
    }

    return nullptr;
}
bool DotOperation::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const DotOperation&>(value);
        return Operation::operator==(other);
    }
    else {
        return false;
    }
}
void DotOperation::createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj) {
    auto arg0EffType = arguments[0]->type->getEffectiveType();
    if (arg0EffType->kind == Type::Kind::DynamicArray || arg0EffType->kind == Type::Kind::ArrayView) {
        createAllocaLlvmIfNeededForReference(llvmObj);
    } else {
        auto accessedDeclaration = ((Variable*)arguments[1])->declaration;
        if (accessedDeclaration->value && accessedDeclaration->variable->isConstexpr) {
            return;
        } else {
            createAllocaLlvmIfNeededForReference(llvmObj);
        }
    }
}
void DotOperation::createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj) {
    auto effectiveType0 = arguments[0]->type->getEffectiveType();
    if (effectiveType0->kind == Type::Kind::DynamicArray) {
        auto fieldName = ((Variable*)arguments[1])->name;
        auto dynamicArrayType = (DynamicArrayType*)effectiveType0;
        if (arguments[0]->valueKind == ValueKind::Operation 
            && (((Operation*)arguments[0])->kind == Operation::Kind::FunctionCall) || ((Operation*)arguments[0])->kind == Operation::Kind::Constructor)
        {
            arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        } else {
            arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        }
    } else if (effectiveType0->kind == Type::Kind::ArrayView) {
        auto fieldName = ((Variable*)arguments[1])->name;
        auto arrayViewType = (ArrayViewType*)effectiveType0;
        if (arguments[0]->valueKind == ValueKind::Operation 
            && (((Operation*)arguments[0])->kind == Operation::Kind::FunctionCall) || ((Operation*)arguments[0])->kind == Operation::Kind::Constructor)
        {
            arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        } else {
            arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        }
    } else {
        if (effectiveType0->kind == Type::Kind::RawPointer || effectiveType0->kind == Type::Kind::OwnerPointer) {
            arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        } else if (effectiveType0->kind == Type::Kind::MaybeError) {
            arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        } else {
            arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        }
    }
}
llvm::Value* DotOperation::getReferenceLlvm(LlvmObject* llvmObj) {
    auto effectiveType0 = arguments[0]->type->getEffectiveType();
    if (effectiveType0->kind == Type::Kind::DynamicArray) {
        auto fieldName = ((Variable*)arguments[1])->name;
        auto dynamicArrayType = (DynamicArrayType*)effectiveType0;
        if (arguments[0]->valueKind == ValueKind::Operation 
            && (((Operation*)arguments[0])->kind == Operation::Kind::FunctionCall) || ((Operation*)arguments[0])->kind == Operation::Kind::Constructor)
        {
            if (fieldName == "size") {
                dynamicArrayType->llvmExtractSize(llvmObj, arguments[0]->createLlvm(llvmObj));
            } else if (fieldName == "capacity") {
                dynamicArrayType->llvmExtractCapacity(llvmObj, arguments[0]->createLlvm(llvmObj));
            } else if (fieldName == "data") {
                dynamicArrayType->llvmExtractData(llvmObj, arguments[0]->createLlvm(llvmObj));
            } else {
                internalError("dot operation on dynamic array type with unknown name during llvm creating");
            }
        } else {
            if (fieldName == "size") {
                return (dynamicArrayType->llvmGepSize(llvmObj, arguments[0]->getReferenceLlvm(llvmObj)));
            } else if (fieldName == "capacity") {
                return (dynamicArrayType->llvmGepCapacity(llvmObj, arguments[0]->getReferenceLlvm(llvmObj)));
            } else if (fieldName == "data") {
                return (dynamicArrayType->llvmGepData(llvmObj, arguments[0]->getReferenceLlvm(llvmObj)));
            } else {
                internalError("dot operation on dynamic array type with unknown name during llvm creating");
            }
        }
    } else if (effectiveType0->kind == Type::Kind::ArrayView) {
        auto fieldName = ((Variable*)arguments[1])->name;
        auto arrayViewType = (ArrayViewType*)effectiveType0;
        if (arguments[0]->valueKind == ValueKind::Operation 
            && (((Operation*)arguments[0])->kind == Operation::Kind::FunctionCall) || ((Operation*)arguments[0])->kind == Operation::Kind::Constructor)
        {
            if (fieldName == "size") {
                arrayViewType->llvmExtractSize(llvmObj, arguments[0]->createLlvm(llvmObj));
            } else if (fieldName == "data") {
                arrayViewType->llvmExtractData(llvmObj, arguments[0]->createLlvm(llvmObj));
            } else {
                internalError("dot operation on array view type with unknown name during llvm creating");
            }
        } else {
            if (fieldName == "size") {
                return (arrayViewType->llvmGepSize(llvmObj, arguments[0]->getReferenceLlvm(llvmObj)));
            } else if (fieldName == "data") {
                return (arrayViewType->llvmGepData(llvmObj, arguments[0]->getReferenceLlvm(llvmObj)));
            } else {
                internalError("dot operation on array view type with unknown name during llvm creating");
            }
        }
    }
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
llvm::Value* DotOperation::createLlvm(LlvmObject* llvmObj) {
    auto arg0EffType = arguments[0]->type->getEffectiveType();
    if (arg0EffType->kind == Type::Kind::DynamicArray || arg0EffType->kind == Type::Kind::ArrayView) {
        return new llvm::LoadInst(getReferenceLlvm(llvmObj), "", llvmObj->block);
    } else {
        auto accessedDeclaration = ((Variable*)arguments[1])->declaration;
        if (accessedDeclaration->value && accessedDeclaration->variable->isConstexpr) {
            // is const member function
            return accessedDeclaration->value->createLlvm(llvmObj);
        }
        return new llvm::LoadInst(getReferenceLlvm(llvmObj), "", llvmObj->block);
    }
}