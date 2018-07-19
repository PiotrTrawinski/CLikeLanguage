#pragma once

#include <variant>
#include <vector>
#include <optional>

#include "Statement.h"
#include "Token.h"
#include "errorMessages.h"
#include "keywords.h"
#include "DeclarationMap.h"

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
        Defer
    };
    struct ReadStatementValue {
        ReadStatementValue(std::unique_ptr<Statement>&& statement) : statement(std::move(statement)) {}
        ReadStatementValue(bool isScopeEnd) : isScopeEnd(isScopeEnd) {}

        operator bool() {
            return statement || isScopeEnd;
        }

        std::unique_ptr<Statement> statement = nullptr;
        bool isScopeEnd = false;
    };

    Scope(const CodePosition& position, Owner owner, Scope* parentScope);
    ReadStatementValue readStatement(const std::vector<Token>& tokens, int& i);
    std::optional<std::vector<std::unique_ptr<Value>>> getReversePolishNotation(const std::vector<Token>& tokens, int& i);
    std::unique_ptr<Type> getType(const std::vector<Token>& tokens, int& i, const std::vector<std::string>& delimiters, bool writeError=true);
    std::unique_ptr<Value> getValue(const std::vector<Token>& tokens, int& i, const std::vector<std::string>& delimiters, bool skipOnGoodDelimiter=false);
    std::optional<std::vector<std::unique_ptr<Type>>> getFunctionArgumentTypes(const std::vector<Token>& tokens, int& i, bool writeError);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i)=0;
    virtual bool interpret()=0;
    virtual Declaration* findAndInterpretDeclaration(const std::string& name)=0;
    Declaration* findDeclaration(Variable* variable);
    virtual bool operator==(const Statement& scope) const;
  
    Scope* parentScope; // nullptr if and only if global scope
    Owner owner;
    DeclarationMap declarationMap;
};

struct CodeScope : Scope {
    CodeScope(const CodePosition& position, Scope::Owner owner, Scope* parentScope, bool isGlobalScope=false);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    virtual bool interpret();
    virtual Declaration* findAndInterpretDeclaration(const std::string& name);
    virtual bool operator==(const Statement& scope) const;

    bool isGlobalScope;
    std::vector<std::unique_ptr<Statement>> statements;
};
struct ClassScope : Scope {
    ClassScope(const CodePosition& position, Scope* parentScope);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    virtual bool interpret();
    virtual Declaration* findAndInterpretDeclaration(const std::string& name);
    virtual bool operator==(const Statement& scope) const;

    std::string name;
    std::vector<std::unique_ptr<TemplateType>> templateTypes;
    std::vector<std::unique_ptr<Declaration>> declarations;
};

struct ForIterData {
    bool operator==(const ForIterData& other) const;

    std::unique_ptr<Variable> iterVariable;
    std::unique_ptr<Value> firstValue;
    std::unique_ptr<Value> step;
    std::unique_ptr<Value> lastValue;
};
struct ForEachData {
    bool operator==(const ForEachData& other) const;

    std::unique_ptr<Value> arrayValue;
    std::unique_ptr<Variable> it;
    std::unique_ptr<Variable> index;
};
struct ForScope : CodeScope {
    ForScope(const CodePosition& position, Scope* parentScope);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    virtual bool operator==(const Statement& scope) const;

    std::variant<ForIterData, ForEachData> data;
};
struct WhileScope : CodeScope {
    WhileScope(const CodePosition& position, Scope* parentScope);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    virtual bool operator==(const Statement& scope) const;

    std::unique_ptr<Value> conditionExpression;
};
struct IfScope : CodeScope {
    IfScope(const CodePosition& position, Scope* parentScope);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
    virtual bool operator==(const Statement& scope) const;

    std::unique_ptr<Value> conditionExpression;
    std::unique_ptr<CodeScope> elseScope;
};
struct ElseScope : CodeScope {
    ElseScope(const CodePosition& position, Scope* parentScope);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
};
struct DeferScope : CodeScope {
    DeferScope(const CodePosition& position, Scope* parentScope);
    virtual bool createCodeTree(const std::vector<Token>& tokens, int& i);
};
