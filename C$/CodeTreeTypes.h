#pragma once

#include <vector>
#include <memory>
#include <optional>
#include <variant>

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
        Bool,
        Integer,
        Float,
        Void,
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
    std::unique_ptr<Type> type = nullptr;
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
    OwnerPointerType(std::unique_ptr<Type>&& underlyingType) : 
        Type(Type::Kind::OwnerPointer),
        underlyingType(move(underlyingType))
    {}
    std::unique_ptr<Type> underlyingType;
};
struct RawPointerType : Type {
    RawPointerType(std::unique_ptr<Type>&& underlyingType) : 
        Type(Type::Kind::RawPointer),
        underlyingType(move(underlyingType))
    {}
    std::unique_ptr<Type> underlyingType;
};
struct MaybeErrorType : Type {
    MaybeErrorType(std::unique_ptr<Type>&& underlyingType) : 
        Type(Type::Kind::MaybeError),
        underlyingType(move(underlyingType))
    {}
    std::unique_ptr<Type> underlyingType;
};
struct ReferenceType : Type {
    ReferenceType(std::unique_ptr<Type>&& underlyingType) : 
        Type(Type::Kind::Reference),
        underlyingType(move(underlyingType))
    {}
    std::unique_ptr<Type> underlyingType;
};
struct StaticArrayType : Type {
    StaticArrayType(std::unique_ptr<Type>&& elementType, std::unique_ptr<Value>&& size) : 
        Type(Type::Kind::StaticArray),
        elementType(move(elementType)),
        size(move(size))
    {}
    std::unique_ptr<Type> elementType;
    std::unique_ptr<Value> size;
};
struct DynamicArrayType : Type {
    DynamicArrayType(std::unique_ptr<Type>&& elementType) : 
        Type(Type::Kind::DynamicArray),
        elementType(move(elementType))
    {}
    std::unique_ptr<Type> elementType;
};
struct ArrayViewType : Type {
    ArrayViewType(std::unique_ptr<Type>&& elementType) : 
        Type(Type::Kind::ArrayView),
        elementType(move(elementType))
    {}
    std::unique_ptr<Type> elementType;
};
/*struct StringType : Type {
    StringType() : Type(Type::Kind::String) {}
};*/
struct ClassType : Type {
    ClassType(const std::string& name) : 
        Type(Type::Kind::Class),
        name(name)
    {}
    std::string name;
    std::vector<std::unique_ptr<Type>> templateTypes;
};
struct FunctionType : Type {
    FunctionType() : Type(Type::Kind::Function) {}
    std::unique_ptr<Type> returnType;
    std::vector<std::unique_ptr<Type>> argumentTypes;
};
struct IntegerType : Type {
    enum class Size { I8, I16, I32, I64, U8, U16, U32, U64 };
    IntegerType(Size size) : 
        Type(Type::Kind::Integer), 
        size(size) 
    {}
    Size size;
};
struct FloatType : Type {
    enum class Size { F32, F64 };
    FloatType(Size size) : 
        Type(Type::Kind::Float),
        size(size)
    {}
    Size size;
};
struct TemplateType : Type {
    TemplateType(const std::string& name) : 
        Type(Type::Kind::Template),
        name(name)
    {}
    std::string name;
};
/*struct TempalteClassType : ClassType {
    TempalteClassType() {
        kind = Type::Kind::TemplateClass;
    }
    std::vector<std::unique_ptr<TemplateType>> templateTypes;
};*/
struct TemplateFunctionType : FunctionType {
    TemplateFunctionType() {
        kind = Type::Kind::TemplateFunction;
    }
    std::vector<std::unique_ptr<TemplateType>> templateTypes;
};



/*
    Operations
*/
struct CastOperation : Operation {
    CastOperation(const CodePosition& position) : 
        Operation(position, Operation::Kind::Cast)
    {}

    std::unique_ptr<Type> argType;
};
struct FunctionCallOperation : Operation {
    FunctionCallOperation(const CodePosition& position) : 
        Operation(position, Operation::Kind::FunctionCall),
        function(position)
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

    struct ReadStatementValue {
        ReadStatementValue(std::unique_ptr<Statement>&& statement) : statement(std::move(statement)) {}
        ReadStatementValue(bool isScopeEnd) : isScopeEnd(isScopeEnd) {}

        operator bool() {
            return statement || isScopeEnd;
        }

        std::unique_ptr<Statement> statement = nullptr;
        bool isScopeEnd = false;
    };
    ReadStatementValue readStatement(const std::vector<Token>& tokens, int& i);
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

    std::string name;
    std::vector<std::unique_ptr<TemplateType>> templateTypes;
    std::vector<std::unique_ptr<Declaration>> declarations;
};

struct ForIterData {
    std::unique_ptr<Variable> iterVariable;
    std::unique_ptr<Value> firstValue;
    std::unique_ptr<Value> step;
    std::unique_ptr<Value> lastValue;
};
struct ForEachData {
    std::unique_ptr<Value> arrayValue;
    std::unique_ptr<Variable> it;
    std::unique_ptr<Variable> index;
};
struct ForScope : CodeScope {
    ForScope(const CodePosition& position, Scope* parentScope) : 
        CodeScope(position, Scope::Owner::For, parentScope)
    {}
    bool interpret(const std::vector<Token>& tokens, int& i);
    std::variant<ForIterData, ForEachData> data;
};
struct WhileScope : CodeScope {
    WhileScope(const CodePosition& position, Scope* parentScope) : 
        CodeScope(position, Scope::Owner::While, parentScope) 
    {}
    virtual bool interpret(const std::vector<Token>& tokens, int& i);
    std::unique_ptr<Value> conditionExpression;
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
    std::unique_ptr<Type> returnType;
    CodeScope body;
};
