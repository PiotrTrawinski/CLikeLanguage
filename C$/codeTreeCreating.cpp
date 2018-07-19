#include "codeTreeCreating.h"

using namespace std;

optional<CodeScope> createCodeTree(vector<Token> tokens) {
    CodeScope globalScope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr, true);

    int i = 0;
    if (globalScope.createCodeTree(tokens, i)) {
        return globalScope;
    } else {
        return nullopt;
    }
}