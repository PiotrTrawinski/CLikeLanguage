#pragma once
/*#include <unordered_set>
#include <vector>
#include <variant>
#include "Statement.h"
#include "Declaration.h"
#include "Scope.h"
#include "Value.h"
#include "Operation.h"*/

/*using StatementType = std::variant<
    Statement, 
    Declaration, 
    Value, 
    Variable, 
    IntegerValue, 
    CharValue,
    FloatValue,
    StringValue,
    StaticArrayValue,
    FunctionValue,
    Operation,
    CastOperation,
    ArrayIndexOperation,
    FunctionCallOperation,
    TemplateFunctionCallOperation,
    Scope,
    CodeScope,
    ClassScope,
    ForScope,
    WhileScope,
    IfScope,
    ElseScope,
    DeferScope
>;*/

//class StatementStorage {
//public:
/*
    Statement* addStatement(const CodePosition& position, Statement::Kind kind) {
        statements.emplace_back(position, kind);
        return &statements.back();
    }
    Declaration* addDeclaration(const CodePosition& position) {
        declarations.emplace_back(position);
        return &declarations.back();
    }
    Value* addValue(const CodePosition& position, Value::ValueKind valueKind) {
        values.emplace_back(position, valueKind);
        return &values.back();
    }
    Variable* addVariable(const CodePosition& position, const std::string& name="") {
        variables.emplace_back(position, name);
        return &variables.back();
    }
    IntegerValue* addIntegerValue(const CodePosition& position, int32_t value) {
        integerValues.emplace_back(position, value);
        return &integerValues.back();
    }
    CharValue* addCharValue(const CodePosition& position, uint8_t value) {
        charValues.emplace_back(position, value);
        return &charValues.back();
    }
    FloatValue* addFloatValue(const CodePosition& position, double value) {
        floatValues.emplace_back(position, value);
        return &floatValues.back();
    }
    StringValue* addStringValue(const CodePosition& position, const std::string& value) {
        stringValues.emplace_back(position, value);
        return &stringValues.back();
    }
    StaticArrayValue* addStaticArrayValue(const CodePosition& position) {
        staticArrayValues.emplace_back(position);
        return &staticArrayValues.back();
    }
    FunctionValue* addFunctionValue(const CodePosition& position, Type* type, Scope* parentScope) {
        functionValues.emplace_back(position, type, parentScope);
        return &functionValues.back();
    }
    Operation* addOperation(const CodePosition& position, Operation::Kind kind) {
        operations.emplace_back(position, kind);
        return &operations.back();
    }
    CastOperation* addCastOperation(const CodePosition& position, Type* argType) {
        castOperations.emplace_back(position, argType);
        return &castOperations.back();
    }
    ArrayIndexOperation* addArrayIndexOperation(const CodePosition& position, Value* index) {
        arrayIndexOperations.emplace_back(position, index);
        return &arrayIndexOperations.back();
    }
    ArraySubArrayOperation* addArraySubArrayOperation(const CodePosition& position, Value* firstIndex, Value* secondIndex) {
        arraySubArrayOperations.emplace_back(position, firstIndex, secondIndex);
        return &arraySubArrayOperations.back();
    }
    FunctionCallOperation* addFunctionCallOperation(const CodePosition& position) {
        functionCallOperations.emplace_back(position);
        return &functionCallOperations.back();
    }
    TemplateFunctionCallOperation* addTemplateFunctionCallOperation(const CodePosition& position) {
        templateFunctionCallOperations.emplace_back(position);
        return &templateFunctionCallOperations.back();
    }
    Scope* addScope(const CodePosition& position, Scope::Owner owner, Scope* parentScope) {
        scopes.emplace_back(position, parentScope);
        return &scopes.back();
    }
    CodeScope* addCodeScope(const CodePosition& position, Scope::Owner owner, Scope* parentScope, bool isGlobalScope=false) {
        codeScopes.emplace_back(position, parentScope);
        return &codeScopes.back();
    }
    ClassScope* addClassScope(const CodePosition& position, Scope* parentScope) {
        classScopes.emplace_back(position, parentScope);
        return &classScopes.back();
    }
    ForScope* addForScope(const CodePosition& position, Scope* parentScope) {
        forScopes.emplace_back(position, parentScope);
        return &forScopes.back();
    }
    WhileScope* addWhileScope(const CodePosition& position, Scope* parentScope) {
        whileScopes.emplace_back(position, parentScope);
        return &whileScopes.back();
    }
    IfScope* addIfScope(const CodePosition& position, Scope* parentScope) {
        ifScopes.emplace_back(position, parentScope);
        return &ifScopes.back();
    }
    ElseScope* addElseScope(const CodePosition& position, Scope* parentScope) {
        elseScopes.emplace_back(position, parentScope);
        return &elseScopes.back();
    }
    DeferScope* addDeferScope(const CodePosition& position, Scope* parentScope) {
        deferScopes.emplace_back(position, parentScope);
        return &deferScopes.back();
    }
    */
//private:
    //std::vector<StatementType> statements;
    
    /*std::vector<Statement> statements;
    std::vector<Declaration> declarations;
    std::vector<Value> values;
    std::vector<Variable> variables;
    std::vector<IntegerValue> integerValues;
    std::vector<CharValue> charValues;
    std::vector<FloatValue> floatValues;
    std::vector<StringValue> stringValues;
    std::vector<StaticArrayValue> staticArrayValues;
    std::vector<FunctionValue> functionValues;
    std::vector<Operation> operations;
    std::vector<CastOperation> castOperations;
    std::vector<ArrayIndexOperation> arrayIndexOperations;
    std::vector<ArraySubArrayOperation> arraySubArrayOperations;
    std::vector<FunctionCallOperation> functionCallOperations;
    std::vector<TemplateFunctionCallOperation> templateFunctionCallOperations;
    std::vector<Scope> scopes;
    std::vector<CodeScope> codeScopes;
    std::vector<ClassScope> classScopes;
    std::vector<ForScope> forScopes;
    std::vector<WhileScope> whileScopes;
    std::vector<IfScope> ifScopes;
    std::vector<ElseScope> elseScopes;
    std::vector<DeferScope> deferScopes;*/
    

    /*
    std::unordered_set<Type> types;
    std::unordered_set<OwnerPointerType> ownerPointerTypes;
    std::unordered_set<RawPointerType> rawPointerTypes;
    std::unordered_set<MaybeErrorType> maybeErrorTypes;
    std::unordered_set<ReferenceType> referenceTypes;
    std::unordered_set<StaticArrayType> staticArrayTypes;
    std::unordered_set<DynamicArrayType> dynamicArrayTypes;
    std::unordered_set<ArrayViewType> arrayViewTypes;
    std::unordered_set<ClassType> classTypes;
    std::unordered_set<FunctionType> functionTypes;
    std::unordered_set<IntegerType> integerTypes;
    std::unordered_set<FloatType> floatTypes;
    std::unordered_set<TemplateType> templateTypes;
    std::unordered_set<TemplateFunctionType> templateFunctionTypes;
    */
//};