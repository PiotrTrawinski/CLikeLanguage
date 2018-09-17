#pragma once

#include <unordered_map>
#include "operator==Utility.h"
#include "CodePosition.h"

struct Type;
struct Scope;

struct Statement {
    enum class Kind {
        Scope,
        Declaration,
        ClassDeclaration,
        Value
    };
    Statement(const CodePosition& position, Kind kind);
    static Statement* Create(const CodePosition& position, Kind kind);
    virtual bool operator==(const Statement& statement) const;
    void templateCopy(Statement* statement, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);

    Kind kind;
    CodePosition position;
    bool isReachable = true;

private:
    static std::vector<std::unique_ptr<Statement>> objects;
};