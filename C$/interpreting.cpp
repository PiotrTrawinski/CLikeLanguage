#include "interpreting.h"

using namespace std;

/*
Declaration* findDeclaration(const string& name, CodeScope* scope) {
    return nullptr;
}

bool evaluate(Declaration* declaration, CodeScope* scope, unordered_set<string>& alreadyEvaluatedInScope, vector<string>& evaluationStack) {
    
    return true;
}

Declaration* findNextConstDeclaration(const vector<unique_ptr<Statement>>& statements, int& i) {
    return nullptr;
}

CodeScope* findNextCodeScope(const vector<unique_ptr<Statement>>& statements, int& i) {
    return nullptr;
}

bool findAndEvaluateAllConstexpr(CodeScope* scope) {
    unordered_set<string> alreadyEvaluatedInScope;

    int declarationIndex = 0;
    Declaration* declaration = nullptr;
    while (declaration = findNextConstDeclaration(scope->statements, declarationIndex)) {
        if (!evaluate(declaration, scope, alreadyEvaluatedInScope)) {
            return false;
        }
    }

    int scopeIndex = 0;
    CodeScope* childScope = nullptr;
    while (childScope = findNextCodeScope(scope->statements, scopeIndex)) {
        if (!findAndEvaluateAllConstexpr(childScope)) {
            return false;
        }
    }

    return true;
}
*/
bool interpret(CodeScope& globalScope) {
    /*if (!findAndEvaluateAllConstexpr(&globalScope)) {
        return false;
    }*/
    // check if all types correct, which variable is which, templates, etc
    return true;
}
