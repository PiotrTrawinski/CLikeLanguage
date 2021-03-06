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
void Value::templateCopy(Value* value, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    value->valueKind = valueKind;
    if (type) {
        value->type = type->templateCopy(parentScope, templateToType);
    } else {
        value->type = nullptr;
    }
    value->isConstexpr = isConstexpr;
    Statement::templateCopy(value, parentScope, templateToType);
}
Statement* Value::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto value = Create(position, valueKind);
    templateCopy(value, parentScope, templateToType);
    return value;
}
optional<Value*> Value::interpret(Scope* scope) {
    if (type && !type->interpret(scope)) {
        return nullopt;
    }
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
void Value::createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj) {}
void Value::createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj) {}
llvm::Value* Value::createLlvm(LlvmObject* llvmObj) {
    return nullptr;
}
llvm::Value* Value::getReferenceLlvm(LlvmObject* llvmObj) {
    return nullptr;
}
void Value::createDestructorLlvm(LlvmObject* llvmObj) {
    internalError("cannot create destructor for this value ", position);
}

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
void Variable::templateCopy(Variable* value, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto foundType = templateToType.find(name);
    value->name = name;
    value->isConst = isConst;
    Value::templateCopy(value, parentScope, templateToType);
}
Statement* Variable::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto value = Create(position, name);
    templateCopy(value, parentScope, templateToType);
    return value;
}
bool Value::isLvalue(Value* value) {
    if (value->type->kind == Type::Kind::Reference) {
        return true;
    }
    if (value->valueKind == Value::ValueKind::Variable) {
        if (!((Variable*)value)->isConst) {
            return true;
        }
    }
    if (value->valueKind == Value::ValueKind::Operation) {
        auto operation = (Operation*)value;
        switch (operation->kind) {
        case Operation::Kind::GetValue:
        case Operation::Kind::Dot:
        case Operation::Kind::ArrayIndex:
            return true;
        default: break;
        }
    }
    return false;
}
bool Variable::interpretTypeAndDeclaration(Scope* scope) {
    if (type && !type->interpret(scope)) {
        return false;
    }
    declaration = scope->findDeclaration(this);
    if (!declaration) {
        return false;
    }
    return true;
}
optional<Value*> Variable::interpret(Scope* scope) {
    if (wasInterpreted) {
        return nullptr;
    }
    if (!interpretTypeAndDeclaration(scope)) {
        return nullopt;
    }
    if (declaration->scope->owner == Scope::Owner::Class) {
        Scope* searchScope = scope;
        while (searchScope) {
            if (searchScope->owner == Scope::Owner::Function) {
                auto functionScope = (FunctionScope*)searchScope;
                auto dotOperation = DotOperation::Create(position);
                dotOperation->arguments.push_back(functionScope->function->arguments.back()->variable);
                dotOperation->arguments.push_back(declaration->variable);
                auto interpretValue = dotOperation->interpret(functionScope);
                if (!interpretValue) internalError("couldn't translate implicit use of class member variable to 'this.'");
                else if (interpretValue.value()) return interpretValue.value();
                else return dotOperation;
            } else {
                searchScope = searchScope->parentScope;
            }
        }
        internalError("couldn't find function scope, but class member variable used", position);
    }

    wasInterpreted = true;
    isConstexpr = declaration->variable->isConstexpr;
    type = declaration->variable->type;
    isConst = declaration->variable->isConst;

    if (scope->maybeUninitializedDeclarations.find(declaration) != scope->maybeUninitializedDeclarations.end()) {
        warningMessage("use of possibly unitialized variable " + name, position);
    }

    if (type->kind != Type::Kind::Function && type->kind != Type::Kind::TemplateFunction && isConstexpr) {
        return declaration->value;
    }

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
llvm::Value* Variable::getReferenceLlvm(LlvmObject* llvmObj, bool ignoreReference) {
    if (!ignoreReference && type->kind == Type::Kind::Reference) {
        return new llvm::LoadInst(declaration->llvmVariable, "", llvmObj->block);
    } else {
        return declaration->llvmVariable;
    }
}
llvm::Value* Variable::getReferenceLlvm(LlvmObject* llvmObj) {
    return getReferenceLlvm(llvmObj, false);
}
llvm::Value* Variable::createLlvm(LlvmObject* llvmObj) {
    return new llvm::LoadInst(getReferenceLlvm(llvmObj), "", llvmObj->block);
}

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
void IntegerValue::templateCopy(IntegerValue* intValue, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    intValue->value = value;
    Value::templateCopy(intValue, parentScope, templateToType);
}
Statement* IntegerValue::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto intValue = Create(position, value);
    templateCopy(intValue, parentScope, templateToType);
    return intValue;
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
llvm::Value* IntegerValue::createLlvm(LlvmObject* llvmObj) {
    return llvm::ConstantInt::get(type->createLlvm(llvmObj), this->value);
}


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
void CharValue::templateCopy(CharValue* charValue, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    charValue->value = value;
    Value::templateCopy(charValue, parentScope, templateToType);
}
Statement* CharValue::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto charValue = Create(position, value);
    templateCopy(charValue, parentScope, templateToType);
    return charValue;
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
llvm::Value* CharValue::createLlvm(LlvmObject* llvmObj) {
    return llvm::ConstantInt::get(type->createLlvm(llvmObj), this->value);
}


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
void FloatValue::templateCopy(FloatValue* floatValue, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    floatValue->value = value;
    Value::templateCopy(floatValue, parentScope, templateToType);
}
Statement* FloatValue::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto floatValue = Create(position, value);
    templateCopy(floatValue, parentScope, templateToType);
    return floatValue;
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
llvm::Value* FloatValue::createLlvm(LlvmObject* llvmObj) {
    return llvm::ConstantFP::get(type->createLlvm(llvmObj), this->value);
}


/*
    BoolValue
*/
BoolValue::BoolValue(const CodePosition& position, bool value) : 
    Value(position, Value::ValueKind::Bool),
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
void BoolValue::templateCopy(BoolValue* boolValue, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    boolValue->value = value;
    Value::templateCopy(boolValue, parentScope, templateToType);
}
Statement* BoolValue::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto boolValue = Create(position, value);
    templateCopy(boolValue, parentScope, templateToType);
    return boolValue;
}
optional<Value*> BoolValue::interpret(Scope* scope) {
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
llvm::Value* BoolValue::createLlvm(LlvmObject* llvmObj) {
    return llvm::ConstantInt::get(type->createLlvm(llvmObj), this->value);
}


/*
    StringValue
*/
StringValue::StringValue(const CodePosition& position, const string& value) : 
    Value(position, Value::ValueKind::String),
    value(value)
{
    isConstexpr = false;
    type = DynamicArrayType::Create(IntegerType::Create(IntegerType::Size::U8));
}
vector<unique_ptr<StringValue>> StringValue::objects;
StringValue* StringValue::Create(const CodePosition& position, const string& value) {
    objects.emplace_back(make_unique<StringValue>(position, value));
    return objects.back().get();
}
void StringValue::templateCopy(StringValue* stringValue, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    stringValue->value = value;
    Value::templateCopy(stringValue, parentScope, templateToType);
}
Statement* StringValue::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto stringValue = Create(position, value);
    templateCopy(stringValue, parentScope, templateToType);
    return stringValue;
}
optional<Value*> StringValue::interpret(Scope* scope) {
    Value* staticArrayValue = StaticArrayValue::Create(position);
    for (char c : value) {
        ((StaticArrayValue*)staticArrayValue)->values.push_back(CharValue::Create(position, c));
    }
    Value* dynArray = ConstructorOperation::Create(position, DynamicArrayType::Create(IntegerType::Create(IntegerType::Size::U8)), {staticArrayValue});

    auto dynArrayInterpret = dynArray->interpret(scope);
    if (!dynArrayInterpret) internalError("failed to interpret string dynamic array constructor", position);
    if (dynArrayInterpret.value()) dynArray = dynArrayInterpret.value();

    return dynArray;
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
void StaticArrayValue::templateCopy(StaticArrayValue* staticValue, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    for (auto value : values) {
        staticValue->values.push_back((Value*)value->templateCopy(parentScope, templateToType));
    }
    Value::templateCopy(staticValue, parentScope, templateToType);
}
Statement* StaticArrayValue::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto value = Create(position);
    templateCopy(value, parentScope, templateToType);
    return value;
}
optional<Value*> StaticArrayValue::interpret(Scope* scope) {
    if (wasInterpreted) {
        return nullptr;
    }
    wasInterpreted = true;

    vector<Type*> elementTypes;
    for (auto& element : values) {
        auto interpretValue = element->interpret(scope);
        if (!interpretValue) return nullopt;
        if (interpretValue.value()) element = interpretValue.value();

        bool found = false;
        for (auto& elementType : elementTypes) {
            if (*element->type->getEffectiveType() == *elementType) {
                found = true;
                break;
            }
        }
        if (!found) {
            elementTypes.push_back(element->type->getEffectiveType());
        }
    }
    if (elementTypes.size() == 1) {
        type = StaticArrayType::Create(*elementTypes.begin(), values.size());
    } else {
        // if multiple different types it has to be arithmetic values (ints, floats)
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

                return errorMessageOpt(message, position);
            }
        }
        for (auto& element : values) {
            auto ctorToDeduced = ConstructorOperation::Create(position, deducedType, {element});
            auto castInterpret = ctorToDeduced->interpret(scope);
            if (!castInterpret) internalError("successfully deduced array type, but couldn't cast memebers to deduced type", position);
            else if (castInterpret.value()) element = castInterpret.value();
            else element = ctorToDeduced;
        }
        type = StaticArrayType::Create(deducedType, values.size());
    }

    for (auto& value : values) {
        value->wasCaptured = true;
    }

    if (type->needsDestruction()) {
        scope->valuesToDestroyBuffer.push_back(this);
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
void StaticArrayValue::createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj) {
    if (!llvmRef) {
        bool allElementsConstexpr = true;
        for (auto value : values) {
            if (!value->isConstexpr) {
                allElementsConstexpr = false;
                break;
            }
        }
        if (!allElementsConstexpr) {
            llvmRef = type->allocaLlvm(llvmObj);
        }
    }
    
}
void StaticArrayValue::createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj) {
    if (!llvmRef) {
        llvmRef = type->allocaLlvm(llvmObj);
    }
}
llvm::Value* StaticArrayValue::createLlvm(LlvmObject* llvmObj) {
    bool allElementsConstexpr = true;
    for (auto value : values) {
        if (!value->isConstexpr) {
            allElementsConstexpr = false;
            break;
        }
    }
    if (allElementsConstexpr) {
        vector<llvm::Constant*> llvmValues;
        for (auto value : values) {
            llvmValues.push_back((llvm::Constant*)value->createLlvm(llvmObj));
        }
        llvmValue = llvm::ConstantArray::get((llvm::ArrayType*)type->createLlvm(llvmObj), llvmValues);
        return llvmValue;
    } else {
        for (int i = 0; i < values.size(); ++i) {
            vector<llvm::Value*> indexList;
            if (((StaticArrayType*)type)->sizeAsInt != -1) {
                indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
            }
            indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), i));
            auto elementPtr = llvm::GetElementPtrInst::Create(
                ((llvm::PointerType*)llvmRef->getType())->getElementType(),
                llvmRef,
                indexList,
                "",
                llvmObj->block
            );
            new llvm::StoreInst(values[i]->createLlvm(llvmObj), elementPtr, llvmObj->block);
        }
        return new llvm::LoadInst(llvmRef, "", llvmObj->block);
    }
}
llvm::Value* StaticArrayValue::getReferenceLlvm(LlvmObject* llvmObj) {
    bool allElementsConstexpr = true;
    for (auto value : values) {
        if (!value->isConstexpr) {
            allElementsConstexpr = false;
            break;
        }
    }
    if (allElementsConstexpr) {
        vector<llvm::Constant*> llvmValues;
        for (auto value : values) {
            llvmValues.push_back((llvm::Constant*)value->createLlvm(llvmObj));
        }
        new llvm::StoreInst(llvm::ConstantArray::get((llvm::ArrayType*)type->createLlvm(llvmObj), llvmValues), llvmRef, llvmObj->block);
    } else {
        for (int i = 0; i < values.size(); ++i) {
            vector<llvm::Value*> indexList;
            if (((StaticArrayType*)type)->sizeAsInt != -1) {
                indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
            }
            indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), i));
            auto elementPtr = llvm::GetElementPtrInst::Create(
                ((llvm::PointerType*)llvmRef->getType())->getElementType(),
                llvmRef,
                indexList,
                "",
                llvmObj->block
            );
            new llvm::StoreInst(values[i]->createLlvm(llvmObj), elementPtr, llvmObj->block);
        }
    }
    return llvmRef;
}
void StaticArrayValue::createDestructorLlvm(LlvmObject* llvmObj) {
    if (llvmRef) {
        type->createLlvmDestructorRef(llvmObj, llvmRef);
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
    body(FunctionScope::Create(position, parentScope, this))
{
    isConstexpr = true;
    this->type = type;
}
vector<unique_ptr<FunctionValue>> FunctionValue::objects;
FunctionValue* FunctionValue::Create(const CodePosition& position, Type* type, Scope* parentScope) {
    objects.emplace_back(make_unique<FunctionValue>(position, type, parentScope));
    return objects.back().get();
}
void FunctionValue::templateCopy(FunctionValue* value, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    for (auto argument : arguments) {
        value->arguments.push_back((Declaration*)argument->templateCopy(parentScope, templateToType));
    }
    value->body = (FunctionScope*)body->templateCopy(parentScope, templateToType);
    value->body->function = value;
    Value::templateCopy(value, parentScope, templateToType);
}
Statement* FunctionValue::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto value = Create(position, nullptr, parentScope);
    templateCopy(value, parentScope, templateToType);
    return value;
}
optional<Value*> FunctionValue::interpret(Scope* scope) {
    if (type && !type->interpret(scope)) {
        return nullopt;
    }
    isConstexpr = true;
    for (auto& argument : arguments) {
        if (argument->value) {
            internalError("function argument declaration has value", argument->position);
        }
        argument->status = Declaration::Status::Completed;
        body->declarationMap.addVariableDeclaration(argument);
        body->declarationsInitState.insert({argument, true});
        body->declarationsOrder.push_back(argument);
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
llvm::Value* FunctionValue::createLlvm(LlvmObject* llvmObj) {
    return createLlvm(llvmObj, "");
}
llvm::Value* FunctionValue::createLlvm(LlvmObject* llvmObj, const string& functionName) {
    if (llvmFunction) {
        if (!didSetLlvmName && !functionName.empty()) {
            didSetLlvmName = true;
            string uniqueFunctionName = functionName;
            int id = 1;
            if (llvmObj->module->getFunction(uniqueFunctionName)) {
                uniqueFunctionName = functionName + to_string(id);
                id += 1;
                while (llvmObj->module->getFunction(uniqueFunctionName)) {
                    uniqueFunctionName = functionName + to_string(id);
                    id += 1;
                }
            }
            llvmFunction->setName(uniqueFunctionName);
        }
        return llvmFunction;
    }
    if (!functionName.empty()) {
        didSetLlvmName = true;
    }
    string uniqueFunctionName = functionName;
    int id = 1;
    if (llvmObj->module->getFunction(uniqueFunctionName)) {
        uniqueFunctionName = functionName + to_string(id);
        id += 1;
        while (llvmObj->module->getFunction(uniqueFunctionName)) {
            uniqueFunctionName = functionName + to_string(id);
            id += 1;
        }
    }
    llvmFunction = llvm::cast<llvm::Function>(llvmObj->module->getOrInsertFunction(
        uniqueFunctionName, 
        (llvm::FunctionType*)((llvm::PointerType*)type->createLlvm(llvmObj))->getElementType())
        );

    auto oldFunction = llvmObj->function; 
    llvmObj->function = llvmFunction;
    this->body->createLlvm(llvmObj);
    llvmObj->function = oldFunction;

    return llvmFunction;
}


/*
    NullValue
*/
NullValue::NullValue(const CodePosition& position, Type* type) : 
    Value(position, Value::ValueKind::Null)
{
    isConstexpr = true;
    this->type = type;
}
vector<unique_ptr<NullValue>> NullValue::objects;
NullValue* NullValue::Create(const CodePosition& position, Type* type) {
    objects.emplace_back(make_unique<NullValue>(position, type));
    return objects.back().get();
}
void NullValue::templateCopy(NullValue* value, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    Value::templateCopy(value, parentScope, templateToType);
}
Statement* NullValue::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto value = Create(position, nullptr);
    templateCopy(value, parentScope, templateToType);
    return value;
}
optional<Value*> NullValue::interpret(Scope* scope) {
    if (!type->interpret(scope)) {
        return nullopt;
    }
    return nullptr;
}
llvm::Value* NullValue::createLlvm(LlvmObject* llvmObj) {
    return llvm::ConstantPointerNull::get((llvm::PointerType*)type->createLlvm(llvmObj));
}


/*
    TemplatedVariable
*/
TemplatedVariable::TemplatedVariable(const CodePosition& position, const string& name) : 
    Value(position, Value::ValueKind::TemplatedVariable),
    name(name)
{}
vector<unique_ptr<TemplatedVariable>> TemplatedVariable::objects;
TemplatedVariable* TemplatedVariable::Create(const CodePosition& position, const string& name) {
    objects.emplace_back(make_unique<TemplatedVariable>(position, name));
    return objects.back().get();
}
void TemplatedVariable::templateCopy(TemplatedVariable* value, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    value->name = name;
    for (auto templatedType : templatedTypes) {
        value->templatedTypes.push_back(templatedType->templateCopy(parentScope, templateToType));
    }
    Value::templateCopy(value, parentScope, templateToType);
}
Statement* TemplatedVariable::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto value = Create(position, name);
    templateCopy(value, parentScope, templateToType);
    return value;
}
optional<Value*> TemplatedVariable::interpret(Scope* scope) {
    if (wasInterpreted) {
        return nullptr;
    }
    wasInterpreted = true;

    return nullptr;
}