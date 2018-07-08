#include "interpreting.h"

using namespace std;

bool findAndEvaluateAllConstexpr(CodeScope& globalScope) {
    return true;
}

bool interpret(CodeScope& globalScope) {
    if (!findAndEvaluateAllConstexpr(globalScope)) {
        return false;
    }
    // check if all types correct, which variable is which, templates, etc
    return true;
}
