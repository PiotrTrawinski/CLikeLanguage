#pragma once

#include "Statement.h"
#include "Value.h"

struct AssignOperation;

struct Declaration : Statement {
    enum class Status {
        None,
        InEvaluation,
        Evaluated,
        Completed
    };
  
    Declaration(const CodePosition& position);
    static Declaration* Create(const CodePosition& position);
    void templateCopy(Declaration* declaration, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    bool interpret(Scope* scope, bool outOfOrder=false);
    virtual bool operator==(const Statement& declaration) const;
    void createAllocaLlvmIfNeeded(LlvmObject* llvmObj);
    void createLlvm(LlvmObject* llvmObj);

    bool isTemplateFunctionDeclaration = false;
    Variable* variable = nullptr;
    Value* value = nullptr;
    bool byReference = false;
    Status status = Status::None;
    Scope* scope = nullptr;
    llvm::Value* llvmVariable;
    AssignOperation* assignOperation = nullptr;
    
private:
    static std::vector<std::unique_ptr<Declaration>> objects;

    bool isFunctionDeclaration();
};