#pragma once

#include <memory>
#include <vector>

struct Value;
struct FunctionValue;

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

    Type (Kind kind);
    virtual bool operator==(const Type& type) const;
    virtual std::unique_ptr<Type> copy();

    Kind kind;
};


struct OwnerPointerType : Type {
    OwnerPointerType(std::unique_ptr<Type>&& underlyingType);
    virtual bool operator==(const Type& type) const;
    virtual std::unique_ptr<Type> copy();

    std::unique_ptr<Type> underlyingType;
};
struct RawPointerType : Type {
    RawPointerType(std::unique_ptr<Type>&& underlyingType);
    virtual bool operator==(const Type& type) const;
    virtual std::unique_ptr<Type> copy();

    std::unique_ptr<Type> underlyingType;
};
struct MaybeErrorType : Type {
    MaybeErrorType(std::unique_ptr<Type>&& underlyingType);
    virtual bool operator==(const Type& type) const;
    virtual std::unique_ptr<Type> copy();

    std::unique_ptr<Type> underlyingType;
};
struct ReferenceType : Type {
    ReferenceType(std::unique_ptr<Type>&& underlyingType);
    virtual bool operator==(const Type& type) const;
    virtual std::unique_ptr<Type> copy();

    std::unique_ptr<Type> underlyingType;
};
struct StaticArrayType : Type {
    StaticArrayType(std::unique_ptr<Type>&& elementType, std::unique_ptr<Value>&& size);
    virtual bool operator==(const Type& type) const;
    virtual std::unique_ptr<Type> copy();

    std::unique_ptr<Type> elementType;
    std::unique_ptr<Value> size;
};
struct DynamicArrayType : Type {
    DynamicArrayType(std::unique_ptr<Type>&& elementType);
    virtual bool operator==(const Type& type) const;
    virtual std::unique_ptr<Type> copy();

    std::unique_ptr<Type> elementType;
};
struct ArrayViewType : Type {
    ArrayViewType(std::unique_ptr<Type>&& elementType);
    virtual bool operator==(const Type& type) const;
    virtual std::unique_ptr<Type> copy();

    std::unique_ptr<Type> elementType;
};
struct ClassType : Type {
    ClassType(const std::string& name);
    virtual bool operator==(const Type& type) const;
    virtual std::unique_ptr<Type> copy();

    std::string name;
    std::vector<std::unique_ptr<Type>> templateTypes;
};
struct FunctionType : Type {
    FunctionType();
    virtual bool operator==(const Type& type) const;
    virtual std::unique_ptr<Type> copy();

    std::unique_ptr<Type> returnType;
    std::vector<std::unique_ptr<Type>> argumentTypes;
};
struct IntegerType : Type {
    enum class Size { I8, I16, I32, I64, U8, U16, U32, U64 };

    IntegerType(Size size);
    virtual bool operator==(const Type& type) const;
    virtual std::unique_ptr<Type> copy();

    Size size;
};
struct FloatType : Type {
    enum class Size { F32, F64 };

    FloatType(Size size);
    virtual bool operator==(const Type& type) const;
    virtual std::unique_ptr<Type> copy();

    Size size;
};
struct TemplateType : Type {
    TemplateType(const std::string& name);
    virtual bool operator==(const Type& type) const;
    virtual std::unique_ptr<Type> copy();

    std::string name;
};

struct TemplateFunctionType : FunctionType {
    TemplateFunctionType();
    virtual bool operator==(const Type& type) const;
    virtual std::unique_ptr<Type> copy();

    std::vector<std::unique_ptr<TemplateType>> templateTypes;
    std::vector<std::unique_ptr<FunctionValue>> implementations; 
};