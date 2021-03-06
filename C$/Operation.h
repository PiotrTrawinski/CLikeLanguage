#pragma once

#include "Value.h"
#include "LlvmObject.h"
#include <algorithm>
#include <functional>

struct Operation : Value {
    enum class Kind {
        Add, Sub, Mul, Div, Mod,
        Minus,
        BitAnd, BitOr, BitNeg, BitXor,
        LogicalAnd, LogicalOr, LogicalXor, LogicalNot,
        LongCircuitLogicalAnd, LongCircuitLogicalOr,
        Eq, Neq,
        Gt, Lt, Gte, Lte,
        Shl, Shr, Sal, Sar,
        Assign,
        AddAssign, SubAssign, MulAssign, DivAssign, ModAssign,
        ShlAssign, ShrAssign, SalAssign, SarAssign, 
        BitNegAssign, BitOrAssign, BitXorAssign,
        Address, GetValue,
        Dot,
        ArrayIndex, ArraySubArray,
        Cast,
        FunctionCall,
        Deallocation,
        //ErrorCoding,
        Break,
        Remove,
        Continue,
        Return,
        ErrorResolve,
        Sizeof,
        Typesize,
        Constructor,
        Destroy,
        Move,
        LeftBracket // not really operator - only for convinience in reverse polish notation
    };

    Operation(const CodePosition& position, Kind kind);
    static Operation* Create(const CodePosition& position, Kind kind);
    void templateCopy(Operation* operation, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);

    static std::optional<Kind> stringToKind(const std::string& str);
    static std::string kindToFunctionName(Kind kind);
    static int priority(Kind kind);
    static bool isLeftAssociative(Kind kind);
    static int numberOfArguments(Kind kind);
    static std::string kindToString(Kind kind);
    int getPriority();
    bool getIsLeftAssociative();
    int getNumberOfArguments();

    bool interpretAllArguments(Scope* scope);
    virtual std::optional<Value*> createCustomFunctionOperation(Scope* scope);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();
    virtual void createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj);
    virtual void createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj);
    virtual llvm::Value* getReferenceLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);
    

    Kind kind;
    bool wasInterpreted = false;
    std::vector<Value*> arguments;
    ErrorResolveOperation* containedErrorResolve = nullptr;

    llvm::Value* shortCircuitLlvmVar = nullptr;

private:
    static std::vector<std::unique_ptr<Operation>> objects;

    std::optional<Value*> expandAssignOperation(Kind kind, Scope* scope);

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
        return nullptr;
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
        return nullptr;
    }

    template<typename Function> Value* tryEvaluate2ArgArithmetic(Function function, Scope* scope) {
        auto type1 = arguments[0]->type->getEffectiveType();
        auto type2 = arguments[1]->type->getEffectiveType();
        type = Type::getSuitingArithmeticType(type1, type2);
        if (type && arguments[0]->isConstexpr && arguments[1]->isConstexpr) {
            return evaluate(arguments[0], arguments[1], function);
        }
        if (type) {
            auto ctor1 = ConstructorOperation::Create(position, type, {arguments[0]});
            auto ctor1Interpret = ctor1->interpret(scope);
            if (!ctor1Interpret) internalError("constructing failed, but shouldn't", position);
            if (ctor1Interpret.value()) arguments[0] = ctor1Interpret.value();
            else arguments[0] = ctor1;

            auto ctor2 = ConstructorOperation::Create(position, type, {arguments[1]});
            auto ctor2Interpret = ctor2->interpret(scope);
            if (!ctor2Interpret) internalError("constructing failed, but shouldn't", position);
            if (ctor2Interpret.value()) arguments[1] = ctor2Interpret.value();
            else arguments[1] = ctor2;
        }
        return nullptr;
    }
    template<typename Function> Value* tryEvaluate2ArgArithmeticBoolTest(Function function, Scope* scope) {
        auto type1 = arguments[0]->type->getEffectiveType();
        auto type2 = arguments[1]->type->getEffectiveType();
        auto argType = Type::getSuitingArithmeticType(type1, type2);
        if (!argType) return nullptr;
        type = Type::Create(Type::Kind::Bool);
        if (arguments[0]->isConstexpr && arguments[1]->isConstexpr) {
            return BoolValue::Create(position, function(arguments[0], arguments[1]));
        }
        auto ctor1 = ConstructorOperation::Create(position, argType, {arguments[0]});
        auto ctor1Interpret = ctor1->interpret(scope);
        if (!ctor1Interpret) internalError("constructing failed, but shouldn't", position);
        if (ctor1Interpret.value()) arguments[0] = ctor1Interpret.value();
        else arguments[0] = ctor1;

        auto ctor2 = ConstructorOperation::Create(position, argType, {arguments[1]});
        auto ctor2Interpret = ctor2->interpret(scope);
        if (!ctor2Interpret) internalError("constructing failed, but shouldn't", position);
        if (ctor2Interpret.value()) arguments[1] = ctor2Interpret.value();
        else arguments[1] = ctor2;
        return nullptr;
    }
    template<typename Function> Value* tryEvaluate2ArgArithmeticIntegerOnly(Function function, Scope* scope) {
        auto type1 = arguments[0]->type->getEffectiveType();
        auto type2 = arguments[1]->type->getEffectiveType();
        type = Type::getSuitingArithmeticType(type1, type2);
        if (type && type->kind == Type::Kind::Float) {
            type = nullptr;
            return nullptr;
        }
        if (type && arguments[0]->isConstexpr && arguments[1]->isConstexpr) {
            return evaluateIntegerOnly(arguments[0], arguments[1], function);
        }
        if (type) {
            auto ctor1 = ConstructorOperation::Create(position, type, {arguments[0]});
            auto ctor1Interpret = ctor1->interpret(scope);
            if (!ctor1Interpret) internalError("constructing failed, but shouldn't", position);
            if (ctor1Interpret.value()) arguments[0] = ctor1Interpret.value();
            else arguments[0] = ctor1;

            auto ctor2 = ConstructorOperation::Create(position, type, {arguments[1]});
            auto ctor2Interpret = ctor2->interpret(scope);
            if (!ctor2Interpret) internalError("constructing failed, but shouldn't", position);
            if (ctor2Interpret.value()) arguments[1] = ctor2Interpret.value();
            else arguments[1] = ctor2;
        }
        return nullptr;
    }
};



