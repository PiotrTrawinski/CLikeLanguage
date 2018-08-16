#include "Scope.h"
#include "DeclarationMap.h"
#include "Operation.h"
#include "Value.h"
#include "Declaration.h"
#include "ClassDeclaration.h"

using namespace std;


/*
    Scope
*/
int Scope::ID_COUNT = 0;
Scope::Scope(const CodePosition& position, Owner owner, Scope* parentScope) : 
    Statement(position, Statement::Kind::Scope),
    owner(owner),
    parentScope(parentScope)
{
    id = ID_COUNT;
    ID_COUNT += 1;
}
bool Scope::operator==(const Statement& scope) const {
    if(typeid(scope) == typeid(*this)){
        const auto& other = static_cast<const Scope&>(scope);
        return this->owner == other.owner
            //&& this->parentScope == other.parentScope
            && Statement::operator==(other);
    } else {
        return false;
    }
}
Scope::ReadStatementValue Scope::readStatement(const vector<Token>& tokens, int& i) {
    auto& token = tokens[i];
    switch (token.type) {
    case Token::Type::Char:
        return errorMessageBool("statement cannot start with a char value", token.codePosition);
    case Token::Type::StringLiteral:
        return errorMessageBool("statement cannot start with a string literal", token.codePosition);
    case Token::Type::Integer:
        return errorMessageBool("statement cannot start with an integer value", token.codePosition);
    case Token::Type::Float:
        return errorMessageBool("statement cannot start with a float value", token.codePosition);
    case Token::Type::Symbol:
        if (token.value == "{") {
            i += 1;
            auto codeScope = CodeScope::Create(token.codePosition, Scope::Owner::None, this);
            if (!codeScope->createCodeTree(tokens, i)) {
                return false;
            }
            return Scope::ReadStatementValue(codeScope);
        }
        else if (token.value == "}") {
            // end of scope
            i += 1;
            return Scope::ReadStatementValue(true);
        }
        else {
            auto value = getValue(tokens, i, {";", "}"}, true);
            if (!value) {
                return false;
            }
            return Scope::ReadStatementValue(value);
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
                return errorMessageBool("statement cannot start with r-value", token.codePosition);
            case Keyword::Kind::TypeName:
                return errorMessageBool("statement cannot start with type name", token.codePosition);
            case Keyword::Kind::ScopeStart: {
                Scope* scope = nullptr;
                switch (((ScopeStartKeyword*)keyword)->value) {
                case ScopeStartKeyword::Value::Class:
                    scope = ClassScope::Create(token.codePosition, this);  break;
                case ScopeStartKeyword::Value::Defer:
                    scope = DeferScope::Create(token.codePosition, this);  break;
                case ScopeStartKeyword::Value::If:
                    scope = IfScope::Create(token.codePosition, this);     break;
                case ScopeStartKeyword::Value::Else:
                    return errorMessageBool("start of an else scope not directly after an if scope", token.codePosition);
                case ScopeStartKeyword::Value::While:
                    scope = WhileScope::Create(token.codePosition, this);  break;
                case ScopeStartKeyword::Value::For:
                    scope = ForScope::Create(token.codePosition, this);    break;
                }
                i += 1;
                if (!scope->createCodeTree(tokens, i)) {
                    return false;
                }
                return Scope::ReadStatementValue(scope);
            }
            case Keyword::Kind::FlowStatement:
                Operation* operation = nullptr;
                switch (((FlowStatementKeyword*)keyword)->value) {
                case FlowStatementKeyword::Value::Break:
                    operation = FlowOperation::Create(token.codePosition, Operation::Kind::Break);    break;
                case FlowStatementKeyword::Value::Continue:
                    operation = FlowOperation::Create(token.codePosition, Operation::Kind::Continue); break;
                case FlowStatementKeyword::Value::Remove:
                    operation = FlowOperation::Create(token.codePosition, Operation::Kind::Remove);   break;
                case FlowStatementKeyword::Value::Return:
                    operation = FlowOperation::Create(token.codePosition, Operation::Kind::Return);   break;
                }
                i += 1;
                auto value = getValue(tokens, i, {";", "}"}, true);
                if (!value) {
                    return false;
                }
                if (value->valueKind != Value::ValueKind::Empty) {
                    operation->arguments.push_back(value);
                }
                return Scope::ReadStatementValue(operation);
            }
        }
        else if (i + 1 >= tokens.size()) {
            return errorMessageBool("unexpected end of file", token.codePosition);
        }
        else if (tokens[i + 1].value == ":" || tokens[i + 1].value == "&") {
            // declaration
            if (i + 3 >= tokens.size()) {
                return errorMessageBool("unexpected end of assignment", token.codePosition);
            }
            if (tokens[i + 3].value == "class") {
                // class declaration/scope
                if (tokens[i + 1].value + tokens[i+2].value != "::") {
                    return errorMessageBool("unexpected symbols in class declaration. Did you mean to use '::'?", tokens[i+1].codePosition);
                }
                i += 4;
                auto classDeclaration = ClassDeclaration::Create(token.codePosition, token.value);
                classDeclaration->body = ClassScope::Create(token.codePosition, this);
                if (!classDeclaration->body->createCodeTree(tokens, i)) {
                    return false;
                }
                return Scope::ReadStatementValue(classDeclaration);
            } else {
                // variable declaration
                bool declareByReference = tokens[i + 1].value == "&";
                i += 2;
                Type* type = nullptr;
                if (tokens[i].value != ":" && tokens[i].value != "=") {
                    type = getType(tokens, i, {":", "=", ";"});
                    if (!type) {
                        return false;
                    }
                }
                auto declaration = Declaration::Create(token.codePosition);
                declaration->byReference = declareByReference;
                declaration->variable->isConst = tokens[i].value == ":";
                declaration->variable->name = token.value;
                declaration->variable->type = type;
                i += 1;
                if (tokens[i - 1].value != ";") {
                    declaration->value = getValue(tokens, i, {";", "}"}, true);
                    if (!declaration->value) {
                        return false;
                    }
                }
                return Scope::ReadStatementValue(declaration);
            }
        }
        else {
            auto value = getValue(tokens, i, {";", "}"}, true);
            if (!value) {
                return false;
            }
            return Scope::ReadStatementValue(value);
        }
    }
}
void appendOperator(vector<Operation*>& stack, vector<Value*>& out, Operation* operation) {
    if (operation->getIsLeftAssociative()) {
        while (stack.size() > 0 && operation->getPriority() >= stack.back()->getPriority()) {
            out.push_back(stack.back());
            stack.pop_back();
        }
    } else {
        while (stack.size() > 0 && operation->getPriority() > stack.back()->getPriority()) {
            out.push_back(stack.back());
            stack.pop_back();
        }
    }
    stack.push_back(operation);
}
void appendOperator(vector<Operation*>& stack, vector<Value*>& out, Operation::Kind kind, const CodePosition& codePosition) {
    appendOperator(stack, out, Operation::Create(codePosition, kind));
}
optional<vector<Value*>> Scope::getReversePolishNotation(const vector<Token>& tokens, int& i) {
    vector<Operation*> stack;
    vector<Value*> out;
    bool endOfExpression = false;
    bool expectValue = true;
    int lastWasLambda = 0;
    int openBracketsCount = 0;
    while (!endOfExpression) {
        if (lastWasLambda >= 1) {
            break;
        }
        lastWasLambda -= 1;
        if (i >= tokens.size()) {
            if (lastWasLambda >= 0) {
                break;
            }
            return errorMessageOpt("unexpected end of file", tokens[i-1].codePosition);
        }
        switch (tokens[i].type) {
        case Token::Type::Integer:
            if (!expectValue) {
                endOfExpression = true;
                break;
            }
            out.push_back(IntegerValue::Create(tokens[i].codePosition, stoi(tokens[i].value)));
            i += 1;
            expectValue = false;
            break;
        case Token::Type::Float:
            if (!expectValue) {
                endOfExpression = true;
                break;
            }
            out.push_back(FloatValue::Create(tokens[i].codePosition, stod(tokens[i].value)));
            i += 1;
            expectValue = false;
            break;
        case Token::Type::Char:
            if (!expectValue) {
                endOfExpression = true;
                break;
            }
            out.push_back(CharValue::Create(tokens[i].codePosition, (int)tokens[i].value[0]));
            i += 1;
            expectValue = false;
            break;
        case Token::Type::StringLiteral:
            if (!expectValue) {
                endOfExpression = true;
                break;
            }
            out.push_back(StringValue::Create(tokens[i].codePosition, tokens[i].value));
            i += 1;
            expectValue = false;
            break;
        case Token::Type::Label:{
            if (!expectValue) {
                if (tokens[i].value == "onError" || tokens[i].value == "onSuccess") {
                    auto operation = ErrorResolveOperation::Create(tokens[i].codePosition);
                    while (tokens[i].value == "onError" || tokens[i].value == "onSuccess") {
                        CodeScope* codeScope = nullptr;
                        if (tokens[i].value == "onError") {
                            if (operation->onErrorScope) {
                                return errorMessageOpt("multiple onError scopes", tokens[i].codePosition);
                            }
                            operation->onErrorScope = CodeScope::Create(tokens[i].codePosition, Scope::Owner::OnError, this);
                            codeScope = operation->onErrorScope;
                        } else {
                            if (operation->onSuccessScope) {
                                return errorMessageOpt("multiple onSuccess scopes", tokens[i].codePosition);
                            }
                            operation->onSuccessScope = CodeScope::Create(tokens[i].codePosition, Scope::Owner::OnSuccess, this);
                            codeScope = operation->onSuccessScope;
                        }
                        i += 1;
                        if (tokens[i].value == "{") {
                            i += 1;
                            codeScope->createCodeTree(tokens, i);
                        } else {
                            auto statementValue = readStatement(tokens, i);
                            if (statementValue.isScopeEnd) {
                                return errorMessageOpt("unexpected '}' (trying to close unopened error-resolve scope)", tokens[i-1].codePosition);
                            } else if (!statementValue.statement) {
                                return nullopt;
                            } else if (statementValue.statement->kind == Statement::Kind::ClassDeclaration) {
                                return errorMessageOpt("expected expression, got class declaration", tokens[i-1].codePosition);
                            }
                            codeScope->statements.push_back(statementValue.statement);
                        }
                    }
                    i -= 1;
                    appendOperator(stack, out, operation);
                }
                endOfExpression = true;
                break;
            }
            if (tokens[i].value == "true") {
                out.push_back(BoolValue::Create(tokens[i].codePosition, true));
                i += 1;
                expectValue = false;
                break;
            }
            else if (tokens[i].value == "false") {
                out.push_back(BoolValue::Create(tokens[i].codePosition, false));
                i += 1;
                expectValue = false;
                break;
            }
            else if (tokens[i].value == "alloc") {
                i += 1;
                auto operation = Operation::Create(tokens[i].codePosition, Operation::Kind::Allocation);
                operation->type = getType(tokens, i, {";"});
                if (!operation->type) {
                    return nullopt;
                }
                operation->type = OwnerPointerType::Create(operation->type);
                out.push_back(operation);
                expectValue = false;
                //appendOperator(stack, out, Operation::Kind::Allocation, tokens[i++].codePosition);
            }
            else if (tokens[i].value == "dealloc") {
                appendOperator(stack, out, Operation::Kind::Deallocation, tokens[i++].codePosition);
                expectValue = true;
            } else {
                bool isTemplateFunctionCall = false;
                if (tokens[i + 1].value == "<") {
                    // either 'less then' operator or function call template arguments
                    // assume its template arguments and check if it makes sense
                    isTemplateFunctionCall = true;
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
                        vector<Type*> templateTypes;
                        int k = i+2;
                        while (k < j) {
                            auto type = getType(tokens, k, {",", ">"}, false);
                            if (type) {
                                templateTypes.push_back(type);
                                k += 1;
                            } else {
                                isTemplateFunctionCall = false;
                                break;
                            }
                        }
                        if (isTemplateFunctionCall) {
                            auto templateCall = TemplateFunctionCallOperation::Create(tokens[i].codePosition);
                            templateCall->templateTypes = templateTypes;
                            templateCall->idName = tokens[i].value;
                            i = k;
                            if (tokens[i].value == "(") {
                                // read function arguments
                                i += 1;
                                if (tokens[i].value == ")") {
                                    i += 1;
                                } else {
                                    bool endOfArguments = false;
                                    do {
                                        auto value = getValue(tokens, i, {",", ")"});
                                        if (!value) return nullopt;
                                        if (tokens[i].value == ")") {
                                            endOfArguments = true;
                                        }
                                        templateCall->arguments.push_back(value);
                                        i += 1;
                                    } while(!endOfArguments);
                                }
                                out.push_back(templateCall);
                                lastWasLambda = 1;
                                expectValue = false;
                            } else {
                                return errorMessageOpt("expected '(' - begining of function call arguments, got" + tokens[i].value, tokens[i].codePosition);
                            }
                        }
                    } else {
                        isTemplateFunctionCall = false;
                    }
                } 
                if (!isTemplateFunctionCall) {
                    // variable
                    auto variable = Variable::Create(tokens[i].codePosition);
                    variable->name = tokens[i].value;
                    out.push_back(variable);
                    i += 1;
                    expectValue = false;
                }
            }
            break;
        }
        case Token::Type::Symbol:
            if (tokens[i].value == "}") {
                return errorMessageOpt("unexpected '}' symbol", tokens[i].codePosition);
            }
            if (expectValue) {
                if (tokens[i].value == "(") {
                    // either normal opening bracket or function value (lambda)
                    int openBrackets = 1;
                    int openSquereBrackets = 0;
                    bool wasColon = false;
                    int j = i + 1;
                    while (j < tokens.size() && openBrackets != 0) {
                        if (tokens[j].value == "(") {
                            openBrackets += 1;
                        } else if (tokens[j].value == ")") {
                            openBrackets -= 1;
                        } else if (tokens[j].value == "[") {
                            openSquereBrackets += 1;
                        } else if (tokens[j].value == "]") {
                            openSquereBrackets -= 1;
                        } else if (tokens[j].value == ":" && openBrackets == 1 && openSquereBrackets == 0) {
                            wasColon = true;
                        }
                        j += 1;
                    }
                    if ((wasColon || j==i+2) && j+1 < tokens.size() && (tokens[j].value+tokens[j+1].value == "->" || tokens[j].value == "{")) {
                        // function value (lambda)
                        auto lambda = FunctionValue::Create(tokens[i].codePosition, nullptr, this);
                        auto lambdaType = FunctionType::Create();
                        i += 1;
                        if (tokens[i].value != ")") {
                            while (true) {
                                if (tokens[i].type != Token::Type::Label) {
                                    return errorMessageOpt("expected function variable name, got " + tokens[i].value, tokens[i].codePosition);
                                }
                                if (tokens[i+1].value != ":") {
                                    return errorMessageOpt("expected ':', got " + tokens[i+1].value, tokens[i+1].codePosition);
                                }
                                int declarationStart = i;
                                //lambda->argumentNames.push_back(tokens[i].value);
                                i += 2;
                                auto type = getType(tokens, i, {",", ")"});
                                if (!type) { return nullopt; }
                                lambdaType->argumentTypes.push_back(type);
                                lambda->arguments.push_back(Declaration::Create(tokens[declarationStart].codePosition));
                                lambda->arguments.back()->variable->name = tokens[declarationStart].value;
                                lambda->arguments.back()->variable->type = type;
                                if (tokens[i].value == ")") {
                                    break;
                                } else {
                                    i += 1;
                                }
                            }
                        }
                        i += 1;
                        if (tokens[i].value + tokens[i + 1].value == "->") {
                            i += 2;
                            lambdaType->returnType = getType(tokens, i, {"{"});
                            if (!lambdaType->returnType) { return nullopt; }
                        } else {
                            lambdaType->returnType = Type::Create(Type::Kind::Void);
                        }
                        i += 1;
                        lambda->type = lambdaType;
                        if (!lambda->body->createCodeTree(tokens, i)) {
                            return nullopt;
                        }
                        out.push_back(lambda);
                        lastWasLambda = 1;
                        expectValue = false;
                    } else {
                        // normal opening bracket
                        stack.push_back(Operation::Create(tokens[i++].codePosition, Operation::Kind::LeftBracket));
                        openBracketsCount += 1;
                        expectValue = true;
                    }
                }
                else if (tokens[i].value == "<") {
                    // template function value
                    auto templateFunction = FunctionValue::Create(tokens[i].codePosition, nullptr, this);
                    auto templateFunctionType = TemplateFunctionType::Create();
                    i += 1;
                    while (tokens[i].type == Token::Type::Label && tokens[i+1].value == ",") {
                        templateFunctionType->templateTypes.push_back(TemplateType::Create(tokens[i].value));
                        i += 2;
                    }
                    if (tokens[i].type != Token::Type::Label) {
                        return errorMessageOpt("expected template type name, got " + tokens[i].value, tokens[i].codePosition);
                    }
                    if (tokens[i + 1].value != ">") {
                        return errorMessageOpt("expected '>', got " + tokens[i+1].value, tokens[i+1].codePosition);
                    }
                    templateFunctionType->templateTypes.push_back(TemplateType::Create(tokens[i].value));
                    i += 2;
                    if (tokens[i].value != "(") {
                        return errorMessageOpt("expected start of templated function type '(', got" + tokens[i].value, tokens[i].codePosition);
                    }
                    i += 1;
                    if (tokens[i].value != ")") {
                        while (true) {
                            if (tokens[i].type != Token::Type::Label) {
                                return errorMessageOpt("expected function variable name, got " + tokens[i].value, tokens[i].codePosition);
                            }
                            if (tokens[i+1].value != ":") {
                                return errorMessageOpt("expected ':', got " + tokens[i+1].value, tokens[i+1].codePosition);
                            }
                            int declarationStart = i;
                            //templateFunction->argumentNames.push_back(tokens[i].value);
                            i += 2;
                            auto type = getType(tokens, i, {",", ")"});
                            if (!type) { return nullopt; }
                            templateFunctionType->argumentTypes.push_back(type);
                            templateFunction->arguments.push_back(Declaration::Create(tokens[declarationStart].codePosition));
                            templateFunction->arguments.back()->variable->name = tokens[declarationStart].value;
                            templateFunction->arguments.back()->variable->type = type;
                            if (tokens[i].value == ")") {
                                break;
                            } else {
                                i += 1;
                            }
                        }
                    }
                    i += 1;
                    if (tokens[i].value + tokens[i + 1].value == "->") {
                        i += 2;
                        templateFunctionType->returnType = getType(tokens, i, {"{"});
                        if (!templateFunctionType->returnType) { return nullopt; }
                    } else {
                        templateFunctionType->returnType = Type::Create(Type::Kind::Void);
                    }
                    i += 1;
                    templateFunction->type = templateFunctionType;
                    if (!templateFunction->body->createCodeTree(tokens, i)) {
                        return nullopt;
                    }
                    out.push_back(templateFunction);
                    lastWasLambda = 1;
                    expectValue = false;
                }
                else if (tokens[i].value == "[") {
                    int firstSquereBracketIndex = i;
                    // static array ([x, y, z, ...]) or type cast ([T])
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
                        return errorMessageOpt("missing closing ']'", tokens[i].codePosition);
                    }

                    if (tokens[j].type != Token::Type::Symbol || tokens[j].value == "(" || tokens[j].value == "!" || tokens[j].value == "@"
                        || tokens[j].value == "~"|| tokens[j].value == "$"|| tokens[j].value == "[") {
                        // type cast
                        i += 1;
                        auto type = getType(tokens, i, {"]"});
                        if (!type) {
                            return nullopt;
                        }
                        /*i += 2;
                        auto argument = getValue(tokens, i, {")"}, true);
                        if (!argument) {
                            return nullopt;
                        }
                        auto castOperation = CastOperation::Create(tokens[firstSquereBracketIndex].codePosition, type);
                        castOperation->arguments.push_back(argument);
                        appendOperator(stack, out, castOperation);
                        expectValue = false;*/
                        i += 1;
                        auto castOperation = CastOperation::Create(tokens[firstSquereBracketIndex].codePosition, type);
                        appendOperator(stack, out, castOperation);
                        expectValue = true;
                    } else {
                        // static array
                        auto staticArray = StaticArrayValue::Create(tokens[i].codePosition);
                        do {
                            i += 1;
                            auto value = getValue(tokens, i, {",", "]"});
                            if (!value) {
                                return nullopt;
                            }
                            if (value->valueKind == Value::ValueKind::Empty) {
                                return errorMessageOpt("expected array value, got '" + tokens[i].value + "'", tokens[i].codePosition);
                            }
                            staticArray->values.push_back(value);
                        } while(tokens[i].value != "]");
                        i += 1;
                        out.push_back(staticArray);
                        expectValue = false;
                    }
                } else if (tokens[i].value == "-") {
                    appendOperator(stack, out, Operation::Kind::Minus, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == "!") {
                    appendOperator(stack, out, Operation::Kind::LogicalNot, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == "@") {
                    appendOperator(stack, out, Operation::Kind::Address, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == "$") {
                    appendOperator(stack, out, Operation::Kind::GetValue, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == "~") {
                    appendOperator(stack, out, Operation::Kind::BitNeg, tokens[i++].codePosition);
                    expectValue = true;
                } else {
                    endOfExpression = true;
                }
            } else {
                if (tokens[i].value == "(") {
                    // function call
                    auto functionCall = FunctionCallOperation::Create(tokens[i].codePosition);
                    i += 1;
                    if (tokens[i].value == ")") {
                        i += 1;
                    } else {
                        bool endOfArguments = false;
                        do {
                            auto value = getValue(tokens, i, {",", ")"});
                            if (!value) return nullopt;
                            if (tokens[i].value == ")") {
                                endOfArguments = true;
                            }
                            functionCall->arguments.push_back(value);
                            i += 1;
                        } while(!endOfArguments);
                    }
                    appendOperator(stack, out, functionCall);
                    expectValue = false;
                }
                else if (tokens[i].value == ")" && openBracketsCount > 0) {
                    while (stack.size() > 0 && stack.back()->kind != Operation::Kind::LeftBracket) {
                        out.push_back(stack.back());
                        stack.pop_back();
                    }
                    if (stack.size() <= 0) {
                        return errorMessageOpt("incorrect bracketing '(' ')'", tokens[i].codePosition);
                    }
                    i += 1;
                    stack.pop_back();
                    openBracketsCount -= 1;
                    expectValue = false;
                }
                else if (tokens[i].value == "[") {
                    // array index/offset ([x]) or subarray indexing ([x:y])
                    bool isSubArrayIndexing = false;
                    int openSquereBrackets = 1;
                    int j = i+1;
                    while (j < tokens.size() && openSquereBrackets != 0) {
                        if (isSubArrayIndexing)
                            if (tokens[j].value == ":" && openSquereBrackets == 1) {
                                if (isSubArrayIndexing) {
                                    return errorMessageOpt("unexpected ':' symbol in subarray indexing", tokens[j].codePosition);
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
                        auto value1 = getValue(tokens, i, {":"}, true);
                        if (!value1) { return nullopt; }

                        auto value2 = getValue(tokens, i, {"]"}, true);
                        if (!value2) { return nullopt; }

                        auto subArrayOperation = ArraySubArrayOperation::Create(tokens[i-1].codePosition, value1, value2);
                        appendOperator(stack, out, subArrayOperation);
                    } else {
                        // array index/offset ([x])
                        i += 1;
                        auto value = getValue(tokens, i, {"]"}, true);
                        if (!value) {
                            return nullopt;
                        }
                        auto indexingOperation = ArrayIndexOperation::Create(tokens[i-1].codePosition, value);
                        appendOperator(stack, out, indexingOperation);
                    }
                    expectValue = false;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == "&&") {
                    appendOperator(stack, out, Operation::Kind::LogicalAnd, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == "||") {
                    appendOperator(stack, out, Operation::Kind::LogicalOr, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == "==") {
                    appendOperator(stack, out, Operation::Kind::Eq, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == "!=") {
                    appendOperator(stack, out, Operation::Kind::Neq, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == "<=") {
                    appendOperator(stack, out, Operation::Kind::Lte, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == ">=") {
                    appendOperator(stack, out, Operation::Kind::Gte, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == "+=") {
                    appendOperator(stack, out, Operation::Kind::AddAssign, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == "-=") {
                    appendOperator(stack, out, Operation::Kind::SubAssign, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == "*=") {
                    appendOperator(stack, out, Operation::Kind::MulAssign, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == "/=") {
                    appendOperator(stack, out, Operation::Kind::DivAssign, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == "%=") {
                    appendOperator(stack, out, Operation::Kind::ModAssign, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == "|=") {
                    appendOperator(stack, out, Operation::Kind::BitOrAssign, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == "^=") {
                    appendOperator(stack, out, Operation::Kind::BitXorAssign, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == "~=") {
                    appendOperator(stack, out, Operation::Kind::BitNegAssign, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+1 < tokens.size() && tokens[i].value + tokens[i + 1].value == "&=") {
                    endOfExpression = true;
                } else if (i+2 < tokens.size() && tokens[i].value + tokens[i + 1].value + tokens[i + 2].value == "<<=") {
                    appendOperator(stack, out, Operation::Kind::ShlAssign, tokens[i].codePosition);
                    i += 3;
                    expectValue = true;
                } else if (i+2 < tokens.size() && tokens[i].value + tokens[i + 1].value + tokens[i + 2].value == ">>=") {
                    appendOperator(stack, out, Operation::Kind::ShrAssign, tokens[i].codePosition);
                    i += 3;
                    expectValue = true;
                } else if (i+3 < tokens.size() && tokens[i].value+tokens[i+1].value+tokens[i+2].value+tokens[i+3].value == "<<<=") {
                    appendOperator(stack, out, Operation::Kind::SalAssign, tokens[i].codePosition);
                    i += 4;
                    expectValue = true;
                } else if (i+3 < tokens.size() && tokens[i].value+tokens[i+1].value+tokens[i+2].value+tokens[i+3].value == ">>>=") {
                    appendOperator(stack, out, Operation::Kind::SarAssign, tokens[i].codePosition);
                    i += 4;
                    expectValue = true;
                } else if (tokens[i].value == "+") {
                    appendOperator(stack, out, Operation::Kind::Add, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == "-") {
                    appendOperator(stack, out, Operation::Kind::Sub, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == "*") {
                    appendOperator(stack, out, Operation::Kind::Mul, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == "/") {
                    appendOperator(stack, out, Operation::Kind::Div, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == "%") {
                    appendOperator(stack, out, Operation::Kind::Mod, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == ".") {
                    appendOperator(stack, out, Operation::Kind::Dot, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == "|") {
                    appendOperator(stack, out, Operation::Kind::BitOr, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == "&") {
                    appendOperator(stack, out, Operation::Kind::BitAnd, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == "^") {
                    appendOperator(stack, out, Operation::Kind::BitXor, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == "<") {
                    appendOperator(stack, out, Operation::Kind::Lt, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == ">") {
                    appendOperator(stack, out, Operation::Kind::Gt, tokens[i++].codePosition);
                    expectValue = true;
                } else if (tokens[i].value == "=") {
                    appendOperator(stack, out, Operation::Kind::Assign, tokens[i++].codePosition);
                    expectValue = true;
                } else {
                    endOfExpression = true;
                }
            }
            break;
        }
    }
    if (lastWasLambda >= 0) {
        i -= 1;
    }

    while (stack.size() > 0) {
        if (stack.back()->kind == Operation::Kind::LeftBracket) {
            return errorMessageOpt("incorrect bracketing '(' ')'", tokens[i-1].codePosition);
        }
        out.push_back(stack.back());
        stack.pop_back();
    }

    return out;
}
Value* solveReversePolishNotation(vector<Value*>& values) {
    vector<Value*> stack;

    for (int i = 0; i < values.size(); ++i) {
        if (values[i]->valueKind == Value::ValueKind::Operation) {
            auto operation = (Operation*)values[i];
            if (operation->kind == Operation::Kind::FunctionCall) {
                ((FunctionCallOperation*)operation)->function = stack.back();
                stack.pop_back();
            } else {
                int numberOfArguments = operation->getNumberOfArguments();
                vector<Value*> arguments;
                for (int i = 0; i < numberOfArguments; ++i) {
                    arguments.push_back(stack.back());
                    stack.pop_back();
                }
                for (int i = 0; i < numberOfArguments; ++i) {
                    operation->arguments.push_back(arguments.back());
                    arguments.pop_back();
                }
            }
            stack.push_back(operation);
        } else {
            stack.push_back(values[i]);
        }
    }
    return stack.back();
}
Value* Scope::getValue(const vector<Token>& tokens, int& i, const vector<string>& delimiters, bool skipOnGoodDelimiter) {
    auto reversePolishNotation = getReversePolishNotation(tokens, i);
    if (!reversePolishNotation) {
        return nullptr;
    }
    if (find(delimiters.begin(), delimiters.end(), tokens[i].value) == delimiters.end()) {
        string message = "";
        if (delimiters.size() == 1) {
            message = "expected '" + delimiters[0] + "'";
        } else {
            message = "expected one of [";
            for (int j = 0; j < delimiters.size(); ++j) {
                message += "'" + delimiters[j] + "'";
                if (j != delimiters.size() - 1) {
                    message += ", ";
                }
            }
            message += "]";
        }
        message += ", got '" + tokens[i].value + "'";
        return errorMessageNull(message, tokens[i].codePosition);
    }
    if (skipOnGoodDelimiter) {
        i += 1;
    }
    if (reversePolishNotation.value().empty()) {
        return Value::Create(tokens[i-1].codePosition, Value::ValueKind::Empty);
    }
    return solveReversePolishNotation(reversePolishNotation.value());
}
optional<vector<Type*>> Scope::getFunctionArgumentTypes(const vector<Token>& tokens, int& i, bool writeError) {
    vector<Type*> types;
    if (tokens[i].value == ")") {
        i += 1;
        return types;
    }
    while (true) {
        auto type = getType(tokens, i, {",", ")"}, writeError);
        if (!type) {
            return nullopt;
        }
        types.push_back(type);
        if (tokens[i].value == ")") {
            i += 1;
            break;
        } else if (tokens[i].value == "," && tokens[i+1].value != ")") {
            i += 1;
        } else {
            if(writeError) errorMessageBool("expected function argument type, got " + tokens[i+1].value, tokens[i+1].codePosition);
            return nullopt;
        }
    }
    return types;
}
Type* Scope::getType(const vector<Token>& tokens, int& i, const vector<string>& delimiters, bool writeError) {
    Type* type = nullptr;
    if (tokens[i].value == "!") {
        i += 1;
        auto underlyingType = getType(tokens, i, delimiters, writeError);
        if (underlyingType) {
            type = OwnerPointerType::Create(underlyingType);
        }
    } else if (tokens[i].value == "*") {
        i += 1;
        auto underlyingType = getType(tokens, i, delimiters, writeError);
        if (underlyingType) {
            type = RawPointerType::Create(underlyingType);
        }
    } else if (tokens[i].value == "&") {
        i += 1;
        auto underlyingType = getType(tokens, i, delimiters, writeError);
        if (underlyingType) {
            type = ReferenceType::Create(underlyingType);
        }
    } else if (tokens[i].value == "?") {
        i += 1;
        auto underlyingType = getType(tokens, i, delimiters, writeError);
        if (underlyingType) {
            type = MaybeErrorType::Create(underlyingType);
        }
    } else if (tokens[i].value == "[") {
        if (tokens[i + 1].value == "]") {
            i += 2;
            auto elementType = getType(tokens, i, delimiters, writeError);
            if (elementType) {
                type = DynamicArrayType::Create(elementType);
            }
        } else if (tokens[i+1].value == "*" && tokens[i+2].value == "]") {
            i += 3;
            auto elementType = getType(tokens, i, delimiters, writeError);
            if (elementType) {
                type = ArrayViewType::Create(elementType);
            }
        } else {
            i += 1;
            auto sizeValue = getValue(tokens, i, {"]"}, true);
            auto elementType = getType(tokens, i, delimiters, writeError);
            if (elementType && sizeValue) {
                type = StaticArrayType::Create(elementType, sizeValue);
            }
        }
    } else if (tokens[i].value == "<") {
        auto templateFunctionType = TemplateFunctionType::Create();
        i += 1;
        while (tokens[i].type == Token::Type::Label && tokens[i+1].value == ",") {
            templateFunctionType->templateTypes.push_back(TemplateType::Create(tokens[i].value));
            i += 2;
        }
        if (tokens[i].type != Token::Type::Label) {
            if (writeError) errorMessageBool("expected template type name, got " + tokens[i].value, tokens[i].codePosition);
            return nullptr;
        }
        if (tokens[i + 1].value != ">") {
            if (writeError) errorMessageBool("expected '>', got " + tokens[i+1].value, tokens[i+1].codePosition);
            return nullptr;
        }
        templateFunctionType->templateTypes.push_back(TemplateType::Create(tokens[i].value));
        i += 2;

        if (tokens[i].value != "(") {
            if (writeError) errorMessageBool("expected start of templated function type '(', got" + tokens[i].value, tokens[i].codePosition);
            return nullptr;
        }
        i += 1;
        auto argumentTypesOpt = getFunctionArgumentTypes(tokens, i, writeError);
        if (!argumentTypesOpt) {
            return nullptr;
        }
        templateFunctionType->argumentTypes = argumentTypesOpt.value();
        if (i+1 >= tokens.size() || tokens[i].value + tokens[i+1].value != "->") {
            templateFunctionType->returnType = Type::Create(Type::Kind::Void);
        } else {
            i += 2;
            templateFunctionType->returnType = getType(tokens, i, delimiters, writeError);
            if (!templateFunctionType->returnType) {
                return nullptr;
            }
        }
        type = templateFunctionType;
    } else if (tokens[i].value == "(") {
        i += 1;
        auto functionType = FunctionType::Create();
        auto argumentTypesOpt = getFunctionArgumentTypes(tokens, i, writeError);
        if (!argumentTypesOpt) {
            return nullptr;
        }
        functionType->argumentTypes = argumentTypesOpt.value();
        if (i+1 >= tokens.size() || tokens[i].value + tokens[i+1].value != "->") {
            functionType->returnType = Type::Create(Type::Kind::Void);
        } else {
            i += 2;
            functionType->returnType = getType(tokens, i, delimiters, writeError);
            if (!functionType->returnType) {
                return nullptr;
            }
        }
        type = functionType;
    } else if (tokens[i].type == Token::Type::Label) {
        auto keyword = Keyword::get(tokens[i].value);
        if (keyword && keyword->kind == Keyword::Kind::TypeName) {
            auto typeValue = ((TypeKeyword*)keyword)->value;
            switch (typeValue) {
            case TypeKeyword::Value::Int:
                type = IntegerType::Create(IntegerType::Size::I64); break;
            case TypeKeyword::Value::I8:
                type = IntegerType::Create(IntegerType::Size::I8);  break;
            case TypeKeyword::Value::I16:
                type = IntegerType::Create(IntegerType::Size::I16); break;
            case TypeKeyword::Value::I32:
                type = IntegerType::Create(IntegerType::Size::I32); break;
            case TypeKeyword::Value::I64:
                type = IntegerType::Create(IntegerType::Size::I64); break;
            case TypeKeyword::Value::U8:
                type = IntegerType::Create(IntegerType::Size::U8);  break;
            case TypeKeyword::Value::U16:
                type = IntegerType::Create(IntegerType::Size::U16); break;
            case TypeKeyword::Value::U32:
                type = IntegerType::Create(IntegerType::Size::U32); break;
            case TypeKeyword::Value::U64:
                type = IntegerType::Create(IntegerType::Size::U64); break;
            case TypeKeyword::Value::Float:
                type = FloatType::Create(FloatType::Size::F64); break;
            case TypeKeyword::Value::F32:
                type = FloatType::Create(FloatType::Size::F32); break;
            case TypeKeyword::Value::F64:
                type = FloatType::Create(FloatType::Size::F64); break;
            case TypeKeyword::Value::Bool:
                type = Type::Create(Type::Kind::Bool); break;
            case TypeKeyword::Value::String:
                type = Type::Create(Type::Kind::String); break;
            case TypeKeyword::Value::Void:
                type = Type::Create(Type::Kind::Void); break;
            default:
                break;
            }
            i += 1;
        } else {
            auto className = ClassType::Create(tokens[i].value);
            i += 1;
            if (tokens[i].value == "<") {
                while (true) {
                    i += 1;
                    auto templateType = getType(tokens, i, {",", ">"}, writeError);
                    if (!templateType) {
                        return nullptr;
                    }
                    className->templateTypes.push_back(templateType);
                    if (tokens[i].value == ">") {
                        break;
                    }
                }
                i += 1;
            }
            type = className;
        }
    } else {
        if (writeError) errorMessageBool("unexpected '" + tokens[i].value + "' during type interpreting", tokens[i].codePosition);
        return nullptr;
    }
    if (!type) {
        return nullptr;
    }
    if (find(delimiters.begin(), delimiters.end(), tokens[i].value) != delimiters.end()) {
        return type;
    } else {
        string message = "";
        if (delimiters.size() == 1) {
            message = "expected '" + delimiters[0] + "'";
        } else {
            message = "expected one of [";
            for (int j = 0; j < delimiters.size(); ++j) {
                message += "'" + delimiters[j] + "'";
                if (j != delimiters.size() - 1) {
                    message += ", ";
                }
            }
            message += "]";
        }
        message += ", got '" + tokens[i].value + "'";
        if (writeError) errorMessageBool(message, tokens[i].codePosition);
        return nullptr;
    }
}
Declaration* Scope::findDeclaration(Variable* variable, bool ignoreClassScopes) {
    if (owner == Owner::Class) {
        if (ignoreClassScopes) {
            return parentScope->findDeclaration(variable, ignoreClassScopes);
        } else {
            ignoreClassScopes = true;
        }
    }
    auto declarations = declarationMap.getDeclarations(variable->name);
    if (declarations.empty()) {
        Declaration* declaration = findAndInterpretDeclaration(variable->name);
        if (declaration && declaration->variable->isConstexpr) {
            return declaration;
        } else {
            if (parentScope == nullptr) {
                return errorMessageNull("missing declaration of variable " + variable->name, variable->position);
            }
            return parentScope->findDeclaration(variable, ignoreClassScopes);
        }
    }
    else if (declarations.size() == 1) {
        declarations[0]->scope = this;
        switch (declarations[0]->status) {
        case Declaration::Status::None:
            internalError("impossible state", variable->position);
        case Declaration::Status::InEvaluation:
            return errorMessageNull("recursive variable dependency", variable->position);
        case Declaration::Status::Evaluated:
            if (declarations[0]->variable->isConstexpr) {
                return declarations[0];
            } else {
                if (parentScope == nullptr) {
                    return errorMessageNull("missing declaration of variable " + variable->name, variable->position);
                }
                return parentScope->findDeclaration(variable, ignoreClassScopes);
            }
        case Declaration::Status::Completed:
            return declarations[0];
        }
    } else {
        string msg = "ambigous reference to variable " + variable->name + ".\n";
        msg += "possible variables at lines: \n";
        for (int i = 0; i < declarations.size(); ++i) {
            msg += to_string(declarations[i]->position.lineNumber);
            if (i != declarations.size() - 1) {
                msg += "\n";
            }
        }
        return errorMessageNull(msg, variable->position);
    }
}
unordered_set<Declaration*> Scope::getUninitializedDeclarations() {
    return maybeUninitializedDeclarations;
}
bool Scope::getHasReturnStatement() {
    return hasReturnStatement;
}
unordered_map<Declaration*, bool> Scope::getDeclarationsInitState() {
    return declarationsInitState;
}

/*
    CodeScope
*/
CodeScope::CodeScope(const CodePosition& position, Scope::Owner owner, Scope* parentScope, bool isGlobalScope) : 
    Scope(position, owner, parentScope),
    isGlobalScope(isGlobalScope)
{}
vector<unique_ptr<CodeScope>> CodeScope::objects;
CodeScope* CodeScope::Create(const CodePosition& position, Scope::Owner owner, Scope* parentScope, bool isGlobalScope) {
    objects.emplace_back(make_unique<CodeScope>(position, owner, parentScope, isGlobalScope));
    return objects.back().get();
}
bool CodeScope::operator==(const Statement& scope) const {
    if(typeid(scope) == typeid(*this)){
        const auto& other = static_cast<const CodeScope&>(scope);
        return this->isGlobalScope == other.isGlobalScope
            && this->statements == other.statements
            && Scope::operator==(other);
    } else {
        return false;
    }
}
bool CodeScope::createCodeTree(const vector<Token>& tokens, int& i) {
    while (i < tokens.size()) {
        auto statementValue = readStatement(tokens, i);
        if (statementValue) {
            if (statementValue.isScopeEnd) {
                if (isGlobalScope) {
                    return errorMessageBool("unexpected '}'. (trying to close global scope)", tokens[i-1].codePosition);
                } else {
                    break;
                }
            } else {
                if (statementValue.statement->kind == Statement::Kind::ClassDeclaration) {
                    if (!classDeclarationMap.add((ClassDeclaration*)statementValue.statement)) {
                        return errorMessageBool("redefinition of class declaration", statementValue.statement->position);
                    }
                }
                statements.push_back(statementValue.statement);
            }
        } else {
            return false;
        }
    }
    return true;
}
bool CodeScope::interpretNoUnitializedDeclarationsSet() {
    bool wereErrors = false;
    Declaration* declarationDependingOnErrorScope = nullptr;
    for (int i = 0; i < statements.size(); ++i) {
        auto& statement = statements[i];
        switch (statement->kind) {
        case Statement::Kind::Declaration: {
            Declaration* declaration = (Declaration*)statement;
            if (!declaration->interpret(this)) {
                wereErrors = true;
                break;
            }
            declarationsInitState.insert({declaration, declaration->value});
            declarationsOrder.push_back(declaration);
            if (hasReturnStatement && !declaration->variable->isConstexpr) {
                warningMessage("unreachable statement", statement->position);
                statement->isReachable = false;
            }
            if (!declaration->value) {
                maybeUninitializedDeclarations.insert(declaration);
            } else if (declaration->value->valueKind == Value::ValueKind::Operation) {
                if (((Operation*)declaration->value)->containsErrorResolve) {
                    maybeUninitializedDeclarations.insert(declaration);
                    declarationDependingOnErrorScope = declaration;
                }
            }
            break;
        }
        case Statement::Kind::ClassDeclaration: {
            ClassDeclaration* declaration = (ClassDeclaration*)statement;
            if (!declaration->interpret()) {
                wereErrors = true;
            }
            break;
        }
        case Statement::Kind::Scope: {
            if (isGlobalScope) {
                return errorMessageBool("global scope can only have variable and class declarations", statement->position);
            }
            if (hasReturnStatement) {
                warningMessage("unreachable scope statement", statement->position);
                statement->isReachable = false;
            }
            Scope* scope = (Scope*)statement;
            scope->hasReturnStatement = hasReturnStatement;
            scope->parentMaybeUninitializedDeclarations = maybeUninitializedDeclarations;
            scope->declarationsInitState = declarationsInitState;
            if (!scope->interpret()) {
                wereErrors = true;
            }
            declarationsInitState = scope->getDeclarationsInitState();
            maybeUninitializedDeclarations = scope->getUninitializedDeclarations();
            hasReturnStatement = hasReturnStatement || scope->getHasReturnStatement();
            break;
        }
        case Statement::Kind::Value: {
            if (isGlobalScope) {
                return errorMessageBool("global scope can only have variable and class declarations", statement->position);
            }
            if (hasReturnStatement) {
                warningMessage("unreachable statement", statement->position);
                statement->isReachable = false;
            }
            Value* value = (Value*)statement;
            auto valueInterpret = value->interpret(this);
            if (!valueInterpret) {
                wereErrors = true;
            } else if (valueInterpret.value()) {
                statement = valueInterpret.value();
                if (hasReturnStatement) {
                    statement->isReachable = false;
                }
            }
            break;
        }
        }

        if (onErrorScopeToInterpret) {
            onErrorScopeToInterpret->hasReturnStatement = hasReturnStatement;
            onErrorScopeToInterpret->parentMaybeUninitializedDeclarations = maybeUninitializedDeclarations;
            onErrorScopeToInterpret->declarationsInitState = declarationsInitState;
            onErrorScopeToInterpret->declarationsInitState.at(declarationDependingOnErrorScope) = false;
            if (!onErrorScopeToInterpret->interpret()) {
                wereErrors = true;
            }
            if (!onSuccessScopeToInterpret) {
                declarationsInitState = onErrorScopeToInterpret->getDeclarationsInitState();
            }

            if (onErrorScopeToInterpret->hasReturnStatement) {
                maybeUninitializedDeclarations.erase(declarationDependingOnErrorScope);
            } else {
                if (onErrorScopeToInterpret->maybeUninitializedDeclarations.find(declarationDependingOnErrorScope) == onErrorScopeToInterpret->maybeUninitializedDeclarations.end()) {
                    maybeUninitializedDeclarations.erase(declarationDependingOnErrorScope);
                }
            }
        }
        
        if (onSuccessScopeToInterpret) {
            onSuccessScopeToInterpret->hasReturnStatement = hasReturnStatement;
            onSuccessScopeToInterpret->parentMaybeUninitializedDeclarations = maybeUninitializedDeclarations;
            onSuccessScopeToInterpret->parentMaybeUninitializedDeclarations.erase(declarationDependingOnErrorScope);
            onSuccessScopeToInterpret->declarationsInitState = declarationsInitState;
            if (!onSuccessScopeToInterpret->interpret()) {
                wereErrors = true;
            }
            if (!onErrorScopeToInterpret) {
                declarationsInitState = onSuccessScopeToInterpret->getDeclarationsInitState();
            }
        }

        if (onErrorScopeToInterpret && onSuccessScopeToInterpret) {
            hasReturnStatement |= onErrorScopeToInterpret->hasReturnStatement && onSuccessScopeToInterpret->hasReturnStatement;
            for (auto declaration : onSuccessScopeToInterpret->maybeUninitializedDeclarations) {
                onErrorScopeToInterpret->maybeUninitializedDeclarations.insert(declaration);
            }
            maybeUninitializedDeclarations = onErrorScopeToInterpret->maybeUninitializedDeclarations;
            auto newDeclarationsInitState = onErrorScopeToInterpret->declarationsInitState;
            for (auto declaration : declarationsOrder) {
                if ((onSuccessScopeToInterpret->declarationsInitState.find(declaration) != onSuccessScopeToInterpret->declarationsInitState.end()
                    && onSuccessScopeToInterpret->declarationsInitState.at(declaration))){
                    newDeclarationsInitState.at(declaration) = true;
                }
            }
            declarationsInitState = newDeclarationsInitState;
        }

        onErrorScopeToInterpret = nullptr;
        onSuccessScopeToInterpret = nullptr;
        declarationDependingOnErrorScope = nullptr;
    }

    if (wereErrors) {
        return false;
    }

    if (!hasReturnStatement) {
        for (auto declaration : declarationsOrder) {
            if (declarationsInitState.at(declaration)
                && maybeUninitializedDeclarations.find(declaration) != maybeUninitializedDeclarations.end()) {
                if (declaration->variable->type->kind == Type::Kind::Class
                    || declaration->variable->type->kind == Type::Kind::OwnerPointer) {
                    warningMessage("end of scope destruction of maybe uninitialized variable " + declaration->variable->name, position);
                }
            }
        }
    }

    if (isGlobalScope) {
        auto expectedMainType = FunctionType::Create();
        expectedMainType->returnType = IntegerType::Create(IntegerType::Size::I64);
        auto mainDeclarations = declarationMap.getDeclarations("main");
        for (auto declaration : mainDeclarations) {
            if (declaration->variable->isConst
                && cmpPtr(declaration->variable->type, (Type*)expectedMainType)) {
                mainFunction = (FunctionValue*)declaration->value;
                return true;
            }
        }
        return errorMessageBool("no correct main function found in global scope");
    }

    return true;
}
bool CodeScope::interpret() {
    maybeUninitializedDeclarations = parentMaybeUninitializedDeclarations;
    return interpretNoUnitializedDeclarationsSet();
}
Declaration* CodeScope::findAndInterpretDeclaration(const string& name) {
    for (int i = 0; i < statements.size(); ++i) {
        if (statements[i]->kind == Statement::Kind::Declaration) {
            Declaration* declaration = (Declaration*)statements[i];
            if (declaration->variable->name == name) {
                if (declaration->interpret(this, true)) {
                    return declaration;
                } else {
                    return nullptr;
                }
            }
        }
    }
    return nullptr;
}
void CodeScope::createLlvm(LlvmObject* llvmObj) {
    for (int i = 0; i < statements.size(); ++i) {
        switch (statements[i]->kind) {
        case Statement::Kind::ClassDeclaration:{
            ClassDeclaration* declaration = (ClassDeclaration*)statements[i];
            declaration->createLlvm(llvmObj);
            break;
        }
        }
    }
    for (int i = 0; i < statements.size(); ++i) {
        auto& statement = statements[i];
        if (!statement->isReachable) {
            continue;
        }
        switch (statement->kind) {
        case Statement::Kind::Declaration:{
            Declaration* declaration = (Declaration*)statement;
            if (!declaration->variable->isConstexpr || declaration->value->type->kind == Type::Kind::Function) {
                declaration->createLlvm(llvmObj);
            }
            break;
        }
        case Statement::Kind::ClassDeclaration:{
            ClassDeclaration* declaration = (ClassDeclaration*)statement;
            declaration->body->createLlvm(llvmObj);
            break;
        }
        case Statement::Kind::Scope: {
            Scope* scope = (Scope*)statement;
            scope->createLlvm(llvmObj);
            break;
        }
        case Statement::Kind::Value: {
            Value* value = (Value*)statement;
            if (isGlobalScope) {

            }
            value->createLlvm(llvmObj);
            break;
        }
        }
    }
}

/*
    ClassScope
*/
ClassScope::ClassScope(const CodePosition& position, Scope* parentScope) : 
    Scope(position, Scope::Owner::Class, parentScope) 
{}
vector<unique_ptr<ClassScope>> ClassScope::objects;
ClassScope* ClassScope::Create(const CodePosition& position, Scope* parentScope) {
    objects.emplace_back(make_unique<ClassScope>(position, parentScope));
    return objects.back().get();
}
bool ClassScope::operator==(const Statement& scope) const {
    if(typeid(scope) == typeid(*this)){
        const auto& other = static_cast<const ClassScope&>(scope);
        return this->declarations == other.declarations
            && Scope::operator==(other);
    } else {
        return false;
    }
}
bool ClassScope::createCodeTree(const vector<Token>& tokens, int& i) {
    if (tokens[i].value != "{") {
        return errorMessageBool("expected '{' (class scope opening)", tokens[i].codePosition);
    }
    i += 1;

    while (i < tokens.size()) {
        auto statementValue = readStatement(tokens, i);
        if (statementValue) {
            if (statementValue.isScopeEnd) {
                break;
            } else {
                if (statementValue.statement->kind == Statement::Kind::Declaration) {
                    auto declaration = (Declaration*)statementValue.statement;
                    declarations.push_back(declaration);
                } else {
                    return errorMessageBool("non-declaration statement found in class scope", tokens[i-1].codePosition);
                }
            }
        } else {
            return false;
        }
    }

    return true;
}
bool ClassScope::interpret() {
    bool wereErrors = false;
    for (auto& declaration : declarations) {
        if (!declaration->value || declaration->value->valueKind != Value::ValueKind::FunctionValue) {
            if (!declaration->interpret(this)) {
                wereErrors = true;
            }
        }
        if (!declaration->value) {
            maybeUninitializedDeclarations.insert(declaration);
        }
    }
    for (auto& declaration : declarations) {
        if (declaration->value && declaration->value->valueKind == Value::ValueKind::FunctionValue) {
            if (declaration->variable->isConst) {
                FunctionValue* lambda = (FunctionValue*)declaration->value;
                FunctionType* lambdaType = (FunctionType*)declaration->value->type;
                lambdaType->argumentTypes.push_back(RawPointerType::Create(ClassType::Create(classDeclaration->name)));
                lambda->arguments.push_back(Declaration::Create(lambda->position));
                lambda->arguments.back()->variable->name = "this";
                lambda->arguments.back()->variable->type = lambdaType->argumentTypes.back();
            }
            if (!declaration->interpret(this)) {
                wereErrors = true;
            }
        }
    }
    return !wereErrors;
}
Declaration* ClassScope::findAndInterpretDeclaration(const string& name) {
    return nullptr;
}
unordered_set<Declaration*> ClassScope::getUninitializedDeclarations() {
    return parentMaybeUninitializedDeclarations;
}
bool ClassScope::getHasReturnStatement() {
    return false;
}
void ClassScope::createLlvm(LlvmObject* llvmObj) {
    for (auto& declaration : declarations) {
        if (declaration->value && declaration->value->valueKind == Value::ValueKind::FunctionValue) {
            declaration->createLlvm(llvmObj);
        }
    }
}

/*
    FunctionScope
*/
FunctionScope::FunctionScope(const CodePosition& position, Scope* parentScope, FunctionValue* function) : 
    CodeScope(position, Owner::Function, parentScope),
    function(function)
{}
vector<unique_ptr<FunctionScope>> FunctionScope::objects;
FunctionScope* FunctionScope::Create(const CodePosition& position, Scope* parentScope, FunctionValue* function) {
    objects.emplace_back(make_unique<FunctionScope>(position, parentScope, function));
    return objects.back().get();
}
unordered_set<Declaration*> FunctionScope::getUninitializedDeclarations() {
    return parentMaybeUninitializedDeclarations;
}
bool FunctionScope::operator==(const Statement& scope) const {
    if(typeid(scope) == typeid(*this)){
        const auto& other = static_cast<const FunctionScope&>(scope);
        return cmpPtr(this->function, other.function)
            && CodeScope::operator==(other);
    } else {
        return false;
    }
}
bool FunctionScope::interpret() {
    if (wasInterpreted) {
        return true;
    }
    wasInterpreted = true;
    bool interpretStatus = CodeScope::interpret();

    if (!hasReturnStatement && ((FunctionType*)function->type)->returnType->kind != Type::Kind::Void) {
        warningMessage("Not all control paths return value", position);
    }

    return interpretStatus;
}
void FunctionScope::createLlvm(LlvmObject* llvmObj) {
    auto oldBlock = llvmObj->block;
    llvmObj->block = llvm::BasicBlock::Create(llvmObj->context, "Begin", llvmObj->function);
    auto* arg = llvmObj->function->args().begin();
    for (int i = 0; i < function->arguments.size(); ++i) {
        function->arguments[i]->llvmVariable = arg;
        function->arguments[i]->llvmVariable->setName(function->arguments[i]->variable->name);
        function->arguments[i]->isFunctionArgument = true;
        arg += 1;
    }
    CodeScope::createLlvm(llvmObj);
    if (!hasReturnStatement) {
        auto returnType = ((FunctionType*)function->type)->returnType;
        if (returnType->kind == Type::Kind::Void) {
            llvm::ReturnInst::Create(llvmObj->context, (llvm::Value*)nullptr, llvmObj->block);
        } else {
            llvm::ReturnInst::Create(llvmObj->context, llvm::UndefValue::get(returnType->createLlvm(llvmObj)), llvmObj->block);
        }
    }
    llvmObj->block = oldBlock;
}


/*
    ForScope
*/
ForScope::ForScope(const CodePosition& position, Scope* parentScope) : 
    CodeScope(position, Scope::Owner::For, parentScope)
{}
vector<unique_ptr<ForScope>> ForScope::objects;
ForScope* ForScope::Create(const CodePosition& position, Scope* parentScope) {
    objects.emplace_back(make_unique<ForScope>(position, parentScope));
    return objects.back().get();
}
bool ForScope::operator==(const Statement& scope) const {
    if(typeid(scope) == typeid(*this)){
        const auto& other = static_cast<const ForScope&>(scope);
        return this->data == other.data
            && CodeScope::operator==(other);
    } else {
        return false;
    }
}
struct ForScopeDeclarationType {
    bool isConst;
    bool byValue;
};
optional<ForScopeDeclarationType> readForScopeDeclarationType(const vector<Token>& tokens, int& i) {
    if ((tokens[i].value != ":" && tokens[i].value != "&") || (tokens[i].value == "&" && (tokens[i+1].value != ":" && tokens[i+1].value != "="))) {
        return errorMessageOpt("expected declaration of for-each array element (:: or := or &: or &= or :)", tokens[i].codePosition);
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
bool ForScope::createCodeTree(const vector<Token>& tokens, int& i) {
    /// Possible legal uses of a for loop:
    // 1. for var1 _declarationType_ _range_ {} (var1 is int; _declarationType_ is one of {:: :=})
    // 2. for _array_ {}
    // 3. for var1 _declarationType_ _array_ {} (var1 is element of array; _declarationType_ is one of {:: := &: &= :})
    // 4. for var1, var2 _declarationType_ _array_ {} (var2 is index of element var1)
    auto firstValue = getValue(tokens, i, {"{", ":", "&", ","});
    if (!firstValue) { return false; }
    if (tokens[i].value == "{") {
        // 2. for _array_ {}
        ForEachData forEachData;
        forEachData.arrayValue = firstValue;
        forEachData.it = Variable::Create(tokens[i].codePosition);
        forEachData.it->name = "it";
        forEachData.it->isConst = true;
        forEachData.index = Variable::Create(tokens[i].codePosition);
        forEachData.index->name = "index";
        forEachData.index->isConst = true;
        forEachData.index->type = IntegerType::Create(IntegerType::Size::I64);
        data = forEachData;
        i += 1;
    } else if(tokens[i].value == ",") {
        // 4. for var1, var2 _declarationType_ _array_ {}
        auto var1 = (Variable*)firstValue;
        if (!var1) {
            return errorMessageBool("expected a new element iterator variable name", tokens[i-1].codePosition);
        }
        if (i + 5 >= tokens.size()) {
            return errorMessageBool("unexpected end of a file (tried to interpret a for loop)", tokens[tokens.size()-1].codePosition);
        }
        i += 1; // now is on the start of var2
        auto var2Value = getValue(tokens, i, {":", "&"});
        auto var2 = (Variable*)var2Value;
        if (!var2) {
            return errorMessageBool("expected a new index variable name", tokens[i-1].codePosition);
        }
        // now i shows start of _declarationType_
        auto declarationTypeOpt = readForScopeDeclarationType(tokens, i);
        if (!declarationTypeOpt) {
            return false;
        }
        auto declarationType = declarationTypeOpt.value();
        auto arrayValue = getValue(tokens, i, {"{"}, true);
        if (!arrayValue) { return false; }

        ForEachData forEachData;
        forEachData.arrayValue = arrayValue;
        forEachData.it = var1;
        forEachData.it->isConst = declarationType.isConst;
        forEachData.index = var2;
        forEachData.index->isConst = true;
        forEachData.index->type = IntegerType::Create(IntegerType::Size::I64);
        data = forEachData;
    } else {
        // 1. for var1 _declarationType_ _range_ {} (var1 is int; _declarationType_ is one of {:: :=})
        // 3. for var1 _declarationType_ _array_ {} (var1 is element of array; _declarationType_ is one of {:: := &: &= :})
        auto var1 = (Variable*)firstValue;
        if (!var1) {
            return errorMessageBool("expected a new for loop iterator variable name", tokens[i-1].codePosition);
        }

        auto declarationTypeOpt = readForScopeDeclarationType(tokens, i);
        if (!declarationTypeOpt) { return false; }
        auto declarationType = declarationTypeOpt.value();

        auto secondValue = getValue(tokens, i, {":", "{"});
        if (!secondValue) { return false; }
        if (tokens[i].value == "{") {
            // 3. for var1 _declarationType_ _array_ {} (var1 is element of array; _declarationType_ is one of {:: := &: &= :})
            ForEachData forEachData;
            forEachData.arrayValue = secondValue;
            forEachData.it = var1;
            forEachData.it->isConst = declarationType.isConst;
            forEachData.index = Variable::Create(tokens[i].codePosition);
            forEachData.index->name = "index";
            forEachData.index->isConst = true;
            forEachData.index->type = IntegerType::Create(IntegerType::Size::I64);
            data = forEachData;
            i += 1;
        }
        else if (tokens[i].value == ":") {
            // 1. for var1 _declarationType_ _range_ {} (var1 is int; _declarationType_ is one of {:: :=})
            i += 1;
            auto thirdValue = getValue(tokens, i, {":", "{"});
            if (!thirdValue) { return false; }
            ForIterData forIterData;
            forIterData.iterVariable = var1;
            forIterData.iterVariable->isConst = declarationType.isConst;
            forIterData.firstValue = secondValue;
            if (tokens[i].value == "{") {
                forIterData.step = IntegerValue::Create(tokens[i].codePosition, 1);
                forIterData.lastValue = thirdValue;
            } else {
                i += 1;
                forIterData.step = thirdValue;
                forIterData.lastValue = getValue(tokens, i, {"{"});
                if (!forIterData.lastValue) { return false; }
            }
            data = forIterData;
            i += 1; // skip '{'
        }
    }

    return CodeScope::createCodeTree(tokens, i);
}
bool ForScope::interpret() {
    maybeUninitializedDeclarations = parentMaybeUninitializedDeclarations;
    if (holds_alternative<ForIterData>(data)) {
        auto& forIterData = get<ForIterData>(data);

        auto firstValue = forIterData.firstValue->interpret(parentScope);
        if (!firstValue) return false;
        if (firstValue.value()) forIterData.firstValue = firstValue.value();
        auto stepValue = forIterData.step->interpret(parentScope);
        if (!stepValue) return false;
        if (stepValue.value()) forIterData.step = stepValue.value();
        auto lastValue = forIterData.lastValue->interpret(parentScope);
        if (!lastValue) return false;
        if (lastValue.value()) forIterData.lastValue = lastValue.value();

        if ((forIterData.firstValue->type->kind != Type::Kind::Integer && forIterData.firstValue->type->kind != Type::Kind::Float)
          || (forIterData.step->type->kind != Type::Kind::Integer && forIterData.step->type->kind != Type::Kind::Float)
          || (forIterData.lastValue->type->kind != Type::Kind::Integer && forIterData.lastValue->type->kind != Type::Kind::Float))
        {
            string message = "for-iter values need to be int or float types. got: ";
            message += DeclarationMap::toString(forIterData.firstValue->type);
            message += "; ";
            message += DeclarationMap::toString(forIterData.step->type);
            message += "; ";
            message += DeclarationMap::toString(forIterData.lastValue->type);
            return errorMessageBool(message, position);
        }

        forIterData.iterVariable->type = Type::getSuitingArithmeticType(
            forIterData.firstValue->type, forIterData.step->type
        );
        forIterData.iterVariable->type = Type::getSuitingArithmeticType(
            forIterData.iterVariable->type, forIterData.lastValue->type
        );

        auto iterDeclaration = Declaration::Create(position);
        iterDeclaration->variable = forIterData.iterVariable;
        iterDeclaration->value = forIterData.firstValue;
        iterDeclaration->status = Declaration::Status::Completed;
        declarationMap.addVariableDeclaration(iterDeclaration);
    } else if (holds_alternative<ForEachData>(data)) {
        auto& forEachData = get<ForEachData>(data);

        auto arrayValue = forEachData.arrayValue->interpret(parentScope);
        if (!arrayValue) return false;
        if (arrayValue.value()) forEachData.arrayValue = arrayValue.value();

        auto itDeclaration = Declaration::Create(position);
        if (forEachData.arrayValue->type->kind == Type::Kind::StaticArray) {
            forEachData.it->type = ((StaticArrayType*)forEachData.arrayValue->type)->elementType;
        } else if (forEachData.arrayValue->type->kind == Type::Kind::DynamicArray) {
            forEachData.it->type = ((DynamicArrayType*)forEachData.arrayValue->type)->elementType;
        } else if (forEachData.arrayValue->type->kind == Type::Kind::ArrayView) {
            forEachData.it->type = ((ArrayViewType*)forEachData.arrayValue->type)->elementType;
        }
        itDeclaration->variable = forEachData.it;
        itDeclaration->status = Declaration::Status::Completed;
        declarationMap.addVariableDeclaration(itDeclaration);

        auto indexDeclaration = Declaration::Create(position);
        indexDeclaration->variable = forEachData.index;
        indexDeclaration->status = Declaration::Status::Completed;
        declarationMap.addVariableDeclaration(indexDeclaration);
    }
    return interpretNoUnitializedDeclarationsSet();
}
unordered_set<Declaration*> ForScope::getUninitializedDeclarations() {
    return parentMaybeUninitializedDeclarations;
}
bool ForScope::getHasReturnStatement() {
    return false;
}
bool ForIterData::operator==(const ForIterData& other) const {
    return cmpPtr(this->iterVariable, other.iterVariable)
        && cmpPtr(this->firstValue, other.firstValue)
        && cmpPtr(this->step, other.step)
        && cmpPtr(this->lastValue, other.lastValue);
}
bool ForEachData::operator==(const ForEachData& other) const {
    return cmpPtr(this->arrayValue, other.arrayValue)
        && cmpPtr(this->it, other.it)
        && cmpPtr(this->index, other.index);
}
void ForScope::createLlvm(LlvmObject* llvmObj) {

}


/*
    WhileScope
*/
WhileScope::WhileScope(const CodePosition& position, Scope* parentScope) : 
    CodeScope(position, Scope::Owner::While, parentScope) 
{}
vector<unique_ptr<WhileScope>> WhileScope::objects;
WhileScope* WhileScope::Create(const CodePosition& position, Scope* parentScope) {
    objects.emplace_back(make_unique<WhileScope>(position, parentScope));
    return objects.back().get();
}
bool WhileScope::operator==(const Statement& scope) const {
    if(typeid(scope) == typeid(*this)){
        const auto& other = static_cast<const WhileScope&>(scope);
        return cmpPtr(this->conditionExpression, other.conditionExpression)
            && CodeScope::operator==(other);
    } else {
        return false;
    }
}
bool WhileScope::createCodeTree(const vector<Token>& tokens, int& i) {
    this->conditionExpression = getValue(tokens, i, {"{"}, true);
    if (!this->conditionExpression) {
        return false;
    }
    return CodeScope::createCodeTree(tokens, i);
}
bool WhileScope::interpret() {
    auto boolCondition = CastOperation::Create(position, Type::Create(Type::Kind::Bool));
    boolCondition->arguments.push_back(conditionExpression);
    auto valueInterpret = boolCondition->interpret(parentScope);
    if (!valueInterpret) return false;
    if (valueInterpret.value()) conditionExpression = valueInterpret.value();
    maybeUninitializedDeclarations = parentScope->maybeUninitializedDeclarations;
    parentMaybeUninitializedDeclarations = maybeUninitializedDeclarations;
    return interpretNoUnitializedDeclarationsSet();
}
unordered_set<Declaration*> WhileScope::getUninitializedDeclarations() {
    return parentMaybeUninitializedDeclarations;
}
bool WhileScope::findBreakStatement(CodeScope* scope) {
    for (auto statement : scope->statements) {
        auto codeScope = dynamic_cast<CodeScope*>(statement);
        if (codeScope && findBreakStatement(codeScope)) {
            return true;
        }

        auto flowOperation = dynamic_cast<FlowOperation*>(statement);
        if (flowOperation && flowOperation->kind == Operation::Kind::Break) {
            return true;
        }
    }
    return false;
}
bool WhileScope::getHasReturnStatement() {
    return conditionExpression->isConstexpr 
        && ((BoolValue*)conditionExpression)->value
        && (hasReturnStatement || !findBreakStatement(this));
}
void WhileScope::createLlvm(LlvmObject* llvmObj) {
    auto whileConditionBlock = llvm::BasicBlock::Create(llvmObj->context, "whileCondition", llvmObj->function);
    auto whileBlock          = llvm::BasicBlock::Create(llvmObj->context, "while",          llvmObj->function);
    llvm::BranchInst::Create(whileConditionBlock, llvmObj->block);
    llvmObj->block = whileBlock;
    CodeScope::createLlvm(llvmObj);
    if (!hasReturnStatement) llvm::BranchInst::Create(whileConditionBlock, llvmObj->block);
    auto afterWhileBlock     = llvm::BasicBlock::Create(llvmObj->context, "afterWhile",     llvmObj->function);
    llvmObj->block = whileConditionBlock;
    llvm::BranchInst::Create(whileBlock, afterWhileBlock, conditionExpression->createLlvm(llvmObj), whileConditionBlock);
    llvmObj->block = afterWhileBlock;
}


/*
    IfScope
*/
IfScope::IfScope(const CodePosition& position, Scope* parentScope) :
    CodeScope(position, Scope::Owner::If, parentScope) 
{}
vector<unique_ptr<IfScope>> IfScope::objects;
IfScope* IfScope::Create(const CodePosition& position, Scope* parentScope) {
    objects.emplace_back(make_unique<IfScope>(position, parentScope));
    return objects.back().get();
}
bool IfScope::operator==(const Statement& scope) const {
    if(typeid(scope) == typeid(*this)){
        const auto& other = static_cast<const IfScope&>(scope);
        return cmpPtr(this->conditionExpression, other.conditionExpression)
            && CodeScope::operator==(other);
    } else {
        return false;
    }
}
bool IfScope::createCodeTree(const vector<Token>& tokens, int& i) {
    this->conditionExpression = getValue(tokens, i, {"{", "then"});
    if (!this->conditionExpression) {
        return false;
    }
    if (tokens[i].value == "{") {
        i += 1;
        if (!CodeScope::createCodeTree(tokens, i)) {
            return false;
        }
    } else {
        i += 1;
        auto statementValue = readStatement(tokens, i);
        if (statementValue.isScopeEnd) {
            return errorMessageBool("unexpected '}' (trying to close unopened if scope)", tokens[i-1].codePosition);
        } else if (!statementValue.statement) {
            return false;
        } else if (statementValue.statement->kind == Statement::Kind::ClassDeclaration) {
            return errorMessageBool("expected expression, got class declaration", tokens[i-1].codePosition);
        }
        statements.push_back(statementValue.statement);
    }
    if (i < tokens.size() && tokens[i].value == "else") {
        this->elseScope = ElseScope::Create(tokens[i].codePosition, this->parentScope);
        i += 1;
        if (!this->elseScope->createCodeTree(tokens, i)) {
            return false;
        }
    }
    return true;
}
bool IfScope::interpret() {
    auto boolCondition = CastOperation::Create(position, Type::Create(Type::Kind::Bool));
    boolCondition->arguments.push_back(conditionExpression);
    auto valueInterpret = boolCondition->interpret(parentScope);
    if (!valueInterpret) return false;
    if (valueInterpret.value()) conditionExpression = valueInterpret.value();
    bool elseScopeErrors = false;
    maybeUninitializedDeclarations = parentScope->maybeUninitializedDeclarations;
    parentMaybeUninitializedDeclarations = maybeUninitializedDeclarations;
    if (!interpretNoUnitializedDeclarationsSet()) {
        return false;
    }
    if (elseScope) {
        elseScope->parentMaybeUninitializedDeclarations = maybeUninitializedDeclarations;
        elseScope->hasReturnStatement = conditionExpression->isConstexpr && ((BoolValue*)conditionExpression)->value;
        elseScope->declarationsInitState = declarationsInitState;
        if (!elseScope->interpret()) {
            elseScopeErrors = true;
        }
    }
    return !elseScopeErrors;
}
unordered_set<Declaration*> IfScope::getUninitializedDeclarations() {
    if (conditionExpression->isConstexpr
        && ((BoolValue*)conditionExpression)->value) {
        return maybeUninitializedDeclarations;
    }
    if (elseScope) {
        auto uninitializedDeclarations = maybeUninitializedDeclarations;
        for (auto declaration : elseScope->maybeUninitializedDeclarations) {
            uninitializedDeclarations.insert(declaration);
        }
        return uninitializedDeclarations;
    }
    return parentMaybeUninitializedDeclarations;
}
bool IfScope::getHasReturnStatement() {
    if (elseScope) {
        return hasReturnStatement && elseScope->getHasReturnStatement();
    } else {
        return hasReturnStatement;
    }
}
unordered_map<Declaration*, bool> IfScope::getDeclarationsInitState() {
    if (elseScope) {
        auto newDeclarationsInitState = declarationsInitState;
        for (auto declaration : parentScope->declarationsOrder) {
            if ((elseScope->declarationsInitState.find(declaration) != elseScope->declarationsInitState.end()
                && elseScope->declarationsInitState.at(declaration))){
                newDeclarationsInitState.at(declaration) = true;
            }
        }
        return newDeclarationsInitState;
    } else {
        return declarationsInitState;
    }
}
void IfScope::createLlvm(LlvmObject* llvmObj) {
    if (elseScope) {
        auto ifBlock = llvm::BasicBlock::Create(llvmObj->context, "if", llvmObj->function);
        auto oldBlock = llvmObj->block;
        llvmObj->block = ifBlock;
        CodeScope::createLlvm(llvmObj);
        auto afterIfBlock = llvmObj->block;
        auto elseBlock = llvm::BasicBlock::Create(llvmObj->context, "else", llvmObj->function);
        llvmObj->block = oldBlock;
        llvm::BranchInst::Create(ifBlock, elseBlock, conditionExpression->createLlvm(llvmObj), llvmObj->block);
        llvmObj->block = elseBlock;
        elseScope->createLlvm(llvmObj);
        auto afterIfElseBlock = llvm::BasicBlock::Create(llvmObj->context, "afterIfElse", llvmObj->function);
        if (!hasReturnStatement) llvm::BranchInst::Create(afterIfElseBlock, afterIfBlock);
        if (!elseScope->hasReturnStatement) llvm::BranchInst::Create(afterIfElseBlock, llvmObj->block);
        llvmObj->block = afterIfElseBlock;
    } else {
        auto ifBlock      = llvm::BasicBlock::Create(llvmObj->context, "if",      llvmObj->function);
        auto oldBlock = llvmObj->block;
        llvmObj->block = ifBlock;
        CodeScope::createLlvm(llvmObj);
        auto afterIfBlock = llvm::BasicBlock::Create(llvmObj->context, "afterIf", llvmObj->function);
        if (!hasReturnStatement) llvm::BranchInst::Create(afterIfBlock, llvmObj->block);
        llvmObj->block = oldBlock;
        llvm::BranchInst::Create(ifBlock, afterIfBlock, conditionExpression->createLlvm(llvmObj), llvmObj->block);
        llvmObj->block = afterIfBlock;
    }
}
/*unordered_map<Declaration*, bool> IfScope::getDeclarationsInitState() {
    return declarationsInitStateCopy;
}
vector<Declaration*> IfScope::getDeclarationsOrder() {
    return declarationsOrderCopy;
}*/

/*
    ElseScope
*/
ElseScope::ElseScope(const CodePosition& position, Scope* parentScope) : 
    CodeScope(position, Scope::Owner::Else, parentScope) 
{}
vector<unique_ptr<ElseScope>> ElseScope::objects;
ElseScope* ElseScope::Create(const CodePosition& position, Scope* parentScope) {
    objects.emplace_back(make_unique<ElseScope>(position, parentScope));
    return objects.back().get();
}
bool ElseScope::createCodeTree(const vector<Token>& tokens, int& i) {
    if (tokens[i].value == "{") {
        i += 1;
        return CodeScope::createCodeTree(tokens, i);
    } else {
        auto statementValue = readStatement(tokens, i);
        if (statementValue.isScopeEnd) {
            return errorMessageBool("unexpected '}' (trying to close unopened else scope)", tokens[i-1].codePosition);
        } else if (!statementValue.statement) {
            return false;
        } else if (statementValue.statement->kind == Statement::Kind::ClassDeclaration) {
            return errorMessageBool("expected expression, got class declaration", tokens[i-1].codePosition);
        }
        statements.push_back(statementValue.statement);
        return true;
    }
}


/*
    DeferScope
*/
DeferScope::DeferScope(const CodePosition& position, Scope* parentScope) : 
    CodeScope(position, Scope::Owner::Defer, parentScope) 
{}
vector<unique_ptr<DeferScope>> DeferScope::objects;
DeferScope* DeferScope::Create(const CodePosition& position, Scope* parentScope) {
    objects.emplace_back(make_unique<DeferScope>(position, parentScope));
    return objects.back().get();
}
bool DeferScope::createCodeTree(const vector<Token>& tokens, int& i) {
    if (tokens[i].value == "{") {
        i += 1;
        return CodeScope::createCodeTree(tokens, i);
    } else {
        auto statementValue = readStatement(tokens, i);
        if (statementValue.isScopeEnd) {
            return errorMessageBool("unexpected '}' (trying to close unopened defer scope)", tokens[i-1].codePosition);
        } else if (!statementValue.statement) {
            return false;
        } else if (statementValue.statement->kind == Statement::Kind::ClassDeclaration) {
            return errorMessageBool("expected expression, got class declaration", tokens[i-1].codePosition);
        }
        statements.push_back(statementValue.statement);
        return true;
    }
}
bool DeferScope::interpret() {
    declarationsInitStateCopy = declarationsInitState;
    return CodeScope::interpret();
}
unordered_set<Declaration*> DeferScope::getUninitializedDeclarations() {
    return parentMaybeUninitializedDeclarations;
}
bool DeferScope::getHasReturnStatement() {
    return false;
}
unordered_map<Declaration*, bool> DeferScope::getDeclarationsInitState() {
    return declarationsInitStateCopy;
}

