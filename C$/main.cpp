#include "parsing.h"
#include "codeTreeCreating.h"
#include "llvmCreating.h"
#include "Value.h"
#include "consoleColors.h"
#include <chrono>
#include <filesystem>
#include <Windows.h>
#include <optional>

using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;

struct CmdFlags {
    bool run = false;
    bool emitReadableLlvm = false;
    bool emitOptimalLlvm = false;
    bool emitAsm = false;
};

optional<CmdFlags> getCmdFlags(int argc, char** argv) {
    CmdFlags flags;
    bool wereErrors = false;
    for (int i = 0; i < argc; ++i) {
        if      (!strcmp(argv[i], "-run")) flags.run = true;
        else if (!strcmp(argv[i], "-emit-readable-llvm")) flags.emitReadableLlvm = true;
        else if (!strcmp(argv[i], "-emit-optimal-llvm")) flags.emitOptimalLlvm = true;
        else if (!strcmp(argv[i], "-emit-asm")) flags.emitAsm = true;
        else {
            printfColorErr(COLOR::RED, "Command Argument Error");
            printfColorErr(COLOR::GREY, ": '");
            printfColorErr(COLOR::MAGENTA, "%s", argv[i]);
            printfColorErr(COLOR::GREY, "' is not recognizable compile flag (see -help)\n");
            wereErrors = true;
        }
    }
    if (wereErrors) return nullopt;
    else return flags;
}

void printHelp() {
    printfColor(COLOR::GREY, "usage: ");
    printfColor(COLOR::WHITE, "C$");
    printfColor(COLOR::MAGENTA, " filename");
    printfColor(COLOR::GREY, " [flags]\n\n");
    printfColor(COLOR::WHITE, "flags:\n");

    printfColor(COLOR::WHITE, "-run                ");
    printfColor(COLOR::GREY, ": run .exe after successful compilation\n");

    printfColor(COLOR::WHITE, "-emit-readable-llvm ");
    printfColor(COLOR::GREY, ": create file '");
    printfColor(COLOR::MAGENTA, "filename");
    printfColor(COLOR::GREY, "_read.ll' with slightly optimized llvm IR\n");

    printfColor(COLOR::WHITE, "-emit-optimal-llvm  ");
    printfColor(COLOR::GREY, ": create file '");
    printfColor(COLOR::MAGENTA, "filename");
    printfColor(COLOR::GREY, "_opt.ll' optimized llvm IR\n");

    printfColor(COLOR::WHITE, "-emit-asm           ");
    printfColor(COLOR::GREY, ": create file '");
    printfColor(COLOR::MAGENTA, "filename");
    printfColor(COLOR::GREY, ".asm' with textual assembly\n");
}

double nanoToSec(long long nanoSecs) {
    return nanoSecs / 1000000000.0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printfColorErr(COLOR::RED, "Compilation FAILED");
        printfColorErr(COLOR::GREY, ": no file to compile\n");
        return 1;
    }
    bool isHelp = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-help")) {
            isHelp = true;
            break;
        }
    }
    if (isHelp) {
        printHelp();
        return 0;
    }

    auto cmdFlagsOpt = getCmdFlags(argc-2, &argv[2]);
    if (!cmdFlagsOpt) {
        printfColorErr(COLOR::RED, "Compilation FAILED");
        printfColorErr(COLOR::GREY, ": bad command line arguments\n");
        return 2;
    }
    auto cmdFlags = cmdFlagsOpt.value();

    string filePath = argv[1];
    string fileName = fs::path(filePath).filename().replace_extension().u8string();

    auto start = high_resolution_clock::now();
    auto tokens = parseFile(filePath);
    auto parsingTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    if (!tokens) {
        printfColorErr(COLOR::RED, "Compilation FAILED");
        printfColorErr(COLOR::GREY, ": there were errors during parsing\n");
        return 3;
    }
    
    start = high_resolution_clock::now();
    auto globalScope = createCodeTree(tokens.value());
    auto codeTreeCreatingTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    if (!globalScope) {
        printfColorErr(COLOR::RED, "Compilation FAILED");
        printfColorErr(COLOR::GREY, ": there were errors during code tree creating\n");
        return 4;
    }

    start = high_resolution_clock::now();
    bool statusInterpreting = globalScope->interpret();
    auto interpretingTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    if (!statusInterpreting) {
        printfColorErr(COLOR::RED, "Compilation FAILED");
        printfColorErr(COLOR::GREY, ": there were errors during interpreting\n");
        return 5;
    }

    start = high_resolution_clock::now();
    auto llvmObj = createLlvm(globalScope);
    auto mainFunction = (llvm::Function*)globalScope->mainFunction->createLlvm(llvmObj.get());
    mainFunction->setName("main");
    auto llvmCreateTime = nanoToSec(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
    if (!llvmObj) {
        printfColorErr(COLOR::RED, "Compilation FAILED");
        printfColorErr(COLOR::GREY, ": there were errors during llvm creating\n");
        return 6;
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

    if (cmdFlags.emitReadableLlvm) {
        printfColor(COLOR::CYAN, "emitting readable llvm... ");
        string cmd = "opt "+fileName+".ll -S -o "+fileName+"_read.ll -adce -dse";
        system(cmd.c_str());
        printfColor(COLOR::WHITE, "DONE\n");
    }
    if (cmdFlags.emitOptimalLlvm) {
        printfColor(COLOR::CYAN, "emitting optimal llvm... ");
        string cmd = "opt "+fileName+".ll -S -o "+fileName+"_opt.ll -O2";
        system(cmd.c_str());
        printfColor(COLOR::WHITE, "DONE\n");
    }
    if (cmdFlags.emitAsm) {
        printfColor(COLOR::CYAN, "emitting asm... ");
        string cmd = "opt "+fileName+".ll -f -O2 | llc -o "+fileName+".asm -O2 -filetype=asm --x86-asm-syntax=intel";
        system(cmd.c_str());
        printfColor(COLOR::WHITE, "DONE\n");
    }
    if (cmdFlags.run) {
        printfColor(COLOR::CYAN, "running...\n");
        system(fileName.c_str());
    }

    return 0;
}