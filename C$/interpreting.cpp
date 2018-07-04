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

unique_ptr<Type> getType(const vector<Token>& tokens, int& i, const vector<string>& delimiters, bool writeError=true);
unique_ptr<Value> getValue(const vector<Token>& tokens, int& i, const string& skipDelimiter=";");

void appendOperator(vector<unique_ptr<Operation>>& stack, vector<unique_ptr<Value>>& out, unique_ptr<Operation> operation) {
    if (operation->getIsLeftAssociative()) {
        while (stack.size() > 0 && operation->getPriority() <= stack.back()->getPriority()) {
            out.push_back(move(stack.back()));
            stack.pop_back();
        }
    } else {
        while (stack.size() > 0 && operation->getPriority() < stack.back()->getPriority()) {
            out.push_back(move(stack.back()));
            stack.pop_back();
        }
    }
    stack.push_back(move(operation));
}
void appendOperator(vector<unique_ptr<Operation>>& stack, vector<unique_ptr<Value>>& out, Operation::Kind kind, const CodePosition& codePosition) {
    appendOperator(stack, out, make_unique<Operation>(codePosition, kind));
}
optional<vector<unique_ptr<Value>>> getReversePolishNotation(const vector<Token>& tokens, int& i, const string& skipDelimiter) {
    vector<unique_ptr<Operation>> stack;
    vector<unique_ptr<Value>> out;
    bool endOfExpression = false;
    bool expectValue = true;
    while (!endOfExpression) {
        switch (tokens[i].type) {
        case Token::Type::Integer:
            out.push_back(make_unique<IntegerValue>(tokens[i].codePosition, stoi(tokens[i].value)));
            break;
        case Token::Type::Float:
            out.push_back(make_unique<FloatValue>(tokens[i].codePosition, stod(tokens[i].value)));
            break;
        case Token::Type::Char:
            out.push_back(make_unique<CharValue>(tokens[i].codePosition, stoi(tokens[i].value)));
            break;
        case Token::Type::StringLiteral:
            out.push_back(make_unique<StringValue>(tokens[i].codePosition, tokens[i].value));
            break;
        case Token::Type::Label:{
            bool isTemplateFunctionCall = true;
            if (tokens[i + 1].value == "<") {
                // either 'less then' operator or function call template arguments
                // assume its template arguments and check if it makes sense
                int openTemplateCount = 1;
                int j = i + 2;
                while (j < tokens.size() && openTemplateCount != 0) {
                    if (tokens[j].value == "<") {
                        openTemplateCount += 1;
                    } else if (tokens[j].value == ">") {
                        openTemplateCount -= 1;
                    }
                    j += 1;
                }
                if (j < tokens.size()) {
                    vector<unique_ptr<Type>> templateTypes;
                    int k = i+2;
                    while (k < j) {
                        auto type = getType(tokens, k, {",", ">"}, false);
                        if (type) {
                            templateTypes.push_back(move(type));
                            k += 1;
                        } else {
                            isTemplateFunctionCall = false;
                            break;
                        }
                    }

                    // its template call
                    i = k;
                    if (tokens[i].value == "(") {
                        // read function arguments
                    } else {
                        errorMessage("expected '(' - begining of function call arguments, got" + tokens[i].value, tokens[i].codePosition);
                        return nullopt;
                    }
                } else {
                    isTemplateFunctionCall = false;
                }
            } 
            if (!isTemplateFunctionCall) {
                // its either (special operator; variable; non-template function call)
                if (tokens[i].value == "alloc") {
                    appendOperator(stack, out, Operation::Kind::Allocation, tokens[i++].codePosition);
                }
                else if (tokens[i].value == "dealloc") {
                    appendOperator(stack, out, Operation::Kind::Deallocation, tokens[i++].codePosition);
                }
                else if (tokens[i + 1].value == "(") {
                    // function call
                    // read function arguments
                } else {
                    // variable
                    auto variable = make_unique<Variable>(tokens[i].codePosition);
                    variable->name = tokens[i].value;
                    out.push_back(move(variable));
                }
            }
            break;
        }
        case Token::Type::Symbol:
            if (expectValue) {
                if (tokens[i].value == "[") {
                    // static array ([x, y, z, ...]) or type cast ([T]())
                    int openSquereBrackets = 1;
                    int j = i+1;
                    while (j < tokens.size() && openSquereBrackets != 0) {
                        if (tokens[j].value == "[") {
                            openSquereBrackets += 1;
                        } else if (tokens[j].value == "]") {
                            openSquereBrackets -= 1;
                        }
                        j += 1;
                    }
                    if (openSquereBrackets != 0) {
                        errorMessage("missing closing ']'", tokens[i].codePosition);
                        return nullopt;
                    }

                    if (tokens[j].value == "(") {
                        // type cast
                        i += 1;
                        auto type = getType(tokens, i, {"]"});
                        if (!type) {
                            return nullopt;
                        }
                        i += 1;
                        auto argument = getValue(tokens, i, ")");
                        if (!argument) {
                            return nullopt;
                        }
                        auto castOperation = make_unique<CastOperation>(tokens[i].codePosition, move(type));
                        castOperation->arguments.push_back(move(argument));
                        appendOperator(stack, out, move(castOperation));
                    } else {
                        // static array
                        auto staticArray = make_unique<StaticArrayValue>(tokens[i].codePosition);
                        // get values seperated with ',' ended with ']'
                        out.push_back(move(staticArray));
                    }
                } 
                else if (tokens[i].value == "-") {
                    appendOperator(stack, out, Operation::Kind::Minus, tokens[i++].codePosition);
                }
                else if (tokens[i].value == "!") {
                    appendOperator(stack, out, Operation::Kind::LogicalNot, tokens[i++].codePosition);
                }
                else if (tokens[i].value == "&") {
                    appendOperator(stack, out, Operation::Kind::Reference, tokens[i++].codePosition);
                }
                else if (tokens[i].value == "@") {
                    appendOperator(stack, out, Operation::Kind::Address, tokens[i++].codePosition);
                }
                else if (tokens[i].value == "$") {
                    appendOperator(stack, out, Operation::Kind::GetValue, tokens[i++].codePosition);
                } else {
                    // end of expression
                }
            } else {
                if (tokens[i].value == "[") {
                    // array index/offset ([x]) or subarray indexing ([x:y])
                    bool isSubArrayIndexing = false;
                    int openSquereBrackets = 1;
                    int j = i+1;
                    while (j < tokens.size() && openSquereBrackets != 0) {
                        if (isSubArrayIndexing)
                        if (tokens[j].value == ":" && openSquereBrackets == 1) {
                            if (isSubArrayIndexing) {
                                errorMessage("unexpected ':' symbol in subarray indexing", tokens[j].codePosition);
                                return nullopt;
                            }
                            isSubArrayIndexing = true;
                        }
                        if (tokens[j].value == "[") {
                            openSquereBrackets += 1;
                        } else if (tokens[j].value == "]") {
                            openSquereBrackets -= 1;
                        }
                        j += 1;
                    }
                    if (isSubArrayIndexing) {
                        // subarray indexing ([x:y])
                        i += 1;
                        auto value1 = getValue(tokens, i, ":");
                        if (!value1) { return nullopt; }

                        auto value2 = getValue(tokens, i, "]");
                        if (!value2) { return nullopt; }

                        auto subArrayOperation = make_unique<Operation>(tokens[i-1].codePosition, Operation::Kind::ArraySubArray);
                        subArrayOperation->arguments.push_back(move(value1));
                        subArrayOperation->arguments.push_back(move(value2));
                        appendOperator(stack, out, move(subArrayOperation));
                    } else {
                        // array index/offset ([x])
                        i += 1;
                        auto value = getValue(tokens, i, "]");
                        if (!value) {
                            return nullopt;
                        }
                        auto indexingOperation = make_unique<Operation>(tokens[i-1].codePosition, Operation::Kind::ArrayIndex);
                        indexingOperation->arguments.push_back(move(value));
                        appendOperator(stack, out, move(indexingOperation));
                    }
                }
                else if (tokens[i].value + tokens[i + 1].value == "&&") {
                    appendOperator(stack, out, Operation::Kind::LogicalAnd, tokens[i].codePosition);
                    i += 2;
                }
                else if (tokens[i].value + tokens[i + 1].value == "||") {
                    appendOperator(stack, out, Operation::Kind::LogicalOr, tokens[i].codePosition);
                    i += 2;
                }
                else if (tokens[i].value + tokens[i + 1].value == "==") {
                    appendOperator(stack, out, Operation::Kind::Eq, tokens[i].codePosition);
                    i += 2;
                }
                else if (tokens[i].value + tokens[i + 1].value == "!=") {
                    appendOperator(stack, out, Operation::Kind::Neq, tokens[i].codePosition);
                    i += 2;
                }
                else if (tokens[i].value + tokens[i + 1].value == "<=") {
                    appendOperator(stack, out, Operation::Kind::Lte, tokens[i].codePosition);
                    i += 2;
                }
                else if (tokens[i].value + tokens[i + 1].value == ">=") {
                    appendOperator(stack, out, Operation::Kind::Gte, tokens[i].codePosition);
                    i += 2;
                }
                else if (tokens[i].value + tokens[i + 1].value == "+=") {
                    appendOperator(stack, out, Operation::Kind::AddAssign, tokens[i].codePosition);
                    i += 2;
                }
                else if (tokens[i].value + tokens[i + 1].value == "-=") {
                    appendOperator(stack, out, Operation::Kind::SubAssign, tokens[i].codePosition);
                    i += 2;
                }
                else if (tokens[i].value + tokens[i + 1].value == "*=") {
                    appendOperator(stack, out, Operation::Kind::MulAssign, tokens[i].codePosition);
                    i += 2;
                }
                else if (tokens[i].value + tokens[i + 1].value == "/=") {
                    appendOperator(stack, out, Operation::Kind::DivAssign, tokens[i].codePosition);
                    i += 2;
                }
                else if (tokens[i].value + tokens[i + 1].value == "%=") {
                    appendOperator(stack, out, Operation::Kind::ModAssign, tokens[i].codePosition);
                    i += 2;
                }
                else if (tokens[i].value + tokens[i + 1].value == "|=") {
                    appendOperator(stack, out, Operation::Kind::BitOrAssign, tokens[i].codePosition);
                    i += 2;
                }
                else if (tokens[i].value + tokens[i + 1].value == "^=") {
                    appendOperator(stack, out, Operation::Kind::BitXorAssign, tokens[i].codePosition);
                    i += 2;
                }
                else if (tokens[i].value + tokens[i + 1].value + tokens[i + 2].value == "<<=") {
                    appendOperator(stack, out, Operation::Kind::ShlAssign, tokens[i].codePosition);
                    i += 3;
                }
                else if (tokens[i].value + tokens[i + 1].value + tokens[i + 2].value == ">>=") {
                    appendOperator(stack, out, Operation::Kind::ShrAssign, tokens[i].codePosition);
                    i += 3;
                }
                else if (tokens[i].value+tokens[i+1].value+tokens[i+2].value+tokens[i+3].value == "<<<=") {
                    appendOperator(stack, out, Operation::Kind::SalAssign, tokens[i].codePosition);
                    i += 4;
                }
                else if (tokens[i].value+tokens[i+1].value+tokens[i+2].value+tokens[i+3].value == ">>>=") {
                    appendOperator(stack, out, Operation::Kind::SarAssign, tokens[i].codePosition);
                    i += 4;
                }
                else if (tokens[i].value == "+") {
                    appendOperator(stack, out, Operation::Kind::Add, tokens[i++].codePosition);
                }
                else if (tokens[i].value == "-") {
                    appendOperator(stack, out, Operation::Kind::Sub, tokens[i++].codePosition);
                }
                else if (tokens[i].value == "*") {
                    appendOperator(stack, out, Operation::Kind::Mul, tokens[i++].codePosition);
                }
                else if (tokens[i].value == "/") {
                    appendOperator(stack, out, Operation::Kind::Div, tokens[i++].codePosition);
                }
                else if (tokens[i].value == "%") {
                    appendOperator(stack, out, Operation::Kind::Mod, tokens[i++].codePosition);
                }
                else if (tokens[i].value == ".") {
                    appendOperator(stack, out, Operation::Kind::Dot, tokens[i++].codePosition);
                }
                else if (tokens[i].value == "|") {
                    appendOperator(stack, out, Operation::Kind::BitOr, tokens[i++].codePosition);
                }
                else if (tokens[i].value == "&") {
                    appendOperator(stack, out, Operation::Kind::BitAnd, tokens[i++].codePosition);
                }
                else if (tokens[i].value == "^") {
                    appendOperator(stack, out, Operation::Kind::BitXor, tokens[i++].codePosition);
                }
                else if (tokens[i].value == "<") {
                    appendOperator(stack, out, Operation::Kind::Lt, tokens[i++].codePosition);
                }
                else if (tokens[i].value == ">") {
                    appendOperator(stack, out, Operation::Kind::Gt, tokens[i++].codePosition);
                }
                else if (tokens[i].value == "=") {
                    appendOperator(stack, out, Operation::Kind::Assign, tokens[i++].codePosition);
                } else {
                    // end of expression
                }
            }
            break;
        }
    }

    return out;
}

