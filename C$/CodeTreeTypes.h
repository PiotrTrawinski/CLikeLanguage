#pragma once

#include <vector>
#include <memory>
#include <optional>
#include <variant>

#include "CodePosition.h"
#include "Token.h"

template<typename T> bool operator==(const std::unique_ptr<T>& lhs, const std::unique_ptr<T>& rhs) {
    if ((lhs && !rhs) || (!lhs && rhs)) {
        return false;
    }
    return (!lhs && !rhs) || (*lhs == *rhs);
}
template<typename T> bool operator==(const std::vector<T>& lhs, const std::vector<T>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (int i = 0; i < lhs.size(); ++i) {
        if (!(lhs[i] == rhs[i])) {
            return false;
        }
    }
    return true;
}

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
    
    virtual bool operator==(const Type& type) const {
        if(typeid(type) == typeid(*this)){
            const Type& other = static_cast<const Type&>(type);
            return this->kind == other.kind;
        }
        else {
            return false;
        }
    }

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

    virtual bool operator==(const Statement& statement) const {
        if(typeid(statement) == typeid(*this)){
            const auto& other = static_cast<const Statement&>(statement);
            return this->kind == other.kind
                && this->position.charNumber == other.position.charNumber
                && this->position.lineNumber == other.position.lineNumber
                && this->position.fileInfo == other.position.fileInfo;
        } else {
            return false;
        }
    }

    Kind kind;
    CodePosition position;
};

struct Value : Statement {
    enum class ValueKind {
        Empty,
        Integer,
        Float,
        Char,
        String,
        StaticArray,
        FunctionValue,
        Variable,
        Operation
    };

    Value(const CodePosition& position, ValueKind valueKind) : 
        Statement(position, Statement::Kind::Value),
        valueKind(valueKind)
    {}

    virtual bool operator==(const Statement& value) const {
        if(typeid(value) == typeid(*this)){
            const auto& other = static_cast<const Value&>(value);
            return this->valueKind == other.valueKind
                && (this->type == other.type || this->type == other.type)
                && this->isConstexpr == other.isConstexpr
                && Statement::operator==(other);
        } else {
            return false;
        }
    }

    ValueKind valueKind;
    std::unique_ptr<Type> type = nullptr;
    bool isConstexpr = false;
};

struct Variable : Value {
    Variable(const CodePosition& position, const std::string& name="") : 
        Value(position, Value::ValueKind::Variable),
        name(name)
    {}

    virtual bool operator==(const Statement& value) const {
        if(typeid(value) == typeid(*this)){
            const auto& other = static_cast<const Variable&>(value);
            return this->name == other.name
                && this->isConst == other.isConst
                && Value::operator==(other);
        } else {
            return false;
        }
    }

    std::string name;
    bool isConst = false;
};

struct Operation : Value {
    enum class Kind {
        Add, Sub, Mul, Div, Mod,
        Minus,
        BitAnd, BitOr, BitNeg, BitXor,
        LogicalAnd, LogicalOr, LogicalNot,
        Eq, Neq,
        Gt, Lt, Gte, Lte,
        Shl, Shr, Sal, Sar,
        Assign,
        AddAssign, SubAssign, MulAssign, DivAssign, ModAssign,
        ShlAssign, ShrAssign, SalAssign, SarAssign, 
        BitNegAssign, BitOrAssign, BitXorAssign,
        Reference, Address, GetValue,
        Dot,
        ArrayIndex, ArraySubArray,
        Cast,
        FunctionCall,
        TemplateFunctionCall,
        Allocation,
        Deallocation,
        //ErrorCoding,
        Break,
        Remove,
        Continue,
        Return,
        LeftBracket // not really operator - only for convinience in reverse polish notation
    };

    Operation(const CodePosition& position, Kind kind) : 
        Value(position, Value::ValueKind::Operation),
        kind(kind)
    {}

