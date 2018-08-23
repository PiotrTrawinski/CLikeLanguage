#include "llvmCreating.h"

using namespace std;

unique_ptr<LlvmObject> createLlvm(CodeScope* globalScope) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    auto llvmObj = make_unique<LlvmObject>();

    auto module = ::make_unique<llvm::Module>("module", llvmObj->context);
    llvmObj->module = module.get();

    llvmObj->mallocFunction = llvm::cast<llvm::Function>(llvmObj->module->getOrInsertFunction("malloc", 
        llvm::FunctionType::get(
            llvm::PointerType::get(llvm::Type::getInt8Ty(llvmObj->context), 0), 
            {llvm::Type::getInt64Ty(llvmObj->context)}, 
            false
        )
    ));
    llvmObj->freeFunction = llvm::cast<llvm::Function>(llvmObj->module->getOrInsertFunction("free", 
        llvm::FunctionType::get(
            llvm::Type::getVoidTy(llvmObj->context),
            {llvm::PointerType::get(llvm::Type::getInt8Ty(llvmObj->context), 0)},
            false
        )
    ));

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