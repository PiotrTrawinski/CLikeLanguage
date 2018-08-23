#include "Type.h"
#include "Value.h"
#include "ClassDeclaration.h"
#include "Declaration.h"
#include "Operation.h"

using namespace std;


/*
    Type
*/
Type::Type (Kind kind) : kind(kind) {}
vector<unique_ptr<Type>> Type::objects;
Type* Type::Create(Kind kind) {
    objects.emplace_back(make_unique<Type>(kind));
    return objects.back().get();
}
bool Type::interpret(Scope* scope, bool needFullDeclaration) {
    return true;
}
bool Type::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const Type& other = static_cast<const Type&>(type);
        return this->kind == other.kind;
    }
    else {
        return false;
    }
}
Type* Type::getEffectiveType() {
    return this;
}
Value* Type::typesize(Scope* scope) {
    return IntegerValue::Create(CodePosition(nullptr,0,0), sizeInBytes());
}
int Type::sizeInBytes() {
    switch (kind) {
    case Kind::Bool:
        return 1;
    case Kind::Void:
        return 0;
    default:
        return 0;
    }
}
/*unique_ptr<Type> Type::copy() {
    return make_unique<Type>(this->kind);
}*/
llvm::Type* Type::createLlvm(LlvmObject* llvmObj) {
    switch (kind) {
    case Kind::Bool:
        return llvm::Type::getInt1Ty(llvmObj->context);
    case Kind::Void:
        return llvm::Type::getVoidTy(llvmObj->context);
    }
    return nullptr;
}
llvm::AllocaInst* Type::allocaLlvm(LlvmObject* llvmObj, const string& name) {
    return new llvm::AllocaInst(createLlvm(llvmObj), 0, name, llvmObj->block);
}

Type* getSuitingIntegerType(IntegerType* i1, IntegerType* i2) {
    if (i1->isSigned() || i2->isSigned()) {
        return IntegerType::Create(IntegerType::Size::I64);
    } else {
        return IntegerType::Create(IntegerType::Size::U64);
    }
}
Type* getSuitingFloatType(FloatType* f1, FloatType* f2) {
    if (f1->size == FloatType::Size::F64 || f2->size == FloatType::Size::F64) {
        return FloatType::Create(FloatType::Size::F64);
    } else {
        return FloatType::Create(FloatType::Size::F32);
    }
}
Type* Type::getSuitingArithmeticType(Type* val1, Type* val2) {
    if (!val1 || !val2) {
        return nullptr;
    }
    if (val1->kind == Type::Kind::Integer && val2->kind == Type::Kind::Integer) {
        return getSuitingIntegerType((IntegerType*)val1, (IntegerType*)val2);
    } else if (val1->kind == Type::Kind::Float && val2->kind == Type::Kind::Float) {
        return getSuitingFloatType((FloatType*)val1, (FloatType*)val2);
    } else if (val1->kind == Type::Kind::Integer && val2->kind == Type::Kind::Float) {
        return val2;
    } else if (val1->kind == Type::Kind::Float && val2->kind == Type::Kind::Integer) {
        return val1;
    } else {
        return nullptr;
    }
}

/*
    OwnerPointerType
*/
OwnerPointerType::OwnerPointerType(Type* underlyingType) : 
    Type(Type::Kind::OwnerPointer),
    underlyingType(underlyingType)
{}
vector<unique_ptr<OwnerPointerType>> OwnerPointerType::objects;
OwnerPointerType* OwnerPointerType::Create(Type* underlyingType) {
    objects.emplace_back(make_unique<OwnerPointerType>(underlyingType));
    return objects.back().get();
}
bool OwnerPointerType::interpret(Scope* scope, bool needFullDeclaration) {
    return underlyingType->interpret(scope, false);
}
bool OwnerPointerType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const OwnerPointerType&>(type);
        return cmpPtr(this->underlyingType, other.underlyingType);
    } else {
        return false;
    }
}
int OwnerPointerType::sizeInBytes() {
    return 8;
}
/*unique_ptr<Type> OwnerPointerType::copy() {
    return make_unique<OwnerPointerType>(this->underlyingType->copy());
}*/
llvm::Type* OwnerPointerType::createLlvm(LlvmObject* llvmObj) {
    if (underlyingType->kind == Type::Kind::Void) {
        return llvm::PointerType::get(llvm::Type::getInt8Ty(llvmObj->context), 0);
    } else {
        return llvm::PointerType::get(underlyingType->createLlvm(llvmObj), 0);
    }
}