    static int priority(Kind kind) {
        switch (kind) {
        case Kind::Dot:
        case Kind::FunctionCall:
        case Kind::ArrayIndex:
        case Kind::ArraySubArray:
            return 1;
        case Kind::Reference:
        case Kind::Address:
        case Kind::GetValue:
        case Kind::Allocation:
        case Kind::Deallocation:
        case Kind::Cast:
        case Kind::BitNeg:
        case Kind::LogicalNot:
        case Kind::Minus:
            return 2;
        case Kind::Mul:
        case Kind::Div:
        case Kind::Mod:
            return 3;
        case Kind::Add:
        case Kind::Sub:
            return 4;
        case Kind::Shl:
        case Kind::Shr:
        case Kind::Sal:
        case Kind::Sar:
            return 5;
        case Kind::Gt:
        case Kind::Lt:
        case Kind::Gte:
        case Kind::Lte:
            return 6;
        case Kind::Eq:
        case Kind::Neq:
            return 7;
        case Kind::BitAnd:
            return 8;
        case Kind::BitXor:
            return 9;
        case Kind::BitOr:
            return 10;
        case Kind::LogicalAnd:
            return 11;
        case Kind::LogicalOr:
            return 12;
        case Kind::Assign:
        case Kind::AddAssign:
        case Kind::SubAssign:
        case Kind::MulAssign:
        case Kind::DivAssign:
        case Kind::ModAssign:
        case Kind::ShlAssign:
        case Kind::ShrAssign:
        case Kind::SalAssign:
        case Kind::SarAssign:
        case Kind::BitNegAssign:
        case Kind::BitOrAssign:
        case Kind::BitXorAssign:
            return 13;
        default: 
            return 14;
        }
    }
    static bool isLeftAssociative(Kind kind) {
        switch (kind) {
        case Kind::Dot:
        case Kind::FunctionCall:
        case Kind::ArrayIndex:
        case Kind::ArraySubArray:
        case Kind::Mul:
        case Kind::Div:
        case Kind::Mod:
        case Kind::Add:
        case Kind::Sub:
        case Kind::Shl:
        case Kind::Shr:
        case Kind::Sal:
        case Kind::Sar:
        case Kind::Gt:
        case Kind::Lt:
        case Kind::Gte:
        case Kind::Lte:
        case Kind::Eq:
        case Kind::Neq:
        case Kind::BitAnd:
        case Kind::BitXor:
        case Kind::BitOr:
        case Kind::LogicalAnd:
        case Kind::LogicalOr:
            return true;
        default: 
            return false;
        }
    }
    static int numberOfArguments(Kind kind) {
        switch (kind) {
        case Kind::FunctionCall:
            return 0;
        case Kind::ArrayIndex:
        case Kind::ArraySubArray:
        case Kind::Reference:
        case Kind::Address:
        case Kind::GetValue:
        case Kind::Allocation:
        case Kind::Deallocation:
        case Kind::Cast:
        case Kind::BitNeg:
        case Kind::LogicalNot:
        case Kind::Minus:
            return 1;
        case Kind::Dot:
        case Kind::Mul:
        case Kind::Div:
        case Kind::Mod:
        case Kind::Add:
        case Kind::Sub:
        case Kind::Shl:
        case Kind::Shr:
        case Kind::Sal:
        case Kind::Sar:
        case Kind::Gt:
        case Kind::Lt:
        case Kind::Gte:
        case Kind::Lte:
        case Kind::Eq:
        case Kind::Neq:
        case Kind::BitAnd:
        case Kind::BitXor:
        case Kind::BitOr:
        case Kind::LogicalAnd:
        case Kind::LogicalOr:
        case Kind::Assign:
        case Kind::AddAssign:
        case Kind::SubAssign:
        case Kind::MulAssign:
        case Kind::DivAssign:
        case Kind::ModAssign:
        case Kind::ShlAssign:
        case Kind::ShrAssign:
        case Kind::SalAssign:
        case Kind::SarAssign:
        case Kind::BitNegAssign:
        case Kind::BitOrAssign:
        case Kind::BitXorAssign:
            return 2;
        default: 
            return 0;
        }
    }
    int getPriority() {
        return priority(kind);
    }
    bool getIsLeftAssociative() {
        return isLeftAssociative(kind);
    }
    int getNumberOfArguments() {
        return numberOfArguments(kind);
    }

    virtual bool operator==(const Statement& value) const {
        if(typeid(value) == typeid(*this)){
            const auto& other = static_cast<const Operation&>(value);
            return this->arguments == other.arguments
                && this->kind == other.kind
                && Value::operator==(other);
        }
        else {
            return false;
        }
    }

    Kind kind;
    std::vector<std::unique_ptr<Value>> arguments;
};


