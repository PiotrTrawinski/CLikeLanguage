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
vector<unique_ptr<Value>> Value::objects;
Value* Value::Create(const CodePosition& position, ValueKind valueKind) {
    objects.emplace_back(make_unique<Value>(position, valueKind));
    return objects.back().get();
}
optional<Value*> Value::interpret(Scope* scope) {
    return nullptr;
}
bool Value::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const Value&>(value);
        return this->valueKind == other.valueKind
            && cmpPtr(this->type, other.type)
            && this->isConstexpr == other.isConstexpr
            && Statement::operator==(other);
    } else {
        return false;
    }
}
/*unique_ptr<Value> Value::copy() {
    auto value = make_unique<Value>(position, valueKind);
    value->type = type;
    value->isConstexpr = isConstexpr;
    return value;
}*/


/*
    Variable
*/
Variable::Variable(const CodePosition& position, const string& name) : 
    Value(position, Value::ValueKind::Variable),
    name(name)
{}
vector<unique_ptr<Variable>> Variable::objects;
Variable* Variable::Create(const CodePosition& position, const string& name) {
    objects.emplace_back(make_unique<Variable>(position, name));
    return objects.back().get();
}
optional<Value*> Variable::interpret(Scope* scope) {
    Declaration* declaration = scope->findDeclaration(this);
    if (!declaration) {
        return nullopt;
    }
    isConstexpr = declaration->variable->isConstexpr;
    type = declaration->variable->type;
    isConst = declaration->variable->isConst;

    /*if (isConstexpr) {
        return Value::copy();
    }*/
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
/*unique_ptr<Value> Variable::copy() {
    auto variable = make_unique<Variable>(position, name);
    variable->type = type;
    variable->isConstexpr = isConstexpr;
    variable->isConst = isConst;
    return variable;
}*/


/*
    IntegerValue
*/
IntegerValue::IntegerValue(const CodePosition& position, uint64_t value) : 
    Value(position, Value::ValueKind::Integer),
    value(value)
{
    isConstexpr = true;
    type = IntegerType::Create(IntegerType::Size::I64);
}
vector<unique_ptr<IntegerValue>> IntegerValue::objects;
IntegerValue* IntegerValue::Create(const CodePosition& position, uint64_t value) {
    objects.emplace_back(make_unique<IntegerValue>(position, value));
    return objects.back().get();
}
optional<Value*> IntegerValue::interpret(Scope* scope) {
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
/*unique_ptr<Value> IntegerValue::copy() {
    auto val = make_unique<IntegerValue>(position, value);
    return val;
}*/


/*
    CharValue
*/
CharValue::CharValue(const CodePosition& position, uint8_t value) : 
    Value(position, Value::ValueKind::Char),
    value(value)
{
    isConstexpr = true;
    type = IntegerType::Create(IntegerType::Size::U8);
}
vector<unique_ptr<CharValue>> CharValue::objects;
CharValue* CharValue::Create(const CodePosition& position, uint8_t value) {
    objects.emplace_back(make_unique<CharValue>(position, value));
    return objects.back().get();
}
optional<Value*> CharValue::interpret(Scope* scope) {
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
/*unique_ptr<Value> CharValue::copy() {
    auto val = make_unique<CharValue>(position, value);
    return val;
}*/


/*
    FloatValue
*/
FloatValue::FloatValue(const CodePosition& position, double value) :
    Value(position, Value::ValueKind::Float),
    value(value)
{
    isConstexpr = true;
    type = FloatType::Create(FloatType::Size::F64);
}
vector<unique_ptr<FloatValue>> FloatValue::objects;
FloatValue* FloatValue::Create(const CodePosition& position, double value) {
    objects.emplace_back(make_unique<FloatValue>(position, value));
    return objects.back().get();
}
optional<Value*> FloatValue::interpret(Scope* scope) {
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
/*unique_ptr<Value> FloatValue::copy() {
    auto val = make_unique<FloatValue>(position, value);
    return val;
}*/


/*
    BoolValue
*/
BoolValue::BoolValue(const CodePosition& position, bool value) : 
    Value(position, Value::ValueKind::Char),
    value(value)
{
    isConstexpr = true;
    type = Type::Create(Type::Kind::Bool);
}
vector<unique_ptr<BoolValue>> BoolValue::objects;
BoolValue* BoolValue::Create(const CodePosition& position, bool value) {
    objects.emplace_back(make_unique<BoolValue>(position, value));
    return objects.back().get();
}
optional<Value*> BoolValue::interpret(Scope* scope) {
    isConstexpr = true;
    return nullptr;
}
bool BoolValue::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const BoolValue&>(value);
        return this->value == other.value
            && Value::operator==(other);
    }
    else {
        return false;
    }
}


/*
    StringValue
*/
StringValue::StringValue(const CodePosition& position, const string& value) : 
    Value(position, Value::ValueKind::String),
    value(value)
{
    isConstexpr = true;
    type = Type::Create(Type::Kind::String);
}
vector<unique_ptr<StringValue>> StringValue::objects;
StringValue* StringValue::Create(const CodePosition& position, const string& value) {
    objects.emplace_back(make_unique<StringValue>(position, value));
    return objects.back().get();
}
optional<Value*> StringValue::interpret(Scope* scope) {
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
/*unique_ptr<Value> StringValue::copy() {
    auto val = make_unique<StringValue>(position, value);
    return val;
}*/


/*
    StaticArrayValue
*/
StaticArrayValue::StaticArrayValue(const CodePosition& position) 
    : Value(position, Value::ValueKind::StaticArray) 
{
    isConstexpr = true;
}
vector<unique_ptr<StaticArrayValue>> StaticArrayValue::objects;
StaticArrayValue* StaticArrayValue::Create(const CodePosition& position) {
    objects.emplace_back(make_unique<StaticArrayValue>(position));
    return objects.back().get();
}
optional<Value*> StaticArrayValue::interpret(Scope* scope) {
    vector<Type*> elementTypes;
    for (auto& element : values) {
        auto interpretValue = element->interpret(scope);
        if (!interpretValue) {
            return nullopt;
        }
        if (interpretValue.value()) {
            element = interpretValue.value();
        }
        bool found = false;
        for (auto& elementType : elementTypes) {
            if (*element->type == *elementType) {
                found = true;
                break;
            }
        }
        if (!found) {
            elementTypes.push_back(element->type);
        }
    }
    if (elementTypes.size() == 1) {
        type = StaticArrayType::Create(*elementTypes.begin(), values.size());
    } else {
        // if multiple different types it have to be arithmetic values (ints, floats)
        // otherwise cannot deduce type of the array
        Type* deducedType = elementTypes[0];
        for (auto& elementType : elementTypes) {
            if (elementType->kind == Type::Kind::Integer || elementType->kind == Type::Kind::Float) {
                deducedType = Type::getSuitingArithmeticType(deducedType, elementType);
            } else {
                string message = "couldn't deduce array type. types are: ";
                for (int i = 0; i < elementTypes.size(); ++i) {
                    message += DeclarationMap::toString(elementTypes[i]);
                    if (i != elementTypes.size() - 1) {
                        message += "; ";
                    }
                }

                errorMessage(message, position);
                return nullopt;
            }
        }
        type = StaticArrayType::Create(deducedType, values.size());
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
/*unique_ptr<Value> StaticArrayValue::copy() {
    auto val = make_unique<StaticArrayValue>(position);
    val->type = type->copy();
    for (auto& value : values) {
        val->values.push_back(value->copy());
    }
    return val;
}*/


/*
    FunctionValue
*/
FunctionValue::FunctionValue(const CodePosition& position, Type* type, Scope* parentScope) : 
    Value(position, Value::ValueKind::FunctionValue),
    body(CodeScope::Create(position, Scope::Owner::Function, parentScope))
{
    isConstexpr = true;
    this->type = type;
}
vector<unique_ptr<FunctionValue>> FunctionValue::objects;
FunctionValue* FunctionValue::Create(const CodePosition& position, Type* type, Scope* parentScope) {
    objects.emplace_back(make_unique<FunctionValue>(position, type, parentScope));
    return objects.back().get();
}
optional<Value*> FunctionValue::interpret(Scope* scope) {
    isConstexpr = true;
    for (auto& argument : arguments) {
        if (argument->value) {
            internalError("function argument declaration has value", argument->position);
            return nullopt;
        }
        argument->status = Declaration::Status::Completed;
        body->declarationMap.addVariableDeclaration(argument);
    }
    if (!body->interpret()) {
        return nullopt;
    }
    return nullptr;
}
bool FunctionValue::operator==(const Statement& value) const {
    if(typeid(value) == typeid(*this)){
        const auto& other = static_cast<const FunctionValue&>(value);
        return this->arguments == other.arguments
            && Value::operator==(other);
    }
    else {
        return false;
    }
}
