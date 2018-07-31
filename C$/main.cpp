#include <iostream>
#include "parsing.h"
#include "codeTreeCreating.h"
#include "interpreting.h"
#include "llvmCreating.h"

using namespace std;

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "You didn't provide a file to compile\n";
        return 1;
    }

    auto tokens = parseFile(argv[1]);
    if (!tokens) {
        cerr << "Compiling failed: there were errors during parsing\n";
        return 2;
    }

    auto globalScope = createCodeTree(tokens.value());
    if (!globalScope) {
        cerr << "Compiling failed: there were errors during code tree creating\n";
        return 3;
    }

    bool statusInterpreting = interpret(globalScope);
    if (!statusInterpreting) {
        cerr << "Compiling failed: there were errors during interpreting\n";
        return 4;
    }

    if (argc < 3 || strcmp(argv[2], "-nollvm")) {
        auto statusLlvmCreating = createLlvm(globalScope);
        if (!statusLlvmCreating) {
            cerr << "Compiling failed: there were errors during llvm creating\n";
            return 5;
        }
    }

    return 0;
}