struct OwnerPointerType : Type {
    OwnerPointerType(std::unique_ptr<Type>&& underlyingType) : 
        Type(Type::Kind::OwnerPointer),
        underlyingType(move(underlyingType))
    {}
    virtual bool operator==(const Type& type) const {
        if(typeid(type) == typeid(*this)){
            const auto& other = static_cast<const OwnerPointerType&>(type);
            return this->underlyingType == other.underlyingType;
        } else {
            return false;
        }
    }
    std::unique_ptr<Type> underlyingType;
};
struct RawPointerType : Type {
    RawPointerType(std::unique_ptr<Type>&& underlyingType) : 
        Type(Type::Kind::RawPointer),
        underlyingType(move(underlyingType))
    {}
    virtual bool operator==(const Type& type) const {
        if(typeid(type) == typeid(*this)){
            const auto& other = static_cast<const RawPointerType&>(type);
            return this->underlyingType == other.underlyingType;
        } else {
            return false;
        }
    }
    std::unique_ptr<Type> underlyingType;
};
struct MaybeErrorType : Type {
    MaybeErrorType(std::unique_ptr<Type>&& underlyingType) : 
        Type(Type::Kind::MaybeError),
        underlyingType(move(underlyingType))
    {}
    virtual bool operator==(const Type& type) const {
        if(typeid(type) == typeid(*this)){
            const auto& other = static_cast<const MaybeErrorType&>(type);
            return this->underlyingType == other.underlyingType;
        } else {
            return false;
        }
    }
    std::unique_ptr<Type> underlyingType;
};
struct ReferenceType : Type {
    ReferenceType(std::unique_ptr<Type>&& underlyingType) : 
        Type(Type::Kind::Reference),
        underlyingType(move(underlyingType))
    {}
    virtual bool operator==(const Type& type) const {
        if(typeid(type) == typeid(*this)){
            const auto& other = static_cast<const ReferenceType&>(type);
            return this->underlyingType == other.underlyingType;
        } else {
            return false;
        }
    }
    std::unique_ptr<Type> underlyingType;
};
struct StaticArrayType : Type {
    StaticArrayType(std::unique_ptr<Type>&& elementType, std::unique_ptr<Value>&& size) : 
        Type(Type::Kind::StaticArray),
        elementType(move(elementType)),
        size(move(size))
    {}
    virtual bool operator==(const Type& type) const {
        if(typeid(type) == typeid(*this)){
            const auto& other = static_cast<const StaticArrayType&>(type);
            return this->elementType == other.elementType
                && this->size == other.size;
        } else {
            return false;
        }
    }
    std::unique_ptr<Type> elementType;
    std::unique_ptr<Value> size;
};
struct DynamicArrayType : Type {
    DynamicArrayType(std::unique_ptr<Type>&& elementType) : 
        Type(Type::Kind::DynamicArray),
        elementType(move(elementType))
    {}
    virtual bool operator==(const Type& type) const {
        if(typeid(type) == typeid(*this)){
            const auto& other = static_cast<const DynamicArrayType&>(type);
            return this->elementType == other.elementType;
        } else {
            return false;
        }
    }
    std::unique_ptr<Type> elementType;
};
struct ArrayViewType : Type {
    ArrayViewType(std::unique_ptr<Type>&& elementType) : 
        Type(Type::Kind::ArrayView),
        elementType(move(elementType))
    {}
    virtual bool operator==(const Type& type) const {
        if(typeid(type) == typeid(*this)){
            const auto& other = static_cast<const ArrayViewType&>(type);
            return this->elementType == other.elementType;
        } else {
            return false;
        }
    }
    std::unique_ptr<Type> elementType;
};
struct ClassType : Type {
    ClassType(const std::string& name) : 
        Type(Type::Kind::Class),
        name(name)
    {}
    std::string name;
    virtual bool operator==(const Type& type) const {
        if(typeid(type) == typeid(*this)){
            const auto& other = static_cast<const ClassType&>(type);
            return this->templateTypes == other.templateTypes;
        } else {
            return false;
        }
    }
    std::vector<std::unique_ptr<Type>> templateTypes;
};
struct FunctionType : Type {
    FunctionType() : Type(Type::Kind::Function) {}
    virtual bool operator==(const Type& type) const {
        if(typeid(type) == typeid(*this)){
            const auto& other = static_cast<const FunctionType&>(type);
            return this->argumentTypes == other.argumentTypes
                && this->returnType == other.returnType;
        } else {
            return false;
        }
    }
    std::unique_ptr<Type> returnType;
    std::vector<std::unique_ptr<Type>> argumentTypes;
};
struct IntegerType : Type {
    enum class Size { I8, I16, I32, I64, U8, U16, U32, U64 };
    IntegerType(Size size) : 
        Type(Type::Kind::Integer), 
        size(size) 
    {}
    virtual bool operator==(const Type& type) const {
        if(typeid(type) == typeid(*this)){
            const auto& other = static_cast<const IntegerType&>(type);
            return this->size == other.size;
        } else {
            return false;
        }
    }
    Size size;
};
struct FloatType : Type {
    enum class Size { F32, F64 };
    FloatType(Size size) : 
        Type(Type::Kind::Float),
        size(size)
    {}
    virtual bool operator==(const Type& type) const {
        if(typeid(type) == typeid(*this)){
            const auto& other = static_cast<const FloatType&>(type);
            return this->size == other.size;
        } else {
            return false;
        }
    }
    Size size;
};
struct TemplateType : Type {
    TemplateType(const std::string& name) : 
        Type(Type::Kind::Template),
        name(name)
    {}
    virtual bool operator==(const Type& type) const {
        if(typeid(type) == typeid(*this)){
            return true;
        } else {
            return false;
        }
    }
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
    virtual bool operator==(const Type& type) const {
        if(typeid(type) == typeid(*this)){
            const auto& other = static_cast<const TemplateFunctionType&>(type);
            return this->templateTypes == other.templateTypes
                && FunctionType::operator==(type);
        } else {
            return false;
        }
    }
    std::vector<std::unique_ptr<TemplateType>> templateTypes;
};



