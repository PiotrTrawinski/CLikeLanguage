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
bool Type::needsDestruction() {
    return false;
}
optional<InterpretConstructorResult> Type::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*> arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    switch (kind) {
    case Kind::Bool:
        switch (arguments.size()) {
        case 0: 
            if (parentIsAssignment) {
                return InterpretConstructorResult(nullptr, nullptr);
            } else {
                return BoolValue::Create(position, false);
            }
        case 1: 
            if (arguments[0]->isConstexpr) {
                switch (arguments[0]->valueKind) {
                case Value::ValueKind::Char:        return BoolValue::Create(position, ((CharValue*)arguments[0])->value != 0);
                case Value::ValueKind::Integer:     return BoolValue::Create(position, ((IntegerValue*)arguments[0])->value != 0);
                case Value::ValueKind::Float:       return BoolValue::Create(position, ((FloatValue*)arguments[0])->value != 0);
                case Value::ValueKind::String:      return BoolValue::Create(position, ((StringValue*)arguments[0])->value.size() != 0);
                case Value::ValueKind::StaticArray: return BoolValue::Create(position, ((StaticArrayValue*)arguments[0])->values.size() != 0);
                }
            } 
            if (arguments[0]->type->getEffectiveType()->kind != Type::Kind::Class) {
                if (arguments[0]->type->kind == Type::Kind::Integer) {
                    auto op = Operation::Create(position, Operation::Kind::Neq);
                    op->arguments.push_back(arguments[0]);
                    auto integerValue = IntegerValue::Create(position, 0);
                    integerValue->type = IntegerType::Create(((IntegerType*)arguments[0]->type)->size);
                    op->arguments.push_back(integerValue);
                    auto opInterpret = op->interpret(scope);
                    if (!opInterpret) return nullopt;
                    if (opInterpret.value()) return opInterpret.value();
                    else return op;
                } 
                if (arguments[0]->type->kind == Type::Kind::Float) {
                    auto op = Operation::Create(position, Operation::Kind::Neq);
                    op->arguments.push_back(arguments[0]);
                    auto floatValue = FloatValue::Create(position, 0);
                    floatValue->type = FloatType::Create(((FloatType*)arguments[0]->type)->size);
                    op->arguments.push_back(floatValue);
                    auto opInterpret = op->interpret(scope);
                    if (!opInterpret) return nullopt;
                    if (opInterpret.value()) return opInterpret.value();
                    else return op;
                }
            }
            if (!onlyTry) errorMessageOpt("no fitting bool constructor (bad argument type)", position);
            return nullopt;
        default:
            if (!onlyTry) errorMessageOpt("no fitting bool constructor (too many arguments)", position);
            return nullopt;
        }
    case Kind::Void:
        if (!onlyTry) errorMessageOpt("cannot construct void value", position);
        return nullopt;
    default:
        internalError("unknown type construction", position);
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
pair<llvm::Value*, llvm::Value*> Type::createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    return { nullptr, nullptr };
}
llvm::Value* Type::createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    return nullptr;
}
void Type::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {}
bool Type::hasLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (kind) {
    case Type::Kind::Bool:
    case Type::Kind::Void:
        return false;
    default:
        return true;
    }
}
void Type::createLlvmAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    createDestructorLlvm(llvmObj, leftLlvmRef);
    createLlvmConstructor(llvmObj, leftLlvmRef, arguments, classConstructor);
}
void Type::createLlvmCopyConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    new llvm::StoreInst(rightLlvmValue, leftLlvmRef, llvmObj->block);
}
void Type::createLlvmMoveConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    new llvm::StoreInst(rightLlvmValue, leftLlvmRef, llvmObj->block);
}
void Type::createLlvmCopyAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    new llvm::StoreInst(rightLlvmValue, leftLlvmRef, llvmObj->block);
}
void Type::createLlvmMoveAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    createDestructorLlvm(llvmObj, leftLlvmRef);
    createLlvmMoveConstructor(llvmObj, leftLlvmRef, rightLlvmValue);
}
void Type::createDestructorLlvm(LlvmObject* llvmObj, llvm::Value* leftLlvmRef) {}

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
bool OwnerPointerType::needsDestruction() {
    return true;
}
optional<InterpretConstructorResult> OwnerPointerType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*> arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    switch (arguments.size()) {
    case 0:
        return NullValue::Create(position, this);
    case 1:
        if (arguments[0]->valueKind == Value::ValueKind::Null) {
            return NullValue::Create(position, this);
        }
        if (!onlyTry) errorMessageOpt("no fitting owner pointer constructor (bad argument type)", position);
        return nullopt;
    default: 
        if (!onlyTry) errorMessageOpt("no fitting owner pointer constructor (too many arguments)", position);
        return nullopt;
    }
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
void OwnerPointerType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    if (Value::isLvalue(arguments[0])) {
        createLlvmCopyConstructor(llvmObj, leftLlvmRef, arguments[0]->createLlvm(llvmObj));
    } else {
        createLlvmMoveConstructor(llvmObj, leftLlvmRef, arguments[0]->createLlvm(llvmObj));
    }
}
void OwnerPointerType::createLlvmAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    internalError("owner pointer should never createLlvmAssignment");
}
void OwnerPointerType::createLlvmCopyConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    auto rightAsInt = new llvm::PtrToIntInst(rightLlvmValue, llvm::Type::getInt64Ty(llvmObj->context), "", llvmObj->block);
    createLlvmConditional(
        llvmObj,
        new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, rightAsInt, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), ""),
        [&]() {
            new llvm::StoreInst(llvm::ConstantPointerNull::get((llvm::PointerType*)createLlvm(llvmObj)), leftLlvmRef, llvmObj->block);
        },
        [&]() {
            auto llvmTypesize = IntegerValue::Create(CodePosition(nullptr,0,0), underlyingType->sizeInBytes());
            auto newAllocatedValue = new llvm::BitCastInst(
                llvm::CallInst::Create(llvmObj->mallocFunction, llvmTypesize->createLlvm(llvmObj), "", llvmObj->block), 
                this->createLlvm(llvmObj), 
                "", 
                llvmObj->block
            );
            if (underlyingType->kind == Type::Kind::Class) {
                underlyingType->createLlvmCopyConstructor(llvmObj, newAllocatedValue, rightLlvmValue);
            } else {
                underlyingType->createLlvmCopyConstructor(llvmObj, newAllocatedValue, new llvm::LoadInst(rightLlvmValue, "", llvmObj->block));
            }
            new llvm::StoreInst(newAllocatedValue, leftLlvmRef, llvmObj->block);
        }
    );
}
void OwnerPointerType::createLlvmCopyAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    auto rightAsInt = new llvm::PtrToIntInst(rightLlvmValue, llvm::Type::getInt64Ty(llvmObj->context), "", llvmObj->block);
    createLlvmConditional(
        llvmObj,
        new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, rightAsInt, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), ""),
        [&]() {
            createDestructorLlvm(llvmObj, leftLlvmRef);
            new llvm::StoreInst(llvm::ConstantPointerNull::get((llvm::PointerType*)createLlvm(llvmObj)), leftLlvmRef, llvmObj->block);
        },
        [&]() {
            auto leftLlvmValue = new llvm::LoadInst(leftLlvmRef, "", llvmObj->block);
            auto leftAsInt = new llvm::PtrToIntInst(leftLlvmValue, llvm::Type::getInt64Ty(llvmObj->context), "", llvmObj->block);
            createLlvmConditional(
                llvmObj,
                new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, leftAsInt, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), ""),
                [&]() {
                    auto llvmTypesize = IntegerValue::Create(CodePosition(nullptr,0,0), underlyingType->sizeInBytes());
                    auto newAllocatedValue = new llvm::BitCastInst(
                        llvm::CallInst::Create(llvmObj->mallocFunction, llvmTypesize->createLlvm(llvmObj), "", llvmObj->block), 
                        this->createLlvm(llvmObj), 
                        "", 
                        llvmObj->block
                    );
                    if (underlyingType->kind == Type::Kind::Class) {
                        underlyingType->createLlvmCopyConstructor(llvmObj, newAllocatedValue, rightLlvmValue);
                    } else {
                        underlyingType->createLlvmCopyConstructor(llvmObj, newAllocatedValue, new llvm::LoadInst(rightLlvmValue, "", llvmObj->block));
                    }
                    new llvm::StoreInst(newAllocatedValue, leftLlvmRef, llvmObj->block);
                },
                [&]() {
                    if (underlyingType->kind == Type::Kind::Class) {
                        underlyingType->createLlvmCopyConstructor(llvmObj, leftLlvmValue, rightLlvmValue);
                    } else {
                        underlyingType->createLlvmCopyConstructor(llvmObj, leftLlvmValue, new llvm::LoadInst(rightLlvmValue, "", llvmObj->block));
                    }
                }
            );
        }
    );
}
void OwnerPointerType::createDestructorLlvm(LlvmObject* llvmObj, llvm::Value* leftLlvmRef) {
    auto leftLlvmValue = new llvm::LoadInst(leftLlvmRef, "", llvmObj->block);
    auto leftAsInt = new llvm::PtrToIntInst(leftLlvmValue, llvm::Type::getInt64Ty(llvmObj->context), "", llvmObj->block);
    createLlvmConditional(
        llvmObj,
        new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_NE, leftAsInt, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), ""),
        [&]() {
            underlyingType->createDestructorLlvm(llvmObj, leftLlvmValue);
            llvm::CallInst::Create(
                llvmObj->freeFunction, 
                new llvm::BitCastInst(leftLlvmValue, llvm::Type::getInt8PtrTy(llvmObj->context), "", llvmObj->block), 
                "", 
                llvmObj->block
            );
        }
    );
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
optional<InterpretConstructorResult> RawPointerType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*> arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    switch (arguments.size()) {
    case 0: 
        if (parentIsAssignment) {
            return InterpretConstructorResult(nullptr, nullptr);
        } else {
            return NullValue::Create(position, this);
        }
    case 1:
        if (arguments[0]->valueKind == Value::ValueKind::Null) {
            return NullValue::Create(position, this);
        }
        if (!onlyTry) errorMessageOpt("no fitting raw pointer constructor (bad argument type)", position);
        return nullopt;
    default: 
        if (!onlyTry) errorMessageOpt("no fitting raw pointer constructor (too many arguments)", position);
        return nullopt;
    }
}
void RawPointerType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 0: 
        break;
    case 1: 
        createLlvmCopyConstructor(llvmObj, leftLlvmRef, arguments[0]->createLlvm(llvmObj));
        break;
    default: 
        internalError("incorrect raw pointer constructor during llvm creating (> 1 argument)");
    }
}
bool RawPointerType::hasLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    return arguments.size() == 1;
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
int MaybeErrorType::sizeInBytes() {
    return underlyingType->sizeInBytes() + 8;
}
bool MaybeErrorType::needsDestruction() {
    return underlyingType->needsDestruction();
}
optional<InterpretConstructorResult> MaybeErrorType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*> arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    return nullopt;
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
optional<InterpretConstructorResult> ReferenceType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*> arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    switch (arguments.size()) {
    case 0: 
        if (!onlyTry) errorMessageOpt("reference constructor requires a value", position);
        return nullopt;
    case 1:
        if (!Value::isLvalue(arguments[0])){
            if (!onlyTry) errorMessageBool("cannot create reference to non-lvalue", position);
            return nullopt;
        }
        if (cmpPtr(arguments[0]->type->getEffectiveType(), this->getEffectiveType())) {
            return arguments[0];
        }
        if (!onlyTry) errorMessageOpt("no fitting reference constructor (bad argument type)", position);
        return nullopt;
    default: 
        if (!onlyTry) errorMessageOpt("no fitting reference constructor (too many arguments)", position);
        return nullopt;
    }
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
        } else {
            return errorMessageBool("static array must have constexpr integer size");
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
int StaticArrayType::sizeInBytes() {
    return sizeAsInt * elementType->sizeInBytes();
}
bool StaticArrayType::needsDestruction() {
    return elementType->needsDestruction();
}
optional<InterpretConstructorResult> StaticArrayType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*> arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    auto result = elementType->interpretConstructor(position, scope, arguments, onlyTry, parentIsAssignment, isExplicit);
    if (result && !isExplicit) {
        if (!onlyTry) errorMessageOpt("cannot implicitly create static array with those arguments", position);
        return nullopt;
    } else {
        return result;
    }
}
pair<llvm::Value*, llvm::Value*> StaticArrayType::createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    auto llvmRef = createLlvmReference(llvmObj, arguments, classConstructor);
    return {llvmRef, new llvm::LoadInst(llvmRef, "", llvmObj->block)};
}
llvm::Value* StaticArrayType::createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    auto llvmRef = allocaLlvm(llvmObj);
    createLlvmConstructor(llvmObj, llvmRef, arguments, classConstructor);
    return llvmRef;
}
void StaticArrayType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    if (elementType->hasLlvmConstructor(llvmObj, leftLlvmRef, arguments, classConstructor)) {
        auto sizeValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), sizeAsInt);
        createLlvmForEachLoop(llvmObj, sizeValue, [&](llvm::Value* index) {
            vector<llvm::Value*> indexList;
            indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
            indexList.push_back(new llvm::LoadInst(index, "", llvmObj->block));
            auto leftGepRef = llvm::GetElementPtrInst::Create(
                ((llvm::PointerType*)leftLlvmRef->getType())->getElementType(), leftLlvmRef, indexList, "", llvmObj->block
            );
            elementType->createLlvmConstructor(llvmObj, leftGepRef, arguments, classConstructor);
        });
    }
}
bool StaticArrayType::hasLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    return elementType->hasLlvmConstructor(llvmObj, leftLlvmRef, arguments, classConstructor);
}
/*unique_ptr<Type> StaticArrayType::copy() {
    if (this->size) {
        return make_unique<StaticArrayType>(this->elementType->copy(), this->size->copy());
    } else {
        return make_unique<StaticArrayType>(this->elementType->copy(), this->sizeAsInt);
    }
}*/
llvm::Type* StaticArrayType::createLlvm(LlvmObject* llvmObj) {
    return llvm::ArrayType::get(elementType->createLlvm(llvmObj), sizeAsInt);
}
llvm::AllocaInst* StaticArrayType::allocaLlvm(LlvmObject* llvmObj, const string& name) {
    return new llvm::AllocaInst(createLlvm(llvmObj), 0, name, llvmObj->block);
}
void StaticArrayType::createLlvmCopyConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    auto sizeValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), sizeAsInt);
    createLlvmForEachLoop(llvmObj, sizeValue, [&](llvm::Value* index) {
        vector<llvm::Value*> indexList;
        indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
        indexList.push_back(new llvm::LoadInst(index, "", llvmObj->block));
        auto leftGepRef = llvm::GetElementPtrInst::Create(
            ((llvm::PointerType*)leftLlvmRef->getType())->getElementType(), leftLlvmRef, indexList, "", llvmObj->block
        );
        auto rightGepRef = llvm::GetElementPtrInst::Create(
            ((llvm::PointerType*)rightLlvmValue->getType())->getElementType(), rightLlvmValue, indexList, "", llvmObj->block
        );
        if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray) {
            elementType->createLlvmCopyConstructor(llvmObj, leftGepRef, rightGepRef);
        } else {
            elementType->createLlvmCopyConstructor(llvmObj, leftGepRef, new llvm::LoadInst(rightGepRef, "", llvmObj->block));
        }  
    });
}
void StaticArrayType::createLlvmCopyAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    auto sizeValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), sizeAsInt);
    createLlvmForEachLoop(llvmObj, sizeValue, [&](llvm::Value* index) {
        vector<llvm::Value*> indexList;
        indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
        indexList.push_back(new llvm::LoadInst(index, "", llvmObj->block));
        auto leftGepRef = llvm::GetElementPtrInst::Create(
            ((llvm::PointerType*)leftLlvmRef->getType())->getElementType(), leftLlvmRef, indexList, "", llvmObj->block
        );
        auto rightGepRef = llvm::GetElementPtrInst::Create(
            ((llvm::PointerType*)rightLlvmValue->getType())->getElementType(), rightLlvmValue, indexList, "", llvmObj->block
        );
        if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray) {
            elementType->createLlvmCopyAssignment(llvmObj, leftGepRef, rightGepRef);
        } else {
            elementType->createLlvmCopyAssignment(llvmObj, leftGepRef, new llvm::LoadInst(rightGepRef, "", llvmObj->block));
        }  
    });
}
void StaticArrayType::createDestructorLlvm(LlvmObject* llvmObj, llvm::Value* leftLlvmRef) {
    if (elementType->needsDestruction()) {
        auto sizeValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), sizeAsInt);
        createLlvmForEachLoop(llvmObj, sizeValue, [&](llvm::Value* index) {
            vector<llvm::Value*> indexList;
            indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
            indexList.push_back(new llvm::LoadInst(index, "", llvmObj->block));
            auto leftGepRef = llvm::GetElementPtrInst::Create(
                ((llvm::PointerType*)leftLlvmRef->getType())->getElementType(), leftLlvmRef, indexList, "", llvmObj->block
            );
            elementType->createDestructorLlvm(llvmObj, leftGepRef);
        });
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
bool DynamicArrayType::needsDestruction() {
    return true;
}
optional<InterpretConstructorResult> DynamicArrayType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*> arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    return nullopt;
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
optional<InterpretConstructorResult> ArrayViewType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*> arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    return nullopt;
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
bool ClassType::needsDestruction() {
    for (auto memberDeclaration : declaration->body->declarations) {
        if (memberDeclaration->variable->type->needsDestruction()) {
            return true;
        }
    }
    return false;
}
optional<InterpretConstructorResult> ClassType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*> arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    if (declaration->body->constructors.empty()) {
        if (arguments.size() == 0) {
            return InterpretConstructorResult(nullptr, declaration->body->inlineConstructors);
        } 
        /*else if (arguments.size() == 1 && cmpPtr(arguments[0]->type->getEffectiveType(), (Type*)classType)) {
            return {classDeclaration->body->copyConstructor, classDeclaration};
        }*/
        else {
            return errorMessageOpt("only default (0 argument) constructor exists, got "
                + to_string(arguments.size()) + " arguments", position
            );
        }
    } else {
        vector<FunctionValue*> viableDeclarations;
        vector<FunctionValue*> perfectMatches;
        for (const auto function : declaration->body->constructors) {
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
            return InterpretConstructorResult(nullptr, perfectMatches.back());
        } else if (perfectMatches.size() > 1) {
            string message = "ambogous constructor call. ";
            message += "Possible constructors at lines: ";
            for (int i = 0; i < perfectMatches.size(); ++i) {
                message += to_string(perfectMatches[i]->position.lineNumber);
                if (i != perfectMatches.size() - 1) {
                    message += ", ";
                }
            }
            return errorMessageOpt(message, position);
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
                if (arguments.size() == 1 && cmpPtr(arguments[0]->type->getEffectiveType(), (Type*)this)) {
                    return InterpretConstructorResult(nullptr, declaration->body->copyConstructor);
                } else {
                    return errorMessageOpt("no fitting constructor to call", position);
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
                return errorMessageOpt(message, position);
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
            return InterpretConstructorResult(nullptr, viableDeclarations[matchId]);
        }
    }
    /*switch (arguments.size()) {
    case 0:
        return NullValue::Create(position, this);
    case 1:
        if (arguments[0]->valueKind == Value::ValueKind::Null) {
            return NullValue::Create(position, this);
        }
        if (!onlyTry) errorMessageOpt("no fitting owner pointer constructor (bad argument type)", position);
        return nullopt;
    default: 
        if (!onlyTry) errorMessageOpt("no fitting owner pointer constructor (too many arguments)", position);
        return nullopt;
    }*/
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
pair<llvm::Value*, llvm::Value*> ClassType::createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    auto llvmRef = createLlvmReference(llvmObj, arguments, classConstructor);
    return {llvmRef, new llvm::LoadInst(llvmRef, "", llvmObj->block)};
}
llvm::Value* ClassType::createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    auto llvmRef = allocaLlvm(llvmObj);
    createLlvmConstructor(llvmObj, llvmRef, arguments, classConstructor);
    return llvmRef;
}
void ClassType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    vector<llvm::Value*> args;
    auto functionType = (FunctionType*)classConstructor->type;
    for (int i = 0; i < arguments.size(); ++i) {
        if (functionType->argumentTypes[i]->kind == Type::Kind::Reference) {
            args.push_back(arguments[i]->getReferenceLlvm(llvmObj));
        } else {
            args.push_back(arguments[i]->createLlvm(llvmObj));
        }
    }
    args.push_back(leftLlvmRef);
    llvm::CallInst::Create(classConstructor->createLlvm(llvmObj), args, "", llvmObj->block);
}
void ClassType::createLlvmCopyConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    llvm::CallInst::Create(declaration->body->copyConstructor->createLlvm(llvmObj), {rightLlvmValue, leftLlvmRef}, "", llvmObj->block);
}
void ClassType::createLlvmMoveConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    new llvm::StoreInst(new llvm::LoadInst(rightLlvmValue, "", llvmObj->block), leftLlvmRef, llvmObj->block);
}
void ClassType::createLlvmCopyAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    llvm::CallInst::Create(declaration->body->operatorEq->createLlvm(llvmObj), {rightLlvmValue, leftLlvmRef}, "", llvmObj->block);
}
void ClassType::createDestructorLlvm(LlvmObject* llvmObj, llvm::Value* leftLlvmRef) {
    if (declaration->body->destructor) {
        llvm::CallInst::Create(declaration->body->destructor->createLlvm(llvmObj), leftLlvmRef, "", llvmObj->block);
    } else if (declaration->body->inlineDestructors) {
        llvm::CallInst::Create(declaration->body->inlineDestructors->createLlvm(llvmObj), leftLlvmRef, "", llvmObj->block);
    }
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
optional<InterpretConstructorResult> FunctionType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*> arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    return nullopt;
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
optional<InterpretConstructorResult> IntegerType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*> arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    switch (arguments.size()) {
    case 0: 
        if (parentIsAssignment) {
            return InterpretConstructorResult(nullptr, nullptr);
        } else {
            auto intValue = IntegerValue::Create(position, 0);
            intValue->type = this;
            return intValue;
        }
    case 1:
        switch (arguments[0]->type->getEffectiveType()->kind) {
        case Type::Kind::Integer:
            if (arguments[0]->isConstexpr) {
                if (arguments[0]->valueKind == Value::ValueKind::Char) {
                    auto intValue = IntegerValue::Create(position, ((CharValue*)arguments[0])->value);
                    intValue->type = this;
                    return intValue;
                }
                arguments[0]->type = this;
                return arguments[0];
            }
            return InterpretConstructorResult(nullptr, nullptr);
        case Type::Kind::Float:
            if (arguments[0]->isConstexpr) {
                auto intValue = IntegerValue::Create(position, ((FloatValue*)arguments[0])->value);
                intValue->type = this;
                return intValue;
            }
            return InterpretConstructorResult(nullptr, nullptr);
        case Type::Kind::Bool:
            if (arguments[0]->isConstexpr) {
                int value = ((BoolValue*)arguments[0])->value ? 1 : 0;
                auto intValue = IntegerValue::Create(position, value);
                intValue->type = this;
                return intValue;
            }
            return InterpretConstructorResult(nullptr, nullptr);
        }
        if (!onlyTry) errorMessageOpt("no fitting integer constructor (bad argument type)", position);
        return nullopt;
    default: 
        if (!onlyTry) errorMessageOpt("no fitting integer constructor (too many arguments)", position);
        return nullopt;
    }
}
pair<llvm::Value*, llvm::Value*> IntegerType::createLlvmValue(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 0:
        internalError("incorrect integer constructor value creating during llvm creating (no arguments)");
    case 1: {
        auto arg0Type = arguments[0]->type->getEffectiveType();
        switch (arg0Type->kind) {
        case Type::Kind::Integer: {
            auto sizeCast = sizeInBytes();
            auto sizeArg = ((IntegerType*)arg0Type)->sizeInBytes();
            if (sizeCast == sizeArg) {
                return { nullptr, arguments[0]->createLlvm(llvmObj) };
            } else if (sizeCast > sizeArg) {
                return { nullptr, new llvm::SExtInst(arguments[0]->createLlvm(llvmObj), createLlvm(llvmObj), "", llvmObj->block) };
            } else {
                return { nullptr, new llvm::TruncInst(arguments[0]->createLlvm(llvmObj), createLlvm(llvmObj), "", llvmObj->block) };
            }
        }
        case Type::Kind::Float:
            if (isSigned()) {
                return { nullptr, new llvm::FPToSIInst(arguments[0]->createLlvm(llvmObj), createLlvm(llvmObj), "", llvmObj->block) };
            } else {
                return { nullptr, new llvm::FPToUIInst(arguments[0]->createLlvm(llvmObj), createLlvm(llvmObj), "", llvmObj->block) };
            }
        default:
            internalError("integer can only be constructed from integer and float values in llvm stage");
        }
    }
    default: 
        internalError("incorrect integer constructor value creating during llvm creating (> 1 argument)");
    }
}
void IntegerType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 0: 
        break;
    case 1: 
        createLlvmCopyConstructor(llvmObj, leftLlvmRef, createLlvmValue(llvmObj, arguments, classConstructor).second);
        break;
    default: 
        internalError("incorrect integer constructor during llvm creating (> 1 argument)");
    }
}
bool IntegerType::hasLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    return arguments.size() == 1;
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
optional<InterpretConstructorResult> FloatType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*> arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    switch (arguments.size()) {
    case 0: 
        if (parentIsAssignment) {
            return InterpretConstructorResult(nullptr, nullptr);
        } else {
            auto floatValue = FloatValue::Create(position, 0);
            floatValue->type = this;
            return floatValue;
        }
    case 1:
        switch (arguments[0]->type->getEffectiveType()->kind) {
        case Type::Kind::Integer:
            if (arguments[0]->isConstexpr) {
                Value* floatValue;
                if (arguments[0]->valueKind == Value::ValueKind::Integer) {
                    floatValue = FloatValue::Create(position, ((IntegerValue*)arguments[0])->value);
                } else {
                    floatValue = FloatValue::Create(position, ((CharValue*)arguments[0])->value);
                }
                floatValue->type = this;
                return floatValue;
            }
            return InterpretConstructorResult(nullptr, nullptr);
        case Type::Kind::Float:
            if (arguments[0]->isConstexpr) {
                arguments[0]->type = this;
                return arguments[0];
            }
            return InterpretConstructorResult(nullptr, nullptr);
        case Type::Kind::Bool:
            if (arguments[0]->isConstexpr) {
                double value = ((BoolValue*)arguments[0])->value ? 1 : 0;
                auto floatValue = FloatValue::Create(position, value);
                floatValue->type = this;
                return floatValue;
            }
            return InterpretConstructorResult(nullptr, nullptr);
        }
        if (!onlyTry) errorMessageOpt("no fitting float constructor (bad argument type)", position);
        return nullopt;
    default: 
        if (!onlyTry) errorMessageOpt("no fitting float constructor (too many arguments)", position);
        return nullopt;
    }
}
pair<llvm::Value*, llvm::Value*> FloatType::createLlvmValue(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 0:
        internalError("incorrect float constructor value creating during llvm creating (no arguments)");
    case 1: {
        auto arg0Type = arguments[0]->type->getEffectiveType();
        switch (arg0Type->kind) {
        case Type::Kind::Integer:
            if (((IntegerType*)arg0Type)->isSigned()) {
                return { nullptr, new llvm::SIToFPInst(arguments[0]->createLlvm(llvmObj), createLlvm(llvmObj), "", llvmObj->block) };
            } else {
                return { nullptr, new llvm::UIToFPInst(arguments[0]->createLlvm(llvmObj), createLlvm(llvmObj), "", llvmObj->block) };
            }
        case Type::Kind::Float:
            if (size == FloatType::Size::F32) {
                return { nullptr, new llvm::FPTruncInst(arguments[0]->createLlvm(llvmObj), createLlvm(llvmObj), "", llvmObj->block) };
            } else {
                return { nullptr, new llvm::FPExtInst(arguments[0]->createLlvm(llvmObj), createLlvm(llvmObj), "", llvmObj->block) };
            }
        default:
            internalError("float can only be constructed from integer and float values in llvm stage");
        }
    }
    default: 
        internalError("incorrect float constructor value creating during llvm creating (> 1 argument)");
    }
}
void FloatType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 0: 
        break;
    case 1: 
        createLlvmCopyConstructor(llvmObj, leftLlvmRef, createLlvmValue(llvmObj, arguments, classConstructor).second);
        break;
    default: 
        internalError("incorrect float constructor during llvm creating (> 1 argument)");
    }
}
bool FloatType::hasLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    return arguments.size() == 1;
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