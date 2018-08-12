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
    static Value* Create(const CodePosition& position, ValueKind valueKind);
    static bool isLvalue(Value* value);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);

    ValueKind valueKind;
    Type* type = nullptr;
    bool isConstexpr = false;

private:
    static std::vector<std::unique_ptr<Value>> objects;
};

struct Variable : Value {
    Variable(const CodePosition& position, const std::string& name="");
    static Variable* Create(const CodePosition& position, const std::string& name="");
    virtual std::optional<Value*> interpret(Scope* scope);
    bool interpretTypeAndDeclaration(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* getReferenceLlvm(LlvmObject* llvmObj);

    std::string name;
    bool isConst = false;
    Declaration* declaration;
    bool wasInterpreted = false;

private:
    static std::vector<std::unique_ptr<Variable>> objects;
};
struct IntegerValue : Value {
    IntegerValue(const CodePosition& position, uint64_t value);
    static IntegerValue* Create(const CodePosition& position, uint64_t value);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();
    llvm::Value* createLlvm(LlvmObject* llvmObj);

    uint64_t value;
    
private:
    static std::vector<std::unique_ptr<IntegerValue>> objects;
};
struct CharValue : Value {
    CharValue(const CodePosition& position, uint8_t value);
    static CharValue* Create(const CodePosition& position, uint8_t value);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();
    llvm::Value* createLlvm(LlvmObject* llvmObj);

    uint8_t value;
    
private:
    static std::vector<std::unique_ptr<CharValue>> objects;
};
struct FloatValue : Value {
    FloatValue(const CodePosition& position, double value);
    static FloatValue* Create(const CodePosition& position, double value);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();
    llvm::Value* createLlvm(LlvmObject* llvmObj);

    double value;
    
private:
    static std::vector<std::unique_ptr<FloatValue>> objects;
};
struct BoolValue : Value {
    BoolValue(const CodePosition& position, bool value);
    static BoolValue* Create(const CodePosition& position, bool value);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    llvm::Value* createLlvm(LlvmObject* llvmObj);

    bool value;

private:
    static std::vector<std::unique_ptr<BoolValue>> objects;
};
struct StringValue : Value {
    StringValue(const CodePosition& position, const std::string& value);
    static StringValue* Create(const CodePosition& position, const std::string& value);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();

    std::string value;
    
private:
    static std::vector<std::unique_ptr<StringValue>> objects;
};
struct StaticArrayValue : Value {
    StaticArrayValue(const CodePosition& position);
    static StaticArrayValue* Create(const CodePosition& position);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();

    std::vector<Value*> values;
    
private:
    static std::vector<std::unique_ptr<StaticArrayValue>> objects;
};
struct FunctionValue : Value {
    FunctionValue(const CodePosition& position, Type* type, Scope* parentScope);
    static FunctionValue* Create(const CodePosition& position, Type* type, Scope* parentScope);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);

    //std::vector<std::string> argumentNames;
    std::vector<Declaration*> arguments;
    
    FunctionScope* body = nullptr;
    llvm::Function* llvmFunction = nullptr;

private:
    static std::vector<std::unique_ptr<FunctionValue>> objects;
};
