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

unique_ptr<Value> getValue(const vector<Token>& tokens, int& i, const string& skipDelimiter=";") {
    unique_ptr<Value> value;
    return value;
}


unique_ptr<Type> getType(const vector<Token>& tokens, int& i, const vector<string>& delimiters);

optional<vector<unique_ptr<Type>>> getFunctionArgumentTypes(const vector<Token>& tokens, int& i) {
    vector<unique_ptr<Type>> types;
    while (tokens[i].value != ")") {
        auto type = getType(tokens, i, {",", ")"});
        if (!type) {
            return nullopt;
        }
        types.push_back(move(type));
        if (tokens[i+1].value == "," && tokens[i+2].value != ")") {
            i += 2;
        } else if (tokens[i+1].value == ")") {
            i += 1;
        } else if (tokens[i+1].value == "," && tokens[i + 2].value == ")") {
            errorMessage("expected function argument type, got ')'", tokens[i+2].codePosition);
            return nullopt;
        }
        else {
            errorMessage(
                "during interpreting function arguments expected ',' or ')', got " + tokens[i+1].value, 
                tokens[i+1].codePosition
            );
            return nullopt;
        }
    }
    return types;
}

unique_ptr<Type> getType(const vector<Token>& tokens, int& i, const vector<string>& delimiters) {
    unique_ptr<Type> type = nullptr;
    if (tokens[i].value == "!") {
        i += 1;
        auto underlyingType = getType(tokens, i, delimiters);
        if (underlyingType) {
            type = make_unique<OwnerPointerType>(move(underlyingType));
        }
    } else if (tokens[i].value == "*") {
        i += 1;
        auto underlyingType = getType(tokens, i, delimiters);
        if (underlyingType) {
            type = make_unique<RawPointerType>(move(underlyingType));
        }
    } else if (tokens[i].value == "&") {
        i += 1;
        auto underlyingType = getType(tokens, i, delimiters);
        if (underlyingType) {
            type = make_unique<ReferenceType>(move(underlyingType));
        }
    } else if (tokens[i].value == "?") {
        i += 1;
        auto underlyingType = getType(tokens, i, delimiters);
        if (underlyingType) {
            type = make_unique<MaybeErrorType>(move(underlyingType));
        }
    } else if (tokens[i].value == "[") {
        if (tokens[i + 1].value == "]") {
            i += 2;
            auto elementType = getType(tokens, i, delimiters);
            if (elementType) {
                type = make_unique<DynamicArrayType>(move(elementType));
            }
        } else if (tokens[i+1].value == "*" && tokens[i+2].value == "]") {
            i += 3;
            auto elementType = getType(tokens, i, delimiters);
            if (elementType) {
                type = make_unique<ArrayViewType>(move(elementType));
            }
        } else {
            i += 1;
            auto sizeValue = getValue(tokens, i, "]");
            auto elementType = getType(tokens, i, delimiters);
            if (elementType && sizeValue) {
                type = make_unique<StaticArrayType>(move(elementType), move(sizeValue));
            }
        }
    } else if (tokens[i].value == "<") {
        auto templateFunctionType = make_unique<TemplateFunctionType>();
        i += 1;
        while (tokens[i].type == Token::Type::Label && tokens[i+1].value == ",") {
            templateFunctionType->templateTypes.push_back(make_unique<TemplateType>(tokens[i].value));
            i += 2;
        }
        if (tokens[i].type != Token::Type::Label) {
            errorMessage("expected template type name, got " + tokens[i].value, tokens[i].codePosition);
            return nullptr;
        }
        if (tokens[i + 1].value != ">") {
            errorMessage("expected '>', got " + tokens[i+1].value, tokens[i+1].codePosition);
            return nullptr;
        }
        templateFunctionType->templateTypes.push_back(make_unique<TemplateType>(tokens[i].value));
        i += 2;

        if (tokens[i].value != "(") {
            errorMessage("expected start of templated function type '(', got" + tokens[i].value, tokens[i].codePosition);
            return nullptr;
        }
        i += 1;
        auto argumentTypesOpt = getFunctionArgumentTypes(tokens, i);
        if (!argumentTypesOpt) {
            return nullptr;
        }
        templateFunctionType->argumentTypes = move(argumentTypesOpt.value());
        if (tokens[i].value + tokens[i+1].value != "->") {
            templateFunctionType->returnType = make_unique<Type>(Type::Kind::Void);
        } else {
            i += 2;
            templateFunctionType->returnType = getType(tokens, i, delimiters);
            if (!templateFunctionType->returnType) {
                return nullptr;
            }
        }
        type = move(templateFunctionType);
    } else if (tokens[i].value == "(") {
        i += 1;
        auto functionType = make_unique<FunctionType>();
        auto argumentTypesOpt = getFunctionArgumentTypes(tokens, i);
        if (!argumentTypesOpt) {
            return nullptr;
        }
        functionType->argumentTypes = move(argumentTypesOpt.value());
        if (tokens[i].value + tokens[i+1].value != "->") {
            functionType->returnType = make_unique<Type>(Type::Kind::Void);
        } else {
            i += 2;
            functionType->returnType = getType(tokens, i, delimiters);
            if (!functionType->returnType) {
                return nullptr;
            }
        }
        type = move(functionType);
    } else if (tokens[i].type == Token::Type::Label) {
        auto keyword = Keyword::get(tokens[i].value);
        if (keyword && keyword->kind == Keyword::Kind::TypeName) {
            auto typeValue = ((TypeKeyword*)keyword)->value;
            switch (typeValue) {
            case TypeKeyword::Value::Int:
                type = make_unique<IntegerType>(IntegerType::Size::I32); break;
            case TypeKeyword::Value::I8:
                type = make_unique<IntegerType>(IntegerType::Size::I8);  break;
            case TypeKeyword::Value::I16:
                type = make_unique<IntegerType>(IntegerType::Size::I16); break;
            case TypeKeyword::Value::I32:
                type = make_unique<IntegerType>(IntegerType::Size::I32); break;
            case TypeKeyword::Value::I64:
                type = make_unique<IntegerType>(IntegerType::Size::I64); break;
            case TypeKeyword::Value::U8:
                type = make_unique<IntegerType>(IntegerType::Size::U8);  break;
            case TypeKeyword::Value::U16:
                type = make_unique<IntegerType>(IntegerType::Size::U16); break;
            case TypeKeyword::Value::U32:
                type = make_unique<IntegerType>(IntegerType::Size::U32); break;
            case TypeKeyword::Value::U64:
                type = make_unique<IntegerType>(IntegerType::Size::U64); break;
            case TypeKeyword::Value::Float:
                type = make_unique<FloatType>(FloatType::Size::F64); break;
            case TypeKeyword::Value::F32:
                type = make_unique<FloatType>(FloatType::Size::F64); break;
            case TypeKeyword::Value::F64:
                type = make_unique<FloatType>(FloatType::Size::F64); break;
            case TypeKeyword::Value::Bool:
                type = make_unique<Type>(Type::Kind::Bool); break;
            case TypeKeyword::Value::String:
                type = make_unique<Type>(Type::Kind::String); break;
            case TypeKeyword::Value::Void:
                type = make_unique<Type>(Type::Kind::Void); break;
            default:
                break;
            }
        } else {
            auto className = make_unique<ClassType>(tokens[i].value);
            i += 1;
            if (tokens[i].value == "<") {
                while (true) {
                    auto templateType = getType(tokens, i, {",", ">"});
                    if (!templateType) {
                        return nullptr;
                    }
                    className->templateTypes.push_back(move(templateType));
                    if (tokens[i].value == ">") {
                        break;
                    }
                    i += 1;
                }
            }
            type = move(className);
        }
    } else {
        errorMessage("unexpected '" + tokens[i].value + "' during type interpreting", tokens[i].codePosition);
        return nullptr;
    }

    if (type && find(delimiters.begin(), delimiters.end(), tokens[i].value) != delimiters.end()) {
        return type;
    } else {
        return nullptr;
    }
}