/*
    RawPointerType
*/
RawPointerType::RawPointerType(Type* underlyingType) : 
    Type(Type::Kind::RawPointer),
    underlyingType(underlyingType)
{}
vector<unique_ptr<RawPointerType>> RawPointerType::objects;
RawPointerType* RawPointerType::Create(Type* underlyingType) {
    objects.emplace_back(make_unique<RawPointerType>(underlyingType));
    return objects.back().get();
}
bool RawPointerType::interpret(Scope* scope, bool needFullDeclaration) {
    return underlyingType->interpret(scope, false);
}
bool RawPointerType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const RawPointerType&>(type);
        return cmpPtr(this->underlyingType, other.underlyingType);
    } else {
        return false;
    }
}
int RawPointerType::sizeInBytes() {
    return 8;
}
/*unique_ptr<Type> RawPointerType::copy() {
    return make_unique<RawPointerType>(this->underlyingType->copy());
}*/
llvm::Type* RawPointerType::createLlvm(LlvmObject* llvmObj) {
    if (underlyingType->kind == Type::Kind::Void) {
        return llvm::PointerType::get(llvm::Type::getInt8Ty(llvmObj->context), 0);
    } else {
        return llvm::PointerType::get(underlyingType->createLlvm(llvmObj), 0);
    }
}

/*
    MaybeErrorType
*/
MaybeErrorType::MaybeErrorType(Type* underlyingType) : 
    Type(Type::Kind::MaybeError),
    underlyingType(underlyingType)
{}
vector<unique_ptr<MaybeErrorType>> MaybeErrorType::objects;
MaybeErrorType* MaybeErrorType::Create(Type* underlyingType) {
    objects.emplace_back(make_unique<MaybeErrorType>(underlyingType));
    return objects.back().get();
}
bool MaybeErrorType::interpret(Scope* scope, bool needFullDeclaration) {
    return underlyingType->interpret(scope, needFullDeclaration);
}
bool MaybeErrorType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const MaybeErrorType&>(type);
        return cmpPtr(this->underlyingType, other.underlyingType);
    } else {
        return false;
    }
}
Value* MaybeErrorType::typesize(Scope* scope) {
    auto position = CodePosition(nullptr,0,0);
    auto add = Operation::Create(position, Operation::Kind::Add);
    add->arguments.push_back(IntegerValue::Create(position, 8));
    add->arguments.push_back(underlyingType->typesize(scope));
    auto addInterpret = add->interpret(scope);
    if (!addInterpret) internalError("couldn't calculate sizeof maybe error type");
    if (addInterpret.value()) return addInterpret.value();
    else return add;
}
int MaybeErrorType::sizeInBytes() {
    return underlyingType->sizeInBytes() + 8;
}
/*unique_ptr<Type> MaybeErrorType::copy() {
    return make_unique<MaybeErrorType>(this->underlyingType->copy());
}*/
llvm::Type* MaybeErrorType::createLlvm(LlvmObject* llvmObj) {
    if (!llvmType) {
        if (underlyingType->kind == Type::Kind::Void) {
            llvmType = llvm::Type::getInt64Ty(llvmObj->context);
        } else {
            llvmType = llvm::StructType::get(llvmObj->context, {
                underlyingType->createLlvm(llvmObj),
                llvm::Type::getInt64Ty(llvmObj->context)
            });
        }
    }
    return llvmType;
}

/*
    ReferenceType
*/
ReferenceType::ReferenceType(Type* underlyingType) : 
    Type(Type::Kind::Reference),
    underlyingType(underlyingType)
{}
vector<unique_ptr<ReferenceType>> ReferenceType::objects;
ReferenceType* ReferenceType::Create(Type* underlyingType) {
    objects.emplace_back(make_unique<ReferenceType>(underlyingType));
    return objects.back().get();
}
bool ReferenceType::interpret(Scope* scope, bool needFullDeclaration) {
    return underlyingType->interpret(scope, false);
}
bool ReferenceType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const ReferenceType&>(type);
        return cmpPtr(this->underlyingType, other.underlyingType);
    } else {
        return false;
    }
}
Type* ReferenceType::getEffectiveType() {
    return underlyingType->getEffectiveType();
}
int ReferenceType::sizeInBytes() {
    return 8;
}
/*unique_ptr<Type> ReferenceType::copy() {
    return make_unique<ReferenceType>(this->underlyingType->copy());
}*/
llvm::Type* ReferenceType::createLlvm(LlvmObject* llvmObj) {
    return llvm::PointerType::get(underlyingType->createLlvm(llvmObj), 0);
}

