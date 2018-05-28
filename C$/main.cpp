#include <iostream>
#include "parsing.h"

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

    return 0;
}