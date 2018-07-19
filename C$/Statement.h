#pragma once

#include "operator==Utility.h"
#include "CodePosition.h"

struct Statement {
    enum class Kind {
        Scope,
        Declaration,
        Value
    };

    Statement(const CodePosition& position, Kind kind);
    virtual bool operator==(const Statement& statement) const;

    Kind kind;
    CodePosition position;
};