/*
    StaticArrayType
*/
StaticArrayType::StaticArrayType(Type* elementType, Value* size) : 
    Type(Type::Kind::StaticArray),
    elementType(elementType),
    size(size)
{}
StaticArrayType::StaticArrayType(Type* elementType, int64_t sizeAsInt) : 
    Type(Type::Kind::StaticArray),
    elementType(elementType),
    size(nullptr),
    sizeAsInt(sizeAsInt)
{}
vector<unique_ptr<StaticArrayType>> StaticArrayType::objects;
StaticArrayType* StaticArrayType::Create(Type* elementType, Value* size) {
    objects.emplace_back(make_unique<StaticArrayType>(elementType, size));
    return objects.back().get();
}
StaticArrayType* StaticArrayType::Create(Type* elementType, int64_t sizeAsInt) {
    objects.emplace_back(make_unique<StaticArrayType>(elementType, sizeAsInt));
    return objects.back().get();
}
bool StaticArrayType::interpret(Scope* scope, bool needFullDeclaration) {
    if (size) {
        auto sizeInterpret = size->interpret(scope);
        if (!sizeInterpret) return false;
        if (sizeInterpret.value()) size = sizeInterpret.value();
        if (size->isConstexpr) {
            sizeAsInt = ((IntegerValue*)size)->value;
            size = nullptr;
        }
    }
    
    return elementType ? elementType->interpret(scope, needFullDeclaration) : true;
}
bool StaticArrayType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const StaticArrayType&>(type);
        return cmpPtr(this->elementType, other.elementType)
            && cmpPtr(this->size, other.size)
            && this->sizeAsInt == other.sizeAsInt;
    } else {
        return false;
    }
}
Value* StaticArrayType::typesize(Scope* scope) {
    Operation* mul;
    if (sizeAsInt == -1) {
        mul = Operation::Create(size->position, Operation::Kind::Mul);
        mul->arguments.push_back(size);
    } else {
        mul = Operation::Create(CodePosition(nullptr,0,0), Operation::Kind::Mul);
        mul->arguments.push_back(IntegerValue::Create(CodePosition(nullptr,0,0), sizeAsInt));
    }
    mul->arguments.push_back(elementType->typesize(scope));
    auto mulInterpret = mul->interpret(scope);
    if (!mulInterpret) {
        if (size) {
            internalError("couldn't calculate sizeof static array type", size->position);
        } else {
            internalError("couldn't calculate sizeof static array type");
        }
    }
    if (mulInterpret.value()) return mulInterpret.value();
    else return mul;
}
int StaticArrayType::sizeInBytes() {
    return sizeAsInt * elementType->sizeInBytes();
}
/*unique_ptr<Type> StaticArrayType::copy() {
    if (this->size) {
        return make_unique<StaticArrayType>(this->elementType->copy(), this->size->copy());
    } else {
        return make_unique<StaticArrayType>(this->elementType->copy(), this->sizeAsInt);
    }
}*/
llvm::Type* StaticArrayType::createLlvm(LlvmObject* llvmObj) {
    if (sizeAsInt == -1) {
        return llvm::PointerType::get(elementType->createLlvm(llvmObj), 0);
    } else {
        return llvm::ArrayType::get(elementType->createLlvm(llvmObj), sizeAsInt);
    }
}
llvm::AllocaInst* StaticArrayType::allocaLlvm(LlvmObject* llvmObj, const string& name) {
    if (sizeAsInt == -1) {
        return new llvm::AllocaInst(elementType->createLlvm(llvmObj), 0, size->createLlvm(llvmObj), name, llvmObj->block);
    } else {
        return new llvm::AllocaInst(createLlvm(llvmObj), 0, name, llvmObj->block);
    }
}


/*
    DynamicArrayType
*/
DynamicArrayType::DynamicArrayType(Type* elementType) : 
    Type(Type::Kind::DynamicArray),
    elementType(elementType)
{}
vector<unique_ptr<DynamicArrayType>> DynamicArrayType::objects;
DynamicArrayType* DynamicArrayType::Create(Type* elementType) {
    objects.emplace_back(make_unique<DynamicArrayType>(elementType));
    return objects.back().get();
}
bool DynamicArrayType::interpret(Scope* scope, bool needFullDeclaration) {
    return elementType ? elementType->interpret(scope, false) : true;
}
bool DynamicArrayType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const DynamicArrayType&>(type);
        return cmpPtr(this->elementType, other.elementType);
    } else {
        return false;
    }
}
int DynamicArrayType::sizeInBytes() {
    return 24;
}
/*unique_ptr<Type> DynamicArrayType::copy() {
    return make_unique<DynamicArrayType>(this->elementType->copy());
}*/
llvm::Type* DynamicArrayType::createLlvm(LlvmObject* llvmObj) {
    return llvm::Type::getFloatTy(llvmObj->context);
}



