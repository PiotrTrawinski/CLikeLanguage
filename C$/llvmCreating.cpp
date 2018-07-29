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
        internalError("Failed to construct LLVM-ExecutionEngine", CodePosition(nullptr,0,0));
        return nullptr;
    }

    if (verifyModule(*llvmObj->module)) {
        internalError("Failed to construct LLVM-Model", CodePosition(nullptr,0,0));
        return nullptr;
    }

    std::error_code ec;
    llvm::raw_fd_ostream llvmCodeFile("llvmCode.ll", ec, llvm::sys::fs::OpenFlags(0));
    llvmCodeFile << *llvmObj->module;

    return llvmObj;
}