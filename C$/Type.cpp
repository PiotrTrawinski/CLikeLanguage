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
Type* Type::changeClassToTemplate(const vector<TemplateType*> templateTypes) {
    return this;
}
bool Type::interpret(Scope* scope) {
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
bool Type::isTemplate() {
    return false;
}
MatchTemplateResult Type::matchTemplate(TemplateFunctionType* templateFunctionType, Type* type) {
    if (kind == type->kind) return MatchTemplateResult::Perfect;
    else return MatchTemplateResult::Viable;
}
Type* Type::substituteTemplate(TemplateFunctionType* templateFunctionType) {
    return this;
}
int Type::compareTemplateDepth(Type* type) {
    return 0;
}
Type* Type::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    return Create(kind);
}
Type* Type::getEffectiveType() {
    return this;
}
Value* Type::typesize(Scope* scope) {
    if (kind == Kind::Void) {
        return IntegerValue::Create(CodePosition(nullptr, 0, 0), 0);
    }
    auto sizeOfOperation = SizeofOperation::Create(CodePosition(nullptr, 0, 0));
    sizeOfOperation->argType = this;
    if (!sizeOfOperation->interpret(scope)) return nullptr;
    return sizeOfOperation;
}
llvm::Value* Type::llvmTypesize(LlvmObject* llvmObj) {
    auto gep = llvm::GetElementPtrInst::Create(
        createLlvm(llvmObj),
        llvm::ConstantPointerNull::get((llvm::PointerType*)RawPointerType::Create(this)->createLlvm(llvmObj)),
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 1),
        "", llvmObj->block
    );
    return new llvm::PtrToIntInst(gep, llvm::Type::getInt64Ty(llvmObj->context), "", llvmObj->block);
}
bool Type::needsDestruction() {
    return false;
}
bool Type::needsReference() {
    return false;
}
optional<pair<Type*,FunctionValue*>> Type::interpretFunction(const CodePosition& position, Scope* scope, const string functionName, vector<Value*> arguments) {
    return errorMessageOpt(DeclarationMap::toString(this) + " type has no member function named " + functionName, position);
}
llvm::Value* Type::createFunctionLlvmReference(const string functionName, LlvmObject* llvmObj, llvm::Value* llvmRef, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    internalError("cannot createFunctionLlvmReference for this type during llvm creating");
    return nullptr;
}
pair<llvm::Value*, llvm::Value*> Type::createFunctionLlvmValue(const string functionName, LlvmObject* llvmObj, llvm::Value* llvmRef, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    internalError("cannot createFunctionLlvmReference for this type during llvm creating");
    return { nullptr, nullptr };
}
optional<InterpretConstructorResult> Type::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    switch (kind) {
    case Kind::Bool:
        switch (arguments.size()) {
        case 0: 
            if (parentIsAssignment) {
                return InterpretConstructorResult(nullptr, nullptr);
            } else {
                return BoolValue::Create(position, false);
            }
        case 1:  {
            if (arguments[0]->isConstexpr) {
                switch (arguments[0]->valueKind) {
                case Value::ValueKind::Char:        return BoolValue::Create(position, ((CharValue*)arguments[0])->value != 0);
                case Value::ValueKind::Integer:     return BoolValue::Create(position, ((IntegerValue*)arguments[0])->value != 0);
                case Value::ValueKind::Float:       return BoolValue::Create(position, ((FloatValue*)arguments[0])->value != 0);
                case Value::ValueKind::String:      return BoolValue::Create(position, ((StringValue*)arguments[0])->value.size() != 0);
                }
            } 
            auto effType = arguments[0]->type->getEffectiveType();
            if (effType->kind != Type::Kind::Class) {
                if (effType->kind == Type::Kind::Integer) {
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
                if (effType->kind == Type::Kind::Float) {
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
                if (effType->kind == Type::Kind::MaybeError) {
                    auto toVoidError = ConstructorOperation::Create(position, MaybeErrorType::Create(Type::Create(Type::Kind::Void)), {arguments[0]});
                    auto toInt = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {toVoidError});
                    auto op = Operation::Create(position, Operation::Kind::Eq);
                    op->arguments.push_back(toInt);
                    auto integerValue = IntegerValue::Create(position, 0);
                    integerValue->type = IntegerType::Create(IntegerType::Size::I64);
                    op->arguments.push_back(integerValue);
                    auto opInterpret = op->interpret(scope);
                    if (!opInterpret) return nullopt;
                    if (opInterpret.value()) return opInterpret.value();
                    else return op;
                }
                if (effType->kind == Type::Kind::RawPointer || effType->kind == Type::Kind::OwnerPointer) {
                    auto toInt = CastOperation::Create(position, IntegerType::Create(IntegerType::Size::I64));
                    toInt->arguments.push_back(arguments[0]);
                    auto op = Operation::Create(position, Operation::Kind::Neq);
                    op->arguments.push_back(toInt);
                    auto integerValue = IntegerValue::Create(position, 0);
                    integerValue->type = IntegerType::Create(IntegerType::Size::I64);
                    op->arguments.push_back(integerValue);
                    auto opInterpret = op->interpret(scope);
                    if (!opInterpret) return nullopt;
                    if (opInterpret.value()) return opInterpret.value();
                    else return op;
                }
            }
            if (!onlyTry) errorMessageOpt("no fitting bool constructor (bad argument type)", position);
            return nullopt;
        }
        default:
            if (!onlyTry) errorMessageOpt("no fitting bool constructor (too many arguments)", position);
            return nullopt;
        }
    case Kind::Void:
        if (!onlyTry) errorMessageOpt("cannot construct void value", position);
        return nullopt;
    default:
        internalError("unknown type construction", position);
        return nullopt;
    }
}
/*unique_ptr<Type> Type::copy() {
    return make_unique<Type>(this->kind);
}*/
llvm::Type* Type::createLlvm(LlvmObject* llvmObj) {
    switch (kind) {
    case Kind::Bool:
        return llvm::Type::getInt8Ty(llvmObj->context);
    case Kind::Void:
        return llvm::Type::getVoidTy(llvmObj->context);
    }
    return nullptr;
}
llvm::AllocaInst* Type::allocaLlvm(LlvmObject* llvmObj, const string& name) {
    return new llvm::AllocaInst(createLlvm(llvmObj), 0, name, llvmObj->block);
}
llvm::Value* Type::createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    return nullptr;
}
pair<llvm::Value*, llvm::Value*> Type::createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref) {
    return { nullptr, nullptr };
}
llvm::Value* Type::createRefForLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    return nullptr;
}
llvm::Value* Type::createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref) {
    return nullptr;
}
llvm::Value* Type::createRefForLlvmCopy(LlvmObject* llvmObj, Value* lValue) {
    lValue->createAllocaLlvmIfNeededForValue(llvmObj);
    return nullptr;
}
llvm::Value* Type::createLlvmCopy(LlvmObject* llvmObj, Value* lValue, llvm::Value* ref) {
    return lValue->createLlvm(llvmObj);
}
void Type::createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor) {}
void Type::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {}
bool Type::hasLlvmConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (kind) {
    case Type::Kind::Bool:
    case Type::Kind::Void:
        return false;
    default:
        return true;
    }
}
void Type::createAllocaIfNeededForAssignment(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    createAllocaIfNeededForConstructor(llvmObj, arguments, classConstructor);
}
void Type::createLlvmAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    createLlvmDestructorRef(llvmObj, leftLlvmRef);
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
    createLlvmDestructorRef(llvmObj, leftLlvmRef);
    createLlvmMoveConstructor(llvmObj, leftLlvmRef, rightLlvmValue);
}
void Type::createLlvmDestructorValue(LlvmObject* llvmObj, llvm::Value* llvmValue) {}
void Type::createLlvmDestructorRef(LlvmObject* llvmObj, llvm::Value* llvmRef) {}

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

