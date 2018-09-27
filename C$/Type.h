#pragma once

#include <memory>
#include <vector>
#include <optional>
#include "CodePosition.h"
#include "LlvmObject.h"
#include "operator==Utility.h"

struct Value;
struct FunctionValue;
struct Declaration;
struct Scope;
struct TemplateType;
struct TemplateFunctionType;

struct InterpretConstructorResult {
    InterpretConstructorResult(Value* value, FunctionValue* classConstructor=nullptr, bool argWasCaptured=false) :
        value(value),
        classConstructor(classConstructor),
        argWasCaptured(argWasCaptured)
    {}
    Value* value;
    FunctionValue* classConstructor;
    bool argWasCaptured;
};
enum class MatchTemplateResult {
    Fail, Viable, Perfect
};

struct Type {
    enum class Kind {
        OwnerPointer,
        RawPointer,
        MaybeError,
        Reference,
        StaticArray,
        DynamicArray,
        ArrayView,
        Class,
        Function,
        Bool,
        Integer,
        Float,
        Void,
        Template,
        TemplateClass,
        TemplateFunction
    };
    Type(Kind kind);
    static Type* Create(Kind kind);
    virtual bool operator==(const Type& type) const;
    virtual bool isTemplate();
    virtual MatchTemplateResult matchTemplate(TemplateFunctionType* templateFunctionType, Type* type);
    virtual Type* substituteTemplate(TemplateFunctionType* templateFunctionType);
    virtual int compareTemplateDepth(Type* type);
    virtual Type* changeClassToTemplate(const std::vector<TemplateType*> templateTypes);
    virtual Type* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool interpret(Scope* scope);
    virtual Type* getEffectiveType();
    virtual Value* typesize(Scope* scope);
    virtual int sizeInBytes();
    virtual bool needsDestruction();
    virtual bool needsReference();
    virtual std::optional<std::pair<Type*, FunctionValue*>> interpretFunction(const CodePosition& position, Scope* scope, const std::string functionName, std::vector<Value*> arguments);
    virtual llvm::Value* createFunctionLlvmReference(const std::string functionName, LlvmObject* llvmObj, llvm::Value* llvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual std::pair<llvm::Value*, llvm::Value*> createFunctionLlvmValue(const std::string functionName, LlvmObject* llvmObj, llvm::Value* llvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual std::optional<InterpretConstructorResult> interpretConstructor(const CodePosition& position, Scope* scope, std::vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit);
    virtual llvm::Type* createLlvm(LlvmObject* llvmObj);
    virtual llvm::AllocaInst* allocaLlvm(LlvmObject* llvmObj, const std::string& name="");
    virtual llvm::Value* createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual std::pair<llvm::Value*, llvm::Value*> createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref);
    virtual llvm::Value* createRefForLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual llvm::Value* createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref);
    virtual llvm::Value* createRefForLlvmCopy(LlvmObject* llvmObj, Value* lValue);
    virtual llvm::Value* createLlvmCopy(LlvmObject* llvmObj, Value* lValue, llvm::Value* ref);
    virtual void createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual bool hasLlvmConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createAllocaIfNeededForAssignment(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmCopyConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmMoveConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmCopyAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmMoveAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmDestructorValue(LlvmObject* llvmObj, llvm::Value* llvmValue);
    virtual void createLlvmDestructorRef(LlvmObject* llvmObj, llvm::Value* llvmRef);

    static Type* getSuitingArithmeticType(Type* val1, Type* val2);

    Kind kind;

private:
    static std::vector<std::unique_ptr<Type>> objects;
};


struct OwnerPointerType : Type {
    OwnerPointerType(Type* underlyingType);
    static OwnerPointerType* Create(Type* underlyingType);
    
    virtual bool operator==(const Type& type) const;
    virtual bool isTemplate();
    virtual MatchTemplateResult matchTemplate(TemplateFunctionType* templateFunctionType, Type* type);
    virtual Type* substituteTemplate(TemplateFunctionType* templateFunctionType);
    virtual int compareTemplateDepth(Type* type);
    virtual Type* changeClassToTemplate(const std::vector<TemplateType*> templateTypes);
    virtual Type* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool interpret(Scope* scope);
    virtual int sizeInBytes();
    virtual bool needsDestruction();
    virtual std::optional<InterpretConstructorResult> interpretConstructor(const CodePosition& position, Scope* scope, std::vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit);
    virtual llvm::Type* createLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* createRefForLlvmCopy(LlvmObject* llvmObj, Value* lValue);
    virtual llvm::Value* createLlvmCopy(LlvmObject* llvmObj, Value* lValue, llvm::Value* ref);
    virtual void createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createAllocaIfNeededForAssignment(LlvmObject* llvmObj, const std::vector<Value*>& arguments);
    virtual void createLlvmAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmCopyConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmCopyAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmDestructorValue(LlvmObject* llvmObj, llvm::Value* llvmValue);
    virtual void createLlvmDestructorRef(LlvmObject* llvmObj, llvm::Value* llvmRef);

