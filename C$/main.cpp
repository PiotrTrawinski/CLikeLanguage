#include <iostream>
#include "parsing.h"

using namespace std;

int main(char** argv, int argc) {
    if (argc < 2) {
        return 1;
    }
    auto tokens = parseFile(argv[1]);
    if (!tokens) {
        cerr << "Compiling failed: there were errors during parsing\n";
        return 2;
    }

    return 0;
}