unique_ptr<Value> solveReversePolishNotation(const vector<unique_ptr<Value>>& stack) {
    return nullptr;
}

unique_ptr<Value> getValue(const vector<Token>& tokens, int& i, const string& skipDelimiter) {
    auto reversePolishNotation = getReversePolishNotation(tokens, i, skipDelimiter);
    if (!reversePolishNotation) {
        return nullptr;
    }
    if (reversePolishNotation.value().empty()) {
        return make_unique<Value>(tokens[i].codePosition, true);
    }
    return solveReversePolishNotation(reversePolishNotation.value());
}


optional<vector<unique_ptr<Type>>> getFunctionArgumentTypes(const vector<Token>& tokens, int& i, bool writeError=true) {
    vector<unique_ptr<Type>> types;
    while (tokens[i].value != ")") {
        auto type = getType(tokens, i, {",", ")"}, writeError);
        if (!type) {
            return nullopt;
        }
        types.push_back(move(type));
        if (tokens[i+1].value == "," && tokens[i+2].value != ")") {
            i += 2;
        } else if (tokens[i+1].value == ")") {
            i += 1;
        } else if (tokens[i+1].value == "," && tokens[i + 2].value == ")") {
            writeError ? errorMessage("expected function argument type, got ')'", tokens[i+2].codePosition) : false;
            return nullopt;
        }
        else {
            writeError ? errorMessage(
                "during interpreting function arguments expected ',' or ')', got " + tokens[i+1].value, 
                tokens[i+1].codePosition
            ) : false;
            return nullopt;
        }
    }
    return types;
}

