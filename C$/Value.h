#pragma once

#include "Statement.h"
#include "CodePosition.h"
#include "Type.h"
#include "Scope.h"

struct Value : Statement {
    enum class ValueKind {
        Empty,
        Null,
        Integer,
        Float,
        Char,
        Bool,
        String,
        StaticArray,
        FunctionValue,
        Variable,
        TemplatedVariable,
        Operation
    };

    Value(const CodePosition& position, ValueKind valueKind);
    static Value* Create(const CodePosition& position, ValueKind valueKind);
    void templateCopy(Value* value, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    static bool isLvalue(Value* value);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();
    virtual void createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj);
    virtual void createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj);
    virtual llvm::Value* getReferenceLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);
    virtual void createDestructorLlvm(LlvmObject* llvmObj);

    ValueKind valueKind;
    Type* type = nullptr;
    bool isConstexpr = false;
    bool wasCaptured = false;
    llvm::Value* llvmValue = nullptr;
    llvm::Value* llvmRef = nullptr;

private:
    static std::vector<std::unique_ptr<Value>> objects;
};

struct Variable : Value {
    Variable(const CodePosition& position, const std::string& name="");
    static Variable* Create(const CodePosition& position, const std::string& name="");
    void templateCopy(Variable* value, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);
    bool interpretTypeAndDeclaration(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* getReferenceLlvm(LlvmObject* llvmObj, bool ignoreReference);
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
    void templateCopy(IntegerValue* intValue, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
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
    void templateCopy(CharValue* charValue, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
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
    void templateCopy(FloatValue* floatValue, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
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
    void templateCopy(BoolValue* boolValue, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
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
    void templateCopy(StringValue* stringValue, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
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
    void templateCopy(StaticArrayValue* staticValue, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();
    virtual void createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj);
    virtual void createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj);
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* getReferenceLlvm(LlvmObject* llvmObj);
    virtual void createDestructorLlvm(LlvmObject* llvmObj);

    std::vector<Value*> values;
    bool wasInterpreted = false;
    
private:
    static std::vector<std::unique_ptr<StaticArrayValue>> objects;
};
struct FunctionValue : Value {
    FunctionValue(const CodePosition& position, Type* type, Scope* parentScope);
    static FunctionValue* Create(const CodePosition& position, Type* type, Scope* parentScope);
    void templateCopy(FunctionValue* value, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);
    llvm::Value* createLlvm(LlvmObject* llvmObj, const std::string& name);

    //std::vector<std::string> argumentNames;
    std::vector<Declaration*> arguments;
    
    FunctionScope* body = nullptr;
    llvm::Function* llvmFunction = nullptr;
    bool didSetLlvmName = false;

private:
    static std::vector<std::unique_ptr<FunctionValue>> objects;
};
struct NullValue : Value {
    NullValue(const CodePosition& position, Type* type);
    static NullValue* Create(const CodePosition& position, Type* type);
    void templateCopy(NullValue* value, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);
    llvm::Value* createLlvm(LlvmObject* llvmObj);

private:
    static std::vector<std::unique_ptr<NullValue>> objects;
};
struct TemplatedVariable : Value {
    TemplatedVariable(const CodePosition& position, const std::string& name="");
    static TemplatedVariable* Create(const CodePosition& position, const std::string& name="");
    void templateCopy(TemplatedVariable* value, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);

    std::string name;
    bool wasInterpreted = false;
    std::vector<Type*> templatedTypes;

private:
    static std::vector<std::unique_ptr<TemplatedVariable>> objects;
};