/*
    Operations
*/
struct CastOperation : Operation {
    CastOperation(const CodePosition& position, std::unique_ptr<Type>&& argType) : 
        Operation(position, Operation::Kind::Cast),
        argType(move(argType))
    {}
    virtual bool operator==(const Statement& value) const {
        if(typeid(value) == typeid(*this)){
            const auto& other = static_cast<const CastOperation&>(value);
            return this->argType == other.argType
                && Operation::operator==(other);
        }
        else {
            return false;
        }
    }

    std::unique_ptr<Type> argType;
};
struct ArrayIndexOperation : Operation {
    ArrayIndexOperation(const CodePosition& position, std::unique_ptr<Value>&& index) : 
        Operation(position, Operation::Kind::ArrayIndex),
        index(move(index))
    {}
    virtual bool operator==(const Statement& value) const {
        if(typeid(value) == typeid(*this)){
            const auto& other = static_cast<const ArrayIndexOperation&>(value);
            return this->index == other.index
                && Operation::operator==(other);
        }
        else {
            return false;
        }
    }

    std::unique_ptr<Value> index;
};
struct ArraySubArrayOperation : Operation {
    ArraySubArrayOperation(const CodePosition& position, std::unique_ptr<Value>&& firstIndex, std::unique_ptr<Value>&& secondIndex) : 
        Operation(position, Operation::Kind::ArraySubArray),
        firstIndex(move(firstIndex)),
        secondIndex(move(secondIndex))
    {}
    virtual bool operator==(const Statement& value) const {
        if(typeid(value) == typeid(*this)){
            const auto& other = static_cast<const ArraySubArrayOperation&>(value);
            return this->firstIndex == other.firstIndex
                && this->secondIndex == other.secondIndex
                && Operation::operator==(other);
        }
        else {
            return false;
        }
    }

    std::unique_ptr<Value> firstIndex;
    std::unique_ptr<Value> secondIndex;
};
struct FunctionCallOperation : Operation {
    FunctionCallOperation(const CodePosition& position) : 
        Operation(position, Operation::Kind::FunctionCall),
        function(position)
    {}
    virtual bool operator==(const Statement& value) const {
        if(typeid(value) == typeid(*this)){
            const auto& other = static_cast<const FunctionCallOperation&>(value);
            return this->function == other.function
                && Operation::operator==(other);
        }
        else {
            return false;
        }
    }

    Variable function;
};
struct TemplateFunctionCallOperation : FunctionCallOperation {
    TemplateFunctionCallOperation(const CodePosition& position) : 
        FunctionCallOperation(position)
    {
        kind = Operation::Kind::TemplateFunctionCall;
    }
    virtual bool operator==(const Statement& value) const {
        if(typeid(value) == typeid(*this)){
            const auto& other = static_cast<const TemplateFunctionCallOperation&>(value);
            return this->templateTypes == other.templateTypes
                && FunctionCallOperation::operator==(other);
        }
        else {
            return false;
        }
    }