/*
    ArrayViewType
*/
ArrayViewType::ArrayViewType(Type* elementType) : 
    Type(Type::Kind::ArrayView),
    elementType(elementType)
{}
vector<unique_ptr<ArrayViewType>> ArrayViewType::objects;
ArrayViewType* ArrayViewType::Create(Type* elementType) {
    objects.emplace_back(make_unique<ArrayViewType>(elementType));
    return objects.back().get();
}
bool ArrayViewType::interpret(Scope* scope, bool needFullDeclaration) {
    return elementType ? elementType->interpret(scope, false) : true;
}
bool ArrayViewType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const ArrayViewType&>(type);
        return cmpPtr(this->elementType, other.elementType);
    } else {
        return false;
    }
}
int ArrayViewType::sizeInBytes() {
    return 16;
}
/*unique_ptr<Type> ArrayViewType::copy() {
    return make_unique<ArrayViewType>(this->elementType->copy());
}*/
llvm::Type* ArrayViewType::createLlvm(LlvmObject* llvmObj) {
    return llvm::Type::getFloatTy(llvmObj->context);
}


/*
    ClassType
*/
ClassType::ClassType(const string& name) : 
    Type(Type::Kind::Class),
    name(name)
{}
vector<unique_ptr<ClassType>> ClassType::objects;
ClassType* ClassType::Create(const string& name) {
    objects.emplace_back(make_unique<ClassType>(name));
    return objects.back().get();
}
bool ClassType::interpret(Scope* scope, bool needFullDeclaration) {
    declaration = scope->classDeclarationMap.getDeclaration(name);
    if (!declaration && scope->parentScope) {
        return interpret(scope->parentScope, needFullDeclaration);
    }
    if (needFullDeclaration) {
        return declaration && declaration->interpret();
    } else {
        return declaration != nullptr;
    }
}
bool ClassType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const ClassType&>(type);
        return this->name == other.name
            && this->declaration == other.declaration
            && this->templateTypes == other.templateTypes;
    } else {
        return false;
    }
}
int ClassType::sizeInBytes() {
    int sum = 0;
    for (auto memberDeclaration : declaration->body->declarations) {
        if (!memberDeclaration->variable->isConstexpr) {
            sum += memberDeclaration->variable->type->sizeInBytes();
        }
    }
    return sum;
}
/*unique_ptr<Type> ClassType::copy() {
    auto type = make_unique<ClassType>(this->name);
    for (auto& templateType : templateTypes) {
        type->templateTypes.push_back(templateType->copy());
    }
    return type;
}*/
llvm::Type* ClassType::createLlvm(LlvmObject* llvmObj) {
    return declaration->getLlvmType(llvmObj);
}



/*
    FunctionType
*/
FunctionType::FunctionType() : Type(Type::Kind::Function) {}
vector<unique_ptr<FunctionType>> FunctionType::objects;
FunctionType* FunctionType::Create() {
    objects.emplace_back(make_unique<FunctionType>());
    return objects.back().get();
}
bool FunctionType::interpret(Scope* scope, bool needFullDeclaration) {
    for (auto* argumentType : argumentTypes) {
        if (!argumentType->interpret(scope, false)) {
            return false;
        }
    }
    return returnType ? returnType->interpret(scope, false) : true;
}
bool FunctionType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const FunctionType&>(type);
        return this->argumentTypes == other.argumentTypes
            && cmpPtr(this->returnType, other.returnType);
    } else {
        return false;
    }
}
int FunctionType::sizeInBytes() {
    return 8;
}
/*unique_ptr<Type> FunctionType::copy() {
    auto type = make_unique<FunctionType>();
    type->returnType = returnType->copy();
    for (auto& argumentType : argumentTypes) {
        type->argumentTypes.push_back(argumentType->copy());
    }
    return type;
}*/
llvm::Type* FunctionType::createLlvm(LlvmObject* llvmObj) {
    vector<llvm::Type*> types;
    for (auto argType : argumentTypes) {
        types.push_back(argType->createLlvm(llvmObj));
    }
    return llvm::PointerType::get(
        llvm::FunctionType::get(returnType->createLlvm(llvmObj), types, false), 
        0
    );
}


