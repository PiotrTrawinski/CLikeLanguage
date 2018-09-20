#pragma once
#include "Statement.h"
#include "LlvmObject.h"

struct ClassScope;
struct Type;
struct TemplateType;

struct ClassDeclaration : Statement {
    enum class Status {
        None,
        InEvaluation,
        Evaluated,
    };

    ClassDeclaration(const CodePosition& position, std::string name);
    static ClassDeclaration* Create(const CodePosition& position, std::string name);
    void templateCopy(ClassDeclaration* classDeclaration, Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    virtual Statement* templateCopy(Scope* parentScope, const std::unordered_map<std::string, Type*>& templateToType);
    ClassDeclaration* get(const std::vector<Type*>& classTemplateTypes);
    bool interpret(const std::vector<Type*>& classTemplateTypes);
    bool interpretAllImplementations();
    virtual bool operator==(const Statement& declaration) const;

    std::string name;
    ClassScope* body = nullptr;
    std::vector<TemplateType*> templateTypes;
    std::vector<std::pair<std::vector<Type*>, ClassDeclaration*>> implementations;
    Status status = Status::None;

    // LLVM:
    void createLlvm(LlvmObject* llvmObj);
    void createLlvmBody(LlvmObject* llvmObj);
    llvm::StructType* getLlvmType(LlvmObject* llvmObj);
    llvm::StructType* llvmType = nullptr;

private:
    static std::vector<std::unique_ptr<ClassDeclaration>> objects;
};