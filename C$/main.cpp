#include "parsing.h"
#include "codeTreeCreating.h"
#include "llvmCreating.h"
#include "Value.h"
#include "consoleColors.h"
#include <chrono>
#include <filesystem>
#include <Windows.h>

using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;

double nanoToSec(long long nanoSecs) {
    return nanoSecs / 1000000000.0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printfColorErr(COLOR::RED, "Compilation FAILED");
        printfColorErr(COLOR::GREY, ": you didn't provide a file to compile\n");
        return 1;
    }
    string filePath = argv[1];
    string fileName = fs::path(filePath).filename().replace_extension().u8string();

    auto start = high_resolution_clock::now();
    auto tokens = parseFile(filePath);
    auto parsingTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    if (!tokens) {
        printfColorErr(COLOR::RED, "Compilation FAILED");
        printfColorErr(COLOR::GREY, ": there were errors during parsing\n");
        return 2;
    }
    
    start = high_resolution_clock::now();
    auto globalScope = createCodeTree(tokens.value());
    auto codeTreeCreatingTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    if (!globalScope) {
        printfColorErr(COLOR::RED, "Compilation FAILED");
        printfColorErr(COLOR::GREY, ": there were errors during code tree creating\n");
        return 3;
    }

    start = high_resolution_clock::now();
    bool statusInterpreting = globalScope->interpret();
    auto interpretingTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    if (!statusInterpreting) {
        printfColorErr(COLOR::RED, "Compilation FAILED");
        printfColorErr(COLOR::GREY, ": there were errors during interpreting\n");
        return 4;
    }

    start = high_resolution_clock::now();
    auto llvmObj = createLlvm(globalScope);
    auto mainFunction = (llvm::Function*)globalScope->mainFunction->createLlvm(llvmObj.get());
    mainFunction->setName("main");
    auto llvmCreateTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    if (!llvmObj) {
        printfColorErr(COLOR::RED, "Compilation FAILED");
        printfColorErr(COLOR::GREY, ": there were errors during llvm creating\n");
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

    printfColor(COLOR::GREEN, "Compilation SUCCEEDED\n");
    printfColor(COLOR::GREY, "- time : ");printfColor(COLOR::CYAN, "%.3f",fullTime);printfColor(COLOR::GREY, "[s]\n");
    printfColor(COLOR::GREY, "-- front-end   : ");printfColor(COLOR::CYAN, "%.3f", frontEndTime);printfColor(COLOR::GREY, "[s] (%.2f%%)\n", 100*frontEndTime/fullTime);
    printfColor(COLOR::GREY, "--- parsing            : ");printfColor(COLOR::CYAN, "%.3f", parsingTime);printfColor(COLOR::GREY, "[s] (%.2f%%)\n", 100*parsingTime/fullTime);
    printfColor(COLOR::GREY, "--- code tree creating : ");printfColor(COLOR::CYAN, "%.3f", codeTreeCreatingTime);printfColor(COLOR::GREY, "[s] (%.2f%%)\n", 100*codeTreeCreatingTime/fullTime);
    printfColor(COLOR::GREY, "--- interpreting       : ");printfColor(COLOR::CYAN, "%.3f", interpretingTime);printfColor(COLOR::GREY, "[s] (%.2f%%)\n", 100*interpretingTime/fullTime);
    printfColor(COLOR::GREY, "--- llvm creating      : ");printfColor(COLOR::CYAN, "%.3f", llvmCreateTime);printfColor(COLOR::GREY, "[s] (%.2f%%)\n", 100*llvmCreateTime/fullTime);
    printfColor(COLOR::GREY, "--- llvm code emiting  : ");printfColor(COLOR::CYAN, "%.3f", emitLlvmTime);printfColor(COLOR::GREY, "[s] (%.2f%%)\n", 100*emitLlvmTime/fullTime);
    printfColor(COLOR::GREY, "-- back-end    : ");printfColor(COLOR::CYAN, "%.3f", backEndTime);printfColor(COLOR::GREY, "[s] (%.2f%%)\n", 100*backEndTime/fullTime);

    if (argc < 3 || (argc >= 3 && strcmp(argv[2], "-run"))) {
        return 0;
    }

    system(fileName.c_str());

    return 0;
}