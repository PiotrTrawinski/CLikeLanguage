#include "codeTreeCreating.h"

using namespace std;

GlobalScope* createCodeTree(vector<Token> tokens) {
    int i = 0;
    if (GlobalScope::Instance.createCodeTree(tokens, i)) {
        return &GlobalScope::Instance;
    } else {
        return nullptr;
    }
}