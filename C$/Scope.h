#pragma once

#include <variant>
#include <vector>
#include <optional>
#include <unordered_set>

#include "Statement.h"
#include "Token.h"
#include "errorMessages.h"
#include "keywords.h"
#include "DeclarationMap.h"
#include "ClassDeclarationMap.h"
#include "llvmObject.h"

struct Operation;
struct Variable;
struct Value;
struct ErrorResolveOperation;

struct Scope : Statement {
    enum class Owner {
        None,
        Function,
        Class,
        For,
        While,
        If,
        Else,
        Defer,
        OnError,
        OnSuccess
    };
    struct ReadStatementValue {
        ReadStatementValue(Statement* statement) {
            this->statement = statement;
        }
        ReadStatementValue(bool isScopeEnd) : isScopeEnd(isScopeEnd) {}

        operator bool() {
            return statement || isScopeEnd;
        }

        Statement* statement = nullptr;
        bool isScopeEnd = false;
    };

    Scope(const CodePosition& position, Owner ownmaybeUninitializedDeclarationser, Scope* parentScope);
    void templateCopy(Scope* scope, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    ReadStatementValue readStatement(const std::vector<Token>& tokens, int& i);
    bool addArguments(Operation* operation, const std::vector<Token>& tokens, int& i);
    bool  addConstructorOperation(std::vector<Value*>& out, const std::vector<Token>& tokens, int& i, bool isHeapAllocation=false);
    std::optional<std::vector<Value*>> getReversePolishNotation(const std::vector<Token>& tokens, int& i, bool canBeFunction);
    Type* getType(const std::vector<Token>& tokens, int& i, const std::vector<std::string>& delimiters, bool writeError=true);
    Value* getValue(const std::vector<Token>& tokens, int& i, const std::vector<std::string>& delimiters, bool skipOnGoodDelimiter=false, bool canBeFunction=true);
    std::optional<std::vector<Type*>> getFunctionArgumentTypes(const std::vector<Token>& tokens, int& i, bool writeError);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i)=0;
    virtual bool interpret()=0;
    virtual Declaration* findAndInterpretDeclaration(const std::string& name)=0;
    Declaration* findDeclaration(Variable* variable, bool ignoreClassScopes=false);
    virtual bool operator==(const Statement& scope) const;
    virtual void createLlvm(LlvmObject* llvmObj)=0;
    virtual std::unordered_set<Declaration*> getUninitializedDeclarations();
    virtual bool getHasReturnStatement();
    virtual std::unordered_map<Declaration*, bool> getDeclarationsInitState();
    virtual void allocaAllDeclarationsLlvm(LlvmObject* llvmObj)=0;

    Scope* parentScope; // nullptr if and only if global scope
    FunctionValue* mainFunction = nullptr; // other then nullptr if and only if global scope
    Owner owner;
    DeclarationMap declarationMap;
    ClassDeclarationMap classDeclarationMap;
    int id = -1;
    std::unordered_set<Declaration*> parentMaybeUninitializedDeclarations;
    std::unordered_set<Declaration*> maybeUninitializedDeclarations;
    std::unordered_map<Declaration*, bool> declarationsInitState;
    std::vector<Declaration*> declarationsOrder;
    std::vector<Value*> valuesToDestroyBuffer;
    bool hasReturnStatement = false;

    Scope* onErrorScopeToInterpret = nullptr;
    Scope* onSuccessScopeToInterpret = nullptr;

protected:
    static int ID_COUNT;
};

struct CodeScope : Scope {
    CodeScope(const CodePosition& position, Scope::Owner owner, Scope* parentScope, bool isGlobalScope=false);
    static CodeScope* Create(const CodePosition& position, Scope::Owner owner, Scope* parentScope, bool isGlobalScope=false);
    void templateCopy(CodeScope* scope, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    bool interpretNoUnitializedDeclarationsSet();
    virtual bool interpret();
    virtual Declaration* findAndInterpretDeclaration(const std::string& name);
    virtual bool operator==(const Statement& scope) const;
    virtual void allocaAllDeclarationsLlvm(LlvmObject* llvmObj);
    virtual void createLlvm(LlvmObject* llvmObj);

    bool isGlobalScope;
    std::vector<Statement*> statements;

    std::unordered_map<Statement*, std::vector<Value*>> valuesToDestroyAfterStatement;
    std::unordered_map<Statement*, ErrorResolveOperation*> errorResolveAfterStatement;
    
private:
    static std::vector<std::unique_ptr<CodeScope>> objects;
};
struct GlobalScope : CodeScope {
    static GlobalScope Instance;

