#include "Type.h"
#include "Value.h"

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
/*unique_ptr<Type> Type::copy() {
    return make_unique<Type>(this->kind);
}*/

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
bool OwnerPointerType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const OwnerPointerType&>(type);
        return cmpPtr(this->underlyingType, other.underlyingType);
    } else {
        return false;
    }
}
/*unique_ptr<Type> OwnerPointerType::copy() {
    return make_unique<OwnerPointerType>(this->underlyingType->copy());
}*/


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
bool RawPointerType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const RawPointerType&>(type);
        return cmpPtr(this->underlyingType, other.underlyingType);
    } else {
        return false;
    }
}
/*unique_ptr<Type> RawPointerType::copy() {
    return make_unique<RawPointerType>(this->underlyingType->copy());
}*/


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
bool MaybeErrorType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const MaybeErrorType&>(type);
        return cmpPtr(this->underlyingType, other.underlyingType);
    } else {
        return false;
    }
}
/*unique_ptr<Type> MaybeErrorType::copy() {
    return make_unique<MaybeErrorType>(this->underlyingType->copy());
}*/


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
/*unique_ptr<Type> ReferenceType::copy() {
    return make_unique<ReferenceType>(this->underlyingType->copy());
}*/


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
/*unique_ptr<Type> StaticArrayType::copy() {
    if (this->size) {
        return make_unique<StaticArrayType>(this->elementType->copy(), this->size->copy());
    } else {
        return make_unique<StaticArrayType>(this->elementType->copy(), this->sizeAsInt);
    }
}*/


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
bool DynamicArrayType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const DynamicArrayType&>(type);
        return cmpPtr(this->elementType, other.elementType);
    } else {
        return false;
    }
}
/*unique_ptr<Type> DynamicArrayType::copy() {
    return make_unique<DynamicArrayType>(this->elementType->copy());
}*/


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
bool ArrayViewType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const ArrayViewType&>(type);
        return cmpPtr(this->elementType, other.elementType);
    } else {
        return false;
    }
}
/*unique_ptr<Type> ArrayViewType::copy() {
    return make_unique<ArrayViewType>(this->elementType->copy());
}*/


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
bool ClassType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const ClassType&>(type);
        return this->templateTypes == other.templateTypes;
    } else {
        return false;
    }
}
/*unique_ptr<Type> ClassType::copy() {
    auto type = make_unique<ClassType>(this->name);
    for (auto& templateType : templateTypes) {
        type->templateTypes.push_back(templateType->copy());
    }
    return type;
}*/


/*
    FunctionType
*/
FunctionType::FunctionType() : Type(Type::Kind::Function) {}
vector<unique_ptr<FunctionType>> FunctionType::objects;
FunctionType* FunctionType::Create() {
    objects.emplace_back(make_unique<FunctionType>());
    return objects.back().get();
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
/*unique_ptr<Type> FunctionType::copy() {
    auto type = make_unique<FunctionType>();
    type->returnType = returnType->copy();
    for (auto& argumentType : argumentTypes) {
        type->argumentTypes.push_back(argumentType->copy());
    }
    return type;
}*/


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
/*unique_ptr<Type> FloatType::copy() {
    return make_unique<FloatType>(size);
}*/


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