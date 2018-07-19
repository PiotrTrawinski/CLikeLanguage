#pragma once

#include "Value.h"

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

    Operation(const CodePosition& position, Kind kind);

    static int priority(Kind kind);
    static bool isLeftAssociative(Kind kind);
    static int numberOfArguments(Kind kind);
    int getPriority();
    bool getIsLeftAssociative();
    int getNumberOfArguments();

    bool resolveTypeOfOperation();
    virtual std::optional<std::unique_ptr<Value>> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual std::unique_ptr<Value> copy();

    Kind kind;
    std::vector<std::unique_ptr<Value>> arguments;
};



struct CastOperation : Operation {
    CastOperation(const CodePosition& position, std::unique_ptr<Type>&& argType);
    virtual std::optional<std::unique_ptr<Value>> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual std::unique_ptr<Value> copy();

    std::unique_ptr<Type> argType;
};
struct ArrayIndexOperation : Operation {
    ArrayIndexOperation(const CodePosition& position, std::unique_ptr<Value>&& index);
    virtual std::optional<std::unique_ptr<Value>> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual std::unique_ptr<Value> copy();

    std::unique_ptr<Value> index;
};
struct ArraySubArrayOperation : Operation {
    ArraySubArrayOperation(const CodePosition& position, std::unique_ptr<Value>&& firstIndex, std::unique_ptr<Value>&& secondIndex);
    virtual std::optional<std::unique_ptr<Value>> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual std::unique_ptr<Value> copy();

    std::unique_ptr<Value> firstIndex;
    std::unique_ptr<Value> secondIndex;
};
struct FunctionCallOperation : Operation {
    FunctionCallOperation(const CodePosition& position);
    virtual std::optional<std::unique_ptr<Value>> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual std::unique_ptr<Value> copy();

    Variable function;
};
struct TemplateFunctionCallOperation : FunctionCallOperation {
    TemplateFunctionCallOperation(const CodePosition& position);
    virtual std::optional<std::unique_ptr<Value>> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual std::unique_ptr<Value> copy();

    std::vector<std::unique_ptr<Type>> templateTypes;
};
