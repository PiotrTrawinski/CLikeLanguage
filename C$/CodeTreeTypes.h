#pragma once

#include <vector>
#include <memory>
#include <optional>

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
        Integer,
        Float,
        Template,
        TemplateClass,
        TemplateFunction,
        Void
    };

    Type (Kind kind) : kind(kind) {}

    Kind kind;
};

struct Statement {
    enum class Kind {
        Scope,
        Declaration,
        Assignment,
        Value
    };

    Statement(Kind kind) : kind(kind) {}

    Kind kind;
};

struct Value : Statement {
    Value(Type* type) : 
        Statement(Statement::Kind::Value), 
        type(type) 
    {}
    Type* type;
    bool isConstexpr;
};

struct Variable : Value {
    Variable(Type* type) : Value(type) {}

    std::string name;
    bool isConst;
};

struct Operation : Value {
    enum class Kind {
        Add, Sub, Mul, Div, Mod,
        Neg,
        And, Or,
        Not,
        Eq, Neq,
        Gt, Lt, Gte, Lte,
        Shl, Shr, Sal, Sar,
        Reference, Address,
        Cast,
        FunctionCall,
        Allocation,
        ErrorCoding,
        Break,
        Remove,
        Continue,
        Return
    };

    Operation(Type* returnType) : Value(returnType) {}

    std::vector<std::unique_ptr<Value>> arguments;
};


struct OwnerPointerType : Type {
    OwnerPointerType() : Type(Type::Kind::OwnerPointer) {}
    Type* underlyingType;
};
struct RawPointerType : Type {
    RawPointerType() : Type(Type::Kind::RawPointer) {}
    Type* underlyingType;
};
struct MaybeErrorType : Type {
    MaybeErrorType() : Type(Type::Kind::MaybeError) {}
    Type* underlyingType;
};
struct ReferenceType : Type {
    ReferenceType() : Type(Type::Kind::Reference) {}
    Type* underlyingType;
};
struct StaticArrayType : Type {
    StaticArrayType() : Type(Type::Kind::StaticArray) {}
    Type* elementType;
    int size;
};
struct DynamicArrayType : Type {
    DynamicArrayType() : Type(Type::Kind::DynamicArray) {}
    Type* elementType;
};
struct ArrayViewType : Type {
    ArrayViewType() : Type(Type::Kind::ArrayView) {}
    Type* elementType;
};
struct StringType : Type {
    StringType() : Type(Type::Kind::String) {}
};
struct ClassType : Type {
    ClassType() : Type(Type::Kind::Class) {}
    std::string name;
    std::vector<Variable> variables;
};
struct FunctionType : Type {
    FunctionType() : Type(Type::Kind::Function) {}
    Type* returnType;
    std::vector<Type*> argumentTypes;
};
struct IntegerType : Type {
    enum class Size { I8, I16, I32, I64 };
    IntegerType() : Type(Type::Kind::Integer) {}
    Size size;
};
struct FloatType : Type {
    enum class Size { F32, F64 };
    FloatType() : Type(Type::Kind::Float) {}
    Size size;
};
struct TemplateType : Type {
    TemplateType() : Type(Type::Kind::Template) {}
    std::string name;
};
struct TempalteClassType : ClassType {
    TempalteClassType() {
        kind = Type::Kind::TemplateClass;
    }
    std::vector<TemplateType*> templateTypes;
};
struct TemplateFunctionType : FunctionType {
    TemplateFunctionType() {
        kind = Type::Kind::TemplateFunction;
    }
    std::vector<TemplateType*> templateTypes;
};



/*
    Operations
*/
struct CastOperation : Operation {
    CastOperation(Type* argType, Type* returnType) : 
        Operation(returnType),
        argType(argType)
    {}

    Type* argType;
};
struct FunctionCallOperation : Operation {
    FunctionCallOperation(Type* returnType, const Variable& function) : 
        Operation(returnType), 
        function(function)
    {}

    Variable function;
};



struct Assignment : Statement {
    Assignment(Type* variableType) : 
        Statement(Statement::Kind::Assignment),
        variable(variableType)
    {}

    Variable variable;
    std::unique_ptr<Value> value;
};
struct Declaration : Statement {
    Declaration(Type* variableType) : 
        Statement(Statement::Kind::Declaration),
        variable(variableType)
    {}

    Variable variable;
    std::unique_ptr<Value> value;
};
struct Scope : Statement {
    enum class Owner {
        None,
        Function,
        Class,
        For,
        While,
        If,
        ElseIf,
        Else,
        Defer
    };

    Scope(Owner owner, Scope* parentScope) : 
        Statement(Statement::Kind::Scope),
        owner(owner),
        parentScope(parentScope)
    {}

    Scope* parentScope; // nullptr if and only if global scope
    Owner owner;
};

struct CodeScope : Scope {
    CodeScope(Scope::Owner owner, Scope* parentScope) : Scope(owner, parentScope) {}

    std::vector<std::unique_ptr<Statement>> statements;
};
struct ClassScope : Scope {
    ClassScope(Scope* parentScope) : Scope(Scope::Owner::Class, parentScope) {}

    std::vector<Declaration> declarations;
};

struct ForScope : CodeScope {
    ForScope(Scope* parentScope) : CodeScope(Scope::Owner::For, parentScope) {}
};
struct WhileScope : CodeScope {
    WhileScope(Scope* parentScope) : CodeScope(Scope::Owner::While, parentScope) {}
};
struct IfScope : CodeScope {
    IfScope(Scope* parentScope) : CodeScope(Scope::Owner::If, parentScope) {}
    std::unique_ptr<Value> conditionExpression;
};
struct ElseIfScope : CodeScope {
    ElseIfScope(Scope* parentScope) : CodeScope(Scope::Owner::ElseIf, parentScope) {}
    std::unique_ptr<Value> conditionExpression;
};
struct ElseScope : CodeScope {
    ElseScope(Scope* parentScope) : CodeScope(Scope::Owner::Else, parentScope) {}
};
struct DeferScope : CodeScope {
    DeferScope(Scope* parentScope) : CodeScope(Scope::Owner::Defer, parentScope) {}
};


/*
    Values
*/
struct IntegerValue : Value {
    IntegerValue(IntegerType* integerType) : Value(integerType) {
        isConstexpr = true;
    }
    int64_t value;
};
struct FloatValue : Value {
    FloatValue(FloatType* floatType) : Value(floatType) {
        isConstexpr = true;
    }
    double value;
};
struct StringValue : Value {
    StringValue(StringType* stringType) : Value(stringType) {
        isConstexpr = true;
    }
    std::string value;
};
struct StaticArrayValue : Value {
    StaticArrayValue(StaticArrayType* staticArrayType) : Value(staticArrayType) {
        isConstexpr = true;
    }
    std::vector<std::unique_ptr<Value>> values;
};
struct FunctionValue : Value {
    FunctionValue(FunctionType* functionType, Scope* parentScope) : 
        Value(functionType),
        body(Scope::Owner::Function, parentScope)
    {
        isConstexpr = true;
    }
    std::vector<Variable> arguments;
    Type* returnType;
    CodeScope body;
};