    Type* underlyingType = nullptr;

private:
    static std::vector<std::unique_ptr<OwnerPointerType>> objects;
};
struct RawPointerType : Type {
    RawPointerType(Type* underlyingType);
    static RawPointerType* Create(Type* underlyingType);
    
    virtual bool operator==(const Type& type) const;
    virtual bool isTemplate();
    virtual MatchTemplateResult matchTemplate(TemplateFunctionType* templateFunctionType, Type* type);
    virtual Type* substituteTemplate(TemplateFunctionType* templateFunctionType);
    virtual int compareTemplateDepth(Type* type);
    virtual Type* changeClassToTemplate(const std::vector<TemplateType*> templateTypes);
    virtual Type* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool interpret(Scope* scope);
    virtual int sizeInBytes();
    virtual std::optional<InterpretConstructorResult> interpretConstructor(const CodePosition& position, Scope* scope, std::vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit);
    virtual void createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual bool hasLlvmConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual llvm::Type* createLlvm(LlvmObject* llvmObj);

    Type* underlyingType = nullptr;
 
private:
    static std::vector<std::unique_ptr<RawPointerType>> objects;
};
struct MaybeErrorType : Type {
    MaybeErrorType(Type* underlyingType);
    static MaybeErrorType* Create(Type* underlyingType);
    
    virtual bool operator==(const Type& type) const;
    virtual bool isTemplate();
    virtual MatchTemplateResult matchTemplate(TemplateFunctionType* templateFunctionType, Type* type);
    virtual Type* substituteTemplate(TemplateFunctionType* templateFunctionType);
    virtual int compareTemplateDepth(Type* type);
    virtual Type* changeClassToTemplate(const std::vector<TemplateType*> templateTypes);
    virtual Type* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool interpret(Scope* scope);
    virtual int sizeInBytes();
    virtual bool needsDestruction();
    virtual bool needsReference();
    virtual std::optional<InterpretConstructorResult> interpretConstructor(const CodePosition& position, Scope* scope, std::vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit);
    virtual llvm::Type* createLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual std::pair<llvm::Value*, llvm::Value*> createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref);
    virtual llvm::Value* createRefForLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual llvm::Value* createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref);
    virtual llvm::Value* createRefForLlvmCopy(LlvmObject* llvmObj, Value* lValue);
    virtual llvm::Value* createLlvmCopy(LlvmObject* llvmObj, Value* lValue, llvm::Value* ref);
    llvm::Value* llvmGepError(LlvmObject* llvmObj, llvm::Value* llvmRef);
    llvm::Value* llvmGepValue(LlvmObject* llvmObj, llvm::Value* llvmRef);
    llvm::Value* llvmExtractError(LlvmObject* llvmObj, llvm::Value* llvmValue);
    llvm::Value* llvmExtractValue(LlvmObject* llvmObj, llvm::Value* llvmValue);
    llvm::Value* llvmInsertError(LlvmObject* llvmObj, llvm::Value* llvmValue, llvm::Value* toInsert);
    llvm::Value* llvmInsertValue(LlvmObject* llvmObj, llvm::Value* llvmValue, llvm::Value* toInsert);
    virtual void createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmCopyConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmCopyAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmDestructorValue(LlvmObject* llvmObj, llvm::Value* llvmValue);
    virtual void createLlvmDestructorRef(LlvmObject* llvmObj, llvm::Value* llvmRef);

    Type* underlyingType = nullptr;
    llvm::Type* llvmType = nullptr;
  
private:
    static std::vector<std::unique_ptr<MaybeErrorType>> objects;
};
struct ReferenceType : Type {
    ReferenceType(Type* underlyingType);
    static ReferenceType* Create(Type* underlyingType);
    
    virtual bool operator==(const Type& type) const;
    virtual bool isTemplate();
    virtual MatchTemplateResult matchTemplate(TemplateFunctionType* templateFunctionType, Type* type);
    virtual Type* substituteTemplate(TemplateFunctionType* templateFunctionType);
    virtual int compareTemplateDepth(Type* type);
    virtual Type* changeClassToTemplate(const std::vector<TemplateType*> templateTypes);
    virtual Type* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool interpret(Scope* scope);
    Type* getEffectiveType();
    virtual int sizeInBytes();
    virtual std::optional<InterpretConstructorResult> interpretConstructor(const CodePosition& position, Scope* scope, std::vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit);
    virtual llvm::Type* createLlvm(LlvmObject* llvmObj);

