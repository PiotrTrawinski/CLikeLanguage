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
    static Operation* Create(const CodePosition& position, Kind kind);

    static int priority(Kind kind);
    static bool isLeftAssociative(Kind kind);
    static int numberOfArguments(Kind kind);
    int getPriority();
    bool getIsLeftAssociative();
    int getNumberOfArguments();

    bool resolveTypeOfOperation();
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();

    Kind kind;
    std::vector<Value*> arguments;
    
private:
    static std::vector<std::unique_ptr<Operation>> objects;
};



struct CastOperation : Operation {
    CastOperation(const CodePosition& position, Type* argType);
    static CastOperation* Create(const CodePosition& position, Type* argType);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();

    Type* argType;
    
private:
    static std::vector<std::unique_ptr<CastOperation>> objects;
};
struct ArrayIndexOperation : Operation {
    ArrayIndexOperation(const CodePosition& position, Value* index);
    static ArrayIndexOperation* Create(const CodePosition& position, Value* index);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();

    Value* index;
    
private:
    static std::vector<std::unique_ptr<ArrayIndexOperation>> objects;
};
struct ArraySubArrayOperation : Operation {
    ArraySubArrayOperation(const CodePosition& position, Value* firstIndex, Value* secondIndex);
    static ArraySubArrayOperation* Create(const CodePosition& position, Value* firstIndex, Value* secondIndex);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();

    Value* firstIndex;
    Value* secondIndex;
    
private:
    static std::vector<std::unique_ptr<ArraySubArrayOperation>> objects;
};
struct FunctionCallOperation : Operation {
    FunctionCallOperation(const CodePosition& position);
    static FunctionCallOperation* Create(const CodePosition& position);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();

    Variable* function;
    
private:
    static std::vector<std::unique_ptr<FunctionCallOperation>> objects;
};
struct TemplateFunctionCallOperation : FunctionCallOperation {
    TemplateFunctionCallOperation(const CodePosition& position);
    static TemplateFunctionCallOperation* Create(const CodePosition& position);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();

    std::vector<Type*> templateTypes;
    
private:
    static std::vector<std::unique_ptr<TemplateFunctionCallOperation>> objects;
};