unique_ptr<Type> getType(const vector<Token>& tokens, int& i, const vector<string>& delimiters, bool writeError) {
    unique_ptr<Type> type = nullptr;
    if (tokens[i].value == "!") {
        i += 1;
        auto underlyingType = getType(tokens, i, delimiters, writeError);
        if (underlyingType) {
            type = make_unique<OwnerPointerType>(move(underlyingType));
        }
    } else if (tokens[i].value == "*") {
        i += 1;
        auto underlyingType = getType(tokens, i, delimiters, writeError);
        if (underlyingType) {
            type = make_unique<RawPointerType>(move(underlyingType));
        }
    } else if (tokens[i].value == "&") {
        i += 1;
        auto underlyingType = getType(tokens, i, delimiters, writeError);
        if (underlyingType) {
            type = make_unique<ReferenceType>(move(underlyingType));
        }
    } else if (tokens[i].value == "?") {
        i += 1;
        auto underlyingType = getType(tokens, i, delimiters, writeError);
        if (underlyingType) {
            type = make_unique<MaybeErrorType>(move(underlyingType));
        }
    } else if (tokens[i].value == "[") {
        if (tokens[i + 1].value == "]") {
            i += 2;
            auto elementType = getType(tokens, i, delimiters, writeError);
            if (elementType) {
                type = make_unique<DynamicArrayType>(move(elementType));
            }
        } else if (tokens[i+1].value == "*" && tokens[i+2].value == "]") {
            i += 3;
            auto elementType = getType(tokens, i, delimiters, writeError);
            if (elementType) {
                type = make_unique<ArrayViewType>(move(elementType));
            }
        } else {
            i += 1;
            auto sizeValue = getValue(tokens, i, "]");
            auto elementType = getType(tokens, i, delimiters, writeError);
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
            writeError ? errorMessage("expected template type name, got " + tokens[i].value, tokens[i].codePosition) : false;
            return nullptr;
        }
        if (tokens[i + 1].value != ">") {
            writeError ? errorMessage("expected '>', got " + tokens[i+1].value, tokens[i+1].codePosition) : false;
            return nullptr;
        }
        templateFunctionType->templateTypes.push_back(make_unique<TemplateType>(tokens[i].value));
        i += 2;

        if (tokens[i].value != "(") {
            writeError ? errorMessage("expected start of templated function type '(', got" + tokens[i].value, tokens[i].codePosition) : false;
            return nullptr;
        }
        i += 1;
        auto argumentTypesOpt = getFunctionArgumentTypes(tokens, i, writeError);
        if (!argumentTypesOpt) {
            return nullptr;
        }
        templateFunctionType->argumentTypes = move(argumentTypesOpt.value());
        if (tokens[i].value + tokens[i+1].value != "->") {
            templateFunctionType->returnType = make_unique<Type>(Type::Kind::Void);
        } else {
            i += 2;
            templateFunctionType->returnType = getType(tokens, i, delimiters, writeError);
            if (!templateFunctionType->returnType) {
                return nullptr;
            }
        }
        type = move(templateFunctionType);
    } else if (tokens[i].value == "(") {
        i += 1;
        auto functionType = make_unique<FunctionType>();
        auto argumentTypesOpt = getFunctionArgumentTypes(tokens, i, writeError);
        if (!argumentTypesOpt) {
            return nullptr;
        }
        functionType->argumentTypes = move(argumentTypesOpt.value());
        if (tokens[i].value + tokens[i+1].value != "->") {
            functionType->returnType = make_unique<Type>(Type::Kind::Void);
        } else {
            i += 2;
            functionType->returnType = getType(tokens, i, delimiters, writeError);
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
                    auto templateType = getType(tokens, i, {",", ">"}, writeError);
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
        writeError ? errorMessage("unexpected '" + tokens[i].value + "' during type interpreting", tokens[i].codePosition) : false;
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