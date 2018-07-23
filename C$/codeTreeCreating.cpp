#include "codeTreeCreating.h"

using namespace std;

CodeScope* createCodeTree(vector<Token> tokens) {
    auto globalScope = CodeScope::Create(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr, true);

    int i = 0;
    if (globalScope->createCodeTree(tokens, i)) {
        return globalScope;
    } else {
        return nullptr;
    }
}