    Type* underlyingType = nullptr;
    
private:
    static std::vector<std::unique_ptr<ReferenceType>> objects;
};
struct StaticArrayType : Type {
    StaticArrayType(Type* elementType, Value* size);
    StaticArrayType(Type* elementType, int64_t sizeAsInt);
    static StaticArrayType* Create(Type* elementType, Value* size);
    static StaticArrayType* Create(Type* elementType, int64_t sizeAsInt);
    
    virtual bool operator==(const Type& type) const;
    virtual bool isTemplate();
    virtual MatchTemplateResult matchTemplate(TemplateFunctionType* templateFunctionType, Type* type);
    virtual Type* substituteTemplate(TemplateFunctionType* templateFunctionType);
    virtual int compareTemplateDepth(Type* type);
    virtual Type* changeClassToTemplate(const std::vector<TemplateType*> templateTypes);
    virtual Type* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool interpret(Scope* scope);
    virtual int sizeInBytes();
    virtual bool needsDestruction();
    virtual bool needsReference();
    virtual std::optional<InterpretConstructorResult> interpretConstructor(const CodePosition& position, Scope* scope, std::vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit);
    virtual llvm::Value* createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual std::pair<llvm::Value*, llvm::Value*> createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref);
    virtual llvm::Value* createRefForLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual llvm::Value* createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref);
    virtual llvm::Value* createRefForLlvmCopy(LlvmObject* llvmObj, Value* lValue);
    virtual llvm::Value* createLlvmCopy(LlvmObject* llvmObj, Value* lValue, llvm::Value* ref);
    virtual void createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual bool hasLlvmConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual llvm::Type* createLlvm(LlvmObject* llvmObj);
    virtual llvm::AllocaInst* allocaLlvm(LlvmObject* llvmObj, const std::string& name="");
    virtual void createLlvmCopyConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmCopyAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmDestructorValue(LlvmObject* llvmObj, llvm::Value* llvmValue);
    virtual void createLlvmDestructorRef(LlvmObject* llvmObj, llvm::Value* llvmRef);

    Type* elementType = nullptr;
    Value* size = nullptr;
    int64_t sizeAsInt = -1;
    llvm::Value* llvmSize = nullptr;
    
private:
    static std::vector<std::unique_ptr<StaticArrayType>> objects;
};
struct DynamicArrayType : Type {
    DynamicArrayType(Type* elementType);
    static DynamicArrayType* Create(Type* elementType);
    
    virtual bool operator==(const Type& type) const;
    virtual bool isTemplate();
    virtual MatchTemplateResult matchTemplate(TemplateFunctionType* templateFunctionType, Type* type);
    virtual Type* substituteTemplate(TemplateFunctionType* templateFunctionType);
    virtual int compareTemplateDepth(Type* type);
    virtual Type* changeClassToTemplate(const std::vector<TemplateType*> templateTypes);
    virtual Type* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool interpret(Scope* scope);
    virtual int sizeInBytes();
    virtual bool needsDestruction();
    virtual std::optional<std::pair<Type*,FunctionValue*>> interpretFunction(const CodePosition& position, Scope* scope, const std::string functionName, std::vector<Value*> arguments);
    virtual llvm::Value* createFunctionLlvmReference(const std::string functionName, LlvmObject* llvmObj, llvm::Value* llvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual std::pair<llvm::Value*, llvm::Value*> createFunctionLlvmValue(const std::string functionName, LlvmObject* llvmObj, llvm::Value* llvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual std::optional<InterpretConstructorResult> interpretConstructor(const CodePosition& position, Scope* scope, std::vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit);
    virtual llvm::Type* createLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual std::pair<llvm::Value*, llvm::Value*> createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref);
    virtual llvm::Value* createRefForLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual llvm::Value* createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref);
    virtual llvm::Value* createRefForLlvmCopy(LlvmObject* llvmObj, Value* lValue);
    virtual llvm::Value* createLlvmCopy(LlvmObject* llvmObj, Value* lValue, llvm::Value* ref);
    llvm::Value* llvmGepSize(LlvmObject* llvmObj, llvm::Value* llvmRef);
    llvm::Value* llvmGepCapacity(LlvmObject* llvmObj, llvm::Value* llvmRef);
    llvm::Value* llvmGepData(LlvmObject* llvmObj, llvm::Value* llvmRef);
    llvm::Value* llvmExtractSize(LlvmObject* llvmObj, llvm::Value* llvmValue);
    llvm::Value* llvmExtractCapacity(LlvmObject* llvmObj, llvm::Value* llvmValue);
    llvm::Value* llvmExtractData(LlvmObject* llvmObj, llvm::Value* llvmValue);
    llvm::Value* llvmGepDataElement(LlvmObject* llvmObj, llvm::Value* data, llvm::Value* index);
    void llvmAllocData(LlvmObject* llvmObj, llvm::Value* llvmRef, llvm::Value* numberOfElements);
    void llvmReallocData(LlvmObject* llvmObj, llvm::Value* llvmRef, llvm::Value* numberOfElements);
    virtual void createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmCopyConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmCopyAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmDestructorValue(LlvmObject* llvmObj, llvm::Value* llvmValue);
    virtual void createLlvmDestructorRef(LlvmObject* llvmObj, llvm::Value* llvmRef);