struct ForScopeDeclarationType {
    bool isConst;
    bool byValue;
};
optional<ForScopeDeclarationType> readForScopeDeclarationType(const vector<Token>& tokens, int& i) {
    if ((tokens[i].value != ":" && tokens[i].value != "&") || (tokens[i].value == "&" && (tokens[i+1].value != ":" && tokens[i+1].value != "="))) {
        errorMessage("expected declaration of for-each array element (:: or := or &: or &= or :)", tokens[i].codePosition);
        return nullopt;
    }

    ForScopeDeclarationType declarationType;
    declarationType.isConst = tokens[i+1].value != "=";
    declarationType.byValue = tokens[i].value == ":" && (tokens[i+1].value == ":" || tokens[i+1].value == "=");
    
    if (tokens[i].value == ":" && (tokens[i].value != ":" && tokens[i].value != "=")) {
        i += 1;
    } else {
        i += 2;
    }
    
    return declarationType;
}
bool ForScope::interpret(const vector<Token>& tokens, int& i) {
    /// Possible legal uses of a for loop:
    // 1. for var1 _declarationType_ _range_ {} (var1 is int; _declarationType_ is one of {:: :=})
    // 2. for _array_ {}
    // 3. for var1 _declarationType_ _array_ {} (var1 is element of array; _declarationType_ is one of {:: := &: &= :})
    // 4. for var1, var2 _declarationType_ _array_ {} (var2 is index of element var1)
    auto firstValue = getValue(tokens, i);
    if (tokens[i].value == "{") {
        // 2. for _array_ {}
        ForEachData forEachData;
        forEachData.arrayValue = move(firstValue);
        forEachData.it = make_unique<Variable>(tokens[i].codePosition);
        forEachData.it->name = "it";
        forEachData.it->isConst = true;
        forEachData.index = make_unique<Variable>(tokens[i].codePosition);
        forEachData.index->name = "index";
        forEachData.index->isConst = true;
        data = move(forEachData);
        i += 1;
    } else if(tokens[i].value == ",") {
        // 4. for var1, var2 _declarationType_ _array_ {}
        unique_ptr<Variable> var1(static_cast<Variable*>(firstValue.release()));
        if (!firstValue) {
            return errorMessage("expected a new element iterator variable name", tokens[i-1].codePosition);
        }
        if (i + 5 >= tokens.size()) {
            return errorMessage("unexpected end of a file (tried to interpret a for loop)", tokens[tokens.size()-1].codePosition);
        }
        i += 1; // now is on the start of var2
        auto var2Value = getValue(tokens, i);
        unique_ptr<Variable> var2(static_cast<Variable*>(var2Value.release()));
        if (!var2) {
            return errorMessage("expected a new index variable name", tokens[i-1].codePosition);
        }
        // now i shows start of _declarationType_
        auto declarationTypeOpt = readForScopeDeclarationType(tokens, i);
        if (!declarationTypeOpt) {
            return false;
        }
        auto declarationType = declarationTypeOpt.value();

        auto arrayValue = getValue(tokens, i);
        if (i >= tokens.size() || tokens[i].value != "{") {
            return errorMessage("expected '{' (opening for scope)", tokens[i-1].codePosition);
        }
  
        ForEachData forEachData;
        forEachData.arrayValue = move(arrayValue);
        forEachData.it = move(var1);
        forEachData.it->isConst = declarationType.isConst;
        forEachData.index = move(var2);
        forEachData.index->isConst = true;
        data = move(forEachData);
        i += 1;
    } else {
        // 1. for var1 _declarationType_ _range_ {} (var1 is int; _declarationType_ is one of {:: :=})
        // 3. for var1 _declarationType_ _array_ {} (var1 is element of array; _declarationType_ is one of {:: := &: &= :})
        unique_ptr<Variable> var1(static_cast<Variable*>(firstValue.release()));
        if (!firstValue) {
            return errorMessage("expected a new for loop iterator variable name", tokens[i-1].codePosition);
        }
        
        auto declarationTypeOpt = readForScopeDeclarationType(tokens, i);
        if (!declarationTypeOpt) {
            return false;
        }
        auto declarationType = declarationTypeOpt.value();

        auto secondValue = getValue(tokens, i, "");
        if (tokens[i].value == "{") {
            // 3. for var1 _declarationType_ _array_ {} (var1 is element of array; _declarationType_ is one of {:: := &: &= :})
            ForEachData forEachData;
            forEachData.arrayValue = move(secondValue);
            forEachData.it = move(var1);
            forEachData.it->isConst = declarationType.isConst;
            forEachData.index = make_unique<Variable>(tokens[i].codePosition);
            forEachData.index->name = "index";
            forEachData.index->isConst = true;
            data = move(forEachData);
            i += 1;
        }
        else if (tokens[i].value == ":") {
            // 1. for var1 _declarationType_ _range_ {} (var1 is int; _declarationType_ is one of {:: :=})
            i += 1;
            ForIterData forIterData;
            forIterData.iterVariable = move(var1);
            forIterData.iterVariable->isConst = declarationType.isConst;
            forIterData.firstValue = move(secondValue);
            forIterData.step = getValue(tokens, i, ":");
            forIterData.lastValue = getValue(tokens, i, "{");
        } else {
            return errorMessage("expected '{' or ':', got " + tokens[i].value, tokens[i].codePosition);
        }
    }

    return CodeScope::interpret(tokens, i);
}

