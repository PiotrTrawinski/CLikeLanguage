#include "Operation.h"

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
Operation* Operation::expandAssignOperation(Kind kind) {
    auto assign = Operation::Create(position, Operation::Kind::Assign);
    auto operation = Operation::Create(position, kind);
    operation->arguments = {arguments[0], arguments[1]};
    assign->arguments = {arguments[0], operation};
    return assign;
}
optional<Value*> Operation::interpret(Scope* scope) {
    for (auto& val : arguments) {
        auto valInterpret = val->interpret(scope);
        if (!valInterpret) {
            return nullopt;
        }
        if (valInterpret.value()) {
            val = valInterpret.value();
        }
    }

    type = nullptr;
    switch (kind) {
    case Kind::Dot:
        break;
    case Kind::Reference:
        break;
    case Kind::Address:
        break;
    case Kind::GetValue:
        break;
    case Kind::Allocation:
        break;
    case Kind::Deallocation:
        break;
    case Kind::Minus: {
        auto argType = arguments[0]->type;
        if (argType->kind != Type::Kind::Integer && argType->kind != Type::Kind::Float) {
            break;
        }
        if (arguments[0]->valueKind == Value::ValueKind::Integer) {
            type = IntegerType::Create(IntegerType::Size::I64);
            if (arguments[0]->isConstexpr) {
                return IntegerValue::Create(position, -(int64_t)((IntegerValue*)arguments[0])->value);
            }
        } else if (arguments[0]->valueKind == Value::ValueKind::Float) {
            type = FloatType::Create(FloatType::Size::F64);
            if (arguments[0]->isConstexpr) {
                return FloatValue::Create(position, -((FloatValue*)arguments[0])->value);
            }
        } else if (arguments[0]->valueKind == Value::ValueKind::Char) {
            type = IntegerType::Create(IntegerType::Size::I64);
            if (arguments[0]->isConstexpr) {
                return IntegerValue::Create(position, -(int64_t)((CharValue*)arguments[0])->value);
            }
        }
        break;
    }
    case Kind::Mul: {
        auto value = tryEvaluate2ArgArithmetic([](auto val1, auto val2){
            return val1 * val2;
        });
        if (value) return value;
        break;
    }
    case Kind::Div: {
        auto value = tryEvaluate2ArgArithmetic([](auto val1, auto val2){
            return val1 / val2;
        });
        if (value) return value;
        break;
    }
    case Kind::Mod: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 % val2;
        });
        if (value) return value;
        break;
    }
    case Kind::Add: {
        auto value = tryEvaluate2ArgArithmetic([](auto val1, auto val2){
            return val1 + val2;
        });
        if (value) return value;
        if (type) break;

        auto type1 = arguments[0]->type;
        auto type2 = arguments[1]->type;
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
        });
        if (value) return value;
        break;
    }
    case Kind::Shl: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 << val2;
        });
        if (value) return value;
        break;
    }
    case Kind::Shr: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 >> val2;
        });
        if (value) return value;
        break;
    }
    case Kind::Sal: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 << val2;
        });
        if (value) return value;
        break;
    } 
    case Kind::Sar: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 >> val2;
        });
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
        });
        if (value) return value;
        break;
    }
    case Kind::BitXor: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 ^ val2;
        });
        if (value) return value;
        break;
    }
    case Kind::BitOr: {
        auto value = tryEvaluate2ArgArithmeticIntegerOnly([](auto val1, auto val2){
            return val1 | val2;
        });
        if (value) return value;
        break;
    }
    case Kind::Assign:{
        if (!isLvalue(arguments[0])) {
            errorMessage("left argument of an assignment must be an l-value", position);
            return nullopt;
        }
        auto cast = CastOperation::Create(arguments[1]->position, arguments[0]->type);
        if (arguments[0]->type->kind == Type::Kind::Reference) {
            cast->argType = ((ReferenceType*)arguments[0]->type)->underlyingType;
            cast->type = cast->argType;
        }
        cast->arguments.push_back(arguments[1]);
        arguments[1] = cast;
        auto castInterpret = arguments[1]->interpret(scope);
        if (!castInterpret) return nullopt;
        if (castInterpret.value()) arguments[1] = castInterpret.value();
        type = arguments[0]->type;
        break;
    }
    case Kind::AddAssign:
        return expandAssignOperation(Operation::Kind::Add)->interpret(scope);
    case Kind::SubAssign:
        return expandAssignOperation(Operation::Kind::Sub)->interpret(scope);
    case Kind::MulAssign:
        return expandAssignOperation(Operation::Kind::Mul)->interpret(scope);
    case Kind::DivAssign:
        return expandAssignOperation(Operation::Kind::Div)->interpret(scope);
    case Kind::ModAssign:
        return expandAssignOperation(Operation::Kind::Mod)->interpret(scope);
    case Kind::ShlAssign:
        return expandAssignOperation(Operation::Kind::Shl)->interpret(scope);
    case Kind::ShrAssign:
        return expandAssignOperation(Operation::Kind::Shr)->interpret(scope);
    case Kind::SalAssign:
        return expandAssignOperation(Operation::Kind::Sal)->interpret(scope);
    case Kind::SarAssign:
        return expandAssignOperation(Operation::Kind::Sar)->interpret(scope);
    case Kind::BitNegAssign:
        return expandAssignOperation(Operation::Kind::BitNeg)->interpret(scope);
    case Kind::BitOrAssign:
        return expandAssignOperation(Operation::Kind::BitOr)->interpret(scope);
    case Kind::BitXorAssign:
        return expandAssignOperation(Operation::Kind::BitXor)->interpret(scope);
    default: 
        break;
    }

    if (!type) {
        string message = "incorrect use of operation '" + kindToString(kind) + "'. ";
        if (arguments.size() == 1) {
            message += "type is: ";
            message += DeclarationMap::toString(arguments[0]->type);
        } else {
            message += "types are: ";
            message += DeclarationMap::toString(arguments[0]->type);
            message += "; ";
            message += DeclarationMap::toString(arguments[1]->type);
        }

        errorMessage(message, position);
        return nullopt;
    }

    return nullptr;
}