    Type* elementType = nullptr;
    llvm::Type* llvmType = nullptr;

private:
    static std::vector<std::unique_ptr<DynamicArrayType>> objects;
};
struct ArrayViewType : Type {
    ArrayViewType(Type* elementType);
    static ArrayViewType* Create(Type* elementType);
    
    virtual bool operator==(const Type& type) const;
    virtual bool isTemplate();
    virtual MatchTemplateResult matchTemplate(TemplateFunctionType* templateFunctionType, Type* type);
    virtual Type* substituteTemplate(TemplateFunctionType* templateFunctionType);
    virtual int compareTemplateDepth(Type* type);
    virtual Type* changeClassToTemplate(const std::vector<TemplateType*> templateTypes);
    virtual Type* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool interpret(Scope* scope);
    virtual int sizeInBytes();
    virtual std::optional<InterpretConstructorResult> interpretConstructor(const CodePosition& position, Scope* scope, std::vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit);
    virtual llvm::Type* createLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual std::pair<llvm::Value*, llvm::Value*> createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref);
    virtual llvm::Value* createRefForLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual llvm::Value* createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref);
    llvm::Value* llvmGepSize(LlvmObject* llvmObj, llvm::Value* llvmRef);
    llvm::Value* llvmGepData(LlvmObject* llvmObj, llvm::Value* llvmRef);
    llvm::Value* llvmExtractSize(LlvmObject* llvmObj, llvm::Value* llvmValue);
    llvm::Value* llvmExtractData(LlvmObject* llvmObj, llvm::Value* llvmValue);
    llvm::Value* llvmGepDataElement(LlvmObject* llvmObj, llvm::Value* data, llvm::Value* index);
    virtual void createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);

    Type* elementType = nullptr;
    llvm::Type* llvmType = nullptr;

private:
    static std::vector<std::unique_ptr<ArrayViewType>> objects;
};
struct ClassDeclaration;
struct ClassType : Type {
    ClassType(const std::string& name);
    static ClassType* Create(const std::string& name);
    
    virtual bool operator==(const Type& type) const;
    virtual bool isTemplate();
    virtual MatchTemplateResult matchTemplate(TemplateFunctionType* templateFunctionType, Type* type);
    virtual Type* substituteTemplate(TemplateFunctionType* templateFunctionType);
    virtual Type* changeClassToTemplate(const std::vector<TemplateType*> templateTypes);
    virtual Type* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool interpret(Scope* scope);
    virtual int sizeInBytes();
    virtual bool needsDestruction();
    virtual bool needsReference();
    virtual std::optional<InterpretConstructorResult> interpretConstructor(const CodePosition& position, Scope* scope, std::vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit);
    virtual llvm::Type* createLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual std::pair<llvm::Value*, llvm::Value*> createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref);
    virtual llvm::Value* createRefForLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual llvm::Value* createLlvmReference(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref);
    virtual llvm::Value* createRefForLlvmCopy(LlvmObject* llvmObj, Value* lValue);
    virtual llvm::Value* createLlvmCopy(LlvmObject* llvmObj, Value* lValue, llvm::Value* ref);
    virtual void createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmCopyConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmMoveConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmCopyAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, llvm::Value* rightLlvmValue);
    virtual void createLlvmDestructorValue(LlvmObject* llvmObj, llvm::Value* llvmValue);
    virtual void createLlvmDestructorRef(LlvmObject* llvmObj, llvm::Value* llvmRef);

    std::string name;
    ClassDeclaration* declaration = nullptr;
    std::vector<Type*> templateTypes;
    
