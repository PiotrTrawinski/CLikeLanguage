#include <iostream>
#include "parsing.h"
#include "codeTreeCreating.h"
#include "interpreting.h"
#include "llvmCreating.h"
#include "Value.h"
#include <chrono>
#include <iomanip>

using namespace std;
using namespace std::chrono;

double nanoToSec(long long nanoSecs) {
    return nanoSecs / 1000000000.0;
}

int main(int argc, char** argv) {
    cout << setprecision(5) << fixed;

    if (argc < 2) {
        cerr << "You didn't provide a file to compile\n";
        return 1;
    }

    auto start = high_resolution_clock::now();
    auto tokens = parseFile(argv[1]);
    auto parsingTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    if (!tokens) {
        cerr << "Compiling failed: there were errors during parsing\n";
        return 2;
    }
    
    
    start = high_resolution_clock::now();
    auto globalScope = createCodeTree(tokens.value());
    auto codeTreeCreatingTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    if (!globalScope) {
        cerr << "Compiling failed: there were errors during code tree creating\n";
        return 3;
    }

    start = high_resolution_clock::now();
    bool statusInterpreting = interpret(globalScope);
    auto interpretingTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    if (!statusInterpreting) {
        cerr << "Compiling failed: there were errors during interpreting\n";
        return 4;
    }

    if (argc >= 3 && !strcmp(argv[2], "-nollvm")) {
        cout << "--------------------------------------\n";
        cout << "| Compiling completed                |\n";
        cout << "| - time : " << parsingTime + codeTreeCreatingTime + interpretingTime << "[s]                |\n";
        cout << "| -- parsing            : " << parsingTime          << "[s] |\n";
        cout << "| -- code tree creating : " << codeTreeCreatingTime << "[s] |\n";
        cout << "| -- interpreting       : " << interpretingTime     << "[s] |\n";
        cout << "--------------------------------------\n";
        return 0;
    }

    start = high_resolution_clock::now();
    auto llvmObj = createLlvm(globalScope);
    auto llvmCreateTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    if (!llvmObj) {
        cerr << "Compiling failed: there were errors during llvm creating\n";
        return 5;
    }

    auto mainFunction = (llvm::Function*)globalScope->mainFunction->createLlvm(llvmObj.get());
    mainFunction->setName("main");

    std::error_code ec;
    llvm::raw_fd_ostream llvmCodeFile("llvmCode.ll", ec, llvm::sys::fs::OpenFlags(0));
    llvmCodeFile << *llvmObj->module;

    cout << "--------------------------------------\n";
    cout << "| Compiling completed                |\n";
    cout << "| - time : " << parsingTime + codeTreeCreatingTime + interpretingTime + llvmCreateTime << "[s]                |\n";
    cout << "| -- parsing            : " << parsingTime          << "[s] |\n";
    cout << "| -- code tree creating : " << codeTreeCreatingTime << "[s] |\n";
    cout << "| -- interpreting       : " << interpretingTime     << "[s] |\n";
    cout << "| -- llvm creating      : " << llvmCreateTime       << "[s] |\n";
    cout << "--------------------------------------\n";

    if (argc < 3 || (argc >= 3 && strcmp(argv[2], "-run"))) {
        return 0;
    }

    cout << "\n";
    cout << "-------------- RUN TIME --------------\n";
    start = high_resolution_clock::now();
    auto result = llvmObj->executionEngine->runFunctionAsMain(mainFunction, {}, nullptr);
    auto runTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    cout << "--------------------------------------\n";
    cout << "| Run completed\n";
    cout << "| - return value : " << result << '\n';
    cout << "| - run time     : " << runTime << "[s]\n";
    cout << "--------------------------------------\n";

    return 0;
}