#pragma once
#include "llvmInclude.h"

struct LlvmObject {
    llvm::LLVMContext context;
    llvm::ExecutionEngine* executionEngine = nullptr;
    llvm::Module* module = nullptr;
    llvm::Function* function = nullptr;
    llvm::BasicBlock* block = nullptr;
    llvm::Function* mallocFunction = nullptr;
    llvm::Function* freeFunction = nullptr;
    llvm::Function* reallocFunction = nullptr;
};