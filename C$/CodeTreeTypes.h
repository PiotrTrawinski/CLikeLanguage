#pragma once

#include <vector>
#include <memory>
#include <optional>

#include "CodePosition.h"
#include "Token.h"

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
        TemplateFunction
    };

    Type (Kind kind) : kind(kind) {}

    Kind kind;
};

struct Statement {
    enum class Kind {
        Scope,
        Declaration,
        Value
    };

    Statement(const CodePosition& position, Kind kind) : 
        kind(kind),
        position(position)
    {}

    Kind kind;
    CodePosition position;
};

struct Value : Statement {
    Value(const CodePosition& position) : 
        Statement(position, Statement::Kind::Value)
    {}
    Type* type = nullptr;
    bool isConstexpr;
};

struct Variable : Value {
    Variable(const CodePosition& position) : Value(position) {}

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
        Assign,
        Reference, Address, GetValue,
        Cast,
        FunctionCall,
        Allocation,
        ErrorCoding,
        Break,
        Remove,
        Continue,
        Return
    };

    Operation(const CodePosition& position, Kind kind) : 
        Value(position),
        kind(kind)
    {}

    Kind kind;
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
    enum class Size { I8, I16, I32, I64, U8, U16, U32, U64 };
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
    CastOperation(const CodePosition& position, Type* argType) : 
        Operation(position, Operation::Kind::Cast),
        argType(argType)
    {}

    Type* argType;
};
struct FunctionCallOperation : Operation {
    FunctionCallOperation(const CodePosition& position, const Variable& function) : 
        Operation(position, Operation::Kind::FunctionCall), 
        function(function)
    {}

    Variable function;
};

struct Declaration : Statement {
    Declaration(const CodePosition& position) : 
        Statement(position, Statement::Kind::Declaration),
        variable(position)
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

    Scope(const CodePosition& position, Owner owner, Scope* parentScope) : 
        Statement(position, Statement::Kind::Scope),
        owner(owner),
        parentScope(parentScope)
    {}

    Scope* parentScope; // nullptr if and only if global scope
    Owner owner;
    virtual bool interpret(const std::vector<Token>& tokens, int& i)=0;
};

struct CodeScope : Scope {
    CodeScope(const CodePosition& position, Scope::Owner owner, Scope* parentScope, bool isGlobalScope=false) : 
        Scope(position, owner, parentScope),
        isGlobalScope(isGlobalScope)
    {}
    virtual bool interpret(const std::vector<Token>& tokens, int& i);

    bool isGlobalScope;
    std::vector<std::unique_ptr<Statement>> statements;
};
struct ClassScope : Scope {
    ClassScope(const CodePosition& position, Scope* parentScope) : 
        Scope(position, Scope::Owner::Class, parentScope) 
    {}
    virtual bool interpret(const std::vector<Token>& tokens, int& i);

    std::vector<Declaration> declarations;
};

struct ForScope : CodeScope {
    ForScope(const CodePosition& position, Scope* parentScope) : 
        CodeScope(position, Scope::Owner::For, parentScope) 
    {}
    virtual bool interpret(const std::vector<Token>& tokens, int& i);
};
struct WhileScope : CodeScope {
    WhileScope(const CodePosition& position, Scope* parentScope) : 
        CodeScope(position, Scope::Owner::While, parentScope) 
    {}
    virtual bool interpret(const std::vector<Token>& tokens, int& i);
};
struct IfScope : CodeScope {
    IfScope(const CodePosition& position, Scope* parentScope) :
        CodeScope(position, Scope::Owner::If, parentScope) 
    {}
    virtual bool interpret(const std::vector<Token>& tokens, int& i);
    std::unique_ptr<Value> conditionExpression;
};
struct ElseIfScope : CodeScope {
    ElseIfScope(const CodePosition& position, Scope* parentScope) : 
        CodeScope(position, Scope::Owner::ElseIf, parentScope) 
    {}
    virtual bool interpret(const std::vector<Token>& tokens, int& i);
    std::unique_ptr<Value> conditionExpression;
};
struct ElseScope : CodeScope {
    ElseScope(const CodePosition& position, Scope* parentScope) : 
        CodeScope(position, Scope::Owner::Else, parentScope) 
    {}
    virtual bool interpret(const std::vector<Token>& tokens, int& i);
};
struct DeferScope : CodeScope {
    DeferScope(const CodePosition& position, Scope* parentScope) : 
        CodeScope(position, Scope::Owner::Defer, parentScope) 
    {}
    virtual bool interpret(const std::vector<Token>& tokens, int& i);
};


/*
    Values
*/
struct IntegerValue : Value {
    IntegerValue(const CodePosition& position) : Value(position) {
        isConstexpr = true;
    }
    int64_t value;
};
struct FloatValue : Value {
    FloatValue(const CodePosition& position) : Value(position) {
        isConstexpr = true;
    }
    double value;
};
struct StringValue : Value {
    StringValue(const CodePosition& position) : Value(position) {
        isConstexpr = true;
    }
    std::string value;
};
struct StaticArrayValue : Value {
    StaticArrayValue(const CodePosition& position) : Value(position) {
        isConstexpr = true;
    }
    std::vector<std::unique_ptr<Value>> values;
};
struct FunctionValue : Value {
    FunctionValue(const CodePosition& position, Scope* parentScope) : 
        Value(position),
        body(position, Scope::Owner::Function, parentScope)
    {
        isConstexpr = true;
    }
    std::vector<Variable> arguments;
    Type* returnType;
    CodeScope body;
};
