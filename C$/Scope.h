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
    ReadStatementValue readStatement(const std::vector<Token>& tokens, int& i);
    std::optional<std::vector<Value*>> getReversePolishNotation(const std::vector<Token>& tokens, int& i);
    Type* getType(const std::vector<Token>& tokens, int& i, const std::vector<std::string>& delimiters, bool writeError=true);
    Value* getValue(const std::vector<Token>& tokens, int& i, const std::vector<std::string>& delimiters, bool skipOnGoodDelimiter=false);
    std::optional<std::vector<Type*>> getFunctionArgumentTypes(const std::vector<Token>& tokens, int& i, bool writeError);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i)=0;
    virtual bool interpret()=0;
    virtual Declaration* findAndInterpretDeclaration(const std::string& name)=0;
    Declaration* findDeclaration(Variable* variable);
    virtual bool operator==(const Statement& scope) const;
    virtual void createLlvm(LlvmObject* llvmObj)=0;
    virtual std::unordered_set<Declaration*> getUninitializedDeclarations();
    virtual bool getHasReturnStatement();
    virtual std::unordered_map<Declaration*, bool> getDeclarationsInitState();

    Scope* parentScope; // nullptr if and only if global scope
    Owner owner;
    DeclarationMap declarationMap;
    ClassDeclarationMap classDeclarationMap;
    int id = -1;
    std::unordered_set<Declaration*> parentMaybeUninitializedDeclarations;
    std::unordered_set<Declaration*> maybeUninitializedDeclarations;
    std::unordered_map<Declaration*, bool> declarationsInitState;
    std::vector<Declaration*> declarationsOrder;
    bool hasReturnStatement = false;

    Scope* onErrorScopeToInterpret = nullptr;
    Scope* onSuccessScopeToInterpret = nullptr;

protected:
    static int ID_COUNT;
};

struct CodeScope : Scope {
    CodeScope(const CodePosition& position, Scope::Owner owner, Scope* parentScope, bool isGlobalScope=false);
    static CodeScope* Create(const CodePosition& position, Scope::Owner owner, Scope* parentScope, bool isGlobalScope=false);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    bool interpretNoUnitializedDeclarationsSet();
    virtual bool interpret();
    virtual Declaration* findAndInterpretDeclaration(const std::string& name);
    virtual bool operator==(const Statement& scope) const;
    virtual void createLlvm(LlvmObject* llvmObj);

    bool isGlobalScope;
    std::vector<Statement*> statements;
    
private:
    static std::vector<std::unique_ptr<CodeScope>> objects;
};
struct FunctionValue;
struct FunctionScope : CodeScope {
    FunctionScope(const CodePosition& position, Scope* parentScope, FunctionValue* function);
    static FunctionScope* Create(const CodePosition& position, Scope* parentScope, FunctionValue* function);
    virtual bool operator==(const Statement& scope) const;
    virtual void createLlvm(LlvmObject* llvmObj);
    virtual std::unordered_set<Declaration*> getUninitializedDeclarations();

    FunctionValue* function;

private:
    static std::vector<std::unique_ptr<FunctionScope>> objects;
};
struct ClassDeclaration;
struct ClassScope : Scope {
    ClassScope(const CodePosition& position, Scope* parentScope);
    static ClassScope* Create(const CodePosition& position, Scope* parentScope);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    virtual bool interpret();
    virtual Declaration* findAndInterpretDeclaration(const std::string& name);
    virtual bool operator==(const Statement& scope) const;
    virtual void createLlvm(LlvmObject* llvmObj);
    virtual std::unordered_set<Declaration*> getUninitializedDeclarations();
    virtual bool getHasReturnStatement();

    std::vector<Declaration*> declarations;
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
};
struct ForEachData {
    bool operator==(const ForEachData& other) const;

    Value* arrayValue = nullptr;
    Variable* it = nullptr;
    Variable* index = nullptr;
};
struct ForScope : CodeScope {
    ForScope(const CodePosition& position, Scope* parentScope);
    static ForScope* Create(const CodePosition& position, Scope* parentScope);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    virtual bool interpret();
    virtual bool operator==(const Statement& scope) const;
    virtual std::unordered_set<Declaration*> getUninitializedDeclarations();
    virtual bool getHasReturnStatement();

    std::variant<ForIterData, ForEachData> data;
    
private:
    static std::vector<std::unique_ptr<ForScope>> objects;
};
struct WhileScope : CodeScope {
    WhileScope(const CodePosition& position, Scope* parentScope);
    static WhileScope* Create(const CodePosition& position, Scope* parentScope);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    virtual bool interpret();
    bool findBreakStatement(CodeScope* scope);
    virtual bool operator==(const Statement& scope) const;
    virtual std::unordered_set<Declaration*> getUninitializedDeclarations();
    virtual bool getHasReturnStatement();

    Value* conditionExpression = nullptr;
    
private:
    static std::vector<std::unique_ptr<WhileScope>> objects;
};
struct IfScope : CodeScope {
    IfScope(const CodePosition& position, Scope* parentScope);
    static IfScope* Create(const CodePosition& position, Scope* parentScope);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    virtual bool interpret();
    virtual bool operator==(const Statement& scope) const;
    virtual std::unordered_set<Declaration*> getUninitializedDeclarations();
    virtual bool getHasReturnStatement();
    virtual std::unordered_map<Declaration*, bool> getDeclarationsInitState();

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