private:
    static std::vector<std::unique_ptr<ClassType>> objects;
};
struct FunctionType : Type {
    FunctionType();
    static FunctionType* Create();
    
    virtual bool operator==(const Type& type) const;
    virtual bool isTemplate();
    virtual MatchTemplateResult matchTemplate(TemplateFunctionType* templateFunctionType, Type* type);
    virtual Type* substituteTemplate(TemplateFunctionType* templateFunctionType);
    virtual int compareTemplateDepth(Type* type);
    virtual Type* changeClassToTemplate(const std::vector<TemplateType*> templateTypes);
    virtual Type* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool interpret(Scope* scope);
    virtual int sizeInBytes();
    virtual std::optional<InterpretConstructorResult> interpretConstructor(const CodePosition& position, Scope* scope, std::vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit);
    virtual llvm::Type* createLlvm(LlvmObject* llvmObj);
    virtual void createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual bool hasLlvmConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);

    Type* returnType = nullptr;
    std::vector<Type*> argumentTypes;
    
private:
    static std::vector<std::unique_ptr<FunctionType>> objects;
};
struct IntegerType : Type {
    enum class Size { I8, I16, I32, I64, U8, U16, U32, U64 };

    IntegerType(Size size);
    static IntegerType* Create(Size size);
    
    bool isSigned();
    int sizeInBytes();
    virtual bool operator==(const Type& type) const;
    virtual MatchTemplateResult matchTemplate(TemplateFunctionType* templateFunctionType, Type* type);
    virtual Type* substituteTemplate(TemplateFunctionType* templateFunctionType);
    virtual Type* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<InterpretConstructorResult> interpretConstructor(const CodePosition& position, Scope* scope, std::vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit);
    virtual llvm::Value* createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual std::pair<llvm::Value*, llvm::Value*> createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref);
    virtual void createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual bool hasLlvmConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual llvm::Type* createLlvm(LlvmObject* llvmObj);

    Size size;
    
private:
    static std::vector<std::unique_ptr<IntegerType>> objects;
};
struct FloatType : Type {
    enum class Size { F32, F64 };

    FloatType(Size size);
    static FloatType* Create(Size size);
    
    virtual bool operator==(const Type& type) const;
    virtual MatchTemplateResult matchTemplate(TemplateFunctionType* templateFunctionType, Type* type);
    virtual Type* substituteTemplate(TemplateFunctionType* templateFunctionType);
    virtual Type* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual int sizeInBytes();
    virtual std::optional<InterpretConstructorResult> interpretConstructor(const CodePosition& position, Scope* scope, std::vector<Value*>& arguments, bool onlyTry, bool parentIsAssignment, bool isExplicit);
    virtual llvm::Value* createRefForLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual std::pair<llvm::Value*, llvm::Value*> createLlvmValue(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor, llvm::Value* ref);
    virtual void createAllocaIfNeededForConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual void createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual bool hasLlvmConstructor(LlvmObject* llvmObj, const std::vector<Value*>& arguments, FunctionValue* classConstructor);
    virtual llvm::Type* createLlvm(LlvmObject* llvmObj);

    Size size;
    
private:
    static std::vector<std::unique_ptr<FloatType>> objects;
};
struct TemplateType : Type {
    TemplateType(const std::string& name);
    static TemplateType* Create(const std::string& name);
    
    virtual bool operator==(const Type& type) const;
    virtual bool isTemplate();
    virtual MatchTemplateResult matchTemplate(TemplateFunctionType* templateFunctionType, Type* type);
    virtual Type* substituteTemplate(TemplateFunctionType* templateFunctionType);
    virtual int compareTemplateDepth(Type* type);
    virtual Type* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    //virtual std::unique_ptr<Type> copy();

    std::string name;

private:
    static std::vector<std::unique_ptr<TemplateType>> objects;
};

struct TemplateFunctionType : FunctionType {
    TemplateFunctionType();
    static TemplateFunctionType* Create();
    
    virtual bool interpret(Scope* scope);
    virtual bool operator==(const Type& type) const;
    virtual Type* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    FunctionValue* getImplementation(Scope* scope, Declaration* declaration);
    void createLlvmImplementations(LlvmObject* llvmObj, const std::string& name);
    //virtual std::unique_ptr<Type> copy();

    std::vector<TemplateType*> templateTypes;
    std::vector<Type*> implementationTypes;
    std::vector<std::pair<std::vector<Type*>, FunctionValue*>> implementations; 
    
private:
    static std::vector<std::unique_ptr<TemplateFunctionType>> objects;
};