#pragma once
#include "Statement.h"
#include "LlvmObject.h"

struct ClassScope;
struct Type;

struct ClassDeclaration : Statement {
    enum class Status {
        None,
        InEvaluation,
        Evaluated,
    };

    ClassDeclaration(const CodePosition& position, std::string name);
    static ClassDeclaration* Create(const CodePosition& position, std::string name);
    bool interpret();
    virtual bool operator==(const Statement& declaration) const;

    std::string name;
    ClassScope* body = nullptr;
    std::vector<Type*> templateTypes;
    Status status = Status::None;

    // LLVM:
    void createLlvm(LlvmObject* llvmObj);
    llvm::StructType* getLlvmType(LlvmObject* llvmObj);
    llvm::StructType* llvmType = nullptr;

private:
    static std::vector<std::unique_ptr<ClassDeclaration>> objects;
};