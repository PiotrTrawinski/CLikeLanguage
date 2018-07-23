#include <iostream>
#include "parsing.h"
#include "codeTreeCreating.h"
#include "interpreting.h"

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

    return 0;
}