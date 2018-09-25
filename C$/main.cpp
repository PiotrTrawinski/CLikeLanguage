#include <iostream>
#include "parsing.h"
#include "codeTreeCreating.h"
#include "llvmCreating.h"
#include "Value.h"
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <Windows.h>

using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;

double nanoToSec(long long nanoSecs) {
    return nanoSecs / 1000000000.0;
}

int main(int argc, char** argv) {
    cout << setprecision(5) << fixed;

    if (argc < 2) {
        cerr << "You didn't provide a file to compile\n";
        return 1;
    }
    string filePath = argv[1];
    string fileName = fs::path(filePath).filename().replace_extension().u8string();

    auto start = high_resolution_clock::now();
    auto tokens = parseFile(filePath);
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
    bool statusInterpreting = globalScope->interpret();
    auto interpretingTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    if (!statusInterpreting) {
        cerr << "Compiling failed: there were errors during interpreting\n";
        return 4;
    }

    start = high_resolution_clock::now();
    auto llvmObj = createLlvm(globalScope);
    auto mainFunction = (llvm::Function*)globalScope->mainFunction->createLlvm(llvmObj.get());
    mainFunction->setName("main");
    auto llvmCreateTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    if (!llvmObj) {
        cerr << "Compiling failed: there were errors during llvm creating\n";
        return 5;
    }

    start = high_resolution_clock::now();
    std::error_code ec;
    llvm::raw_fd_ostream llvmCodeFile(fileName+".ll", ec, llvm::sys::fs::OpenFlags(0));
    llvmCodeFile << *llvmObj->module;
    llvmCodeFile.close();
    auto emitLlvmTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    
    string cmdClang = "clang "+fileName+".ll -o "+fileName+".exe -O2 > NUL 2> NUL";
    start = high_resolution_clock::now();
    system(cmdClang.c_str());
    auto backEndTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    
    auto frontEndTime = parsingTime + codeTreeCreatingTime + interpretingTime + llvmCreateTime + emitLlvmTime;
    auto fullTime = frontEndTime + backEndTime;
    cout << "---------------------------------------\n";
    cout << "| Compiling completed\n";
    cout << "| - time : "                 << fullTime             << "[s]\n";
    cout << "| -- front-end   : "         << frontEndTime         << "[s] (" << 100*frontEndTime/fullTime << "%)\n";
    cout << "| --- parsing            : " << parsingTime          << "[s] (" << 100*parsingTime/fullTime << "%)\n";
    cout << "| --- code tree creating : " << codeTreeCreatingTime << "[s] (" << 100*codeTreeCreatingTime/fullTime << "%)\n";
    cout << "| --- interpreting       : " << interpretingTime     << "[s] (" << 100*interpretingTime/fullTime << "%)\n";
    cout << "| --- llvm creating      : " << llvmCreateTime       << "[s] (" << 100*llvmCreateTime/fullTime << "%)\n";
    cout << "| --- llvm code emiting  : " << emitLlvmTime         << "[s] (" << 100*emitLlvmTime/fullTime << "%)\n";
    cout << "| -- back-end    : "         << backEndTime          << "[s] (" << 100*backEndTime/fullTime << "%)\n";
    cout << "---------------------------------------\n";

    if (argc < 3 || (argc >= 3 && strcmp(argv[2], "-run"))) {
        return 0;
    }

    system(fileName.c_str());

    return 0;
}