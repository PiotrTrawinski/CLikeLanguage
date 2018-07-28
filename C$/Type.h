#pragma once

#include <memory>
#include <vector>
#include "operator==Utility.h"

struct Value;
struct FunctionValue;
struct Scope;

struct Type {
    enum class Kind {
        OwnerPointer,
        RawPointer,
        MaybeError,
        Reference,
        StaticArray,
        DynamicArray,
        ArrayView,
        String,
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
    virtual bool interpret(Scope* scope, bool needFullDeclaration=true);
    virtual Type* getEffectiveType();
    //virtual std::unique_ptr<Type> copy();

    static Type* getSuitingArithmeticType(Type* val1, Type* val2);

    Kind kind;

private:
    static std::vector<std::unique_ptr<Type>> objects;
};


struct OwnerPointerType : Type {
    OwnerPointerType(Type* underlyingType);
    static OwnerPointerType* Create(Type* underlyingType);
    
    virtual bool operator==(const Type& type) const;
    virtual bool interpret(Scope* scope, bool needFullDeclaration=true);
    //virtual std::unique_ptr<Type> copy();

    Type* underlyingType = nullptr;

private:
    static std::vector<std::unique_ptr<OwnerPointerType>> objects;
};
struct RawPointerType : Type {
    RawPointerType(Type* underlyingType);
    static RawPointerType* Create(Type* underlyingType);
    
    virtual bool operator==(const Type& type) const;
    virtual bool interpret(Scope* scope, bool needFullDeclaration=true);
    //virtual std::unique_ptr<Type> copy();

    Type* underlyingType = nullptr;
 
private:
    static std::vector<std::unique_ptr<RawPointerType>> objects;
};
struct MaybeErrorType : Type {
    MaybeErrorType(Type* underlyingType);
    static MaybeErrorType* Create(Type* underlyingType);
    
    virtual bool operator==(const Type& type) const;
    virtual bool interpret(Scope* scope, bool needFullDeclaration=true);
    //virtual std::unique_ptr<Type> copy();

    Type* underlyingType = nullptr;
  
private:
    static std::vector<std::unique_ptr<MaybeErrorType>> objects;
};
struct ReferenceType : Type {
    ReferenceType(Type* underlyingType);
    static ReferenceType* Create(Type* underlyingType);
    
    virtual bool operator==(const Type& type) const;
    virtual bool interpret(Scope* scope, bool needFullDeclaration=true);
    Type* getEffectiveType();
    //virtual std::unique_ptr<Type> copy();

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
    virtual bool interpret(Scope* scope, bool needFullDeclaration=true);
    //virtual std::unique_ptr<Type> copy();

    Type* elementType = nullptr;
    Value* size = nullptr;
    int64_t sizeAsInt = -1;
    
private:
    static std::vector<std::unique_ptr<StaticArrayType>> objects;
};
struct DynamicArrayType : Type {
    DynamicArrayType(Type* elementType);
    static DynamicArrayType* Create(Type* elementType);
    
    virtual bool operator==(const Type& type) const;
    virtual bool interpret(Scope* scope, bool needFullDeclaration=true);
    //virtual std::unique_ptr<Type> copy();

    Type* elementType = nullptr;

private:
    static std::vector<std::unique_ptr<DynamicArrayType>> objects;
};
struct ArrayViewType : Type {
    ArrayViewType(Type* elementType);
    static ArrayViewType* Create(Type* elementType);
    
    virtual bool operator==(const Type& type) const;
    virtual bool interpret(Scope* scope, bool needFullDeclaration=true);
    //virtual std::unique_ptr<Type> copy();

    Type* elementType = nullptr;
    
private:
    static std::vector<std::unique_ptr<ArrayViewType>> objects;
};
struct ClassDeclaration;
struct ClassType : Type {
    ClassType(const std::string& name);
    static ClassType* Create(const std::string& name);
    
    virtual bool operator==(const Type& type) const;
    virtual bool interpret(Scope* scope, bool needFullDeclaration=true);
    //virtual std::unique_ptr<Type> copy();

    std::string name;
    ClassDeclaration* declaration;
    std::vector<Type*> templateTypes;
    
private:
    static std::vector<std::unique_ptr<ClassType>> objects;
};
struct FunctionType : Type {
    FunctionType();
    static FunctionType* Create();
    
    virtual bool operator==(const Type& type) const;
    virtual bool interpret(Scope* scope, bool needFullDeclaration=true);
    //virtual std::unique_ptr<Type> copy();

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
    virtual bool operator==(const Type& type) const;
    //virtual std::unique_ptr<Type> copy();

    Size size;
    
private:
    static std::vector<std::unique_ptr<IntegerType>> objects;
};
struct FloatType : Type {
    enum class Size { F32, F64 };

    FloatType(Size size);
    static FloatType* Create(Size size);
    
    virtual bool operator==(const Type& type) const;
    //virtual std::unique_ptr<Type> copy();

    Size size;
    
private:
    static std::vector<std::unique_ptr<FloatType>> objects;
};
struct TemplateType : Type {
    TemplateType(const std::string& name);
    static TemplateType* Create(const std::string& name);
    
    virtual bool operator==(const Type& type) const;
    //virtual std::unique_ptr<Type> copy();

    std::string name;

private:
    static std::vector<std::unique_ptr<TemplateType>> objects;
};

struct TemplateFunctionType : FunctionType {
    TemplateFunctionType();
    static TemplateFunctionType* Create();
    
    virtual bool operator==(const Type& type) const;
    //virtual std::unique_ptr<Type> copy();

    std::vector<TemplateType*> templateTypes;
    std::vector<FunctionValue*> implementations; 
    
private:
    static std::vector<std::unique_ptr<TemplateFunctionType>> objects;
};