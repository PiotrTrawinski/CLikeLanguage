#include "interpreting.h"

using namespace std;

bool interpret(CodeScope& globalScope) {
    return globalScope.interpret();
}
