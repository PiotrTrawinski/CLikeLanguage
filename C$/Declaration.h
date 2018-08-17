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
    static Declaration* Create(const CodePosition& position);
    bool interpret(Scope* scope, bool outOfOrder=false);
    virtual bool operator==(const Statement& declaration) const;
    void createLlvm(LlvmObject* llvmObj);

    Variable* variable = nullptr;
    Value* value = nullptr;
    bool byReference = false;
    Status status = Status::None;
    Scope* scope = nullptr;
    llvm::Value* llvmVariable;
    
private:
    static std::vector<std::unique_ptr<Declaration>> objects;

    bool isFunctionDeclaration();
};