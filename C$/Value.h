#pragma once

#include "Statement.h"
#include "CodePosition.h"
#include "Type.h"
#include "Scope.h"

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

    Value(const CodePosition& position, ValueKind valueKind);
    virtual std::optional<std::unique_ptr<Value>> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual std::unique_ptr<Value> copy();

    ValueKind valueKind;
    std::unique_ptr<Type> type = nullptr;
    bool isConstexpr = false;
};

struct Variable : Value {
    Variable(const CodePosition& position, const std::string& name="");
    virtual std::optional<std::unique_ptr<Value>> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual std::unique_ptr<Value> copy();

    std::string name;
    bool isConst = false;
};
struct IntegerValue : Value {
    IntegerValue(const CodePosition& position, int32_t value);
    virtual std::optional<std::unique_ptr<Value>> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual std::unique_ptr<Value> copy();

    uint64_t value;
};
struct CharValue : Value {
    CharValue(const CodePosition& position, uint8_t value);
    virtual std::optional<std::unique_ptr<Value>> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual std::unique_ptr<Value> copy();

    uint8_t value;
};
struct FloatValue : Value {
    FloatValue(const CodePosition& position, double value);
    virtual std::optional<std::unique_ptr<Value>> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual std::unique_ptr<Value> copy();

    double value;
};
struct StringValue : Value {
    StringValue(const CodePosition& position, const std::string& value);
    virtual std::optional<std::unique_ptr<Value>> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual std::unique_ptr<Value> copy();

    std::string value;
};
struct StaticArrayValue : Value {
    StaticArrayValue(const CodePosition& position);
    virtual std::optional<std::unique_ptr<Value>> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual std::unique_ptr<Value> copy();

    std::vector<std::unique_ptr<Value>> values;
};
struct FunctionValue : Value {
    FunctionValue(const CodePosition& position, std::unique_ptr<Type>&& type, Scope* parentScope);
    virtual std::optional<std::unique_ptr<Value>> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;

    std::vector<std::string> argumentNames;
    CodeScope body;
};
