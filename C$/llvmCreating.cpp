#include "llvmCreating.h"

using namespace std;

unique_ptr<LlvmObject> createLlvm(CodeScope* globalScope) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    auto llvmObj = make_unique<LlvmObject>();

    auto module = ::make_unique<llvm::Module>("module", llvmObj->context);
    llvmObj->module = module.get();

    globalScope->createLlvm(llvmObj.get());

    string errStr;
    llvmObj->executionEngine = llvm::EngineBuilder(move(module)).setErrorStr(&errStr).create();

    if (!llvmObj->executionEngine) {
        internalError("Failed to construct LLVM-ExecutionEngine");
    }

    if (verifyModule(*llvmObj->module)) {
        internalError("Failed to construct LLVM-Model");
    }

    return llvmObj;
}