bool WhileScope::interpret(const vector<Token>& tokens, int& i) {
    this->conditionExpression = getValue(tokens, i, "{");
    return CodeScope::interpret(tokens, i);
}

bool IfScope::interpret(const vector<Token>& tokens, int& i) {
    this->conditionExpression = getValue(tokens, i, "{");
    return CodeScope::interpret(tokens, i);
}

bool ElseIfScope::interpret(const vector<Token>& tokens, int& i) {
    this->conditionExpression = getValue(tokens, i, "{");
    return CodeScope::interpret(tokens, i);
}

bool ElseScope::interpret(const vector<Token>& tokens, int& i) {
    return CodeScope::interpret(tokens, i);
}

bool ClassScope::interpret(const vector<Token>& tokens, int& i) {
    if (tokens[i].value != "{") {
        return errorMessage("expected '{' (class scope opening)", tokens[i].codePosition);
    }
    i += 1;

    while (i < tokens.size()) {
        auto statementValue = readStatement(tokens, i);
        if (statementValue) {
            if (statementValue.isScopeEnd) {
                break;
            } else {
                if (statementValue.statement->kind == Statement::Kind::Declaration) {
                    unique_ptr<Declaration> declaration(static_cast<Declaration*>(statementValue.statement.release()));
                    declarations.push_back(move(declaration));
                } else {
                    return errorMessage("non-declaration statement found in class scope", tokens[i-1].codePosition);
                }
            }
        } else {
            return false;
        }
    }

    return true;
}

bool DeferScope::interpret(const vector<Token>& tokens, int& i) {
    if (tokens[i].value == "{") {
        i += 1;
        return CodeScope::interpret(tokens, i);
    } else {
        auto statementValue = readStatement(tokens, i);
        if (statementValue.isScopeEnd) {
            return errorMessage("unexpected '}' (trying to close unopened defer scope)", tokens[i-1].codePosition);
        } else if (!statementValue.statement) {
            return false;
        }
        statements.push_back(move(statementValue.statement));
        return true;
    }
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
            i += 1;
            auto codeScope = make_unique<CodeScope>(token.codePosition, Scope::Owner::None, this);
            codeScope->interpret(tokens, i);
            return Scope::ReadStatementValue(move(codeScope));
        }
        else if (token.value == "}") {
            // end of scope
            i += 1;
            return Scope::ReadStatementValue(true);
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
                if (isGlobalScope) {
                    return errorMessage("unexpected '}'. (trying to close global scope)", tokens[i-1].codePosition);
                } else {
                    break;
                }
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