/*
    IntegerType
*/
IntegerType::IntegerType(Size size) : 
    Type(Type::Kind::Integer), 
    size(size) 
{}
vector<unique_ptr<IntegerType>> IntegerType::objects;
IntegerType* IntegerType::Create(Size size) {
    objects.emplace_back(make_unique<IntegerType>(size));
    return objects.back().get();
}
bool IntegerType::isSigned() {
    switch (size) {
    case IntegerType::Size::I8:
    case IntegerType::Size::I16:
    case IntegerType::Size::I32:
    case IntegerType::Size::I64:
        return true;
    default:
        return false;
    }
}
int IntegerType::sizeInBytes() {
    switch (size) {
    case Size::I8:
    case Size::U8:
        return 1;
    case Size::I16:
    case Size::U16:
        return 2;
    case Size::I32:
    case Size::U32:
        return 4;
    case Size::I64:
    case Size::U64:
        return 8;
    default: 
        return 0;
    }
}
bool IntegerType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const IntegerType&>(type);
        return this->size == other.size;
    } else {
        return false;
    }
}
/*unique_ptr<Type> IntegerType::copy() {
    return make_unique<IntegerType>(size);
}*/
llvm::Type* IntegerType::createLlvm(LlvmObject* llvmObj) {
    switch (size) {
    case Size::I8:
    case Size::U8:
        return llvm::Type::getInt8Ty(llvmObj->context);
    case Size::I16:
    case Size::U16:
        return llvm::Type::getInt16Ty(llvmObj->context);
    case Size::I32:
    case Size::U32:
        return llvm::Type::getInt32Ty(llvmObj->context);
    case Size::I64:
    case Size::U64:
        return llvm::Type::getInt64Ty(llvmObj->context);
    }
}


/*
    FloatType
*/
FloatType::FloatType(Size size) : 
    Type(Type::Kind::Float),
    size(size)
{}
vector<unique_ptr<FloatType>> FloatType::objects;
FloatType* FloatType::Create(Size size) {
    objects.emplace_back(make_unique<FloatType>(size));
    return objects.back().get();
}
bool FloatType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const FloatType&>(type);
        return this->size == other.size;
    } else {
        return false;
    }
}
int FloatType::sizeInBytes() {
    switch (size) {
    case Size::F32:
        return 4;
    case Size::F64:
        return 8;
    default: 
        return 0;
    }
}
/*unique_ptr<Type> FloatType::copy() {
    return make_unique<FloatType>(size);
}*/
llvm::Type* FloatType::createLlvm(LlvmObject* llvmObj) {
    switch (size) {
    case Size::F32:
        return llvm::Type::getFloatTy(llvmObj->context);
    case Size::F64:
        return llvm::Type::getDoubleTy(llvmObj->context);
    }
}


/*
    TemplateType
*/
TemplateType::TemplateType(const string& name) : 
    Type(Type::Kind::Template),
    name(name)
{}
vector<unique_ptr<TemplateType>> TemplateType::objects;
TemplateType* TemplateType::Create(const string& name) {
    objects.emplace_back(make_unique<TemplateType>(name));
    return objects.back().get();
}
bool TemplateType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        return true;
    } else {
        return false;
    }
}
/*unique_ptr<Type> TemplateType::copy() {
    return make_unique<TemplateType>(name);
}*/


/*
    TemplateFunctionType
*/
TemplateFunctionType::TemplateFunctionType() {
    kind = Type::Kind::TemplateFunction;
}
vector<unique_ptr<TemplateFunctionType>> TemplateFunctionType::objects;
TemplateFunctionType* TemplateFunctionType::Create() {
    objects.emplace_back(make_unique<TemplateFunctionType>());
    return objects.back().get();
}
bool TemplateFunctionType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const TemplateFunctionType&>(type);
        return this->templateTypes == other.templateTypes
            && FunctionType::operator==(type);
    } else {
        return false;
    }
}
/*unique_ptr<Type> TemplateFunctionType::copy() {
    auto type = make_unique<TemplateFunctionType>();
    for (auto& templateType : templateTypes) {
        auto typeCopy = templateType->copy();
        unique_ptr<TemplateType> templateTypeCopy(static_cast<TemplateType*>(typeCopy.release()));
        type->templateTypes.push_back(move(templateTypeCopy));
    }
    //for (auto& implementation : implementations) {
    //    auto valueCopy = implementation->copy();
    //    unique_ptr<FunctionValue> templateTypeCopy(static_cast<FunctionValue*>(valueCopy.release()));
    //    type->implementations.push_back(move(templateTypeCopy));
    //}
    return type;
}*/