llvm::Constant* llvmInt(LlvmObject* llvmObj, int value) {
    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), value);
}
llvm::Value* llvmLoad(LlvmObject* llvmObj, llvm::Value* ptrToLoad) {
    return new llvm::LoadInst(ptrToLoad, "", llvmObj->block);
}
void llvmStore(LlvmObject* llvmObj, llvm::Value* value, llvm::Value* ptr) {
    new llvm::StoreInst(value, ptr, llvmObj->block);
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
Type* OwnerPointerType::changeClassToTemplate(const vector<TemplateType*> templateTypes) {
    underlyingType = underlyingType->changeClassToTemplate(templateTypes);
    return this;
}
Type* OwnerPointerType::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    return Create(underlyingType->templateCopy(parentScope, templateToType));
}
bool OwnerPointerType::interpret(Scope* scope) {
    return underlyingType->interpret(scope);
}
bool OwnerPointerType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const OwnerPointerType&>(type);
        return cmpPtr(this->underlyingType, other.underlyingType);
    } else {
        return false;
    }
}
bool OwnerPointerType::isTemplate() {
    return underlyingType->isTemplate();
}
MatchTemplateResult OwnerPointerType::matchTemplate(TemplateFunctionType* templateFunctionType, Type* type) {
    if (kind == type->kind) return underlyingType->matchTemplate(templateFunctionType, ((OwnerPointerType*)type)->underlyingType);
    else return MatchTemplateResult::Fail;
}
Type* OwnerPointerType::substituteTemplate(TemplateFunctionType* templateFunctionType) {
    return Create(underlyingType->substituteTemplate(templateFunctionType));
}
int OwnerPointerType::compareTemplateDepth(Type* type) {
    if (type->kind != Type::Kind::OwnerPointer) return -1;
    return underlyingType->compareTemplateDepth(((OwnerPointerType*)type)->underlyingType);
}
bool OwnerPointerType::needsDestruction() {
    return true;
}
optional<InterpretConstructorResult> OwnerPointerType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    switch (arguments.size()) {
    case 0:
        return NullValue::Create(position, this);
    case 1: {
        if (arguments[0]->valueKind == Value::ValueKind::Null) {
            return NullValue::Create(position, this);
        }
        auto effType = arguments[0]->type->getEffectiveType();
        if (effType->kind == Type::Kind::RawPointer && cmpPtr(((RawPointerType*)effType)->underlyingType, underlyingType)) {
            if (isExplicit) {
                return InterpretConstructorResult(nullptr, nullptr);
            } else {
                if (!onlyTry) errorMessageOpt("cannot implicitly create owner pointer from raw pointer", position);
                return nullopt;
            }
        }
        if (!onlyTry) errorMessageOpt("no fitting owner pointer constructor (bad argument type)", position);
        return nullopt;
    }
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
llvm::Value* OwnerPointerType::createRefForLlvmCopy(LlvmObject* llvmObj, Value* lValue) {
    lValue->createAllocaLlvmIfNeededForValue(llvmObj);
    return allocaLlvm(llvmObj);
}
llvm::Value* OwnerPointerType::createLlvmCopy(LlvmObject* llvmObj, Value* lValue, llvm::Value* ref) {
    createLlvmCopyConstructor(llvmObj, ref, lValue->createLlvm(llvmObj));
    return new llvm::LoadInst(ref, "", llvmObj->block);
}
void OwnerPointerType::createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
}
void OwnerPointerType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    if (arguments[0]->type->getEffectiveType()->kind == Type::Kind::RawPointer) {
        llvmStore(llvmObj, arguments[0]->createLlvm(llvmObj), leftLlvmRef);
    } else {
        if (Value::isLvalue(arguments[0])) {
            createLlvmCopyConstructor(llvmObj, leftLlvmRef, arguments[0]->createLlvm(llvmObj));
        } else {
            createLlvmMoveConstructor(llvmObj, leftLlvmRef, arguments[0]->createLlvm(llvmObj));
        }
    }
}
void OwnerPointerType::createAllocaIfNeededForAssignment(LlvmObject* llvmObj, const vector<Value*>& arguments) {
    arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
}
void OwnerPointerType::createLlvmAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    if (arguments[0]->type->getEffectiveType()->kind == Type::Kind::RawPointer) {
        createLlvmDestructorRef(llvmObj, leftLlvmRef);
        llvmStore(llvmObj, arguments[0]->createLlvm(llvmObj), leftLlvmRef);
    } else {
        internalError("owner pointer can only createLlvmAssignment with raw pointer");
    }
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
            auto newAllocatedValue = new llvm::BitCastInst(
                llvm::CallInst::Create(llvmObj->mallocFunction, underlyingType->llvmTypesize(llvmObj), "", llvmObj->block), 
                this->createLlvm(llvmObj), 
                "", 
                llvmObj->block
            );
            if (underlyingType->kind == Type::Kind::Class || underlyingType->kind == Type::Kind::StaticArray || underlyingType->kind == Type::Kind::DynamicArray || underlyingType->kind == Type::Kind::MaybeError) {
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
            createLlvmDestructorRef(llvmObj, leftLlvmRef);
            new llvm::StoreInst(llvm::ConstantPointerNull::get((llvm::PointerType*)createLlvm(llvmObj)), leftLlvmRef, llvmObj->block);
        },
        [&]() {
            auto leftLlvmValue = new llvm::LoadInst(leftLlvmRef, "", llvmObj->block);
            auto leftAsInt = new llvm::PtrToIntInst(leftLlvmValue, llvm::Type::getInt64Ty(llvmObj->context), "", llvmObj->block);
            createLlvmConditional(
                llvmObj,
                new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, leftAsInt, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), ""),
                [&]() {
                    auto newAllocatedValue = new llvm::BitCastInst(
                        llvm::CallInst::Create(llvmObj->mallocFunction, underlyingType->llvmTypesize(llvmObj), "", llvmObj->block), 
                        this->createLlvm(llvmObj), 
                        "", 
                        llvmObj->block
                    );
                    if (underlyingType->kind == Type::Kind::Class || underlyingType->kind == Type::Kind::StaticArray || underlyingType->kind == Type::Kind::DynamicArray || underlyingType->kind == Type::Kind::MaybeError) {
                        underlyingType->createLlvmCopyConstructor(llvmObj, newAllocatedValue, rightLlvmValue);
                    } else {
                        underlyingType->createLlvmCopyConstructor(llvmObj, newAllocatedValue, new llvm::LoadInst(rightLlvmValue, "", llvmObj->block));
                    }
                    new llvm::StoreInst(newAllocatedValue, leftLlvmRef, llvmObj->block);
                },
                [&]() {
                    if (underlyingType->kind == Type::Kind::Class || underlyingType->kind == Type::Kind::StaticArray || underlyingType->kind == Type::Kind::DynamicArray || underlyingType->kind == Type::Kind::MaybeError) {
                        underlyingType->createLlvmCopyConstructor(llvmObj, leftLlvmValue, rightLlvmValue);
                    } else {
                        underlyingType->createLlvmCopyConstructor(llvmObj, leftLlvmValue, new llvm::LoadInst(rightLlvmValue, "", llvmObj->block));
                    }
                }
            );
        }
    );
}
void OwnerPointerType::createLlvmDestructorValue(LlvmObject* llvmObj, llvm::Value* llvmValue) {
    auto leftAsInt = new llvm::PtrToIntInst(llvmValue, llvm::Type::getInt64Ty(llvmObj->context), "", llvmObj->block);
    createLlvmConditional(
        llvmObj,
        new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_NE, leftAsInt, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), ""),
        [&]() {
            underlyingType->createLlvmDestructorRef(llvmObj, llvmValue);
            llvm::CallInst::Create(
                llvmObj->freeFunction, 
                new llvm::BitCastInst(llvmValue, llvm::Type::getInt8PtrTy(llvmObj->context), "", llvmObj->block), 
                "", 
                llvmObj->block
            );
        }
    );
}
void OwnerPointerType::createLlvmDestructorRef(LlvmObject* llvmObj, llvm::Value* llvmRef) {
    createLlvmDestructorValue(llvmObj, new llvm::LoadInst(llvmRef, "", llvmObj->block));
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
Type* RawPointerType::changeClassToTemplate(const vector<TemplateType*> templateTypes) {
    underlyingType = underlyingType->changeClassToTemplate(templateTypes);
    return this;
}
Type* RawPointerType::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    return Create(underlyingType->templateCopy(parentScope, templateToType));
}
bool RawPointerType::interpret(Scope* scope) {
    return underlyingType->interpret(scope);
}
bool RawPointerType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const RawPointerType&>(type);
        return cmpPtr(this->underlyingType, other.underlyingType);
    } else {
        return false;
    }
}
bool RawPointerType::isTemplate() {
    return underlyingType->isTemplate();
}
MatchTemplateResult RawPointerType::matchTemplate(TemplateFunctionType* templateFunctionType, Type* type) {
    if (kind == type->kind) return underlyingType->matchTemplate(templateFunctionType, ((RawPointerType*)type)->underlyingType);
    else return MatchTemplateResult::Fail;
}
Type* RawPointerType::substituteTemplate(TemplateFunctionType* templateFunctionType) {
    return Create(underlyingType->substituteTemplate(templateFunctionType));
}
int RawPointerType::compareTemplateDepth(Type* type) {
    if (type->kind != Type::Kind::RawPointer) return -1;
    return underlyingType->compareTemplateDepth(((RawPointerType*)type)->underlyingType);
}
optional<InterpretConstructorResult> RawPointerType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    switch (arguments.size()) {
    case 0: 
        if (parentIsAssignment) {
            return InterpretConstructorResult(nullptr, nullptr);
        } else {
            return NullValue::Create(position, this);
        }
    case 1: {
        if (arguments[0]->valueKind == Value::ValueKind::Null) {
            return NullValue::Create(position, this);
        }
        auto effType = arguments[0]->type->getEffectiveType();
        if (effType->kind == Type::Kind::OwnerPointer && cmpPtr(((OwnerPointerType*)effType)->underlyingType, underlyingType)) {
            if (Value::isLvalue(arguments[0])) {
                return InterpretConstructorResult(nullptr, nullptr);
            } else {
                if (isExplicit) {
                    return InterpretConstructorResult(nullptr, nullptr, true);
                } else {
                    if (!onlyTry) errorMessageOpt("cannot implicitly create raw pointer from non-lvalue owner pointer", position);
                    return nullopt;
                }
            }
        }
        if (!onlyTry) errorMessageOpt("no fitting raw pointer constructor (bad argument type)", position);
        return nullopt;
    }
    default: 
        if (!onlyTry) errorMessageOpt("no fitting raw pointer constructor (too many arguments)", position);
        return nullopt;
    }
}
void RawPointerType::createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    if (arguments.size() == 1) {
        arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
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
bool RawPointerType::hasLlvmConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
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
Type* MaybeErrorType::changeClassToTemplate(const vector<TemplateType*> templateTypes) {
    underlyingType = underlyingType->changeClassToTemplate(templateTypes);
    return this;
}
bool MaybeErrorType::interpret(Scope* scope) {
    return underlyingType->interpret(scope);
}
Type* MaybeErrorType::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    return Create(underlyingType->templateCopy(parentScope, templateToType));
}
bool MaybeErrorType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const MaybeErrorType&>(type);
        return cmpPtr(this->underlyingType, other.underlyingType);
    } else {
        return false;
    }
}
bool MaybeErrorType::isTemplate() {
    return underlyingType->isTemplate();
}
MatchTemplateResult MaybeErrorType::matchTemplate(TemplateFunctionType* templateFunctionType, Type* type) {
    if (kind == type->kind) return underlyingType->matchTemplate(templateFunctionType, ((MaybeErrorType*)type)->underlyingType);
    else return MatchTemplateResult::Fail;
}
Type* MaybeErrorType::substituteTemplate(TemplateFunctionType* templateFunctionType) {
    return Create(underlyingType->substituteTemplate(templateFunctionType));
}
int MaybeErrorType::compareTemplateDepth(Type* type) {
    if (type->kind != Type::Kind::MaybeError) return -1;
    return underlyingType->compareTemplateDepth(((MaybeErrorType*)type)->underlyingType);
}
bool MaybeErrorType::needsDestruction() {
    return underlyingType->needsDestruction();
}
bool MaybeErrorType::needsReference() {
    return underlyingType->needsReference();
}
optional<InterpretConstructorResult> MaybeErrorType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    switch (arguments.size()) {
    case 0:
        return InterpretConstructorResult(nullptr, nullptr);
    case 1: {
        auto argEffType = arguments[0]->type->getEffectiveType();
        if (cmpPtr(argEffType, underlyingType)) {
            return InterpretConstructorResult(nullptr, nullptr);
        }
        if (isExplicit && underlyingType->kind == Type::Kind::Void && argEffType->kind == Type::Kind::Integer) {
            return InterpretConstructorResult(nullptr, nullptr);
        }
        if (argEffType->kind == Type::Kind::MaybeError) {
            if (underlyingType->kind == Type::Kind::Void || ((MaybeErrorType*)argEffType)->underlyingType->kind == Type::Kind::Void) {
                return InterpretConstructorResult(nullptr, nullptr);
            }
        }
        if (!onlyTry) errorMessageOpt("no fitting maybe error constructor (bad argument type)", position);
        return nullopt;
    }
    default: 
        if (!onlyTry) errorMessageOpt("no fitting maybe error constructor (too many arguments)", position);
        return nullopt;
    }
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
llvm::Value* MaybeErrorType::createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 0:
        if (underlyingType->kind == Type::Kind::Void) {
            return nullptr;
        } else if (!underlyingType->needsDestruction()){
            return nullptr;
        }
        break;
    case 1: {
        auto argEffType = arguments[0]->type->getEffectiveType();
        if (underlyingType->kind == Type::Kind::Void) {
            if (argEffType->kind == Type::Kind::Integer) {
                arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
                return nullptr;
            } else if (argEffType->kind == Type::Kind::MaybeError) {
                arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
                return nullptr;
            }
        }
        break;
    }
    default: 
        break;
    }
    return createRefForLlvmReference(llvmObj, arguments, classConstructor);
}
pair<llvm::Value*, llvm::Value*> MaybeErrorType::createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref) {
    switch (arguments.size()) {
    case 0:
        if (underlyingType->kind == Type::Kind::Void) {
            return { nullptr, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), -1) };
        } else if (!underlyingType->needsDestruction()){
            return { 
                nullptr,
                llvm::ConstantStruct::get((llvm::StructType*)createLlvm(llvmObj), {
                    llvm::UndefValue::get(underlyingType->createLlvm(llvmObj)),
                    llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), -1)
                }) 
            };
        }
        break;
    case 1: {
        auto argEffType = arguments[0]->type->getEffectiveType();
        if (underlyingType->kind == Type::Kind::Void) {
            if (argEffType->kind == Type::Kind::Integer) {
                return { nullptr, arguments[0]->createLlvm(llvmObj) };
            } else if (argEffType->kind == Type::Kind::MaybeError) {
                return { nullptr, new llvm::LoadInst(((MaybeErrorType*)argEffType)->llvmGepError(llvmObj, arguments[0]->getReferenceLlvm(llvmObj)), "", llvmObj->block) };
            }
        }
        break;
    }
    default: 
        break;
    }
    auto llvmRef = createLlvmReference(llvmObj, arguments, classConstructor, ref);
    return {llvmRef, new llvm::LoadInst(llvmRef, "", llvmObj->block)};
}
llvm::Value* MaybeErrorType::createRefForLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    createAllocaIfNeededForConstructor(llvmObj, arguments, classConstructor);
    return allocaLlvm(llvmObj);
}
llvm::Value* MaybeErrorType::createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref) {
    createLlvmConstructor(llvmObj, ref, arguments, classConstructor);
    return ref;
}
llvm::Value* MaybeErrorType::createRefForLlvmCopy(LlvmObject* llvmObj, Value* lValue) {
    lValue->createAllocaLlvmIfNeededForReference(llvmObj);
    return allocaLlvm(llvmObj);
}
llvm::Value* MaybeErrorType::createLlvmCopy(LlvmObject* llvmObj, Value* lValue, llvm::Value* ref) {
    createLlvmCopyConstructor(llvmObj, ref, lValue->getReferenceLlvm(llvmObj));
    return new llvm::LoadInst(ref, "", llvmObj->block);
}
llvm::Value* MaybeErrorType::llvmGepError(LlvmObject* llvmObj, llvm::Value* llvmRef) {
    vector<llvm::Value*> indexList;
    indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
    indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvmObj->context), 1));
    return llvm::GetElementPtrInst::Create(
        ((llvm::PointerType*)llvmRef->getType())->getElementType(), llvmRef, indexList, "", llvmObj->block
    );
}
llvm::Value* MaybeErrorType::llvmGepValue(LlvmObject* llvmObj, llvm::Value* llvmRef) {
    vector<llvm::Value*> indexList;
    indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
    indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvmObj->context), 0));
    return llvm::GetElementPtrInst::Create(
        ((llvm::PointerType*)llvmRef->getType())->getElementType(), llvmRef, indexList, "", llvmObj->block
    );
}
llvm::Value* MaybeErrorType::llvmExtractError(LlvmObject* llvmObj, llvm::Value* llvmValue) {
    return llvm::ExtractValueInst::Create(llvmValue, {1}, "", llvmObj->block);
}
llvm::Value* MaybeErrorType::llvmExtractValue(LlvmObject* llvmObj, llvm::Value* llvmValue) {
    return llvm::ExtractValueInst::Create(llvmValue, {0}, "", llvmObj->block);
}
llvm::Value* MaybeErrorType::llvmInsertError(LlvmObject* llvmObj, llvm::Value* llvmValue, llvm::Value* toInsert) {
    return llvm::InsertValueInst::Create(llvmValue, toInsert, {1}, "", llvmObj->block);
}
llvm::Value* MaybeErrorType::llvmInsertValue(LlvmObject* llvmObj, llvm::Value* llvmValue, llvm::Value* toInsert) {
    return llvm::InsertValueInst::Create(llvmValue, toInsert, {0}, "", llvmObj->block);
}
void MaybeErrorType::createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 1: {
        auto argEffType = arguments[0]->type->getEffectiveType();
        if (cmpPtr(argEffType, underlyingType)) {
            if (underlyingType->kind == Type::Kind::Class || underlyingType->kind == Type::Kind::StaticArray || underlyingType->kind == Type::Kind::DynamicArray || underlyingType->kind == Type::Kind::MaybeError) {
                arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
            } else {
                arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
            }
        }
        else if (underlyingType->kind == Type::Kind::Void && argEffType->kind == Type::Kind::Integer) {
            arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        }
        else if (argEffType->kind == Type::Kind::MaybeError) {
            if (underlyingType->kind == Type::Kind::Void) {
                arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
            } else if (((MaybeErrorType*)argEffType)->underlyingType->kind == Type::Kind::Void) {
                arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
            }
        }
        break;
    }
    default: 
        break;
    }
}
void MaybeErrorType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 0:
        if (underlyingType->kind == Type::Kind::Void) {
            new llvm::StoreInst(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), -1), leftLlvmRef, llvmObj->block);
        } else {
            new llvm::StoreInst(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), -1), llvmGepError(llvmObj, leftLlvmRef), llvmObj->block);
        }
        break;
    case 1: {
        auto argEffType = arguments[0]->type->getEffectiveType();
        if (cmpPtr(argEffType, underlyingType)) {
            if (Value::isLvalue(arguments[0])) {
                if (underlyingType->kind == Type::Kind::Class || underlyingType->kind == Type::Kind::StaticArray || underlyingType->kind == Type::Kind::DynamicArray || underlyingType->kind == Type::Kind::MaybeError) {
                    underlyingType->createLlvmCopyConstructor(llvmObj, llvmGepValue(llvmObj, leftLlvmRef), arguments[0]->getReferenceLlvm(llvmObj));
                } else {
                    underlyingType->createLlvmCopyConstructor(llvmObj, llvmGepValue(llvmObj, leftLlvmRef), arguments[0]->createLlvm(llvmObj));
                }
            } else {
                arguments[0]->wasCaptured = true;
                if (underlyingType->kind == Type::Kind::Class || underlyingType->kind == Type::Kind::StaticArray || underlyingType->kind == Type::Kind::DynamicArray || underlyingType->kind == Type::Kind::MaybeError) {
                    underlyingType->createLlvmMoveConstructor(llvmObj, llvmGepValue(llvmObj, leftLlvmRef), arguments[0]->getReferenceLlvm(llvmObj));
                } else {
                    underlyingType->createLlvmMoveConstructor(llvmObj, llvmGepValue(llvmObj, leftLlvmRef), arguments[0]->createLlvm(llvmObj));
                }
            }
            new llvm::StoreInst(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), llvmGepError(llvmObj, leftLlvmRef), llvmObj->block);
        }
        else if (underlyingType->kind == Type::Kind::Void && argEffType->kind == Type::Kind::Integer) {
            new llvm::StoreInst(arguments[0]->createLlvm(llvmObj), leftLlvmRef, llvmObj->block);
        }
        else if (argEffType->kind == Type::Kind::MaybeError) {
            if (underlyingType->kind == Type::Kind::Void) {
                new llvm::StoreInst(new llvm::LoadInst(llvmGepError(llvmObj, arguments[0]->getReferenceLlvm(llvmObj)), "", llvmObj->block), leftLlvmRef, llvmObj->block);
            } else if (((MaybeErrorType*)argEffType)->underlyingType->kind == Type::Kind::Void) {
                new llvm::StoreInst(arguments[0]->createLlvm(llvmObj), llvmGepError(llvmObj, leftLlvmRef), llvmObj->block);
            }
        } else {
            internalError("incorrect maybe error constructor during llvm creating (bad argument)");
        }
        break;
    }
    default: 
        internalError("incorrect maybe error constructor during llvm creating (>1 argument)");
        break;
    }
}
void MaybeErrorType::createLlvmCopyConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    if (underlyingType->kind == Type::Kind::Void) {
        new llvm::StoreInst(new llvm::LoadInst(rightLlvmValue, "", llvmObj->block), leftLlvmRef, llvmObj->block);
    } else {
        auto rightError = new llvm::LoadInst(llvmGepError(llvmObj, rightLlvmValue), "", llvmObj->block);
        createLlvmConditional(
            llvmObj,
            new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, rightError, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), ""),
            [&]() {
                if (underlyingType->kind == Type::Kind::Class || underlyingType->kind == Type::Kind::StaticArray || underlyingType->kind == Type::Kind::DynamicArray || underlyingType->kind == Type::Kind::MaybeError) {
                    underlyingType->createLlvmCopyConstructor(llvmObj, llvmGepValue(llvmObj, leftLlvmRef), llvmGepValue(llvmObj, rightLlvmValue));
                } else {
                    underlyingType->createLlvmCopyConstructor(
                        llvmObj, 
                        llvmGepValue(llvmObj, leftLlvmRef), 
                        new llvm::LoadInst(llvmGepValue(llvmObj, rightLlvmValue), "", llvmObj->block)
                    );
                }
                new llvm::StoreInst(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), llvmGepError(llvmObj, leftLlvmRef), llvmObj->block);
            },
            [&]() {
                new llvm::StoreInst(rightError, llvmGepError(llvmObj, leftLlvmRef), llvmObj->block);
            }
        );
    }
}
void MaybeErrorType::createLlvmMoveConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    new llvm::StoreInst(new llvm::LoadInst(rightLlvmValue, "", llvmObj->block), leftLlvmRef, llvmObj->block);
}
void MaybeErrorType::createLlvmCopyAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    if (underlyingType->kind == Type::Kind::Void) {
        new llvm::StoreInst(new llvm::LoadInst(rightLlvmValue, "", llvmObj->block), leftLlvmRef, llvmObj->block);
    } else {
        auto rightError = new llvm::LoadInst(llvmGepError(llvmObj, rightLlvmValue), "", llvmObj->block);
        auto leftError = new llvm::LoadInst(llvmGepError(llvmObj, leftLlvmRef), "", llvmObj->block);
        createLlvmConditional(
            llvmObj,
            new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, rightError, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), ""),
            [&]() {
                createLlvmConditional(
                    llvmObj, 
                    new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, leftError, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), ""),
                    [&]() {
                        if (underlyingType->kind == Type::Kind::Class || underlyingType->kind == Type::Kind::StaticArray || underlyingType->kind == Type::Kind::DynamicArray || underlyingType->kind == Type::Kind::MaybeError) {
                            underlyingType->createLlvmCopyAssignment(llvmObj, llvmGepValue(llvmObj, leftLlvmRef), llvmGepValue(llvmObj, rightLlvmValue));
                        } else {
                            underlyingType->createLlvmCopyAssignment(
                                llvmObj, 
                                llvmGepValue(llvmObj, leftLlvmRef), 
                                new llvm::LoadInst(llvmGepValue(llvmObj, rightLlvmValue), "", llvmObj->block)
                            );
                        }
                    },
                    [&]() {
                        if (underlyingType->kind == Type::Kind::Class || underlyingType->kind == Type::Kind::StaticArray || underlyingType->kind == Type::Kind::DynamicArray || underlyingType->kind == Type::Kind::MaybeError) {
                            underlyingType->createLlvmCopyConstructor(llvmObj, llvmGepValue(llvmObj, leftLlvmRef), llvmGepValue(llvmObj, rightLlvmValue));
                        } else {
                            underlyingType->createLlvmCopyConstructor(
                                llvmObj, 
                                llvmGepValue(llvmObj, leftLlvmRef), 
                                new llvm::LoadInst(llvmGepValue(llvmObj, rightLlvmValue), "", llvmObj->block)
                            );
                        }
                        new llvm::StoreInst(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), llvmGepError(llvmObj, leftLlvmRef), llvmObj->block);
                    }
                );
            },
            [&]() {
                createLlvmConditional(
                    llvmObj,
                    new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, leftError, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), ""),
                    [&]() {
                        underlyingType->createLlvmDestructorRef(llvmObj, llvmGepValue(llvmObj, leftLlvmRef));
                    }
                );
                new llvm::StoreInst(rightError, llvmGepError(llvmObj, leftLlvmRef), llvmObj->block);
            }
        );
    }
}
void MaybeErrorType::createLlvmDestructorValue(LlvmObject* llvmObj, llvm::Value* llvmValue) {
    if (!needsReference()) {
        if (underlyingType->needsDestruction()) {
            auto errorValue = llvmExtractError(llvmObj, llvmValue);
            createLlvmConditional(
                llvmObj,
                new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, errorValue, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), ""),
                [&]() {
                    underlyingType->createLlvmDestructorValue(llvmObj, llvmExtractValue(llvmObj, llvmValue));
                    llvmInsertError(llvmObj, llvmValue, llvmInt(llvmObj, -1));
                }
            );
        }
    } else {
        internalError("value destructor that needsReference on MaybeErrorType");
    }
}
void MaybeErrorType::createLlvmDestructorRef(LlvmObject* llvmObj, llvm::Value* llvmRef) {
    if (underlyingType->needsDestruction()) {
        auto errorValue = new llvm::LoadInst(llvmGepError(llvmObj, llvmRef), "", llvmObj->block);
        createLlvmConditional(
            llvmObj,
            new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, errorValue, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), ""),
            [&]() {
                underlyingType->createLlvmDestructorRef(llvmObj, llvmGepValue(llvmObj, llvmRef));
                new llvm::StoreInst(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), -1), llvmGepError(llvmObj, llvmRef), llvmObj->block);
            }
        );
    }
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
Type* ReferenceType::changeClassToTemplate(const vector<TemplateType*> templateTypes) {
    underlyingType = underlyingType->changeClassToTemplate(templateTypes);
    return this;
}
Type* ReferenceType::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    return Create(underlyingType->templateCopy(parentScope, templateToType));
}
bool ReferenceType::interpret(Scope* scope) {
    return underlyingType->interpret(scope);
}
bool ReferenceType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const ReferenceType&>(type);
        return cmpPtr(this->underlyingType, other.underlyingType);
    } else {
        return false;
    }
}
bool ReferenceType::isTemplate() {
    return underlyingType->isTemplate();
}
MatchTemplateResult ReferenceType::matchTemplate(TemplateFunctionType* templateFunctionType, Type* type) {
    if (kind == type->kind) return underlyingType->matchTemplate(templateFunctionType, ((ReferenceType*)type)->underlyingType);
    else return MatchTemplateResult::Fail;
}
Type* ReferenceType::substituteTemplate(TemplateFunctionType* templateFunctionType) {
    return Create(underlyingType->substituteTemplate(templateFunctionType));
}
int ReferenceType::compareTemplateDepth(Type* type) {
    if (type->kind != Type::Kind::Reference) return -1;
    return underlyingType->compareTemplateDepth(((ReferenceType*)type)->underlyingType);
}
Type* ReferenceType::getEffectiveType() {
    return underlyingType->getEffectiveType();
}
optional<InterpretConstructorResult> ReferenceType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
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
Type* StaticArrayType::changeClassToTemplate(const vector<TemplateType*> templateTypes) {
    elementType = elementType->changeClassToTemplate(templateTypes);
    return this;
}
Type* StaticArrayType::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    return Create(elementType->templateCopy(parentScope, templateToType), (Value*)size->templateCopy(parentScope, templateToType));
}
bool StaticArrayType::interpret(Scope* scope) {
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
    
    return elementType->interpret(scope);
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
bool StaticArrayType::isTemplate() {
    return elementType->isTemplate();
}
MatchTemplateResult StaticArrayType::matchTemplate(TemplateFunctionType* templateFunctionType, Type* type) {
    if (kind == type->kind && sizeAsInt == ((StaticArrayType*)type)->sizeAsInt) {
        return elementType->matchTemplate(templateFunctionType, ((StaticArrayType*)type)->elementType);
    }
    else return MatchTemplateResult::Fail;
}
Type* StaticArrayType::substituteTemplate(TemplateFunctionType* templateFunctionType) {
    return Create(elementType->substituteTemplate(templateFunctionType), sizeAsInt);
}
int StaticArrayType::compareTemplateDepth(Type* type) {
    if (type->kind != Type::Kind::StaticArray) return -1;
    return elementType->compareTemplateDepth(((StaticArrayType*)type)->elementType);
}
bool StaticArrayType::needsDestruction() {
    return elementType->needsDestruction();
}
bool StaticArrayType::needsReference() {
    return true;
}
optional<InterpretConstructorResult> StaticArrayType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    auto result = elementType->interpretConstructor(position, scope, arguments, onlyTry, parentIsAssignment, isExplicit);
    if (result && !isExplicit) {
        if (!onlyTry) errorMessageOpt("cannot implicitly create static array with those arguments", position);
        return nullopt;
    } else {
        return result;
    }
}
llvm::Value* StaticArrayType::createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    return createRefForLlvmReference(llvmObj, arguments, classConstructor);
}
pair<llvm::Value*, llvm::Value*> StaticArrayType::createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref) {
    auto llvmRef = createLlvmReference(llvmObj, arguments, classConstructor, ref);
    return {llvmRef, new llvm::LoadInst(llvmRef, "", llvmObj->block)};
}
llvm::Value* StaticArrayType::createRefForLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    createAllocaIfNeededForConstructor(llvmObj, arguments, classConstructor);
    return allocaLlvm(llvmObj);
}
llvm::Value* StaticArrayType::createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref) {
    createLlvmConstructor(llvmObj, ref, arguments, classConstructor);
    return ref;
}
llvm::Value* StaticArrayType::createRefForLlvmCopy(LlvmObject* llvmObj, Value* lValue) {
    lValue->createAllocaLlvmIfNeededForReference(llvmObj);
    return allocaLlvm(llvmObj);
}
llvm::Value* StaticArrayType::createLlvmCopy(LlvmObject* llvmObj, Value* lValue, llvm::Value* ref) {
    createLlvmCopyConstructor(llvmObj, ref, lValue->getReferenceLlvm(llvmObj));
    return new llvm::LoadInst(ref, "", llvmObj->block);
}
void StaticArrayType::createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    elementType->createAllocaIfNeededForConstructor(llvmObj, arguments, classConstructor);
}
void StaticArrayType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    if (elementType->hasLlvmConstructor(llvmObj, arguments, classConstructor)) {
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
bool StaticArrayType::hasLlvmConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    return elementType->hasLlvmConstructor(llvmObj, arguments, classConstructor);
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
        if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray || elementType->kind == Type::Kind::DynamicArray || elementType->kind == Type::Kind::MaybeError) {
            elementType->createLlvmCopyConstructor(llvmObj, leftGepRef, rightGepRef);
        } else {
            elementType->createLlvmCopyConstructor(llvmObj, leftGepRef, new llvm::LoadInst(rightGepRef, "", llvmObj->block));
        }  
    });
}
void StaticArrayType::createLlvmMoveConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    new llvm::StoreInst(new llvm::LoadInst(rightLlvmValue, "", llvmObj->block), leftLlvmRef, llvmObj->block);
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
        if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray || elementType->kind == Type::Kind::DynamicArray || elementType->kind == Type::Kind::MaybeError) {
            elementType->createLlvmCopyAssignment(llvmObj, leftGepRef, rightGepRef);
        } else {
            elementType->createLlvmCopyAssignment(llvmObj, leftGepRef, new llvm::LoadInst(rightGepRef, "", llvmObj->block));
        }  
    });
}
void StaticArrayType::createLlvmDestructorValue(LlvmObject* llvmObj, llvm::Value* llvmValue) {
    internalError("value destructor on StaticArrayType");
}
void StaticArrayType::createLlvmDestructorRef(LlvmObject* llvmObj, llvm::Value* llvmRef) {
    if (elementType->needsDestruction()) {
        auto sizeValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), sizeAsInt);
        createLlvmForEachLoop(llvmObj, sizeValue, [&](llvm::Value* index) {
            vector<llvm::Value*> indexList;
            indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
            indexList.push_back(new llvm::LoadInst(index, "", llvmObj->block));
            auto leftGepRef = llvm::GetElementPtrInst::Create(
                ((llvm::PointerType*)llvmRef->getType())->getElementType(), llvmRef, indexList, "", llvmObj->block
            );
            elementType->createLlvmDestructorRef(llvmObj, leftGepRef);
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
Type* DynamicArrayType::changeClassToTemplate(const vector<TemplateType*> templateTypes) {
    elementType = elementType->changeClassToTemplate(templateTypes);
    return this;
}
Type* DynamicArrayType::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    return Create(elementType->templateCopy(parentScope, templateToType));
}
bool DynamicArrayType::interpret(Scope* scope) {
    return elementType->interpret(scope);
}
bool DynamicArrayType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const DynamicArrayType&>(type);
        return cmpPtr(this->elementType, other.elementType);
    } else {
        return false;
    }
}
bool DynamicArrayType::isTemplate() {
    return elementType->isTemplate();
}
MatchTemplateResult DynamicArrayType::matchTemplate(TemplateFunctionType* templateFunctionType, Type* type) {
    if (kind == type->kind) {
        return elementType->matchTemplate(templateFunctionType, ((DynamicArrayType*)type)->elementType);
    }
    MatchTemplateResult matchResult = MatchTemplateResult::Fail;
    if (type->kind == Type::Kind::ArrayView) {
        auto matchResult = elementType->matchTemplate(templateFunctionType, ((ArrayViewType*)type)->elementType);
    } else if (type->kind == Type::Kind::StaticArray) {
        auto matchResult = elementType->matchTemplate(templateFunctionType, ((StaticArrayType*)type)->elementType);
    }
    if (matchResult == MatchTemplateResult::Perfect) {
        return MatchTemplateResult::Viable;
    } else {
        return MatchTemplateResult::Fail;
    }
}
Type* DynamicArrayType::substituteTemplate(TemplateFunctionType* templateFunctionType) {
    return Create(elementType->substituteTemplate(templateFunctionType));
}
int DynamicArrayType::compareTemplateDepth(Type* type) {
    if (type->kind != Type::Kind::DynamicArray) return -1;
    return elementType->compareTemplateDepth(((DynamicArrayType*)type)->elementType);
}
bool DynamicArrayType::needsDestruction() {
    return true;
}
bool DynamicArrayType::needsReference() {
    return true;
}
optional<pair<Type*,FunctionValue*>> DynamicArrayType::interpretFunction(const CodePosition& position, Scope* scope, const string functionName, vector<Value*> arguments) {
    if (functionName == "push") {
        auto result = elementType->interpretConstructor(position, scope, arguments, false, false, true);
        if (!result) return nullopt; 
        if (result.value().value) arguments = {result.value().value};
        return pair<Type*,FunctionValue*>(Type::Create(Type::Kind::Void), result.value().classConstructor);
    } else if (functionName == "pushArray") {
        return errorMessageOpt("dynamic array type function " + functionName + " is not implemented yet", position);
    } else if (functionName == "insert") {
        if (arguments.size() == 0) {
            return errorMessageOpt("dynamic array 'insert' function requires an int index argument", position);
        } else {
            arguments[0] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[0]});
            auto intCtorInterpret = arguments[0]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[0] = intCtorInterpret.value();

            auto arg0 = arguments[0];
            arguments.erase(arguments.begin());
            auto result = elementType->interpretConstructor(position, scope, arguments, false, false, true);
            if (!result) return nullopt; 
            if (result.value().value) arguments = {result.value().value};
            arguments.insert(arguments.begin(), arg0);

            return pair<Type*,FunctionValue*>(Type::Create(Type::Kind::Void), result.value().classConstructor);
        }
    } else if (functionName == "insertArray") {
        return errorMessageOpt("dynamic array type function " + functionName + " is not implemented yet", position);
    } else if (functionName == "resize") {
        if (arguments.size() == 0) {
            return errorMessageOpt("dynamic array 'resize' function requires an int size argument", position);
        } else {
            arguments[0] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[0]});
            auto intCtorInterpret = arguments[0]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[0] = intCtorInterpret.value();

            auto arg0 = arguments[0];
            arguments.erase(arguments.begin());
            auto result = elementType->interpretConstructor(position, scope, arguments, false, false, true);
            if (!result) return nullopt; 
            if (result.value().value) arguments = {result.value().value};
            arguments.insert(arguments.begin(), arg0);

            return pair<Type*,FunctionValue*>(Type::Create(Type::Kind::Void), result.value().classConstructor);
        }
    } else if (functionName == "extend") {
        if (arguments.size() == 0) {
            return errorMessageOpt("dynamic array 'extend' function requires an int size argument", position);
        } else {
            arguments[0] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[0]});
            auto intCtorInterpret = arguments[0]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[0] = intCtorInterpret.value();

            auto arg0 = arguments[0];
            arguments.erase(arguments.begin());
            auto result = elementType->interpretConstructor(position, scope, arguments, false, false, true);
            if (!result) return nullopt; 
            if (result.value().value) arguments = {result.value().value};
            arguments.insert(arguments.begin(), arg0);

            return pair<Type*,FunctionValue*>(Type::Create(Type::Kind::Void), result.value().classConstructor);
        }
    } else if (functionName == "shrink") {
        if (arguments.size() == 2) {
            arguments[0] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[0]});
            auto intCtorInterpret = arguments[0]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[0] = intCtorInterpret.value();
            arguments[1] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[1]});
            intCtorInterpret = arguments[1]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[1] = intCtorInterpret.value();
            return pair<Type*,FunctionValue*>(Type::Create(Type::Kind::Void), nullptr);
        } else if (arguments.size() == 3) {
            arguments[0] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[0]});
            auto intCtorInterpret = arguments[0]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[0] = intCtorInterpret.value();
            arguments[1] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[1]});
            intCtorInterpret = arguments[1]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[1] = intCtorInterpret.value();
            arguments[2] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[2]});
            intCtorInterpret = arguments[2]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[2] = intCtorInterpret.value();
            return pair<Type*,FunctionValue*>(Type::Create(Type::Kind::Void), nullptr);
        } else {
            return errorMessageOpt("dynamic array 'shrink' function takes 2 or 3 int arguments", position);
        }
    } else if (functionName == "reserve") {
        if (arguments.size() == 1) {
            arguments[0] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[0]});
            auto intCtorInterpret = arguments[0]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[0] = intCtorInterpret.value();
            return pair<Type*,FunctionValue*>(Type::Create(Type::Kind::Void), nullptr);
        } else {
            return errorMessageOpt("dynamic array 'reserve' function takes 1 int arguments", position);
        }
    } else if (functionName == "pop") {
        if (arguments.size() == 0) {
            return pair<Type*,FunctionValue*>(Type::Create(Type::Kind::Void), nullptr);
        } else {
            return errorMessageOpt("dynamic array 'pop' function takes no arguments", position);
        }
    } else if (functionName == "remove") {
        if (arguments.size() == 1) {
            arguments[0] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[0]});
            auto intCtorInterpret = arguments[0]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[0] = intCtorInterpret.value();
            return pair<Type*,FunctionValue*>(Type::Create(Type::Kind::Void), nullptr);
        } else if (arguments.size() == 2) {
            arguments[0] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[0]});
            auto intCtorInterpret = arguments[0]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[0] = intCtorInterpret.value();
            arguments[1] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[1]});
            intCtorInterpret = arguments[1]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[1] = intCtorInterpret.value();
            return pair<Type*,FunctionValue*>(Type::Create(Type::Kind::Void), nullptr);
        } else {
            return errorMessageOpt("dynamic array 'remove' function takes 1 or 2 int arguments", position);
        }
    } else if (functionName == "clear") {
        if (arguments.size() == 0) {
            return pair<Type*,FunctionValue*>(Type::Create(Type::Kind::Void), nullptr);
        } else {
            return errorMessageOpt("dynamic array 'clear' function takes no arguments", position);
        }
    } else if (functionName == "last") {
        if (arguments.size() == 0) {
            return pair<Type*,FunctionValue*>(ReferenceType::Create(elementType), nullptr);
        } else {
            return errorMessageOpt("dynamic array 'last' function takes no arguments", position);
        }
    } else if (functionName == "trim") {
        if (arguments.size() == 1) {
            arguments[0] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[0]});
            auto intCtorInterpret = arguments[0]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[0] = intCtorInterpret.value();
            return pair<Type*,FunctionValue*>(Type::Create(Type::Kind::Void), nullptr);
        } else {
            return errorMessageOpt("dynamic array 'trim' function takes 1 int arguments", position);
        }
    } else if (functionName == "slice") {
        if (arguments.size() == 2) {
            arguments[0] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[0]});
            auto intCtorInterpret = arguments[0]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[0] = intCtorInterpret.value();
            arguments[1] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[1]});
            intCtorInterpret = arguments[1]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[1] = intCtorInterpret.value();
            return pair<Type*,FunctionValue*>(this, nullptr);
        } else if (arguments.size() == 3) {
            arguments[0] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[0]});
            auto intCtorInterpret = arguments[0]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[0] = intCtorInterpret.value();
            arguments[1] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[1]});
            intCtorInterpret = arguments[1]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[1] = intCtorInterpret.value();
            arguments[2] = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[2]});
            intCtorInterpret = arguments[2]->interpret(scope);
            if (!intCtorInterpret) return nullopt;
            if (intCtorInterpret.value()) arguments[2] = intCtorInterpret.value();
            return pair<Type*,FunctionValue*>(this, nullptr);
        } else {
            return errorMessageOpt("dynamic array 'slice' function takes 2 or 3 int arguments", position);
        }
    } else {
        return errorMessageOpt(DeclarationMap::toString(this) + " type has no member function named " + functionName, position);
    }
}
llvm::Value* DynamicArrayType::createFunctionLlvmReference(const string functionName, LlvmObject* llvmObj, llvm::Value* llvmRef, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    if (functionName == "push") {
        auto size = llvmLoad(llvmObj, llvmGepSize(llvmObj, llvmRef));
        auto capacity = llvmLoad(llvmObj, llvmGepCapacity(llvmObj, llvmRef));
        createLlvmConditional(llvmObj, new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLE, capacity, size, ""), [&](){
            auto newCapacity = llvm::BinaryOperator::CreateMul(capacity,llvmInt(llvmObj, 2), "", llvmObj->block);
            llvmReallocData(llvmObj, llvmRef, newCapacity);
            llvmStore(llvmObj, newCapacity, llvmGepCapacity(llvmObj, llvmRef));
        });
        if (elementType->hasLlvmConstructor(llvmObj, arguments, classConstructor)) {
            auto data = llvmLoad(llvmObj, llvmGepData(llvmObj, llvmRef));
            auto gepRef = llvmGepDataElement(llvmObj, data, size);
            elementType->createLlvmConstructor(llvmObj, gepRef, arguments, classConstructor);
        }
        llvmStore(llvmObj, llvm::BinaryOperator::CreateAdd(size, llvmInt(llvmObj, 1), "", llvmObj->block), llvmGepSize(llvmObj, llvmRef));
    } else if (functionName == "pushArray") {
    } else if (functionName == "insert") {
        vector<Value*> constructorArgs(arguments.begin()+1, arguments.end());
        auto index = arguments[0]->createLlvm(llvmObj);
        auto size = llvmLoad(llvmObj, llvmGepSize(llvmObj, llvmRef));
        auto capacity = llvmLoad(llvmObj, llvmGepCapacity(llvmObj, llvmRef));
        createLlvmConditional(llvmObj, new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLE, capacity, size, ""), [&](){
            auto newCapacity = llvm::BinaryOperator::CreateMul(capacity,llvmInt(llvmObj, 2), "", llvmObj->block);
            llvmReallocData(llvmObj, llvmRef, newCapacity);
            llvmStore(llvmObj, newCapacity, llvmGepCapacity(llvmObj, llvmRef));
        });
        auto data = llvmLoad(llvmObj, llvmGepData(llvmObj, llvmRef));
        createLlvmForEachLoopReversed(llvmObj, size, index, [&](llvm::Value* iter){
            auto iterValue = llvmLoad(llvmObj, iter);
            auto gepRefI = llvmGepDataElement(llvmObj, data, iterValue);
            auto interMinus1 = llvm::BinaryOperator::CreateSub(iterValue, llvmInt(llvmObj, 1), "", llvmObj->block);
            auto gepRefIMinus1 = llvmGepDataElement(llvmObj, data, interMinus1);
            llvmStore(llvmObj, llvmLoad(llvmObj, gepRefIMinus1), gepRefI);
        });
        if (elementType->hasLlvmConstructor(llvmObj, constructorArgs, classConstructor)) {
            auto data = llvmLoad(llvmObj, llvmGepData(llvmObj, llvmRef));
            auto gepRef = llvmGepDataElement(llvmObj, data, index);
            elementType->createLlvmConstructor(llvmObj, gepRef, constructorArgs, classConstructor);
        }
        llvmStore(llvmObj, llvm::BinaryOperator::CreateAdd(size, llvmInt(llvmObj, 1), "", llvmObj->block), llvmGepSize(llvmObj, llvmRef));
    } else if (functionName == "insertArray") {
    } else if (functionName == "resize") {
        auto newSize = arguments[0]->createLlvm(llvmObj);
        auto size = llvmLoad(llvmObj, llvmGepSize(llvmObj, llvmRef));
        createLlvmConditional(llvmObj, new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLT, size, newSize, ""), 
            [&](){
                createFunctionLlvmReference("extend", llvmObj, llvmRef, arguments, classConstructor);
            },
            [&]() {
                createFunctionLlvmReference("trim", llvmObj, llvmRef, arguments, classConstructor);
            }
        );
    } else if (functionName == "extend") {
        vector<Value*> constructorArgs(arguments.begin()+1, arguments.end());
        auto newSize = arguments[0]->createLlvm(llvmObj);
        createFunctionLlvmReference("reserve", llvmObj, llvmRef, arguments, classConstructor);
        if (elementType->hasLlvmConstructor(llvmObj, constructorArgs, classConstructor)) {
            auto data = llvmLoad(llvmObj, llvmGepData(llvmObj, llvmRef));
            auto size = llvmLoad(llvmObj, llvmGepSize(llvmObj, llvmRef));
            createLlvmForEachLoop(llvmObj, size, newSize, false, [&](llvm::Value* index){
                auto gepRef = llvmGepDataElement(llvmObj, data, llvmLoad(llvmObj, index));
                elementType->createLlvmConstructor(llvmObj, gepRef, constructorArgs, classConstructor);
            });
        }
        llvmStore(llvmObj, newSize, llvmGepSize(llvmObj, llvmRef));
    } else if (functionName == "shrink") {
        if (arguments.size() == 2) {
            auto start = arguments[0]->createLlvm(llvmObj);
            auto end = arguments[1]->createLlvm(llvmObj);
            auto endSubStart = llvm::BinaryOperator::CreateSub(end, start, "", llvmObj->block);
            auto newSize = llvm::BinaryOperator::CreateAdd(endSubStart, llvmInt(llvmObj, 1), "", llvmObj->block);
            auto data = llvmLoad(llvmObj, llvmGepData(llvmObj, llvmRef));
            if (elementType->needsDestruction()) {
                createLlvmForEachLoop(llvmObj, start, [&](llvm::Value* index){
                    auto gepRef = llvmGepDataElement(llvmObj, data, llvmLoad(llvmObj, index));
                    elementType->createLlvmDestructorRef(llvmObj, gepRef);
                });
                auto endPlus1 = llvm::BinaryOperator::CreateAdd(end, llvmInt(llvmObj, 1), "", llvmObj->block);
                auto size = llvmLoad(llvmObj, llvmGepSize(llvmObj, llvmRef));
                createLlvmForEachLoop(llvmObj, endPlus1, size, false, [&](llvm::Value* index){
                    auto gepRef = llvmGepDataElement(llvmObj, data, llvmLoad(llvmObj, index));
                    elementType->createLlvmDestructorRef(llvmObj, gepRef);
                });
            }
            createLlvmForEachLoop(llvmObj, newSize, [&](llvm::Value* index){
                auto indexValue = llvmLoad(llvmObj, index);
                auto indexPlusStart = llvm::BinaryOperator::CreateAdd(indexValue, start, "", llvmObj->block);
                auto destinationGepRef = llvmGepDataElement(llvmObj, data, indexValue);
                auto SourceGepRef = llvmGepDataElement(llvmObj, data, indexPlusStart);
                llvmStore(llvmObj, llvmLoad(llvmObj, SourceGepRef), destinationGepRef);
            });
            llvmStore(llvmObj, newSize, llvmGepSize(llvmObj, llvmRef));
        }
        if (arguments.size() == 3) {
            auto start = arguments[0]->createLlvm(llvmObj);
            auto step = arguments[1]->createLlvm(llvmObj);
            auto end = arguments[2]->createLlvm(llvmObj);
            auto endSubStart = llvm::BinaryOperator::CreateSub(end, start, "", llvmObj->block);
            auto endSubStartDivStep = llvm::BinaryOperator::CreateSDiv(endSubStart, step, "", llvmObj->block);
            auto newSize = llvm::BinaryOperator::CreateAdd(endSubStartDivStep, llvmInt(llvmObj, 1), "", llvmObj->block);
            auto data = llvmLoad(llvmObj, llvmGepData(llvmObj, llvmRef));
            if (elementType->needsDestruction()) {
                createLlvmForEachLoop(llvmObj, start, [&](llvm::Value* index){
                    auto gepRef = llvmGepDataElement(llvmObj, data, llvmLoad(llvmObj, index));
                    elementType->createLlvmDestructorRef(llvmObj, gepRef);
                });
                auto endPlus1 = llvm::BinaryOperator::CreateAdd(end, llvmInt(llvmObj, 1), "", llvmObj->block);
                auto size = llvmLoad(llvmObj, llvmGepSize(llvmObj, llvmRef));
                createLlvmForEachLoop(llvmObj, endPlus1, size, false, [&](llvm::Value* index){
                    auto gepRef = llvmGepDataElement(llvmObj, data, llvmLoad(llvmObj, index));
                    elementType->createLlvmDestructorRef(llvmObj, gepRef);
                });
            }
            auto newArrayIndex = IntegerType::Create(IntegerType::Size::I64)->allocaLlvm(llvmObj);
            llvmStore(llvmObj, llvmInt(llvmObj, 0), newArrayIndex);
            createLlvmForEachLoop(llvmObj, start, end, true, [&](llvm::Value* index){
                auto indexValue = llvmLoad(llvmObj, index);
                auto indexValueMinusStart = llvm::BinaryOperator::CreateSub(indexValue, start, "", llvmObj->block);
                auto indexMinusStartDivStepRem = llvm::BinaryOperator::CreateSRem(indexValueMinusStart, step, "", llvmObj->block);
                createLlvmConditional(llvmObj, new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_EQ, indexMinusStartDivStepRem, llvmInt(llvmObj, 0), ""),
                    [&]() {
                        auto newArrayIndexValue = llvmLoad(llvmObj, newArrayIndex);
                        auto destinationGepRef = llvmGepDataElement(llvmObj, data, newArrayIndexValue);
                        auto indexValue = llvmLoad(llvmObj, index);
                        auto SourceGepRef = llvmGepDataElement(llvmObj, data, indexValue);
                        llvmStore(llvmObj, llvmLoad(llvmObj, SourceGepRef), destinationGepRef);
                        llvmStore(llvmObj, llvm::BinaryOperator::CreateAdd(newArrayIndexValue, llvmInt(llvmObj, 1), "", llvmObj->block), newArrayIndex);
                    },
                    [&]() {
                        auto gepRef = llvmGepDataElement(llvmObj, data, llvmLoad(llvmObj, index));
                        elementType->createLlvmDestructorRef(llvmObj, gepRef);
                    }
                );
            });
            llvmStore(llvmObj, newSize, llvmGepSize(llvmObj, llvmRef));
        }
    } else if (functionName == "reserve") {
        auto newCapacity = arguments[0]->createLlvm(llvmObj);
        auto capacity = llvmLoad(llvmObj, llvmGepCapacity(llvmObj, llvmRef));
        createLlvmConditional(llvmObj, new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLT, capacity, newCapacity, ""), [&](){
            llvmReallocData(llvmObj, llvmRef, newCapacity);
            llvmStore(llvmObj, newCapacity, llvmGepCapacity(llvmObj, llvmRef));
        });
    } else if (functionName == "pop") {
        auto size = llvmLoad(llvmObj, llvmGepSize(llvmObj, llvmRef));
        auto sizeMinus1 = llvm::BinaryOperator::CreateSub(size, llvmInt(llvmObj, 1), "", llvmObj->block);
        llvmStore(llvmObj, sizeMinus1, llvmGepSize(llvmObj, llvmRef));
        auto data = llvmLoad(llvmObj, llvmGepData(llvmObj, llvmRef));
        elementType->createLlvmDestructorRef(llvmObj, llvmGepDataElement(llvmObj, data, sizeMinus1));
    } else if (functionName == "remove") {
        if (arguments.size() == 1) {
            auto indexToRemove = arguments[0]->createLlvm(llvmObj);
            auto data = llvmLoad(llvmObj, llvmGepData(llvmObj, llvmRef));
            elementType->createLlvmDestructorRef(llvmObj, llvmGepDataElement(llvmObj, data, indexToRemove));
            auto size = llvmLoad(llvmObj, llvmGepSize(llvmObj, llvmRef));
            auto sizeMinus1 = llvm::BinaryOperator::CreateSub(size, llvmInt(llvmObj, 1), "", llvmObj->block);
            llvmStore(llvmObj, sizeMinus1, llvmGepSize(llvmObj, llvmRef));
            createLlvmForEachLoop(llvmObj, indexToRemove, sizeMinus1, false, [&](llvm::Value* index){
                auto indexValue = llvmLoad(llvmObj, index);
                auto gepRefI = llvmGepDataElement(llvmObj, data, indexValue);
                auto indexPlus1 = llvm::BinaryOperator::CreateAdd(indexValue, llvmInt(llvmObj, 1), "", llvmObj->block);
                auto gepRefIPlus1 = llvmGepDataElement(llvmObj, data, indexPlus1);
                llvmStore(llvmObj, llvmLoad(llvmObj, gepRefIPlus1), gepRefI);
            });
        } else if (arguments.size() == 2) {
            auto startIndex = arguments[0]->createLlvm(llvmObj);
            auto endIndex = arguments[1]->createLlvm(llvmObj);
            auto data = llvmLoad(llvmObj, llvmGepData(llvmObj, llvmRef));
            auto endIndexPlus1 = llvm::BinaryOperator::CreateAdd(endIndex, llvmInt(llvmObj, 1), "", llvmObj->block);
            if (elementType->needsDestruction()) {
                createLlvmForEachLoop(llvmObj, startIndex, endIndexPlus1, false, [&](llvm::Value* index){
                    elementType->createLlvmDestructorRef(llvmObj, llvmGepDataElement(llvmObj, data, llvmLoad(llvmObj, index)));
                });
            }
            auto removeSize = llvm::BinaryOperator::CreateSub(endIndexPlus1, startIndex, "", llvmObj->block);
            auto size = llvmLoad(llvmObj, llvmGepSize(llvmObj, llvmRef));
            auto sizeMinusRemoveSize = llvm::BinaryOperator::CreateSub(size, removeSize, "", llvmObj->block);
            llvmStore(llvmObj, sizeMinusRemoveSize, llvmGepSize(llvmObj, llvmRef));
            createLlvmForEachLoop(llvmObj, startIndex, sizeMinusRemoveSize, false, [&](llvm::Value* index){
                auto indexValue = llvmLoad(llvmObj, index);
                auto gepRefI = llvmGepDataElement(llvmObj, data, indexValue);
                auto indexPlusOffset = llvm::BinaryOperator::CreateAdd(indexValue, removeSize, "", llvmObj->block);
                auto gepRefIPlusOffset = llvmGepDataElement(llvmObj, data, indexPlusOffset);
                llvmStore(llvmObj, llvmLoad(llvmObj, gepRefIPlusOffset), gepRefI);
            });
        }
    } else if (functionName == "clear") {
        if (elementType->needsDestruction()) {
            auto data = llvmLoad(llvmObj, llvmGepData(llvmObj, llvmRef));
            auto size = llvmLoad(llvmObj, llvmGepSize(llvmObj, llvmRef));
            createLlvmForEachLoop(llvmObj, size, [&](llvm::Value* index){
                auto leftGepRef = llvmGepDataElement(llvmObj, data, llvmLoad(llvmObj, index));
                elementType->createLlvmDestructorRef(llvmObj, leftGepRef);
            });
        }
        llvmStore(llvmObj, llvmInt(llvmObj, 0), llvmGepSize(llvmObj, llvmRef));
    } else if (functionName == "last") {
        auto data = llvmLoad(llvmObj, llvmGepData(llvmObj, llvmRef));
        auto size = llvmLoad(llvmObj, llvmGepSize(llvmObj, llvmRef));
        return llvmGepDataElement(llvmObj, data, llvm::BinaryOperator::CreateSub(size, llvmInt(llvmObj, 1), "", llvmObj->block));
    } else if (functionName == "trim") {
        auto newSize = arguments[0]->createLlvm(llvmObj);
        if (elementType->needsDestruction()) {
            auto data = llvmLoad(llvmObj, llvmGepData(llvmObj, llvmRef));
            auto size = llvmLoad(llvmObj, llvmGepSize(llvmObj, llvmRef));
            createLlvmForEachLoop(llvmObj, newSize, size, false, [&](llvm::Value* index){
                auto gepRef = llvmGepDataElement(llvmObj, data, llvmLoad(llvmObj, index));
                elementType->createLlvmDestructorRef(llvmObj, gepRef);
            });
        }
        llvmStore(llvmObj, newSize, llvmGepSize(llvmObj, llvmRef));
    } else if (functionName == "slice") {
        if (arguments.size() == 2) {
            auto start = arguments[0]->createLlvm(llvmObj);
            auto end = arguments[1]->createLlvm(llvmObj);
            auto newArray = allocaLlvm(llvmObj);
            auto endSubStart = llvm::BinaryOperator::CreateSub(end, start, "", llvmObj->block);
            auto newArraySize = llvm::BinaryOperator::CreateAdd(endSubStart, llvmInt(llvmObj, 1), "", llvmObj->block);
            llvmStore(llvmObj, newArraySize, llvmGepSize(llvmObj, newArray));
            createLlvmConditional(llvmObj, new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLE, newArraySize, llvmInt(llvmObj, 50), ""), 
                [&]() {
                llvmStore(llvmObj, llvmInt(llvmObj, 50), llvmGepCapacity(llvmObj, newArray));
            },
                [&]() {
                llvmStore(llvmObj, newArraySize, llvmGepCapacity(llvmObj, newArray));
            }
            );
            llvmAllocData(llvmObj, newArray, llvmLoad(llvmObj, llvmGepCapacity(llvmObj, newArray)));
            auto newData = llvmLoad(llvmObj, llvmGepData(llvmObj, newArray));
            auto thisData = llvmLoad(llvmObj, llvmGepData(llvmObj, llvmRef));
            createLlvmForEachLoop(llvmObj, newArraySize, [&](llvm::Value* index){
                auto indexValue = llvmLoad(llvmObj, index);
                auto indexPlusStart = llvm::BinaryOperator::CreateAdd(indexValue, start, "", llvmObj->block);
                auto newGepRef = llvmGepDataElement(llvmObj, newData, indexValue);
                auto thisGepRef = llvmGepDataElement(llvmObj, thisData, indexPlusStart);
                if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray || elementType->kind == Type::Kind::DynamicArray || elementType->kind == Type::Kind::MaybeError) {
                    elementType->createLlvmCopyConstructor(llvmObj, newGepRef, thisGepRef);
                } else {
                    elementType->createLlvmCopyConstructor(llvmObj, newGepRef, llvmLoad(llvmObj, thisGepRef));
                } 
            });
            return newArray;
        }
        if (arguments.size() == 3) {
            auto start = arguments[0]->createLlvm(llvmObj);
            auto step = arguments[1]->createLlvm(llvmObj);
            auto end = arguments[2]->createLlvm(llvmObj);
            auto newArray = allocaLlvm(llvmObj);
            auto endSubStart = llvm::BinaryOperator::CreateSub(end, start, "", llvmObj->block);
            auto endSubStartDivStep = llvm::BinaryOperator::CreateSDiv(endSubStart, step, "", llvmObj->block);
            auto newArraySize = llvm::BinaryOperator::CreateAdd(endSubStartDivStep, llvmInt(llvmObj, 1), "", llvmObj->block);
            llvmStore(llvmObj, newArraySize, llvmGepSize(llvmObj, newArray));
            createLlvmConditional(llvmObj, new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLE, newArraySize, llvmInt(llvmObj, 50), ""), 
                [&]() {
                    llvmStore(llvmObj, llvmInt(llvmObj, 50), llvmGepCapacity(llvmObj, newArray));
                },
                [&]() {
                    llvmStore(llvmObj, newArraySize, llvmGepCapacity(llvmObj, newArray));
                }
            );
            llvmAllocData(llvmObj, newArray, llvmLoad(llvmObj, llvmGepCapacity(llvmObj, newArray)));
            auto newData = llvmLoad(llvmObj, llvmGepData(llvmObj, newArray));
            auto thisData = llvmLoad(llvmObj, llvmGepData(llvmObj, llvmRef));
            auto thisArrayIndex = IntegerType::Create(IntegerType::Size::I64)->allocaLlvm(llvmObj);
            llvmStore(llvmObj, start, thisArrayIndex);
            createLlvmForEachLoop(llvmObj, newArraySize, [&](llvm::Value* index){
                auto thisIndexValue = llvmLoad(llvmObj, thisArrayIndex);
                auto newGepRef = llvmGepDataElement(llvmObj, newData, llvmLoad(llvmObj, index));
                auto thisGepRef = llvmGepDataElement(llvmObj, thisData, thisIndexValue);
                if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray || elementType->kind == Type::Kind::DynamicArray || elementType->kind == Type::Kind::MaybeError) {
                    elementType->createLlvmCopyConstructor(llvmObj, newGepRef, thisGepRef);
                } else {
                    elementType->createLlvmCopyConstructor(llvmObj, newGepRef, llvmLoad(llvmObj, thisGepRef));
                } 
                llvmStore(llvmObj, llvm::BinaryOperator::CreateAdd(thisIndexValue, step, "", llvmObj->block), thisArrayIndex);
            });
            return newArray;
        }
    } else {
        internalError("unknown functionName during dynamic array createFunctionLlvmReference (" + functionName + ")");
    } 
    return nullptr;
}
pair<llvm::Value*, llvm::Value*> DynamicArrayType::createFunctionLlvmValue(const string functionName, LlvmObject* llvmObj, llvm::Value* llvmRef, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    auto ref = createFunctionLlvmReference(functionName, llvmObj, llvmRef, arguments, classConstructor);
    if (functionName == "last" || functionName == "slice") {
        return { ref, llvmLoad(llvmObj, ref) };
    } else {
        return { ref, nullptr };
    }
}
optional<InterpretConstructorResult> DynamicArrayType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    switch (arguments.size()) {
    case 0:
        return InterpretConstructorResult(nullptr, nullptr);
    case 1: {
        auto effType = arguments[0]->type->getEffectiveType();
        if (effType->kind == Type::Kind::StaticArray) {
            if (cmpPtr(elementType, ((StaticArrayType*)effType)->elementType)) {
                if (Value::isLvalue(arguments[0])) {
                    return InterpretConstructorResult(nullptr, nullptr);
                } else {
                    return InterpretConstructorResult(nullptr, nullptr, true);
                }
            }
        } else if (effType->kind == Type::Kind::ArrayView) {
            if (cmpPtr(elementType, ((ArrayViewType*)effType)->elementType)) {
                return InterpretConstructorResult(nullptr, nullptr);
            }
        } else {
            // capacity: i64
            auto intCtor = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[0]});
            auto intCtorInterpret = intCtor->interpret(scope, true);
            if (!intCtorInterpret) {
                if (!onlyTry) errorMessageOpt("no fitting dynamic array constructor (bad argument type)", position);
                return nullopt;
            }
            if (!onlyTry) {
                auto intCtorInterpret = intCtor->interpret(scope, false);
                if (intCtorInterpret.value()) arguments[0] = intCtorInterpret.value();
                else arguments[0] = intCtor;
            }
            return InterpretConstructorResult(nullptr, nullptr);
        }
        if (!onlyTry) errorMessageOpt("no fitting dynamic array constructor (bad argument type)", position);
        return nullopt;
    }
    default: {
        // capacity: i64
        auto intCtor = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[0]});
        auto intCtorInterpret = intCtor->interpret(scope);
        if (!intCtorInterpret) return nullopt;

        // size: i64
        auto intCtor2 = ConstructorOperation::Create(position, IntegerType::Create(IntegerType::Size::I64), {arguments[1]});
        auto intCtorInterpret2 = intCtor2->interpret(scope);
        if (!intCtorInterpret2) return nullopt;

        auto arg0 = arguments[0];
        auto arg1 = arguments[1];
        arguments.erase(arguments.begin(), arguments.begin()+2);
        auto result = elementType->interpretConstructor(position, scope, arguments, onlyTry, parentIsAssignment, true);
        if (result && !isExplicit) {
            if (!onlyTry) errorMessageOpt("cannot implicitly create dynamic array with those arguments", position);
            return nullopt;
        } else if (!result) {
            return nullopt;
        } else if (!onlyTry) {
            if (result.value().value) {
                arguments = {result.value().value};
            }
            arguments.insert(arguments.begin(), arg1);
            arguments.insert(arguments.begin(), arg0);
            if (intCtorInterpret.value()) arguments[0] = intCtorInterpret.value();
            else arguments[0] = intCtor;
            if (intCtorInterpret2.value()) arguments[1] = intCtorInterpret2.value();
            else arguments[1] = intCtor2;
        } 
        return InterpretConstructorResult(nullptr, result.value().classConstructor);
    }
    }
}
/*unique_ptr<Type> DynamicArrayType::copy() {
    return make_unique<DynamicArrayType>(this->elementType->copy());
}*/
llvm::Type* DynamicArrayType::createLlvm(LlvmObject* llvmObj) {
    if (!llvmType) {
        llvmType = llvm::StructType::get(llvmObj->context, {
            llvm::Type::getInt64Ty(llvmObj->context),
            llvm::Type::getInt64Ty(llvmObj->context),
            llvm::PointerType::get(elementType->createLlvm(llvmObj), 0),
        });
    }
    return llvmType;
}
llvm::Value* DynamicArrayType::createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    return createRefForLlvmReference(llvmObj, arguments, classConstructor);
}
pair<llvm::Value*, llvm::Value*> DynamicArrayType::createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref) {
    auto llvmRef = createLlvmReference(llvmObj, arguments, classConstructor, ref);
    return {llvmRef, llvmLoad(llvmObj, llvmRef)};
}
llvm::Value* DynamicArrayType::createRefForLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    createAllocaIfNeededForConstructor(llvmObj, arguments, classConstructor);
    return allocaLlvm(llvmObj);
}
llvm::Value* DynamicArrayType::createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref) {
    createLlvmConstructor(llvmObj, ref, arguments, classConstructor);
    return ref;
}
llvm::Value* DynamicArrayType::createRefForLlvmCopy(LlvmObject* llvmObj, Value* lValue) {
    lValue->createAllocaLlvmIfNeededForReference(llvmObj);
    return allocaLlvm(llvmObj);
}
llvm::Value* DynamicArrayType::createLlvmCopy(LlvmObject* llvmObj, Value* lValue, llvm::Value* ref) {
    createLlvmCopyConstructor(llvmObj, ref, lValue->getReferenceLlvm(llvmObj));
    return new llvm::LoadInst(ref, "", llvmObj->block);
}
llvm::Value* DynamicArrayType::llvmGepSize(LlvmObject* llvmObj, llvm::Value* llvmRef) {
    vector<llvm::Value*> indexList;
    indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
    indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvmObj->context), 0));
    return llvm::GetElementPtrInst::Create(
        ((llvm::PointerType*)llvmRef->getType())->getElementType(), llvmRef, indexList, "", llvmObj->block
    );
}
llvm::Value* DynamicArrayType::llvmGepCapacity(LlvmObject* llvmObj, llvm::Value* llvmRef) {
    vector<llvm::Value*> indexList;
    indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
    indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvmObj->context), 1));
    return llvm::GetElementPtrInst::Create(
        ((llvm::PointerType*)llvmRef->getType())->getElementType(), llvmRef, indexList, "", llvmObj->block
    );
}
llvm::Value* DynamicArrayType::llvmGepData(LlvmObject* llvmObj, llvm::Value* llvmRef) {
    vector<llvm::Value*> indexList;
    indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
    indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvmObj->context), 2));
    return llvm::GetElementPtrInst::Create(
        ((llvm::PointerType*)llvmRef->getType())->getElementType(), llvmRef, indexList, "", llvmObj->block
    );
}
llvm::Value* DynamicArrayType::llvmExtractSize(LlvmObject* llvmObj, llvm::Value* llvmValue) {
    return llvm::ExtractValueInst::Create(llvmValue, {0}, "", llvmObj->block);
}
llvm::Value* DynamicArrayType::llvmExtractCapacity(LlvmObject* llvmObj, llvm::Value* llvmValue) {
    return llvm::ExtractValueInst::Create(llvmValue, {1}, "", llvmObj->block);
}
llvm::Value* DynamicArrayType::llvmExtractData(LlvmObject* llvmObj, llvm::Value* llvmValue) {
    return llvm::ExtractValueInst::Create(llvmValue, {2}, "", llvmObj->block);
}
llvm::Value* DynamicArrayType::llvmGepDataElement(LlvmObject* llvmObj, llvm::Value* data, llvm::Value* index) {
    return llvm::GetElementPtrInst::Create(((llvm::PointerType*)data->getType())->getElementType(), data, {index}, "", llvmObj->block);
}
void DynamicArrayType::llvmAllocData(LlvmObject* llvmObj, llvm::Value* llvmRef, llvm::Value* numberOfElements) {
    new llvm::StoreInst(
        new llvm::BitCastInst(
            llvm::CallInst::Create(
                llvmObj->mallocFunction, 
                llvm::BinaryOperator::CreateMul(
                    elementType->llvmTypesize(llvmObj),
                    numberOfElements, 
                    "", llvmObj->block
                ),
                "", llvmObj->block
            ),
            RawPointerType::Create(elementType)->createLlvm(llvmObj),
            "", llvmObj->block
        ),
        llvmGepData(llvmObj, llvmRef), 
        llvmObj->block
    );
}
void DynamicArrayType::llvmReallocData(LlvmObject* llvmObj, llvm::Value* llvmRef, llvm::Value* numberOfElements) {
    auto dataRef = llvmGepData(llvmObj, llvmRef);
    new llvm::StoreInst(
        new llvm::BitCastInst(
            llvm::CallInst::Create(
                llvmObj->reallocFunction, 
                { 
                    new llvm::BitCastInst(
                        new llvm::LoadInst(dataRef, "", llvmObj->block), 
                        llvm::Type::getInt8PtrTy(llvmObj->context), 
                        "", llvmObj->block
                    ), 
                    llvm::BinaryOperator::CreateMul(
                        elementType->llvmTypesize(llvmObj),
                        numberOfElements, 
                        "", llvmObj->block
                    )
                },
                "", llvmObj->block
            ),
            RawPointerType::Create(elementType)->createLlvm(llvmObj),
            "", llvmObj->block
        ),
        dataRef, 
        llvmObj->block
    );
}
void DynamicArrayType::createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 0:
        break;
    case 1: {
        auto effType = arguments[0]->type->getEffectiveType();
        if (effType->kind == Type::Kind::StaticArray) {
            arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        } else if (effType->kind == Type::Kind::ArrayView) {
            if (Value::isLvalue(arguments[0])) {
                arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
            } else {
                arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
            }
        } else {
            arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        }
        break;
    }
    default: {
        arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        arguments[1]->createAllocaLlvmIfNeededForValue(llvmObj);
        vector<Value*> constructorArgs(arguments.begin()+2, arguments.end());
        if (elementType->hasLlvmConstructor(llvmObj, constructorArgs, classConstructor)) {
            elementType->createAllocaIfNeededForConstructor(llvmObj, constructorArgs, classConstructor);
        }
        break;
    }
    }
}
void DynamicArrayType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    const int DEFAULT_CAPACITY = 50;
    switch (arguments.size()) {
    case 0: {
        new llvm::StoreInst(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), llvmGepSize(llvmObj, leftLlvmRef), llvmObj->block);
        new llvm::StoreInst(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), DEFAULT_CAPACITY), llvmGepCapacity(llvmObj, leftLlvmRef), llvmObj->block);
        llvmAllocData(llvmObj, leftLlvmRef, llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), DEFAULT_CAPACITY));
        break;
    }
    case 1: {
        auto effType = arguments[0]->type->getEffectiveType();
        if (effType->kind == Type::Kind::StaticArray) {
            auto staticArrayType = (StaticArrayType*)effType;
            auto capacityInt = max(staticArrayType->sizeAsInt, (int64_t)DEFAULT_CAPACITY);
            llvmStore(llvmObj, llvmInt(llvmObj, staticArrayType->sizeAsInt), llvmGepSize(llvmObj, leftLlvmRef));
            llvmStore(llvmObj, llvmInt(llvmObj, capacityInt), llvmGepCapacity(llvmObj, leftLlvmRef));
            llvmAllocData(llvmObj, leftLlvmRef, llvmInt(llvmObj, capacityInt));
            auto data = llvmLoad(llvmObj, llvmGepData(llvmObj, leftLlvmRef));
            auto argRef = arguments[0]->getReferenceLlvm(llvmObj);
            vector<llvm::Value*> indexList;
            indexList.push_back(llvmInt(llvmObj, 0));
            indexList.push_back(llvmInt(llvmObj, 0));
            if (staticArrayType->sizeAsInt <= 50) {
                for (int i = 0; i < staticArrayType->sizeAsInt; ++i) {
                    auto gepDynamic = llvmGepDataElement(llvmObj, data, llvmInt(llvmObj, i));
                    indexList[1] = llvmInt(llvmObj, i);
                    auto gepStatic = llvm::GetElementPtrInst::Create(
                        ((llvm::PointerType*)argRef->getType())->getElementType(), argRef, indexList, "", llvmObj->block
                    );
                    if (Value::isLvalue(arguments[0])) {
                        if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray || elementType->kind == Type::Kind::DynamicArray || elementType->kind == Type::Kind::MaybeError) {
                            elementType->createLlvmCopyConstructor(llvmObj, gepDynamic, gepStatic);
                        } else {
                            elementType->createLlvmCopyConstructor(llvmObj, gepDynamic, llvmLoad(llvmObj, gepStatic));
                        } 
                    } else {
                        if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray || elementType->kind == Type::Kind::DynamicArray || elementType->kind == Type::Kind::MaybeError) {
                            elementType->createLlvmMoveConstructor(llvmObj, gepDynamic, gepStatic);
                        } else {
                            elementType->createLlvmMoveConstructor(llvmObj, gepDynamic, llvmLoad(llvmObj, gepStatic));
                        }
                    }
                }
            } else {
                createLlvmForEachLoop(llvmObj, llvmInt(llvmObj, staticArrayType->sizeAsInt), [&](llvm::Value* index) {
                    auto indexValue = llvmLoad(llvmObj, index);
                    auto gepDynamic = llvmGepDataElement(llvmObj, data, indexValue);
                    indexList[1] = indexValue;
                    auto gepStatic = llvm::GetElementPtrInst::Create(
                        ((llvm::PointerType*)argRef->getType())->getElementType(), argRef, indexList, "", llvmObj->block
                    );
                    if (Value::isLvalue(arguments[0])) {
                        if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray || elementType->kind == Type::Kind::DynamicArray || elementType->kind == Type::Kind::MaybeError) {
                            elementType->createLlvmCopyConstructor(llvmObj, gepDynamic, gepStatic);
                        } else {
                            elementType->createLlvmCopyConstructor(llvmObj, gepDynamic, llvmLoad(llvmObj, gepStatic));
                        } 
                    } else {
                        if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray || elementType->kind == Type::Kind::DynamicArray || elementType->kind == Type::Kind::MaybeError) {
                            elementType->createLlvmMoveConstructor(llvmObj, gepDynamic, gepStatic);
                        } else {
                            elementType->createLlvmMoveConstructor(llvmObj, gepDynamic, llvmLoad(llvmObj, gepStatic));
                        }
                    }
                });
            }
        } else if (effType->kind == Type::Kind::ArrayView) {
            auto arrayViewType = (ArrayViewType*)effType;
            llvm::Value* viewSize;
            llvm::Value* viewData;
            if (Value::isLvalue(arguments[0])) {
                auto viewRef = arguments[0]->getReferenceLlvm(llvmObj);
                viewSize = llvmLoad(llvmObj, arrayViewType->llvmGepSize(llvmObj, viewRef));
                viewData = llvmLoad(llvmObj, arrayViewType->llvmGepData(llvmObj, viewRef));
            } else {
                auto viewVal = arguments[0]->createLlvm(llvmObj);
                viewSize = arrayViewType->llvmExtractSize(llvmObj, viewVal);
                viewData = arrayViewType->llvmExtractData(llvmObj, viewVal);
            }
            llvmStore(llvmObj, viewSize, llvmGepSize(llvmObj, leftLlvmRef));
            createLlvmConditional(llvmObj, new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLT, viewSize, llvmInt(llvmObj, 50), ""), 
                [&]() {
                    llvmStore(llvmObj, llvmInt(llvmObj, 50), llvmGepCapacity(llvmObj, leftLlvmRef));
                    llvmAllocData(llvmObj, leftLlvmRef, llvmInt(llvmObj, 50));
                },
                [&]() {
                    llvmStore(llvmObj, viewSize, llvmGepCapacity(llvmObj, leftLlvmRef));
                    llvmAllocData(llvmObj, leftLlvmRef, viewSize);
                }
            );
            auto data = llvmLoad(llvmObj, llvmGepData(llvmObj, leftLlvmRef));
            createLlvmForEachLoop(llvmObj, viewSize, [&](llvm::Value* index) {
                auto indexValue = llvmLoad(llvmObj, index);
                auto dynamicGep = llvmGepDataElement(llvmObj, data, indexValue);
                auto viewGep = arrayViewType->llvmGepDataElement(llvmObj, viewData, indexValue);
                if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray || elementType->kind == Type::Kind::DynamicArray || elementType->kind == Type::Kind::MaybeError) {
                    elementType->createLlvmCopyConstructor(llvmObj, dynamicGep, viewGep);
                } else {
                    elementType->createLlvmCopyConstructor(llvmObj, dynamicGep, llvmLoad(llvmObj, viewGep));
                } 
            });
        } else {
            auto capacity = arguments[0]->createLlvm(llvmObj);
            new llvm::StoreInst(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), llvmGepSize(llvmObj, leftLlvmRef), llvmObj->block);
            new llvm::StoreInst(capacity, llvmGepCapacity(llvmObj, leftLlvmRef), llvmObj->block);
            llvmAllocData(llvmObj, leftLlvmRef, capacity);
        }
        break;
    }
    default: {
        auto capacity = arguments[0]->createLlvm(llvmObj);
        auto size     = arguments[1]->createLlvm(llvmObj);
        new llvm::StoreInst(size, llvmGepSize(llvmObj, leftLlvmRef), llvmObj->block);
        new llvm::StoreInst(capacity, llvmGepCapacity(llvmObj, leftLlvmRef), llvmObj->block);
        llvmAllocData(llvmObj, leftLlvmRef, capacity);

        vector<Value*> constructorArgs(arguments.begin()+2, arguments.end());
        if (elementType->hasLlvmConstructor(llvmObj, constructorArgs, classConstructor)) {
            auto data = new llvm::LoadInst(llvmGepData(llvmObj, leftLlvmRef), "", llvmObj->block);
            createLlvmForEachLoop(llvmObj, size, [&](llvm::Value* index) {
                auto leftGepRef = llvmGepDataElement(llvmObj, data, new llvm::LoadInst(index, "", llvmObj->block));
                elementType->createLlvmConstructor(llvmObj, leftGepRef, constructorArgs, classConstructor);
            });
        }
        break;
    }
    }
}
void DynamicArrayType::createLlvmCopyConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    auto rightSize = new llvm::LoadInst(llvmGepSize(llvmObj, rightLlvmValue), "", llvmObj->block);
    new llvm::StoreInst(rightSize, llvmGepSize(llvmObj, leftLlvmRef), llvmObj->block);
    auto rightCapacity = new llvm::LoadInst(llvmGepCapacity(llvmObj, rightLlvmValue), "", llvmObj->block);
    new llvm::StoreInst(rightCapacity, llvmGepCapacity(llvmObj, leftLlvmRef), llvmObj->block);
    llvmAllocData(llvmObj, leftLlvmRef, rightCapacity);
    auto leftData = new llvm::LoadInst(llvmGepData(llvmObj, leftLlvmRef), "", llvmObj->block);
    auto rightData = new llvm::LoadInst(llvmGepData(llvmObj, rightLlvmValue), "", llvmObj->block);
    createLlvmForEachLoop(llvmObj, rightSize, [&](llvm::Value* index) {
        auto indexValue = new llvm::LoadInst(index, "", llvmObj->block);
        auto leftGepRef = llvmGepDataElement(llvmObj, leftData, indexValue);
        auto rightGepRef = llvmGepDataElement(llvmObj, rightData, indexValue);
        if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray || elementType->kind == Type::Kind::DynamicArray || elementType->kind == Type::Kind::MaybeError) {
            elementType->createLlvmCopyConstructor(llvmObj, leftGepRef, rightGepRef);
        } else {
            elementType->createLlvmCopyConstructor(llvmObj, leftGepRef, new llvm::LoadInst(rightGepRef, "", llvmObj->block));
        } 
    });
}
void DynamicArrayType::createLlvmMoveConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    new llvm::StoreInst(new llvm::LoadInst(rightLlvmValue, "", llvmObj->block), leftLlvmRef, llvmObj->block);
}
void DynamicArrayType::createLlvmCopyAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue) {
    auto leftCapacity  = new llvm::LoadInst(llvmGepCapacity(llvmObj, leftLlvmRef),    "", llvmObj->block);
    auto rightCapacity = new llvm::LoadInst(llvmGepCapacity(llvmObj, rightLlvmValue), "", llvmObj->block);
    createLlvmConditional(llvmObj, new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLT, leftCapacity, rightCapacity, ""), 
        [&]() {
            llvmReallocData(llvmObj, leftLlvmRef, rightCapacity);
            new llvm::StoreInst(rightCapacity, llvmGepCapacity(llvmObj, leftLlvmRef), llvmObj->block);
        }
    );
    auto leftSize  = new llvm::LoadInst(llvmGepSize(llvmObj, leftLlvmRef),    "", llvmObj->block);
    auto rightSize = new llvm::LoadInst(llvmGepSize(llvmObj, rightLlvmValue), "", llvmObj->block); 
    auto leftData = new llvm::LoadInst(llvmGepData(llvmObj, leftLlvmRef), "", llvmObj->block);
    auto rightData = new llvm::LoadInst(llvmGepData(llvmObj, rightLlvmValue), "", llvmObj->block);
    createLlvmConditional(llvmObj, new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLT, leftSize, rightSize, ""), 
        [&]() {
            createLlvmForEachLoop(llvmObj, leftSize, [&](llvm::Value* index) {
                auto indexValue = new llvm::LoadInst(index, "", llvmObj->block);
                auto leftGepRef = llvmGepDataElement(llvmObj, leftData, indexValue);
                auto rightGepRef = llvmGepDataElement(llvmObj, rightData, indexValue);
                if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray || elementType->kind == Type::Kind::DynamicArray || elementType->kind == Type::Kind::MaybeError) {
                    elementType->createLlvmCopyAssignment(llvmObj, leftGepRef, rightGepRef);
                } else {
                    elementType->createLlvmCopyAssignment(llvmObj, leftGepRef, new llvm::LoadInst(rightGepRef, "", llvmObj->block));
                } 
            });
            createLlvmForEachLoop(llvmObj, leftSize, rightSize, false, [&](llvm::Value* index) {
                auto indexValue = new llvm::LoadInst(index, "", llvmObj->block);
                auto leftGepRef = llvmGepDataElement(llvmObj, leftData, indexValue);
                auto rightGepRef = llvmGepDataElement(llvmObj, rightData, indexValue);
                if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray || elementType->kind == Type::Kind::DynamicArray || elementType->kind == Type::Kind::MaybeError) {
                    elementType->createLlvmCopyConstructor(llvmObj, leftGepRef, rightGepRef);
                } else {
                    elementType->createLlvmCopyConstructor(llvmObj, leftGepRef, new llvm::LoadInst(rightGepRef, "", llvmObj->block));
                } 
            });
        },
        [&]() {
            createLlvmForEachLoop(llvmObj, rightSize, [&](llvm::Value* index) {
                auto indexValue = new llvm::LoadInst(index, "", llvmObj->block);
                auto leftGepRef = llvmGepDataElement(llvmObj, leftData, indexValue);
                auto rightGepRef = llvmGepDataElement(llvmObj, rightData, indexValue);
                if (elementType->kind == Type::Kind::Class || elementType->kind == Type::Kind::StaticArray || elementType->kind == Type::Kind::DynamicArray || elementType->kind == Type::Kind::MaybeError) {
                    elementType->createLlvmCopyAssignment(llvmObj, leftGepRef, rightGepRef);
                } else {
                    elementType->createLlvmCopyAssignment(llvmObj, leftGepRef, new llvm::LoadInst(rightGepRef, "", llvmObj->block));
                } 
            });
        }
    );
}
void DynamicArrayType::createLlvmDestructorValue(LlvmObject* llvmObj, llvm::Value* llvmValue) {
    auto data = llvmExtractData(llvmObj, llvmValue);
    if (elementType->needsDestruction()) {
        auto size = llvmExtractSize(llvmObj, llvmValue);
        createLlvmForEachLoop(llvmObj, size, [&](llvm::Value* index) {
            auto leftGepRef = llvmGepDataElement(llvmObj, data, llvmLoad(llvmObj, index));
            elementType->createLlvmDestructorRef(llvmObj, leftGepRef);
        });
    }
    llvm::CallInst::Create(
        llvmObj->freeFunction, 
        new llvm::BitCastInst(
            data, 
            llvm::Type::getInt8PtrTy(llvmObj->context), "", llvmObj->block
        ), 
        "", llvmObj->block
    );
}
void DynamicArrayType::createLlvmDestructorRef(LlvmObject* llvmObj, llvm::Value* llvmRef) {
    auto data = new llvm::LoadInst(llvmGepData(llvmObj, llvmRef), "", llvmObj->block);
    if (elementType->needsDestruction()) {
        auto sizeValue = new llvm::LoadInst(llvmGepSize(llvmObj, llvmRef), "", llvmObj->block);
        createLlvmForEachLoop(llvmObj, sizeValue, [&](llvm::Value* index) {
            auto leftGepRef = llvmGepDataElement(llvmObj, data, new llvm::LoadInst(index, "", llvmObj->block));
            elementType->createLlvmDestructorRef(llvmObj, leftGepRef);
        });
    }
    llvm::CallInst::Create(
        llvmObj->freeFunction, 
        new llvm::BitCastInst(
            data, 
            llvm::Type::getInt8PtrTy(llvmObj->context), "", llvmObj->block
        ), 
        "", llvmObj->block
    );
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
Type* ArrayViewType::changeClassToTemplate(const vector<TemplateType*> templateTypes) {
    elementType = elementType->changeClassToTemplate(templateTypes);
    return this;
}
Type* ArrayViewType::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    return Create(elementType->templateCopy(parentScope, templateToType));
}
bool ArrayViewType::interpret(Scope* scope) {
    return elementType->interpret(scope);
}
bool ArrayViewType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const ArrayViewType&>(type);
        return cmpPtr(this->elementType, other.elementType);
    } else {
        return false;
    }
}
bool ArrayViewType::isTemplate() {
    return elementType->isTemplate();
}
MatchTemplateResult ArrayViewType::matchTemplate(TemplateFunctionType* templateFunctionType, Type* type) {
    if (kind == type->kind) {
        return elementType->matchTemplate(templateFunctionType, ((ArrayViewType*)type)->elementType);
    }
    MatchTemplateResult matchResult = MatchTemplateResult::Fail;
    if (type->kind == Type::Kind::DynamicArray) {
        matchResult = elementType->matchTemplate(templateFunctionType, ((DynamicArrayType*)type)->elementType);
    } else if (type->kind == Type::Kind::StaticArray) {
        matchResult = elementType->matchTemplate(templateFunctionType, ((StaticArrayType*)type)->elementType);
    }
    if (matchResult == MatchTemplateResult::Perfect) {
        return MatchTemplateResult::Viable;
    } else {
        return MatchTemplateResult::Fail;
    }
}
Type* ArrayViewType::substituteTemplate(TemplateFunctionType* templateFunctionType) {
    return Create(elementType->substituteTemplate(templateFunctionType));
}
int ArrayViewType::compareTemplateDepth(Type* type) {
    if (type->kind != Type::Kind::ArrayView) return -1;
    return elementType->compareTemplateDepth(((ArrayViewType*)type)->elementType);
}
optional<InterpretConstructorResult> ArrayViewType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    switch (arguments.size()) {
    case 0:
        return InterpretConstructorResult(nullptr, nullptr);
    case 1: {
        auto argEffType = arguments[0]->type->getEffectiveType();
        if (argEffType->kind == Type::Kind::StaticArray && cmpPtr(elementType, ((StaticArrayType*)argEffType)->elementType)) {
            return InterpretConstructorResult(nullptr, nullptr);
        }
        if (argEffType->kind == Type::Kind::DynamicArray && cmpPtr(elementType, ((DynamicArrayType*)argEffType)->elementType)) {
            return InterpretConstructorResult(nullptr, nullptr);
        }
        if (!onlyTry) errorMessageOpt("no fitting array view constructor (argument is not of array type)", position);
        return nullopt;
    }
    default: 
        if (!onlyTry) errorMessageOpt("no fitting array view constructor (>1 argument)", position);
        return nullopt;
    }
}
/*unique_ptr<Type> ArrayViewType::copy() {
    return make_unique<ArrayViewType>(this->elementType->copy());
}*/
llvm::Type* ArrayViewType::createLlvm(LlvmObject* llvmObj) {
    if (!llvmType) {
        llvmType = llvm::StructType::get(llvmObj->context, {
            llvm::Type::getInt64Ty(llvmObj->context),
            llvm::PointerType::get(elementType->createLlvm(llvmObj), 0),
        });
    }
    return llvmType;
}
llvm::Value* ArrayViewType::createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    if (arguments.size() == 0) {
        return nullptr;
    } else if (arguments.size() == 1) {
        arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
        return nullptr;
    }
}
pair<llvm::Value*, llvm::Value*> ArrayViewType::createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref) {
    if (arguments.size() == 0) {
        return { 
            nullptr,
            llvm::ConstantStruct::get((llvm::StructType*)createLlvm(llvmObj), {
                llvmInt(llvmObj, 0),
                llvm::UndefValue::get(RawPointerType::Create(elementType)->createLlvm(llvmObj))
            }) 
        };
    } else if (arguments.size() == 1) {
        auto argEffType = arguments[0]->type->getEffectiveType();
        auto arrayPtr = arguments[0]->getReferenceLlvm(llvmObj);
        if (argEffType->kind == Type::Kind::StaticArray) {
            return {
                nullptr,
                llvm::InsertValueInst::Create(
                    llvm::ConstantStruct::get((llvm::StructType*)createLlvm(llvmObj), {
                        llvmInt(llvmObj, ((StaticArrayType*)argEffType)->sizeAsInt),
                        llvm::UndefValue::get(RawPointerType::Create(elementType)->createLlvm(llvmObj))
                    }),
                    new llvm::BitCastInst(arrayPtr, RawPointerType::Create(elementType)->createLlvm(llvmObj), "", llvmObj->block),
                    {1}, "", llvmObj->block
                )
            };
        } else if (argEffType->kind == Type::Kind::DynamicArray) {
            return {
                nullptr,
                llvm::InsertValueInst::Create(
                    llvm::InsertValueInst::Create(
                        llvm::ConstantStruct::get((llvm::StructType*)createLlvm(llvmObj), {
                            llvm::UndefValue::get(IntegerType::Create(IntegerType::Size::I64)->createLlvm(llvmObj)),
                            llvm::UndefValue::get(RawPointerType::Create(elementType)->createLlvm(llvmObj))
                        }),
                        llvmLoad(llvmObj, ((DynamicArrayType*)argEffType)->llvmGepSize(llvmObj, arrayPtr)),
                        {0}, "", llvmObj->block
                    ),
                    llvmLoad(llvmObj, ((DynamicArrayType*)argEffType)->llvmGepData(llvmObj, arrayPtr)),
                    {1}, "", llvmObj->block
                )
            };
        } else {
            internalError("incorrect array view createLlvmValue (argument is not array)");
            return {nullptr, nullptr};
        }
    } else {
        internalError("incorrect array view createLlvmValue (too many arguments)");
        return {nullptr, nullptr};
    }
}
llvm::Value* ArrayViewType::createRefForLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    createAllocaIfNeededForConstructor(llvmObj, arguments, classConstructor);
    return allocaLlvm(llvmObj);
}
llvm::Value* ArrayViewType::createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref) {
    createLlvmConstructor(llvmObj, ref, arguments, classConstructor);
    return ref;
}
llvm::Value* ArrayViewType::llvmGepSize(LlvmObject* llvmObj, llvm::Value* llvmRef) {
    vector<llvm::Value*> indexList;
    indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
    indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvmObj->context), 0));
    return llvm::GetElementPtrInst::Create(
        ((llvm::PointerType*)llvmRef->getType())->getElementType(), llvmRef, indexList, "", llvmObj->block
    );
}
llvm::Value* ArrayViewType::llvmGepData(LlvmObject* llvmObj, llvm::Value* llvmRef) {
    vector<llvm::Value*> indexList;
    indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0));
    indexList.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvmObj->context), 1));
    return llvm::GetElementPtrInst::Create(
        ((llvm::PointerType*)llvmRef->getType())->getElementType(), llvmRef, indexList, "", llvmObj->block
    );
}
llvm::Value* ArrayViewType::llvmExtractSize(LlvmObject* llvmObj, llvm::Value* llvmValue) {
    return llvm::ExtractValueInst::Create(llvmValue, {0}, "", llvmObj->block);
}
llvm::Value* ArrayViewType::llvmExtractData(LlvmObject* llvmObj, llvm::Value* llvmValue) {
    return llvm::ExtractValueInst::Create(llvmValue, {1}, "", llvmObj->block);
}
llvm::Value* ArrayViewType::llvmGepDataElement(LlvmObject* llvmObj, llvm::Value* data, llvm::Value* index) {
    return llvm::GetElementPtrInst::Create(((llvm::PointerType*)data->getType())->getElementType(), data, {index}, "", llvmObj->block);
}
void ArrayViewType::createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    if (arguments.size() > 0) {
        arguments[0]->createAllocaLlvmIfNeededForReference(llvmObj);
    }
}
void ArrayViewType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 0:
        llvmStore(llvmObj, llvmInt(llvmObj, 0), llvmGepSize(llvmObj, leftLlvmRef));
        break;
    case 1: {
        auto argEffType = arguments[0]->type->getEffectiveType();
        auto arrayPtr = arguments[0]->getReferenceLlvm(llvmObj);
        if (argEffType->kind == Type::Kind::StaticArray) {
            llvmStore(llvmObj, 
                new llvm::BitCastInst(arrayPtr, RawPointerType::Create(elementType)->createLlvm(llvmObj), "", llvmObj->block), 
                llvmGepData(llvmObj, leftLlvmRef)
            );
            llvmStore(llvmObj, 
                llvmInt(llvmObj, ((StaticArrayType*)argEffType)->sizeAsInt), 
                llvmGepSize(llvmObj, leftLlvmRef)
            );
        } else if (argEffType->kind == Type::Kind::DynamicArray) {
            llvmStore(llvmObj, 
                llvmLoad(llvmObj, ((DynamicArrayType*)argEffType)->llvmGepData(llvmObj, arrayPtr)), 
                llvmGepData(llvmObj, leftLlvmRef)
            );
            llvmStore(llvmObj, 
                llvmLoad(llvmObj, ((DynamicArrayType*)argEffType)->llvmGepSize(llvmObj, arrayPtr)),
                llvmGepSize(llvmObj, leftLlvmRef)
            );
        } else {
            internalError("incorrect array view constructor during llvm creating (argument is not array)");
        }
        break;
    }
    default: 
        internalError("incorrect array view constructor during llvm creating (>1 argument)");
        break;
    }
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
Type* ClassType::changeClassToTemplate(const vector<TemplateType*> templateTypes) {
    for (auto templateType : templateTypes) {
        if (name == templateType->name) {
            return templateType;
        }
    }
    for (auto& thisTemplateType : this->templateTypes) {
        thisTemplateType = thisTemplateType->changeClassToTemplate(templateTypes);
    }
    return this;
}
Type* ClassType::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto foundType = templateToType.find(name);
    if (foundType != templateToType.end()) {
        return foundType->second;
    } else {
        auto classType = Create(name);
        for (auto templateType : templateTypes) {
            classType->templateTypes.push_back(templateType->templateCopy(parentScope, templateToType));
        }
        return classType;
    }
}
bool ClassType::interpret(Scope* scope) {
    if (templateTypes.size() > 0) {
        for (auto templateType : templateTypes) {
            if (templateType->isTemplate()) {
                return true;
            }
        }
    }
    if (declaration) {
        return declaration->shallowInterpret(templateTypes);
    }
    declaration = scope->classDeclarationMap.getDeclaration(name);
    if (!declaration && scope->parentScope) {
        return interpret(scope->parentScope);
    }
    if (!declaration) {
        return false;
    }
    declaration = declaration->get(templateTypes);
    return declaration->shallowInterpret(templateTypes);
}
bool ClassType::operator==(const Type& type) const {
    if(typeid(type) == typeid(*this)){
        const auto& other = static_cast<const ClassType&>(type);
        if (this->declaration) {
            return this->declaration == other.declaration;
        } else {
            return this->name == other.name
                && this->declaration == other.declaration
                && this->templateTypes == other.templateTypes;
        }
    } else {
        return false;
    }
}
bool ClassType::isTemplate() {
    for (auto templateType : templateTypes) {
        if (templateType->isTemplate()) return true;
    }
    return false;
}
MatchTemplateResult ClassType::matchTemplate(TemplateFunctionType* templateFunctionType, Type* type) {
    if (kind != type->kind) return MatchTemplateResult::Viable;
    auto classType = (ClassType*)type;
    if (declaration == classType->declaration && templateTypes == classType->templateTypes) {
        return MatchTemplateResult::Perfect;
    } else if (declaration == nullptr) {
        bool isPerfect = true;
        for (int i = 0; i < templateTypes.size(); ++i) {
            auto matchArgResult = templateTypes[i]->matchTemplate(templateFunctionType, classType->templateTypes[i]);
            if (matchArgResult == MatchTemplateResult::Fail) return MatchTemplateResult::Fail;
            if (matchArgResult == MatchTemplateResult::Viable) isPerfect = false;
        }
        if (isPerfect) return MatchTemplateResult::Perfect;
        else return MatchTemplateResult::Viable;
    } 
    else {
        return MatchTemplateResult::Viable;
    }
}
Type* ClassType::substituteTemplate(TemplateFunctionType* templateFunctionType) {
    auto classType = Create(name);
    classType->declaration = declaration;
    classType->templateTypes = templateTypes;
    return classType;
}
bool ClassType::needsDestruction() {
    return declaration->body->destructor || declaration->body->inlineDestructors;
}
bool ClassType::needsReference() {
    return true;
}
optional<InterpretConstructorResult> ClassType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    if (arguments.size() > 0 && !isExplicit && !cmpPtr(arguments[0]->type->getEffectiveType(), (Type*)this)) {
        if (!onlyTry) errorMessageOpt("class constructor needs to be explicit", position);
        return nullopt;
    }
    if (declaration->body->constructors.empty()) {
        if (arguments.size() == 0) {
            return InterpretConstructorResult(nullptr, nullptr);
        } else {
            if (!onlyTry) errorMessageOpt("only default (0 argument) constructor exists, got "
                + to_string(arguments.size()) + " arguments", position
            );
            return nullopt;
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
            if(!onlyTry) errorMessageOpt(message, position);
            return nullopt;
        } else {
            vector<optional<vector<ConstructorOperation*>>> neededCtors;
            for (auto function : viableDeclarations) {
                neededCtors.push_back(vector<ConstructorOperation*>());
                auto argumentTypes = ((FunctionType*)function->type)->argumentTypes;
                for (int i = 0; i < arguments.size(); ++i) {
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
                    if (!onlyTry) return errorMessageOpt("no fitting constructor to call", position);
                    return nullopt;
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
                if (!onlyTry) return errorMessageOpt(message, position);
                return nullopt;
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
llvm::Value* ClassType::createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    return createRefForLlvmReference(llvmObj, arguments, classConstructor);
}
pair<llvm::Value*, llvm::Value*> ClassType::createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref) {
    auto llvmRef = createLlvmReference(llvmObj, arguments, classConstructor, ref);
    return {llvmRef, new llvm::LoadInst(llvmRef, "", llvmObj->block)};
}
llvm::Value* ClassType::createRefForLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    createAllocaIfNeededForConstructor(llvmObj, arguments, classConstructor);
    return allocaLlvm(llvmObj);
}
llvm::Value* ClassType::createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref) {
    createLlvmConstructor(llvmObj, ref, arguments, classConstructor);
    return ref;
}
llvm::Value* ClassType::createRefForLlvmCopy(LlvmObject* llvmObj, Value* lValue) {
    lValue->createAllocaLlvmIfNeededForReference(llvmObj);
    return allocaLlvm(llvmObj);
}
llvm::Value* ClassType::createLlvmCopy(LlvmObject* llvmObj, Value* lValue, llvm::Value* ref) {
    createLlvmCopyConstructor(llvmObj, ref, lValue->getReferenceLlvm(llvmObj));
    return new llvm::LoadInst(ref, "", llvmObj->block);
}
void ClassType::createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    if (classConstructor) {
        auto functionType = (FunctionType*)classConstructor->type;
        for (int i = 0; i < arguments.size(); ++i) {
            if (functionType->argumentTypes[i]->kind == Type::Kind::Reference) {
                arguments[i]->createAllocaLlvmIfNeededForReference(llvmObj);
            } else {
                arguments[i]->createAllocaLlvmIfNeededForValue(llvmObj);
            }
        }
    }
}
void ClassType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    if (declaration->body->inlineConstructors) {
        llvm::CallInst::Create(declaration->body->inlineConstructors->createLlvm(llvmObj), leftLlvmRef, "", llvmObj->block);
    }
    if (classConstructor) {
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
void ClassType::createLlvmDestructorValue(LlvmObject* llvmObj, llvm::Value* llvmValue) {
    internalError("value destructor on ClassType");
}
void ClassType::createLlvmDestructorRef(LlvmObject* llvmObj, llvm::Value* llvmRef) {
    if (declaration->body->destructor) {
        llvm::CallInst::Create(declaration->body->destructor->createLlvm(llvmObj), llvmRef, "", llvmObj->block);
    } 
    if (declaration->body->inlineDestructors) {
        llvm::CallInst::Create(declaration->body->inlineDestructors->createLlvm(llvmObj), llvmRef, "", llvmObj->block);
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
Type* FunctionType::changeClassToTemplate(const vector<TemplateType*> templateTypes) {
    for (auto& argumentType : argumentTypes) {
        argumentType = argumentType->changeClassToTemplate(templateTypes);
    }
    returnType = returnType->changeClassToTemplate(templateTypes);
    return this;
}
Type* FunctionType::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto funType = Create();
    for (auto argumentType : argumentTypes) {
        funType->argumentTypes.push_back(argumentType->templateCopy(parentScope, templateToType));
    }
    funType->returnType = returnType->templateCopy(parentScope, templateToType);
    return funType;
}
bool FunctionType::interpret(Scope* scope) {
    for (auto* argumentType : argumentTypes) {
        if (!argumentType->interpret(scope)) {
            return false;
        }
    }
    return returnType->interpret(scope);
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
bool FunctionType::isTemplate() {
    for (auto argumentType : argumentTypes) {
        if (argumentType->isTemplate()) return true;
    }
    return returnType->isTemplate();
}
MatchTemplateResult FunctionType::matchTemplate(TemplateFunctionType* templateFunctionType, Type* type) {
    if (kind != type->kind) return MatchTemplateResult::Viable;
    auto funType = (FunctionType*)type;
    if (argumentTypes.size() != funType->argumentTypes.size()) return MatchTemplateResult::Viable;
    bool isPerfect = true;
    for (int i = 0; i < argumentTypes.size(); ++i) {
        auto matchArgResult = argumentTypes[i]->matchTemplate(templateFunctionType, funType->argumentTypes[i]);
        if (matchArgResult == MatchTemplateResult::Fail) return MatchTemplateResult::Fail;
        if (matchArgResult == MatchTemplateResult::Viable) isPerfect = false;
    }
    auto matchReturnResult = returnType->matchTemplate(templateFunctionType, funType->returnType);
    if (matchReturnResult == MatchTemplateResult::Fail) return MatchTemplateResult::Fail;
    if (matchReturnResult == MatchTemplateResult::Viable) isPerfect = false;
    if (isPerfect) {
        return MatchTemplateResult::Perfect;
    } else {
        return MatchTemplateResult::Viable;
    }
}
Type* FunctionType::substituteTemplate(TemplateFunctionType* templateFunctionType) {
    auto funType = Create();
    for (auto argType : argumentTypes) {
        funType->argumentTypes.push_back(argType->substituteTemplate(templateFunctionType));
    }
    funType->returnType = returnType->substituteTemplate(templateFunctionType);
    return funType;
}
int FunctionType::compareTemplateDepth(Type* type) {
    if (type->kind != Type::Kind::Function) return -1;
    auto funType = (FunctionType*)type;
    int winner = 0;
    for (int i = 0; i < argumentTypes.size(); ++i) {
        int cmpResult = argumentTypes[i]->compareTemplateDepth(funType->argumentTypes[i]);
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
    if (winner == -2) return -2;
    int cmpResult = returnType->compareTemplateDepth(funType->returnType);
    if (cmpResult == -2) return -2;
    else if (cmpResult == -1) {
        if (winner == 1) return -2;
        else return -1;
    } else if (cmpResult == 1) {
        if (winner == -1) return -2;
        else return 1;
    } else {
        return 0;
    }
}
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
/*unique_ptr<Type> FunctionType::copy() {
    auto type = make_unique<FunctionType>();
    type->returnType = returnType->copy();
    for (auto& argumentType : argumentTypes) {
        type->argumentTypes.push_back(argumentType->copy());
    }
    return type;
}*/
optional<InterpretConstructorResult> FunctionType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
    switch (arguments.size()) {
    case 0: 
        return InterpretConstructorResult(nullptr, nullptr);
    case 1:
        return arguments[0];
    default: 
        if (!onlyTry) errorMessageOpt("no fitting raw pointer constructor (too many arguments)", position);
        return nullopt;
    }
}
void FunctionType::createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    if (arguments.size() > 0) {
        arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
    }
}
void FunctionType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 0: 
        break;
    case 1: 
        createLlvmCopyConstructor(llvmObj, leftLlvmRef, arguments[0]->createLlvm(llvmObj));
        break;
    default: 
        internalError("incorrect function constructor during llvm creating (> 1 argument)");
    }
}
bool FunctionType::hasLlvmConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    return arguments.size() == 1;
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
Type* IntegerType::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    return Create(size);
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
optional<InterpretConstructorResult> IntegerType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
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
        case Type::Kind::MaybeError:
            if (((MaybeErrorType*)arguments[0]->type->getEffectiveType())->underlyingType->kind == Type::Kind::Void) {
                return InterpretConstructorResult(nullptr, nullptr);
            }
        }
        if (!onlyTry) errorMessageOpt("no fitting integer constructor (bad argument type)", position);
        return nullopt;
    default: 
        if (!onlyTry) errorMessageOpt("no fitting integer constructor (too many arguments)", position);
        return nullopt;
    }
}
llvm::Value* IntegerType::createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 0:
        return nullptr;
    case 1:
        arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        return nullptr;
    default: 
        return nullptr;
    }
}
pair<llvm::Value*, llvm::Value*> IntegerType::createLlvmValue(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref) {
    switch (arguments.size()) {
    case 0:
        internalError("incorrect integer constructor value creating during llvm creating (no arguments)");
        return { nullptr, nullptr };
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
        case Type::Kind::MaybeError:
            return { nullptr, arguments[0]->createLlvm(llvmObj)};
        default:
            internalError("integer can only be constructed from integer and float values in llvm stage");
            return { nullptr, nullptr };
        }
    }
    default: 
        internalError("incorrect integer constructor value creating during llvm creating (> 1 argument)");
        return { nullptr, nullptr };
    }
}
void IntegerType::createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    if (arguments.size() > 0) {
        arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
    }
}
void IntegerType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 0: 
        break;
    case 1: 
        createLlvmCopyConstructor(llvmObj, leftLlvmRef, createLlvmValue(llvmObj, arguments, classConstructor, nullptr).second);
        break;
    default: 
        internalError("incorrect integer constructor during llvm creating (> 1 argument)");
    }
}
bool IntegerType::hasLlvmConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
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
MatchTemplateResult IntegerType::matchTemplate(TemplateFunctionType* templateFunctionType, Type* type) {
    if (kind == type->kind && size == ((IntegerType*)type)->size) return MatchTemplateResult::Perfect;
    else return MatchTemplateResult::Viable;
}
Type* IntegerType::substituteTemplate(TemplateFunctionType* templateFunctionType) {
    return this;
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
    default:
        return nullptr;
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
MatchTemplateResult FloatType::matchTemplate(TemplateFunctionType* templateFunctionType, Type* type) {
    if (kind == type->kind && size == ((FloatType*)type)->size) return MatchTemplateResult::Perfect;
    else return MatchTemplateResult::Viable;
}
Type* FloatType::substituteTemplate(TemplateFunctionType* templateFunctionType) {
    return this;
}
Type* FloatType::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    return Create(size);
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
optional<InterpretConstructorResult> FloatType::interpretConstructor(const CodePosition& position, Scope* scope, vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit) {
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
llvm::Value* FloatType::createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 0:
        return nullptr;
    case 1:
        arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
        return nullptr;
    default: 
        return nullptr;
    }
}
pair<llvm::Value*, llvm::Value*> FloatType::createLlvmValue(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref) {
    switch (arguments.size()) {
    case 0:
        internalError("incorrect float constructor value creating during llvm creating (no arguments)");
        return { nullptr, nullptr };
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
            if (((FloatType*)arg0Type)->size == size) {
                return { nullptr, arguments[0]->createLlvm(llvmObj) };
            }
            if (size == FloatType::Size::F32) {
                return { nullptr, new llvm::FPTruncInst(arguments[0]->createLlvm(llvmObj), createLlvm(llvmObj), "", llvmObj->block) };
            } else {
                return { nullptr, new llvm::FPExtInst(arguments[0]->createLlvm(llvmObj), createLlvm(llvmObj), "", llvmObj->block) };
            }
        default:
            internalError("float can only be constructed from integer and float values in llvm stage");
            return { nullptr, nullptr };
        }
    }
    default: 
        internalError("incorrect float constructor value creating during llvm creating (> 1 argument)");
        return { nullptr, nullptr };
    }
}
void FloatType::createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    if (arguments.size() > 0) {
        arguments[0]->createAllocaLlvmIfNeededForValue(llvmObj);
    }
}
void FloatType::createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const vector<Value*>& arguments, FunctionValue* classConstructor) {
    switch (arguments.size()) {
    case 0: 
        break;
    case 1: 
        createLlvmCopyConstructor(llvmObj, leftLlvmRef, createLlvmValue(llvmObj, arguments, classConstructor, nullptr).second);
        break;
    default: 
        internalError("incorrect float constructor during llvm creating (> 1 argument)");
    }
}
bool FloatType::hasLlvmConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor) {
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
    default:
        return nullptr;
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
MatchTemplateResult TemplateType::matchTemplate(TemplateFunctionType* templateFunctionType, Type* type) {
    for (int i = 0; i < templateFunctionType->templateTypes.size(); ++i) {
        if (name == templateFunctionType->templateTypes[i]->name) {
            if (templateFunctionType->implementationTypes[i]) {
                if (cmpPtr(templateFunctionType->implementationTypes[i], type)) {
                    return MatchTemplateResult::Perfect;
                } else {
                    return MatchTemplateResult::Fail;
                }
            } else {
                templateFunctionType->implementationTypes[i] = type;
                return MatchTemplateResult::Perfect;
            }
        }
    }
    return MatchTemplateResult::Fail;
}
bool TemplateType::isTemplate() {
    return true;
}
Type* TemplateType::substituteTemplate(TemplateFunctionType* templateFunctionType) {
    for (int i = 0; i < templateFunctionType->templateTypes.size(); ++i) {
        if (name == templateFunctionType->templateTypes[i]->name) {
            return templateFunctionType->implementationTypes[i];
        }
    }
    return nullptr;
}
int TemplateType::compareTemplateDepth(Type* type) {
    if (type->kind == Type::Kind::Template) return 0;
    return 1;
}
Type* TemplateType::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto foundType = templateToType.find(name);
    if (foundType != templateToType.end()) {
        return foundType->second;
    } else {
        return Create(name);
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
Type* TemplateFunctionType::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto funType = Create();
    for (auto argumentType : argumentTypes) {
        funType->argumentTypes.push_back(argumentType->templateCopy(parentScope, templateToType));
    }
    for (auto templateType : templateTypes) {
        funType->templateTypes.push_back(TemplateType::Create(templateType->name));
    }
    funType->returnType = returnType->templateCopy(parentScope, templateToType);
    return funType;
}
bool TemplateFunctionType::interpret(Scope* scope) {
    for (auto& argumentType : argumentTypes) {
        argumentType = argumentType->changeClassToTemplate(templateTypes);
        if (!argumentType->interpret(scope)) {
            return false;
        }
    }
    returnType = returnType->changeClassToTemplate(templateTypes);
    return returnType->interpret(scope);
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
FunctionValue* TemplateFunctionType::getImplementation(Scope* scope, Declaration* declaration) {
    for (auto& implementation : implementations) {
        bool allMatch = true;
        for (int i = 0; i < templateTypes.size(); ++i) {
            if (!cmpPtr(implementation.first[i], implementationTypes[i])) {
                allMatch = false;
                break;
            }
        }
        if (allMatch) {
            return implementation.second;
        }
    }
    unordered_map<string, Type*> templateToType;
    for (int i = 0; i < templateTypes.size(); ++i) {
        templateToType.insert({templateTypes[i]->name, implementationTypes[i]});
    }
    auto implementation = (FunctionValue*)declaration->value->templateCopy(scope, templateToType);
    for (auto& argument : implementation->arguments) {
        if (!argument->variable->type->interpret(scope)) {
            return nullptr;
        }
    }
    if (!implementation->type->interpret(scope)) return nullptr;
    implementations.push_back({implementationTypes, implementation});
    return implementation;
}
void TemplateFunctionType::createLlvmImplementations(LlvmObject* llvmObj, const string& name) {
    for (auto& implementation : implementations) {
        implementation.second->createLlvm(llvmObj, name);
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