#pragma once

#include "Statement.h"
#include "Value.h"

struct Declaration : Statement {
    enum class Status {
        None,
        InEvaluation,
        Evaluated,
        Completed
    };

    Declaration(const CodePosition& position);
    bool interpret(Scope* scope, bool outOfOrder=false);
    virtual bool operator==(const Statement& declaration) const;

    Variable variable;
    std::unique_ptr<Value> value;
    Status status = Status::None;

private:
    bool isFunctionDeclaration();
};