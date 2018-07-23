#include "Operation.h"

using namespace std;
/*
optional<IntegerType::Size> getIntegerSize(IntegerType::Size size1, IntegerType::Size size2, CodePosition codePosition) {
    if (isSigned(size1) && isSigned(size2)) {
        if (size1 == IntegerType::Size::I64 || size2 == IntegerType::Size::I64) {
            return IntegerType::Size::I64;
        }
        if (size1 == IntegerType::Size::I32 || size2 == IntegerType::Size::I32) {
            return IntegerType::Size::I32;
        }
        if (size1 == IntegerType::Size::I16 || size2 == IntegerType::Size::I16) {
            return IntegerType::Size::I16;
        }
        if (size1 == IntegerType::Size::I8 || size2 == IntegerType::Size::I8) {
            return IntegerType::Size::I8;
        }
    } else if (!isSigned(size1) && !isSigned(size2)) {
        if (size1 == IntegerType::Size::U64 || size2 == IntegerType::Size::U64) {
            return IntegerType::Size::U64;
        }
        if (size1 == IntegerType::Size::U32 || size2 == IntegerType::Size::U32) {
            return IntegerType::Size::U32;
        }
        if (size1 == IntegerType::Size::U16 || size2 == IntegerType::Size::U16) {
            return IntegerType::Size::U16;
        }
        if (size1 == IntegerType::Size::U8 || size2 == IntegerType::Size::U8) {
            return IntegerType::Size::U8;
        }
    } else {
        errorMessage("signed/unsigned", codePosition);
        return nullopt;
    }
}
FloatType::Size getFloatSize(FloatType::Size size1, FloatType::Size size2) {
    if (size1 == FloatType::Size::F64 || size2 == FloatType::Size::F64) {
        return FloatType::Size::F64;
    } else {
        return FloatType::Size::F32;
    }
}

*/

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
optional<Value*> Operation::interpret(Scope* scope) {
    bool allArgsConstexpr = true;
    for (auto& val : arguments) {
        if (!val->interpret(scope)) {
            return nullopt;
        }
        if (!val->isConstexpr) {
            allArgsConstexpr = false;
        }
    }
    //switch (operation->kind) {
    //case Operation::Kind::Add:
    //if (allArgsConstexpr) {
    //evaluate(operation->position, operation->arguments[0].get(), operation->arguments[1].get(), operationAdd);
    //} else {
    resolveTypeOfOperation();
    if (!type) {
        return nullopt;
    }
    //}
    if (allArgsConstexpr) {
        isConstexpr = true;
    }
    return nullptr;
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

bool Operation::resolveTypeOfOperation() {
    if (getNumberOfArguments() == 2) {
        auto type1 = arguments[0]->type;
        auto type2 = arguments[1]->type;

        type = Type::getSuitingArithmeticType(type1, type2);
        if (type) {
            return true;
        }

 /*       if (type1->kind == Type::Kind::Integer && type2->kind == Type::Kind::Integer) {
            auto integerSize = getIntegerSize(((IntegerType*)type1)->size,((IntegerType*)type2)->size, position);
            if (!integerSize) {
                return false;
            }
            type = make_unique<IntegerType>(integerSize.value());
        }
        else if (type1->kind == Type::Kind::Float && type2->kind == Type::Kind::Float) {
            auto floatSize = getFloatSize(((FloatType*)type1)->size,((FloatType*)type2)->size);
            type = make_unique<FloatType>(floatSize);
        }
        else if (type1->kind == Type::Kind::Integer && type2->kind == Type::Kind::Float) {
            type = type2->copy();
        }
        else if (type1->kind == Type::Kind::Float && type2->kind == Type::Kind::Integer) {
            type = type1->copy();
        }*/
        else if (type1->kind == Type::Kind::Bool && type2->kind == Type::Kind::Bool) {
            type = type1;
        }
        else if (type1->kind == Type::Kind::String && type2->kind == Type::Kind::String) {
            type = type1;
        } 
        else {
            return errorMessage("cannot do 2 arg operation on thoes types", position);
        }
    } else {
        return false;
    }

    return true;
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
{}
vector<unique_ptr<CastOperation>> CastOperation::objects;
CastOperation* CastOperation::Create(const CodePosition& position, Type* argType) {
    objects.emplace_back(make_unique<CastOperation>(position, argType));
    return objects.back().get();
}
optional<Value*> CastOperation::interpret(Scope* scope) {
    return nullptr;
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