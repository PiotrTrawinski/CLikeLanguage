#pragma once

#include "operator==Utility.h"
#include "CodePosition.h"

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

    Kind kind;
    CodePosition position;

private:
    static std::vector<std::unique_ptr<Statement>> objects;
};