    bool setCDeclaration(const CodePosition& codePosition, const std::string& name, Type* type);
    virtual bool interpret();
    virtual void createLlvm(LlvmObject* llvmObj);

private:
    GlobalScope();
    std::vector<Declaration*> cDeclarations;
};
struct FunctionValue;
struct FunctionScope : CodeScope {
    FunctionScope(const CodePosition& position, Scope* parentScope, FunctionValue* function);
    static FunctionScope* Create(const CodePosition& position, Scope* parentScope, FunctionValue* function);
    void templateCopy(FunctionScope* scope, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool operator==(const Statement& scope) const;
    virtual bool interpret();
    virtual void createLlvm(LlvmObject* llvmObj);
    virtual std::unordered_set<Declaration*> getUninitializedDeclarations();

    bool wasInterpreted = false;
    FunctionValue* function;

private:
    static std::vector<std::unique_ptr<FunctionScope>> objects;
};
struct ClassDeclaration;
struct ClassScope : Scope {
    ClassScope(const CodePosition& position, Scope* parentScope);
    static ClassScope* Create(const CodePosition& position, Scope* parentScope);
    void templateCopy(ClassScope* scope, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    virtual bool interpret();
    virtual Declaration* findAndInterpretDeclaration(const std::string& name);
    virtual bool operator==(const Statement& scope) const;
    virtual void allocaAllDeclarationsLlvm(LlvmObject* llvmObj);
    virtual void createLlvm(LlvmObject* llvmObj);
    virtual std::unordered_set<Declaration*> getUninitializedDeclarations();
    virtual bool getHasReturnStatement();

    bool wasInterpreted = false;
    std::vector<Declaration*> declarations;
    FunctionValue* inlineConstructors = nullptr;
    FunctionValue* copyConstructor = nullptr;
    FunctionValue* operatorEq = nullptr;
    std::vector<FunctionValue*> constructors;
    FunctionValue* inlineDestructors = nullptr;
    FunctionValue* destructor = nullptr;
    ClassDeclaration* classDeclaration = nullptr;
    
private:
    static std::vector<std::unique_ptr<ClassScope>> objects;
};

struct ForIterData {
    bool operator==(const ForIterData& other) const;

    Variable* iterVariable = nullptr;
    Value* firstValue = nullptr;
    Value* step = nullptr;
    Value* lastValue = nullptr;
    Declaration* iterDeclaration = nullptr;
    Value* stepOperation = nullptr;
    Value* conditionOperation = nullptr;
};
struct ForEachData {
    bool operator==(const ForEachData& other) const;

    Value* arrayValue = nullptr;
    Variable* it = nullptr;
    Variable* index = nullptr;
    Declaration* itDeclaration = nullptr;
    Declaration* indexDeclaration = nullptr;
};
struct ForScope : CodeScope {
    ForScope(const CodePosition& position, Scope* parentScope);
    static ForScope* Create(const CodePosition& position, Scope* parentScope);
    void templateCopy(ForScope* scope, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    bool setForLoopDirection(const std::vector<Token>& tokens, int& i);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    virtual bool interpret();
    virtual bool operator==(const Statement& scope) const;
    virtual std::unordered_set<Declaration*> getUninitializedDeclarations();
    virtual bool getHasReturnStatement();
    virtual void allocaAllDeclarationsLlvm(LlvmObject* llvmObj);
    virtual void createLlvm(LlvmObject* llvmObj);

    std::variant<ForIterData, ForEachData> data;
    bool loopForward = true;

    llvm::BasicBlock* llvmStepBlock = nullptr;
    llvm::BasicBlock* llvmAfterBlock = nullptr;
    
private:
    static std::vector<std::unique_ptr<ForScope>> objects;
};
struct WhileScope : CodeScope {
    WhileScope(const CodePosition& position, Scope* parentScope);
    static WhileScope* Create(const CodePosition& position, Scope* parentScope);
    void templateCopy(WhileScope* scope, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    virtual bool interpret();
    bool findBreakStatement(CodeScope* scope);
    virtual bool operator==(const Statement& scope) const;
    virtual std::unordered_set<Declaration*> getUninitializedDeclarations();
    virtual bool getHasReturnStatement();
    virtual void createLlvm(LlvmObject* llvmObj);

    Value* conditionExpression = nullptr;

    llvm::BasicBlock* llvmConditionBlock = nullptr;
    llvm::BasicBlock* llvmAfterBlock = nullptr;
    
private:
    static std::vector<std::unique_ptr<WhileScope>> objects;
};
struct IfScope : CodeScope {
    IfScope(const CodePosition& position, Scope* parentScope);
    static IfScope* Create(const CodePosition& position, Scope* parentScope);
    void templateCopy(IfScope* scope, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    virtual bool interpret();
    virtual bool operator==(const Statement& scope) const;
    virtual std::unordered_set<Declaration*> getUninitializedDeclarations();
    virtual bool getHasReturnStatement();
    virtual std::unordered_map<Declaration*, bool> getDeclarationsInitState();
    virtual void createLlvm(LlvmObject* llvmObj);

    Value* conditionExpression = nullptr;
    CodeScope* elseScope = nullptr;
    
private:
    static std::vector<std::unique_ptr<IfScope>> objects;
};
struct ElseScope : CodeScope {
    ElseScope(const CodePosition& position, Scope* parentScope);
    static ElseScope* Create(const CodePosition& position, Scope* parentScope);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);

private:
    static std::vector<std::unique_ptr<ElseScope>> objects;
};
struct DeferScope : CodeScope {
    DeferScope(const CodePosition& position, Scope* parentScope);
    static DeferScope* Create(const CodePosition& position, Scope* parentScope);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    virtual bool interpret();
    virtual std::unordered_set<Declaration*> getUninitializedDeclarations();
    virtual bool getHasReturnStatement();
    virtual std::unordered_map<Declaration*, bool> getDeclarationsInitState();

    std::unordered_map<Declaration*, bool> declarationsInitStateCopy;
    std::vector<Declaration*> declarationsOrderCopy;

private:
    static std::vector<std::unique_ptr<DeferScope>> objects;
};