struct CastOperation : Operation {
    CastOperation(const CodePosition& position, Type* argType);
    static CastOperation* Create(const CodePosition& position, Type* argType);
    void templateCopy(CastOperation* operation, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();
    virtual void createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj);
    virtual void createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj);
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* getReferenceLlvm(LlvmObject* llvmObj);

    Type* argType;
    bool argIsLValue = false;
    
private:
    static std::vector<std::unique_ptr<CastOperation>> objects;
};
struct ArrayIndexOperation : Operation {
    ArrayIndexOperation(const CodePosition& position, Value* index);
    static ArrayIndexOperation* Create(const CodePosition& position, Value* index);
    void templateCopy(ArrayIndexOperation* operation, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();
    virtual void createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj);
    virtual void createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj);
    virtual llvm::Value* getReferenceLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);

    Value* index;
    
private:
    static std::vector<std::unique_ptr<ArrayIndexOperation>> objects;

    template<typename T> std::optional<Value*> evaluateConstexprIntegerIndex(std::vector<Value*> staticArrayValues, uint64_t indexValue) {
        auto value = (T)indexValue;
        if (value >= staticArrayValues.size()) {
            return errorMessageOpt("array index outside the bounds of an array", position);
        }
        return staticArrayValues[value];
    }
};
struct ArraySubArrayOperation : Operation {
    ArraySubArrayOperation(const CodePosition& position, Value* firstIndex, Value* secondIndex);
    static ArraySubArrayOperation* Create(const CodePosition& position, Value* firstIndex, Value* secondIndex);
    void templateCopy(ArraySubArrayOperation* operation, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();
    virtual void createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj);
    virtual void createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj);
    virtual llvm::Value* getReferenceLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);

    Value* firstIndex;
    Value* secondIndex;
    Value* size = nullptr;

    llvm::Value* viewRef = nullptr;
    
private:
    static std::vector<std::unique_ptr<ArraySubArrayOperation>> objects;
};
struct FunctionCallOperation : Operation {
    FunctionCallOperation(const CodePosition& position);
    static FunctionCallOperation* Create(const CodePosition& position);
    void templateCopy(FunctionCallOperation* operation, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    //virtual std::unique_ptr<Value> copy();
    virtual void createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj);
    virtual void createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj);
    llvm::Value* createLlvmCall(LlvmObject* llvmObj);
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* getReferenceLlvm(LlvmObject* llvmObj);
    virtual void createDestructorLlvm(LlvmObject* llvmObj);

    Value* function = nullptr;
    std::string buildInFunctionName = "";
    FunctionValue* classConstructor = nullptr;
    std::string idName = "";

    std::vector<llvm::Value*> argRefs;
    std::vector<llvm::Value*> argValues;
    
private:
    enum FindFunctionStatus {
        Fail, Success, Error
    };
    FindFunctionStatus findFunction(Scope* scope, Scope* searchScope, std::string functionName, const std::vector<Type*>& templatedTypes={});
    bool createTemplateFunctionCall(Scope* scope, Declaration* declaration);

    static std::vector<std::unique_ptr<FunctionCallOperation>> objects;
};
struct FlowOperation : Operation {
    FlowOperation(const CodePosition& position, Kind kind);
    static FlowOperation* Create(const CodePosition& position, Kind kind);
    void templateCopy(FlowOperation* operation, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;

    Scope* scopePtr = nullptr;
    bool isReturnMaybeErrorVoidType = false;
    std::vector<Operation*> variablesDestructors;
    llvm::Value* llvmReturnValue = nullptr;
    Type* returnType = nullptr;

    virtual void createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj);
    virtual void createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj);
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);