    std::vector<std::unique_ptr<Type>> templateTypes;
};

struct Declaration : Statement {
    Declaration(const CodePosition& position) : 
        Statement(position, Statement::Kind::Declaration),
        variable(position)
    {}

    virtual bool operator==(const Statement& declaration) const {
        if(typeid(declaration) == typeid(*this)){
            const auto& other = static_cast<const Declaration&>(declaration);
            return this->variable == other.variable
                && this->value == other.value
                && Statement::operator==(other);
        } else {
            return false;
        }
    }

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

    virtual bool operator==(const Statement& scope) const {
        if(typeid(scope) == typeid(*this)){
            const auto& other = static_cast<const Scope&>(scope);
            return this->owner == other.owner
                //&& this->parentScope == other.parentScope
                && Statement::operator==(other);
        } else {
            return false;
        }
    }

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

    virtual bool operator==(const Statement& scope) const {
        if(typeid(scope) == typeid(*this)){
            const auto& other = static_cast<const CodeScope&>(scope);
            return this->isGlobalScope == other.isGlobalScope
                && this->statements == other.statements
                && Scope::operator==(other);
        } else {
            return false;
        }
    }

    bool isGlobalScope;
    std::vector<std::unique_ptr<Statement>> statements;
};
struct ClassScope : Scope {
    ClassScope(const CodePosition& position, Scope* parentScope) : 
        Scope(position, Scope::Owner::Class, parentScope) 
    {}
    virtual bool interpret(const std::vector<Token>& tokens, int& i);

    virtual bool operator==(const Statement& scope) const {
        if(typeid(scope) == typeid(*this)){
            const auto& other = static_cast<const ClassScope&>(scope);
            return this->name == other.name
                && this->templateTypes == other.templateTypes
                && this->declarations == other.declarations
                && Scope::operator==(other);
        } else {
            return false;
        }
    }

    std::string name;
    std::vector<std::unique_ptr<TemplateType>> templateTypes;
    std::vector<std::unique_ptr<Declaration>> declarations;
};

struct ForIterData {
    std::unique_ptr<Variable> iterVariable;
    std::unique_ptr<Value> firstValue;
    std::unique_ptr<Value> step;
    std::unique_ptr<Value> lastValue;

    bool operator==(const ForIterData& other) const {
        return this->iterVariable == other.iterVariable
            && this->firstValue == other.firstValue
            && this->step == other.step
            && this->lastValue == other.lastValue;
    }
};
struct ForEachData {
    std::unique_ptr<Value> arrayValue;
    std::unique_ptr<Variable> it;
    std::unique_ptr<Variable> index;

    bool operator==(const ForEachData& other) const {
        return this->arrayValue == other.arrayValue
            && this->it == other.it
            && this->index == other.index;
    }
};
struct ForScope : CodeScope {
    ForScope(const CodePosition& position, Scope* parentScope) : 
        CodeScope(position, Scope::Owner::For, parentScope)
    {}

    virtual bool operator==(const Statement& scope) const {
        if(typeid(scope) == typeid(*this)){
            const auto& other = static_cast<const ForScope&>(scope);
            return this->data == other.data
                && CodeScope::operator==(other);
        } else {
            return false;
        }
    }