string Operation::kindToString(Kind kind) {
    switch (kind) {
    case Kind::Dot: return ". (dot)";
    case Kind::FunctionCall: return "function call";
    case Kind::ArrayIndex: return "[x] (array index)";
    case Kind::ArraySubArray: return "[x:y] (sub-array)";
    case Kind::Reference: return "& (reference)";
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
        return 1;
    case Kind::Reference:
    case Kind::Address:
    case Kind::GetValue:
    case Kind::Allocation:
    case Kind::Deallocation:
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
    case Kind::Cast:
        return 0;
    case Kind::ArrayIndex:
    case Kind::ArraySubArray:
    case Kind::Reference:
    case Kind::Address:
    case Kind::GetValue:
    case Kind::Allocation:
    case Kind::Deallocation:
    case Kind::BitNeg:
    case Kind::LogicalNot:
    case Kind::Minus:
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
    auto valInterpret = arguments[0]->interpret(scope);
    if (!valInterpret) return nullopt;
    if (valInterpret.value()) arguments[0] = valInterpret.value();

    if (cmpPtr(arguments[0]->type, type)) {
        return arguments[0];
    } 
    else if (type->kind == Type::Kind::Bool) {
        if (arguments[0]->type->kind != Type::Kind::Class) {
            return nullptr;
        }
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
    }
    else if (type->kind == Type::Kind::Integer) {
        if (arguments[0]->type->kind == Type::Kind::Integer) {
            return nullptr;
        }
        if (arguments[0]->type->kind == Type::Kind::Float) {
            return nullptr;
        }
        if (arguments[0]->type->kind == Type::Kind::Bool) {
            if (arguments[0]->isConstexpr) {
                int value = ((BoolValue*)arguments[0])->value ? 1 : 0;
                auto intValue = IntegerValue::Create(position, value);
                intValue->type = type;
                return intValue;
            }
            return nullptr;
        }
    }
    else if (type->kind == Type::Kind::Float) {
        if (arguments[0]->type->kind == Type::Kind::Integer) {
            return nullptr;
        }
        if (arguments[0]->type->kind == Type::Kind::Float) {
            return nullptr;
        }
        if (arguments[0]->type->kind == Type::Kind::Bool) {
            if (arguments[0]->isConstexpr) {
                double value = ((BoolValue*)arguments[0])->value ? 1 : 0;
                auto floatValue = FloatValue::Create(position, value);
                floatValue->type = type;
                return floatValue;
            }
            return nullptr;
        }
    }
    else if (type->kind == Type::Kind::RawPointer && arguments[0]->type->kind == Type::Kind::RawPointer) {
        return nullptr;
    }

    errorMessage("cannot cast " + 
        DeclarationMap::toString(arguments[0]->type) + 
        " to " + DeclarationMap::toString(type), position
    );
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
    Operation(position, Operation::Kind::FunctionCall),
    function(Variable::Create(position))
{}
vector<unique_ptr<FunctionCallOperation>> FunctionCallOperation::objects;
FunctionCallOperation* FunctionCallOperation::Create(const CodePosition& position) {
    objects.emplace_back(make_unique<FunctionCallOperation>(position));
    return objects.back().get();
}
optional<Value*> FunctionCallOperation::interpret(Scope* scope) {
    return nullptr;
}
bool FunctionCallOperation::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const FunctionCallOperation&>(value);
        return cmpPtr(this->function, other.function)
            && Operation::operator==(other);
    }
    else {
        return false;
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