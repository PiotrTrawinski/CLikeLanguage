#pragma once
#include "llvmInclude.h"

struct LlvmObject {
    llvm::LLVMContext context;
    llvm::ExecutionEngine* executionEngine;
    llvm::Module* module;
};