private:
    static std::vector<std::unique_ptr<FlowOperation>> objects;
};

struct ErrorResolveOperation : Operation {
    ErrorResolveOperation(const CodePosition& position);
    static ErrorResolveOperation* Create(const CodePosition& position);
    void templateCopy(ErrorResolveOperation* operation, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual void createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj);
    virtual void createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj);
    virtual llvm::Value* getReferenceLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);
    virtual void createLlvmSuccessDestructor(LlvmObject* llvmObj);

    CodeScope* onErrorScope = nullptr;
    CodeScope* onSuccessScope = nullptr;
    Variable* errorValueVariable = nullptr;
    Declaration* errorValueDeclaration = nullptr;

    llvm::BasicBlock* llvmErrorBlock = nullptr;
    llvm::BasicBlock* llvmSuccessBlock = nullptr;

    llvm::Value* llvmErrorValue = nullptr;

private:
    static std::vector<std::unique_ptr<ErrorResolveOperation>> objects;
};

struct SizeofOperation : Operation {
    SizeofOperation(const CodePosition& position);
    static SizeofOperation* Create(const CodePosition& position);
    void templateCopy(SizeofOperation* operation, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual bool operator==(const Statement& value) const;
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);

    Type* argType = nullptr;

private:
    static std::vector<std::unique_ptr<SizeofOperation>> objects;
};

void createLlvmForEachLoop(LlvmObject* llvmObj, llvm::Value* sizeValue, std::function<void(llvm::Value*)> bodyFunction);
void createLlvmForEachLoop(LlvmObject* llvmObj, llvm::Value* start, llvm::Value* end, bool inlcudeEnd, std::function<void(llvm::Value*)> bodyFunction);
void createLlvmForEachLoopReversed(LlvmObject* llvmObj, llvm::Value* start, llvm::Value* end, std::function<void(llvm::Value*)> bodyFunction);
void createLlvmConditional(LlvmObject* llvmObj, llvm::Value* condition, std::function<void()> ifTrueFunction, std::function<void()> ifFalseFunction);
void createLlvmConditional(LlvmObject* llvmObj, llvm::Value* condition, std::function<void()> ifTrueFunction);

struct ConstructorOperation : Operation {
    ConstructorOperation(const CodePosition& position, Type* constructorType, std::vector<Value*> arguments={}, bool isHeapAllocation=false, bool isExplicit=false);
    static ConstructorOperation* Create(const CodePosition& position, Type* constructorType, std::vector<Value*> arguments={}, bool isHeapAllocation=false, bool isExplicit=false);
    void templateCopy(ConstructorOperation* operation, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);
    std::optional<Value*> interpret(Scope* scope, bool onlyTry, bool parentIsAssignment=false);
    void createAllocaLlvmIfNeededForConstructor(LlvmObject* llvmObj);
    void createAllocaLlvmIfNeededForAssignment(LlvmObject* llvmObj);
    void createLlvmConstructor(LlvmObject* llvmObj, llvm::Value* leftLlvmRef);
    void createLlvmAssignment(LlvmObject* llvmObj, llvm::Value* leftLlvmRef);
    virtual void createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj);
    virtual void createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj);
    virtual llvm::Value* getReferenceLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);
    virtual void createDestructorLlvm(LlvmObject* llvmObj);

    Type* constructorType = nullptr;
    bool isHeapAllocation = false;
    bool isExplicit = false;
    Value* typesize = nullptr;
    FunctionValue* classConstructor = nullptr;

private:
    static std::vector<std::unique_ptr<ConstructorOperation>> objects;
};

struct AssignOperation : Operation {
    AssignOperation(const CodePosition& position, bool forceConstruction=false);
    static AssignOperation* Create(const CodePosition& position, bool forceConstruction=false);
    void templateCopy(AssignOperation* operation, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual void createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj);
    virtual void createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj);
    virtual llvm::Value* getReferenceLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);
    virtual bool operator==(const Statement& value) const;

    bool isConstruction = false;

private:
    static std::vector<std::unique_ptr<AssignOperation>> objects;
};

struct DotOperation : Operation {
    DotOperation(const CodePosition& position);
    static DotOperation* Create(const CodePosition& position);
    void templateCopy(DotOperation* operation, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual std::optional<Value*> interpret(Scope* scope);
    virtual void createAllocaLlvmIfNeededForValue(LlvmObject* llvmObj);
    virtual void createAllocaLlvmIfNeededForReference(LlvmObject* llvmObj);
    virtual llvm::Value* getReferenceLlvm(LlvmObject* llvmObj);
    virtual llvm::Value* createLlvm(LlvmObject* llvmObj);
    virtual bool operator==(const Statement& value) const;

    bool isBuildInOperation = false;

private:
    static std::vector<std::unique_ptr<DotOperation>> objects;
};