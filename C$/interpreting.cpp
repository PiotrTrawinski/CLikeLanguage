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

unique_ptr<Value> getValue(const vector<Token>& tokens, int& i, const string& delimiter=";") {
    unique_ptr<Value> value;
    return value;
}

unique_ptr<Type> getType(const vector<Token>& tokens, int& i, const vector<string>& delimiters) {
    return nullptr;
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
    return CodeScope::interpret(tokens, i);
}

bool ClassScope::interpret(const vector<Token>& tokens, int& i) {
    return true;
}

bool DeferScope::interpret(const vector<Token>& tokens, int& i) {
    return CodeScope::interpret(tokens, i);
}

Scope::ReadStatementValue Scope::readStatement(const vector<Token>& tokens, int& i) {
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
            auto statement = make_unique<CodeScope>(token.codePosition, Scope::Owner::None, this);
            statement->interpret(tokens, i);
            return Scope::ReadStatementValue(move(statement));
        }
        else {
            auto value = getValue(tokens, i);
            if (!value) {
                return false;
            }
            return Scope::ReadStatementValue(move(value));
        }
    case Token::Type::Label:
        /// Possible legal uses of label at start of expression:
        // keyword
        // variable declaration (x :: 3; x := 5; x: int; x :: ())
        // class declaration (MyClass :: class {}; MyClass :: class<T,U> {})
        // expression (value) (x = 3; fun(2,1) = 5; x.fun(2,1).y = 1)

        Keyword* keyword = Keyword::get(token.value);
        if (keyword) {
            switch (keyword->kind) {
            case Keyword::Kind::SpecialValue:
                return errorMessage("statement cannot start with r-value", token.codePosition);
            case Keyword::Kind::TypeName:
                return errorMessage("statement cannot start with type name", token.codePosition);
            case Keyword::Kind::ScopeStart: {
                unique_ptr<Scope> scope = nullptr;
                switch (((ScopeStartKeyword*)keyword)->value) {
                case ScopeStartKeyword::Value::Class:
                    scope = make_unique<ClassScope>(token.codePosition, this);  break;
                case ScopeStartKeyword::Value::Defer:
                    scope = make_unique<DeferScope>(token.codePosition, this);  break;
                case ScopeStartKeyword::Value::If:
                    scope = make_unique<IfScope>(token.codePosition, this);     break;
                case ScopeStartKeyword::Value::ElseIf:
                    scope = make_unique<ElseIfScope>(token.codePosition, this); break;
                case ScopeStartKeyword::Value::Else:
                    scope = make_unique<ElseScope>(token.codePosition, this);   break;
                case ScopeStartKeyword::Value::While:
                    scope = make_unique<WhileScope>(token.codePosition, this);  break;
                case ScopeStartKeyword::Value::For:
                    scope = make_unique<ForScope>(token.codePosition, this);    break;
                }
                i += 1;
                scope->interpret(tokens, i);
                return Scope::ReadStatementValue(move(scope));
            }
            case Keyword::Kind::FlowStatement:
                unique_ptr<Operation> operation = nullptr;
                switch (((FlowStatementKeyword*)keyword)->value) {
                case FlowStatementKeyword::Value::Break:
                    operation = make_unique<Operation>(token.codePosition, Operation::Kind::Break);    break;
                case FlowStatementKeyword::Value::Continue:
                    operation = make_unique<Operation>(token.codePosition, Operation::Kind::Continue); break;
                case FlowStatementKeyword::Value::Remove:
                    operation = make_unique<Operation>(token.codePosition, Operation::Kind::Remove);   break;
                case FlowStatementKeyword::Value::Return:
                    operation = make_unique<Operation>(token.codePosition, Operation::Kind::Return);   break;
                }
                i += 1;
                auto value = getValue(tokens, i);
                if (value) {
                    operation->arguments.push_back(move(value));
                }
                return Scope::ReadStatementValue(move(operation));
            }
        }
        else if (i + 1 >= tokens.size()) {
            return errorMessage("unexpected end of file", token.codePosition);
        }
        else if (tokens[i + 1].value == ":" || tokens[i + 1].value == "&") {
            // declaration
            if (i + 3 >= tokens.size()) {
                return errorMessage("unexpected end of assignment", token.codePosition);
            }
            if (tokens[i + 3].value == "class") {
                // class declaration/scope
                if (tokens[i + 1].value + tokens[i+2].value != "::") {
                    return errorMessage("unexpected symbols in class declaration. Did you mean to use '::'?", tokens[i+1].codePosition);
                }
                i += 4;
                auto classScope = make_unique<ClassScope>(token.codePosition, this);
                classScope->interpret(tokens, i);
                return Scope::ReadStatementValue(move(classScope));
            } else {
                // variable declaration
                i += 2;
                unique_ptr<Type> type = nullptr;
                if (tokens[i].value != ":" && tokens[i].value != "=") {
                    type = getType(tokens, i, {":", "="});
                    if (!type) {
                        return false;
                    }
                }
                bool declareByReference = tokens[i - 1].value == "&";
                auto declaration = make_unique<Declaration>(token.codePosition);
                declaration->variable.isConst = tokens[i].value == ":";
                declaration->variable.name = token.value;
                declaration->variable.type = move(type);
                i += 1;
                declaration->value = getValue(tokens, i);
                return Scope::ReadStatementValue(move(declaration));
            }
        }
        else {
            auto value = getValue(tokens, i);
            if (!value) {
                return false;
            }
            return Scope::ReadStatementValue(move(value));
        }
    }
}

bool CodeScope::interpret(const vector<Token>& tokens, int& i) {
    while (i < tokens.size()) {
        auto statementValue = readStatement(tokens, i);
        if (statementValue) {
            if (statementValue.isScopeEnd) {
                break;
            } else {
                statements.push_back(move(statementValue.statement));
            }
        } else {
            return false;
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