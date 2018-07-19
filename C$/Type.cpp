#include "Type.h"
#include "Value.h"

using namespace std;


/*
    Type
*/
Type::Type (Kind kind) : kind(kind) {}
bool Type::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const Type& other = static_cast<const Type&>(type);
        return this->kind == other.kind;
    }
    else {
        return false;
    }
}
unique_ptr<Type> Type::copy() {
    return make_unique<Type>(this->kind);
}


/*
    OwnerPointerType
*/
OwnerPointerType::OwnerPointerType(unique_ptr<Type>&& underlyingType) : 
    Type(Type::Kind::OwnerPointer),
    underlyingType(move(underlyingType))
{}
bool OwnerPointerType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const OwnerPointerType&>(type);
        return this->underlyingType == other.underlyingType;
    } else {
        return false;
    }
}
unique_ptr<Type> OwnerPointerType::copy() {
    return make_unique<OwnerPointerType>(this->underlyingType->copy());
}


/*
    RawPointerType
*/
RawPointerType::RawPointerType(unique_ptr<Type>&& underlyingType) : 
    Type(Type::Kind::RawPointer),
    underlyingType(move(underlyingType))
{}
bool RawPointerType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const RawPointerType&>(type);
        return this->underlyingType == other.underlyingType;
    } else {
        return false;
    }
}
unique_ptr<Type> RawPointerType::copy() {
    return make_unique<RawPointerType>(this->underlyingType->copy());
}


/*
    MaybeErrorType
*/
MaybeErrorType::MaybeErrorType(unique_ptr<Type>&& underlyingType) : 
    Type(Type::Kind::MaybeError),
    underlyingType(move(underlyingType))
{}
bool MaybeErrorType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const MaybeErrorType&>(type);
        return this->underlyingType == other.underlyingType;
    } else {
        return false;
    }
}
unique_ptr<Type> MaybeErrorType::copy() {
    return make_unique<MaybeErrorType>(this->underlyingType->copy());
}


/*
    ReferenceType
*/
ReferenceType::ReferenceType(unique_ptr<Type>&& underlyingType) : 
    Type(Type::Kind::Reference),
    underlyingType(move(underlyingType))
{}
bool ReferenceType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const ReferenceType&>(type);
        return this->underlyingType == other.underlyingType;
    } else {
        return false;
    }
}
unique_ptr<Type> ReferenceType::copy() {
    return make_unique<ReferenceType>(this->underlyingType->copy());
}


/*
    StaticArrayType
*/
StaticArrayType::StaticArrayType(unique_ptr<Type>&& elementType, unique_ptr<Value>&& size) : 
    Type(Type::Kind::StaticArray),
    elementType(move(elementType)),
    size(move(size))
{}
bool StaticArrayType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const StaticArrayType&>(type);
        return this->elementType == other.elementType
            && this->size == other.size;
    } else {
        return false;
    }
}
unique_ptr<Type> StaticArrayType::copy() {
    return make_unique<StaticArrayType>(this->elementType->copy(), this->size->copy());
}


/*
    DynamicArrayType
*/
DynamicArrayType::DynamicArrayType(unique_ptr<Type>&& elementType) : 
    Type(Type::Kind::DynamicArray),
    elementType(move(elementType))
{}
bool DynamicArrayType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const DynamicArrayType&>(type);
        return this->elementType == other.elementType;
    } else {
        return false;
    }
}
unique_ptr<Type> DynamicArrayType::copy() {
    return make_unique<DynamicArrayType>(this->elementType->copy());
}


/*
    ArrayViewType
*/
ArrayViewType::ArrayViewType(unique_ptr<Type>&& elementType) : 
    Type(Type::Kind::ArrayView),
    elementType(move(elementType))
{}
bool ArrayViewType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const ArrayViewType&>(type);
        return this->elementType == other.elementType;
    } else {
        return false;
    }
}
unique_ptr<Type> ArrayViewType::copy() {
    return make_unique<ArrayViewType>(this->elementType->copy());
}


/*
    ClassType
*/
ClassType::ClassType(const string& name) : 
    Type(Type::Kind::Class),
    name(name)
{}
bool ClassType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const ClassType&>(type);
        return this->templateTypes == other.templateTypes;
    } else {
        return false;
    }
}
unique_ptr<Type> ClassType::copy() {
    auto type = make_unique<ClassType>(this->name);
    for (auto& templateType : templateTypes) {
        type->templateTypes.push_back(templateType->copy());
    }
    return type;
}


/*
    FunctionType
*/
FunctionType::FunctionType() : Type(Type::Kind::Function) {}
bool FunctionType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const FunctionType&>(type);
        return this->argumentTypes == other.argumentTypes
            && this->returnType == other.returnType;
    } else {
        return false;
    }
}
unique_ptr<Type> FunctionType::copy() {
    auto type = make_unique<FunctionType>();
    type->returnType = returnType->copy();
    for (auto& argumentType : argumentTypes) {
        type->argumentTypes.push_back(argumentType->copy());
    }
    return type;
}


/*
    IntegerType
*/
IntegerType::IntegerType(Size size) : 
    Type(Type::Kind::Integer), 
    size(size) 
{}
bool IntegerType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const IntegerType&>(type);
        return this->size == other.size;
    } else {
        return false;
    }
}
unique_ptr<Type> IntegerType::copy() {
    return make_unique<IntegerType>(size);
}


/*
    FloatType
*/
FloatType::FloatType(Size size) : 
    Type(Type::Kind::Float),
    size(size)
{}
bool FloatType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const FloatType&>(type);
        return this->size == other.size;
    } else {
        return false;
    }
}
unique_ptr<Type> FloatType::copy() {
    return make_unique<FloatType>(size);
}


/*
    TemplateType
*/
TemplateType::TemplateType(const string& name) : 
    Type(Type::Kind::Template),
    name(name)
{}
bool TemplateType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        return true;
    } else {
        return false;
    }
}
unique_ptr<Type> TemplateType::copy() {
    return make_unique<TemplateType>(name);
}


/*
    TemplateFunctionType
*/
TemplateFunctionType::TemplateFunctionType() {
    kind = Type::Kind::TemplateFunction;
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
unique_ptr<Type> TemplateFunctionType::copy() {
    auto type = make_unique<TemplateFunctionType>();
    for (auto& templateType : templateTypes) {
        auto typeCopy = templateType->copy();
        unique_ptr<TemplateType> templateTypeCopy(static_cast<TemplateType*>(typeCopy.release()));
        type->templateTypes.push_back(move(templateTypeCopy));
    }
    /*for (auto& implementation : implementations) {
        auto valueCopy = implementation->copy();
        unique_ptr<FunctionValue> templateTypeCopy(static_cast<FunctionValue*>(valueCopy.release()));
        type->implementations.push_back(move(templateTypeCopy));
    }*/
    return type;
}