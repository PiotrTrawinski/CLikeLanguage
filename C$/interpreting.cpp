#include "interpreting.h"

using namespace std;

void printErrorCodePosition(const CodePosition& codePosition) {
    cerr << "in file: " << codePosition.fileInfo->name << " "
         << "at line: " << codePosition.lineNumber << " "
         << "at char: " << codePosition.charNumber << "\n";
}

bool errorMessage(string message, const CodePosition& codePosition) {
    cerr << "Interpret Error: " << message << "\n";
    printErrorCodePosition(codePosition);
    return false;
}

bool internalError(string message, const CodePosition& codePosition) {
    cerr << "Internal Error: " << message << "\n";
    printErrorCodePosition(codePosition);
    return false;
}

bool ForScope::interpret(const vector<Token>& tokens, int& i) {
    return true;
}

bool WhileScope::interpret(const vector<Token>& tokens, int& i) {
    return true;
}

bool IfScope::interpret(const vector<Token>& tokens, int& i) {
    return true;
}

bool ElseIfScope::interpret(const vector<Token>& tokens, int& i) {
    return true;
}

bool ElseScope::interpret(const vector<Token>& tokens, int& i) {
    return true;
}

bool ClassScope::interpret(const vector<Token>& tokens, int& i) {
    return true;
}

bool DeferScope::interpret(const vector<Token>& tokens, int& i) {
    return true;
}

bool CodeScope::interpret(const vector<Token>& tokens, int& i) {
    while (i < tokens.size()) {
        auto& token = tokens[i];
        switch (token.type) {
        case Token::Type::Char:
            return errorMessage("statement cannot start with a char value", token.codePosition);
        case Token::Type::StringLiteral:
            return errorMessage("statement cannot start with a string literal", token.codePosition);
        case Token::Type::Integer:
            return errorMessage("statement cannot start with an integer value", token.codePosition);
        case Token::Type::Float:
            return errorMessage("statement cannot start with a float value", token.codePosition);
        case Token::Type::Symbol:
            if (token.value == "{") {
                this->statements.push_back(make_unique<CodeScope>(
                    token.codePosition, Scope::Owner::None, this
                ));
                ((Scope*)this->statements.back().get())->interpret(tokens, i);
            }
            break;
        case Token::Type::Label:
            break;

        }
    }

    return true;
}


optional<CodeScope> interpret(vector<Token> tokens) {
    CodeScope globalScope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr, true);

    int i = 0;
    if (globalScope.interpret(tokens, i)) {
        return globalScope;
    } else {
        return nullopt;
    }
}