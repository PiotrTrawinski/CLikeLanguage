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
    static std::string kindToString(Kind kind);
    int getPriority();
    bool getIsLeftAssociative();
    int getNumberOfArguments();

    bool resolveTypeOfOperation(bool allArgsConstexpr);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();

    Kind kind;
    std::vector<Value*> arguments;
    
private:
    static std::vector<std::unique_ptr<Operation>> objects;
    
    template<typename Function> Value* evaluate(Value* val1, Value* val2, Function function) {
        if (val1->valueKind == Value::ValueKind::Integer && val2->valueKind == Value::ValueKind::Integer) {
            int64_t result = function((int64_t)((IntegerValue*)val1)->value, (int64_t)((IntegerValue*)val2)->value);
            auto value = IntegerValue::Create(position, result);
            value->type = type;
            return value;
        }
        if (val1->valueKind == Value::ValueKind::Float && val2->valueKind == Value::ValueKind::Integer) {
            double result = function((double)((FloatValue*)val1)->value, (double)((IntegerValue*)val2)->value);
            auto value = FloatValue::Create(position, result);
            value->type = type;
            return value;
        }
        if (val1->valueKind == Value::ValueKind::Integer && val2->valueKind == Value::ValueKind::Float) {
            double result = function((double)((IntegerValue*)val1)->value, (double)((FloatValue*)val2)->value);
            auto value = FloatValue::Create(position, result);
            value->type = type;
            return value;
        } 
        if (val1->valueKind == Value::ValueKind::Float && val2->valueKind == Value::ValueKind::Float) {
            double result = function((double)((FloatValue*)val1)->value, (double)((FloatValue*)val2)->value);
            auto value = FloatValue::Create(position, result);
            value->type = type;
            return value;
        }
        if (val1->valueKind == Value::ValueKind::Char && val2->valueKind == Value::ValueKind::Char) {
            int64_t result = function((int64_t)((CharValue*)val1)->value, (int64_t)((CharValue*)val2)->value);
            auto value = IntegerValue::Create(position, result);
            value->type = type;
            return value;
        }
        if (val1->valueKind == Value::ValueKind::Float && val2->valueKind == Value::ValueKind::Char) {
            double result = function((double)((FloatValue*)val1)->value, (double)((CharValue*)val2)->value);
            auto value = FloatValue::Create(position, result);
            value->type = type;
            return value;
        }
        if (val1->valueKind == Value::ValueKind::Char && val2->valueKind == Value::ValueKind::Float) {
            double result = function((double)((CharValue*)val1)->value, (double)((FloatValue*)val2)->value);
            auto value = FloatValue::Create(position, result);
            value->type = type;
            return value;
        }
        if (val1->valueKind == Value::ValueKind::Char && val2->valueKind == Value::ValueKind::Integer) {
            int64_t result = function((int64_t)((CharValue*)val1)->value, (int64_t)((IntegerValue*)val2)->value);
            auto value = IntegerValue::Create(position, result);
            value->type = type;
            return value;
        }
        if (val1->valueKind == Value::ValueKind::Integer && val2->valueKind == Value::ValueKind::Char) {
            int64_t result = function((int64_t)((IntegerValue*)val1)->value, (int64_t)((CharValue*)val2)->value);
            auto value = IntegerValue::Create(position, result);
            value->type = type;
            return value;
        }
    }
    template<typename Function> Value* evaluateIntegerOnly(Value* val1, Value* val2, Function function) {
        if (val1->valueKind == Value::ValueKind::Integer && val2->valueKind == Value::ValueKind::Integer) {
            int64_t result = function((int64_t)((IntegerValue*)val1)->value, (int64_t)((IntegerValue*)val2)->value);
            auto value = IntegerValue::Create(position, result);
            value->type = type;
            return value;
        }
        if (val1->valueKind == Value::ValueKind::Char && val2->valueKind == Value::ValueKind::Char) {
            int64_t result = function((int64_t)((CharValue*)val1)->value, (int64_t)((CharValue*)val2)->value);
            auto value = IntegerValue::Create(position, result);
            value->type = type;
            return value;
        }
        if (val1->valueKind == Value::ValueKind::Char && val2->valueKind == Value::ValueKind::Integer) {
            int64_t result = function((int64_t)((CharValue*)val1)->value, (int64_t)((IntegerValue*)val2)->value);
            auto value = IntegerValue::Create(position, result);
            value->type = type;
            return value;
        }
        if (val1->valueKind == Value::ValueKind::Integer && val2->valueKind == Value::ValueKind::Char) {
            int64_t result = function((int64_t)((IntegerValue*)val1)->value, (int64_t)((CharValue*)val2)->value);
            auto value = IntegerValue::Create(position, result);
            value->type = type;
            return value;
        }
    }

    template<typename Function> Value* tryEvaluate2ArgArithmetic(Function function) {
        auto type1 = arguments[0]->type;
        auto type2 = arguments[1]->type;
        type = Type::getSuitingArithmeticType(type1, type2);
        if (type && arguments[0]->isConstexpr && arguments[1]->isConstexpr) {
            return evaluate(arguments[0], arguments[1], function);
        }
        return nullptr;
    }
    template<typename Function> Value* tryEvaluate2ArgArithmeticBoolTest(Function function) {
        auto type1 = arguments[0]->type;
        auto type2 = arguments[1]->type;
        if (!Type::getSuitingArithmeticType(type1, type2)) {
            return nullptr;
        }
        type = Type::Create(Type::Kind::Bool);
        if (arguments[0]->isConstexpr && arguments[1]->isConstexpr) {
            return BoolValue::Create(position, function(arguments[0], arguments[1]));
        }
        return nullptr;
    }
    template<typename Function> Value* tryEvaluate2ArgArithmeticIntegerOnly(Function function) {
        auto type1 = arguments[0]->type;
        auto type2 = arguments[1]->type;
        type = Type::getSuitingArithmeticType(type1, type2);
        if (type && type->kind == Type::Kind::Float) {
            type = nullptr;
            return nullptr;
        }
        if (type && arguments[0]->isConstexpr && arguments[1]->isConstexpr) {
            return evaluateIntegerOnly(arguments[0], arguments[1], function);
        }
        return nullptr;
    }
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
