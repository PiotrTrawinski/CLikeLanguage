#pragma once

#include <vector>
#include <memory>
#include <optional>

#include "CodePosition.h"

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

    Statement(const CodePosition& position, Kind kind) : 
        kind(kind),
        position(position)
    {}

    Kind kind;
    CodePosition position;
};

struct Value : Statement {
    Value(const CodePosition& position, Type* type) : 
        Statement(position, Statement::Kind::Value), 
        type(type) 
    {}
    Type* type;
    bool isConstexpr;
};

struct Variable : Value {
    Variable(const CodePosition& position, Type* type) : Value(position, type) {}

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

    Operation(const CodePosition& position, Type* returnType) : Value(position, returnType) {}

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
    CastOperation(const CodePosition& position, Type* argType, Type* returnType) : 
        Operation(position, returnType),
        argType(argType)
    {}

    Type* argType;
};
struct FunctionCallOperation : Operation {
    FunctionCallOperation(const CodePosition& position, Type* returnType, const Variable& function) : 
        Operation(position, returnType), 
        function(function)
    {}

    Variable function;
};



struct Assignment : Statement {
    Assignment(const CodePosition& position, Type* variableType) : 
        Statement(position, Statement::Kind::Assignment),
        variable(position, variableType)
    {}

    Variable variable;
    std::unique_ptr<Value> value;
};
struct Declaration : Statement {
    Declaration(const CodePosition& position, Type* variableType) : 
        Statement(position, Statement::Kind::Declaration),
        variable(position, variableType)
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

    static std::optional<Owner> keywordToOwner(Keyword keyword) {
        switch (keyword) {
        case Keyword::Class:  return Owner::Class;
        case Keyword::For:    return Owner::For;
        case Keyword::While:  return Owner::While;
        case Keyword::If:     return Owner::If;
        case Keyword::ElseIf: return Owner::ElseIf;
        case Keyword::Else:   return Owner::Else;
        case Keyword::Defer:  return Owner::Defer;
        default: return std::nullopt;
        }
    }

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
    IntegerValue(const CodePosition& position, IntegerType* integerType) : Value(position, integerType) {
        isConstexpr = true;
    }
    int64_t value;
};
struct FloatValue : Value {
    FloatValue(const CodePosition& position, FloatType* floatType) : Value(position, floatType) {
        isConstexpr = true;
    }
    double value;
};
struct StringValue : Value {
    StringValue(const CodePosition& position, StringType* stringType) : Value(position, stringType) {
        isConstexpr = true;
    }
    std::string value;
};
struct StaticArrayValue : Value {
    StaticArrayValue(const CodePosition& position, StaticArrayType* staticArrayType) : Value(position, staticArrayType) {
        isConstexpr = true;
    }
    std::vector<std::unique_ptr<Value>> values;
};
struct FunctionValue : Value {
    FunctionValue(const CodePosition& position, FunctionType* functionType, Scope* parentScope) : 
        Value(position, functionType),
        body(position, Scope::Owner::Function, parentScope)
    {
        isConstexpr = true;
    }
    std::vector<Variable> arguments;
    Type* returnType;
    CodeScope body;
};