    bool interpret(const std::vector<Token>& tokens, int& i);
    std::variant<ForIterData, ForEachData> data;
};
struct WhileScope : CodeScope {
    WhileScope(const CodePosition& position, Scope* parentScope) : 
        CodeScope(position, Scope::Owner::While, parentScope) 
    {}
    virtual bool interpret(const std::vector<Token>& tokens, int& i);
    virtual bool operator==(const Statement& scope) const {
        if(typeid(scope) == typeid(*this)){
            const auto& other = static_cast<const WhileScope&>(scope);
            return this->conditionExpression == other.conditionExpression
                && CodeScope::operator==(other);
        } else {
            return false;
        }
    }
    std::unique_ptr<Value> conditionExpression;
};
struct IfScope : CodeScope {
    IfScope(const CodePosition& position, Scope* parentScope) :
        CodeScope(position, Scope::Owner::If, parentScope) 
    {}
    virtual bool interpret(const std::vector<Token>& tokens, int& i);
    virtual bool operator==(const Statement& scope) const {
        if(typeid(scope) == typeid(*this)){
            const auto& other = static_cast<const IfScope&>(scope);
            return this->conditionExpression == other.conditionExpression
                && CodeScope::operator==(other);
        } else {
            return false;
        }
    }
    std::unique_ptr<Value> conditionExpression;
};
struct ElseIfScope : CodeScope {
    ElseIfScope(const CodePosition& position, Scope* parentScope) : 
        CodeScope(position, Scope::Owner::ElseIf, parentScope) 
    {}
    virtual bool interpret(const std::vector<Token>& tokens, int& i);
    virtual bool operator==(const Statement& scope) const {
        if(typeid(scope) == typeid(*this)){
            const auto& other = static_cast<const ElseIfScope&>(scope);
            return this->conditionExpression == other.conditionExpression
                && CodeScope::operator==(other);
        } else {
            return false;
        }
    }
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
    IntegerValue(const CodePosition& position, int32_t value) : 
        Value(position, Value::ValueKind::Integer),
        value(value)
    {
        isConstexpr = true;
        type = std::make_unique<IntegerType>(IntegerType::Size::I32);
    }
    virtual bool operator==(const Statement& value) const {
        if(typeid(value) == typeid(*this)){
            const auto& other = static_cast<const IntegerValue&>(value);
            return this->value == other.value
                && Value::operator==(other);
        }
        else {
            return false;
        }
    }
    int32_t value;
};
struct CharValue : Value {
    CharValue(const CodePosition& position, uint8_t value) : 
        Value(position, Value::ValueKind::Char),
        value(value)
    {
        isConstexpr = true;
        type = std::make_unique<IntegerType>(IntegerType::Size::U8);
    }
    virtual bool operator==(const Statement& value) const {
        if(typeid(value) == typeid(*this)){
            const auto& other = static_cast<const CharValue&>(value);
            return this->value == other.value
                && Value::operator==(other);
        }
        else {
            return false;
        }
    }
    uint8_t value;
};
struct FloatValue : Value {
    FloatValue(const CodePosition& position, double value) :
        Value(position, Value::ValueKind::Float),
        value(value)
    {
        isConstexpr = true;
        type = std::make_unique<FloatType>(FloatType::Size::F64);
    }
    virtual bool operator==(const Statement& value) const {
        if(typeid(value) == typeid(*this)){
            const auto& other = static_cast<const FloatValue&>(value);
            return this->value == other.value
                && Value::operator==(other);
        }
        else {
            return false;
        }
    }
    double value;
};
struct StringValue : Value {
    StringValue(const CodePosition& position, const std::string& value) : 
        Value(position, Value::ValueKind::String),
        value(value)
    {
        isConstexpr = true;
        type = std::make_unique<Type>(Type::Kind::String);
    }
    virtual bool operator==(const Statement& value) const {
        if(typeid(value) == typeid(*this)){
            const auto& other = static_cast<const StringValue&>(value);
            return this->value == other.value
                && Value::operator==(other);
        }
        else {
            return false;
        }
    }
    std::string value;
};
struct StaticArrayValue : Value {
    StaticArrayValue(const CodePosition& position) : Value(position, Value::ValueKind::StaticArray) {
        isConstexpr = true;
    }
    virtual bool operator==(const Statement& value) const {
        if(typeid(value) == typeid(*this)){
            const auto& other = static_cast<const StaticArrayValue&>(value);
            return this->values == other.values
                && Value::operator==(other);
        }
        else {
            return false;
        }
    }
    std::vector<std::unique_ptr<Value>> values;
};
struct FunctionValue : Value {
    FunctionValue(const CodePosition& position, std::unique_ptr<Type>&& type, Scope* parentScope) : 
        Value(position, Value::ValueKind::FunctionValue),
        body(position, Scope::Owner::Function, parentScope)
    {
        isConstexpr = true;
        this->type = move(type);
    }
    virtual bool operator==(const Statement& value) const {
        if(typeid(value) == typeid(*this)){
            const auto& other = static_cast<const FunctionValue&>(value);
            return this->argumentNames == other.argumentNames
                && Value::operator==(other);
        }
        else {
            return false;
        }
    }
    std::vector<std::string> argumentNames;
    CodeScope body;
};
