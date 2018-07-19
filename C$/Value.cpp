#include "Value.h"
#include "Operation.h"
#include "Declaration.h"
#include "Scope.h"

using namespace std;

/*
    Value
*/
Value::Value(const CodePosition& position, ValueKind valueKind) : 
    Statement(position, Statement::Kind::Value),
    valueKind(valueKind)
{}
optional<unique_ptr<Value>> Value::interpret(Scope* scope) {
    return nullptr;
}
bool Value::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const Value&>(value);
        return this->valueKind == other.valueKind
            && (this->type == other.type || this->type == other.type)
            && this->isConstexpr == other.isConstexpr
            && Statement::operator==(other);
    } else {
        return false;
    }
}
unique_ptr<Value> Value::copy() {
    auto value = make_unique<Value>(position, valueKind);
    value->type = type->copy();
    value->isConstexpr = isConstexpr;
    return value;
}


/*
    Variable
*/
Variable::Variable(const CodePosition& position, const string& name) : 
    Value(position, Value::ValueKind::Variable),
    name(name)
{}
optional<unique_ptr<Value>> Variable::interpret(Scope* scope) {
    Declaration* declaration = scope->findDeclaration(this);
    if (!declaration) {
        return nullopt;
    }
    isConstexpr = declaration->variable.isConstexpr;
    type = declaration->variable.type->copy();
    isConst = declaration->variable.isConst;
    return nullptr;
}
bool Variable::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const Variable&>(value);
        return this->name == other.name
            && this->isConst == other.isConst
            && Value::operator==(other);
    } else {
        return false;
    }
}
unique_ptr<Value> Variable::copy() {
    auto variable = make_unique<Variable>(position, name);
    variable->type = type->copy();
    variable->isConstexpr = isConstexpr;
    variable->isConst = isConst;
    return variable;
}


/*
    IntegerValue
*/
IntegerValue::IntegerValue(const CodePosition& position, int32_t value) : 
    Value(position, Value::ValueKind::Integer),
    value(value)
{
    isConstexpr = true;
    type = make_unique<IntegerType>(IntegerType::Size::I32);
}
optional<unique_ptr<Value>> IntegerValue::interpret(Scope* scope) {
    isConstexpr = true;
    return nullptr;
}
bool IntegerValue::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const IntegerValue&>(value);
        return this->value == other.value
            && Value::operator==(other);
    }
    else {
        return false;
    }
}
unique_ptr<Value> IntegerValue::copy() {
    auto val = make_unique<IntegerValue>(position, value);
    return val;
}


/*
    CharValue
*/
CharValue::CharValue(const CodePosition& position, uint8_t value) : 
    Value(position, Value::ValueKind::Char),
    value(value)
{
    isConstexpr = true;
    type = make_unique<IntegerType>(IntegerType::Size::U8);
}
optional<unique_ptr<Value>> CharValue::interpret(Scope* scope) {
    isConstexpr = true;
    return nullptr;
}
bool CharValue::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const CharValue&>(value);
        return this->value == other.value
            && Value::operator==(other);
    }
    else {
        return false;
    }
}
unique_ptr<Value> CharValue::copy() {
    auto val = make_unique<CharValue>(position, value);
    return val;
}


/*
    FloatValue
*/
FloatValue::FloatValue(const CodePosition& position, double value) :
    Value(position, Value::ValueKind::Float),
    value(value)
{
    isConstexpr = true;
    type = make_unique<FloatType>(FloatType::Size::F64);
}
optional<unique_ptr<Value>> FloatValue::interpret(Scope* scope) {
    isConstexpr = true;
    return nullptr;
}
bool FloatValue::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const FloatValue&>(value);
        return this->value == other.value
            && Value::operator==(other);
    }
    else {
        return false;
    }
}
unique_ptr<Value> FloatValue::copy() {
    auto val = make_unique<FloatValue>(position, value);
    return val;
}


/*
    StringValue
*/
StringValue::StringValue(const CodePosition& position, const string& value) : 
    Value(position, Value::ValueKind::String),
    value(value)
{
    isConstexpr = true;
    type = make_unique<Type>(Type::Kind::String);
}
optional<unique_ptr<Value>> StringValue::interpret(Scope* scope) {
    isConstexpr = true;
    return nullptr;
}
bool StringValue::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const StringValue&>(value);
        return this->value == other.value
            && Value::operator==(other);
    }
    else {
        return false;
    }
}
unique_ptr<Value> StringValue::copy() {
    auto val = make_unique<StringValue>(position, value);
    return val;
}


/*
    StaticArrayValue
*/
StaticArrayValue::StaticArrayValue(const CodePosition& position) 
    : Value(position, Value::ValueKind::StaticArray) 
{
    isConstexpr = true;
}
optional<unique_ptr<Value>> StaticArrayValue::interpret(Scope* scope) {
    vector<unique_ptr<Type>> elementTypes;
    for (auto& element : values) {
        auto interpretValue = element->interpret(scope);
        if (!interpretValue) {
            return nullopt;
        }
        if (interpretValue.value()) {
            element = move(interpretValue.value());
        }
        if (find(elementTypes.begin(), elementTypes.end(), element->type->copy()) == elementTypes.end()) {
            elementTypes.push_back(element->type->copy());
        }
    }
    if (elementTypes.size() == 1) {
        type = make_unique<StaticArrayType>(
            (*elementTypes.begin())->copy(),
            make_unique<IntegerValue>(position, values.size())
            );
    } else {
        bool isFloat = false;
        for (auto& type : elementTypes) {
            if (type->kind == Type::Kind::Float) {
                isFloat = true;
                break;
            } else if (type->kind != Type::Kind::Integer) {
                errorMessage("couldn't deduce array type", position);
                return nullopt;
            }
        }
        if (isFloat) {
            type = make_unique<StaticArrayType>(
                make_unique<FloatType>(FloatType::Size::F64),
                make_unique<IntegerValue>(position, values.size())
                );
        } else {
            type = make_unique<StaticArrayType>(
                make_unique<IntegerType>(IntegerType::Size::I64),
                make_unique<IntegerValue>(position, values.size())
                );
        }
    }
    return nullptr;
}
bool StaticArrayValue::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const StaticArrayValue&>(value);
        return this->values == other.values
            && Value::operator==(other);
    }
    else {
        return false;
    }
}
unique_ptr<Value> StaticArrayValue::copy() {
    auto val = make_unique<StaticArrayValue>(position);
    val->type = type->copy();
    for (auto& value : values) {
        val->values.push_back(value->copy());
    }
    return val;
}


/*
    FunctionValue
*/
FunctionValue::FunctionValue(const CodePosition& position, unique_ptr<Type>&& type, Scope* parentScope) : 
    Value(position, Value::ValueKind::FunctionValue),
    body(position, Scope::Owner::Function, parentScope)
{
    isConstexpr = true;
    this->type = move(type);
}
optional<unique_ptr<Value>> FunctionValue::interpret(Scope* scope) {
    isConstexpr = true;
    return nullptr;
}
bool FunctionValue::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const FunctionValue&>(value);
        return this->argumentNames == other.argumentNames
            && Value::operator==(other);
    }
    else {
        return false;
    }
}
