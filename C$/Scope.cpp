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
void Scope::templateCopy(Scope* scope, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    scope->owner = owner;
    scope->parentScope = parentScope;
    Statement::templateCopy(scope, parentScope, templateToType);
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
    case Token::Type::RawStringLiteral:
        return errorMessageBool("statement cannot start with a raw string literal", token.codePosition);
    case Token::Type::Integer:
        return errorMessageBool("statement cannot start with an integer value", token.codePosition);
    case Token::Type::Float:
        return errorMessageBool("statement cannot start with a float value", token.codePosition);
    case Token::Type::Symbol:
        if (token.value == "#") {
            if (i + 1 >= tokens.size()) {
                return errorMessageBool("uncompleted special statement (#)", token.codePosition);
            }
            i += 1;
            if (tokens[i].value == "declare") {
                if (i + 2 >= tokens.size()) {
                    return errorMessageBool("uncompleted #declare statement", token.codePosition);
                }
                if (tokens[i + 1].type != Token::Type::Label) {
                    return errorMessageBool("expected #declare statement function name, got " 
                        + tokens[i+1].value, tokens[i+1].codePosition
                    );
                }
                auto position = tokens[i+1].codePosition;
                auto functionName = tokens[i+1].value;
                i += 2;
                auto functionType = getType(tokens, i, {";"});
                if (!functionType) return false;
                if (!GlobalScope::Instance.setCDeclaration(position, functionName, functionType)) {
                    return false;
                }
                return Scope::ReadStatementValue(Value::Create(token.codePosition, Value::ValueKind::Empty));
            } else {
                return errorMessageBool("unknown special statement (#)", token.codePosition);
            }
        }
        else if (token.value == "{") {
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
        else if (tokens[i].value == "operator") {
            string operatorSymbols = "";
            i += 1;
            if (i + 3 < tokens.size() && tokens[i].value + tokens[i + 1].value + tokens[i + 2].value == "[:]") {
                operatorSymbols += "[:]";
                i += 3;
            }
            while (i < tokens.size() && tokens[i].value != ":") {
                operatorSymbols += tokens[i].value;
                i += 1;
            }
            i += 1;
            if (i >= tokens.size()) {
                return errorMessageBool("unexpected end of operator function", token.codePosition);
            }
            auto nameToKind = Operation::stringToKind(operatorSymbols);
            if (!nameToKind) {
                return errorMessageBool("unknown operator '" + operatorSymbols + "'", token.codePosition);
            }
            auto operKind = nameToKind.value();
            auto declaration = Declaration::Create(token.codePosition);
            declaration->byReference = false;
            declaration->variable->isConst = true;
            declaration->variable->type = nullptr;
            declaration->value = getValue(tokens, i, {";", "}"}, true);
            if (!declaration->value) return false;
            if (declaration->value->valueKind != Value::ValueKind::FunctionValue) {
                return errorMessageBool("operator name used for non-function value", token.codePosition);
            }
            auto functionType = (FunctionType*)declaration->value->type;
            if (operKind == Operation::Kind::Sub && functionType->argumentTypes.size() == 1) {
                operKind = Operation::Kind::Minus;
            }
            declaration->variable->name = Operation::kindToFunctionName(operKind);
            if (operKind == Operation::Kind::ArrayIndex) {
                if (functionType->argumentTypes.size() != 2) {
                    return errorMessageBool("operator '[]' must take 2 arguments", token.codePosition);
                }
            } else if (operKind == Operation::Kind::ArraySubArray) {
                if (functionType->argumentTypes.size() != 3) {
                    return errorMessageBool("operator '[:]' must take 3 arguments", token.codePosition);
                }
            } else if (Operation::numberOfArguments(operKind) != functionType->argumentTypes.size()) {
                return errorMessageBool("operator '" + operatorSymbols + "' must take " + to_string(Operation::numberOfArguments(operKind)) + " arguments", token.codePosition);
            }

            return Scope::ReadStatementValue(declaration);
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
        } else {
            bool isClass = false;
            if (tokens[i + 1].value == "<" || tokens[i + 1].value == "class") {
                // shorthand noration for class declaration or template class declaration 
                isClass = true;
                vector<TemplateType*> templateTypes;
                if (tokens[i + 1].value == "<") {
                    int k = i + 2;
                    while (tokens[k].type == Token::Type::Label && tokens[k+1].value == ",") {
                        templateTypes.push_back(TemplateType::Create(tokens[k].value));
                        k += 2;
                    }
                    if (tokens[k].type != Token::Type::Label) {
                        isClass = false;
                    }
                    if (tokens[k + 1].value != ">") {
                        isClass = false;
                    }
                    templateTypes.push_back(TemplateType::Create(tokens[k].value));
                    k += 2;
                    if (tokens[k].value + tokens[k + 1].value == "::") {
                        k += 2;
                    }
                    if (tokens[k].value != "class") {
                        isClass = false;
                    }
                    k += 1;
                    if (isClass) {
                        i = k;
                    }
                } else {
                    i += 2;
                }
                if (isClass) {
                    auto classDeclaration = ClassDeclaration::Create(token.codePosition, token.value);
                    classDeclaration->body = ClassScope::Create(token.codePosition, this);
                    if (!classDeclaration->body->createCodeTree(tokens, i)) {
                        return false;
                    }
                    classDeclaration->templateTypes = templateTypes;
                    return Scope::ReadStatementValue(classDeclaration);
                }
            }
            if (!isClass) {
                // value or shorthand notation for constexpr function declaration
                int j = i + 1;
                if (tokens[j].value == "<") {
                    j += 1;
                    int openTemplate = 1;
                    while (tokens.size() > j && openTemplate > 0) {
                        if (tokens[j].value == "<") openTemplate += 1;
                        else if (tokens[j].value == ">") openTemplate -= 1;
                        j += 1;
                    }
                }
                if (tokens[j].value == "(") {
                    j += 1;
                    int openparentheses = 1;
                    while (tokens.size() > j && openparentheses > 0) {
                        if (tokens[j].value == "(") openparentheses += 1;
                        else if (tokens[j].value == ")") openparentheses -= 1;
                        j += 1;
                    }
                    if (tokens.size() > j+2) {
                        if (tokens[j].value == "{" || tokens[j].value + tokens[j + 1].value == "->") {
                            // shorthand notation for constexpr function declaration
                            i += 1;
                            auto declaration = Declaration::Create(token.codePosition);
                            declaration->byReference = false;
                            declaration->variable->isConst = true;
                            declaration->variable->name = token.value;
                            declaration->variable->type = nullptr;
                            declaration->value = getValue(tokens, i, {";", "}"}, true);
                            if (!declaration->value) {
                                return false;
                            }
                            return Scope::ReadStatementValue(declaration);
                        }
                    }
                }

                auto value = getValue(tokens, i, {";", "}"}, true);
                if (!value) {
                    return false;
                }
                return Scope::ReadStatementValue(value);
            }
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
bool Scope::addArguments(Operation* operation, const vector<Token>& tokens, int& i) {
    if (tokens[i].value == "(") {
        i += 1;
        if (tokens[i].value != ")") {
            do {
                auto value = getValue(tokens, i, {",", ")"});
                if (!value) return false;     
                operation->arguments.push_back(value);
                i += 1;
            } while (tokens[i-1].value != ")");
        } else {
            i += 1;
        }
    }
    return true;
}
bool Scope::addConstructorOperation(vector<Value*>& out, const vector<Token>& tokens, int& i, bool isHeapAllocation) {
    int oldI = i;
    auto type = getType(tokens, i, {});
    if (!type) return false;
    auto constructorOp = ConstructorOperation::Create(tokens[oldI].codePosition, type, {}, isHeapAllocation, true);
    if (!addArguments(constructorOp, tokens, i)) return false;
    out.push_back(constructorOp);
    return true;
}
optional<Value*> getInteger(const string& str, const CodePosition& position) {
    uint64_t value;
    bool isSigned = false;
    if (str[0] == '-') {
        isSigned = true;
        try {
            value = stoll(str);
        }
        catch (...) {
            return errorMessageOpt("integer literal is too big (> max i64 value)", position);
        }
    } else {
        try {
            value = stoull(str);
        }
        catch (...) {
            return errorMessageOpt("integer literal is too big (> max u64 value)", position);
        }
    }
    auto intValue = IntegerValue::Create(position, value);
    if (value > (uint64_t)numeric_limits<int64_t>::max()) {
        intValue->type = IntegerType::Create(IntegerType::Size::U64);
    }
    return intValue;
}
optional<vector<Value*>> Scope::getReversePolishNotation(const vector<Token>& tokens, int& i, bool canBeFunction) {
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
        case Token::Type::Integer: {
            if (!expectValue) {
                endOfExpression = true;
                break;
            }
            auto getIntegerResult = getInteger(tokens[i].value, tokens[i].codePosition);
            if (!getIntegerResult) return nullopt;
            out.push_back(getIntegerResult.value());
            i += 1;
            expectValue = false;
            break;
        }
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
            out.push_back(CharValue::Create(tokens[i].codePosition, (int)tokens[i].value[1]));
            i += 1;
            expectValue = false;
            break;
        case Token::Type::StringLiteral:
            if (!expectValue) {
                endOfExpression = true;
                break;
            }
            out.push_back(StringValue::Create(tokens[i].codePosition, tokens[i].value.substr(1)));
            i += 1;
            expectValue = false;
            break;
        case Token::Type::RawStringLiteral: {
            if (!expectValue) {
                endOfExpression = true;
                break;
            }
            auto staticArray = StaticArrayValue::Create(tokens[i].codePosition);
            for (int j = 1; j < tokens[i].value.size(); ++j) {
                staticArray->values.push_back(CharValue::Create(tokens[i].codePosition, tokens[i].value[j]));
            }
            out.push_back(staticArray);
            i += 1;
            expectValue = false;
            break;
        }
        case Token::Type::Label:{
            if (!expectValue) {
                if (tokens[i].value == "onerror" || tokens[i].value == "onsuccess") {
                    auto operation = ErrorResolveOperation::Create(tokens[i].codePosition);
                    while (tokens[i].value == "onerror" || tokens[i].value == "onsuccess") {
                        CodeScope* codeScope = nullptr;
                        if (tokens[i].value == "onerror") {
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
            auto typeKeyword = Keyword::get(tokens[i].value);
            if (typeKeyword && typeKeyword->kind == Keyword::Kind::TypeName) {
                addConstructorOperation(out, tokens, i);
                expectValue = false;
            }
            else if (tokens[i].value == "true") {
                out.push_back(BoolValue::Create(tokens[i].codePosition, true));
                i += 1;
                expectValue = false;
            }
            else if (tokens[i].value == "false") {
                out.push_back(BoolValue::Create(tokens[i].codePosition, false));
                i += 1;
                expectValue = false;
            }
            else if (tokens[i].value == "null") {
                out.push_back(NullValue::Create(tokens[i].codePosition, RawPointerType::Create(Type::Create(Type::Kind::Void))));
                i += 1;
                expectValue = false;
            }
            else if (tokens[i].value == "alloc") {
                i += 1;
                addConstructorOperation(out, tokens, i, true);
                expectValue = false;
            }
            else if (tokens[i].value == "dealloc") {
                appendOperator(stack, out, Operation::Kind::Deallocation, tokens[i++].codePosition);
                expectValue = true;
            }
            else if (tokens[i].value == "destroy") {
                appendOperator(stack, out, Operation::Kind::Destroy, tokens[i++].codePosition);
                expectValue = true;
            }
            else if (tokens[i].value == "sizeof") {
                auto operation = SizeofOperation::Create(tokens[i].codePosition);
                i += 1;
                if (tokens[i].value != "(") {
                    return errorMessageOpt("expected open bracket '(' symbol of sizeof operation, got " 
                        + tokens[i].value, tokens[i].codePosition
                    );
                }
                i += 1;
                operation->argType = getType(tokens, i, {")"});
                i += 1;
                if (!operation->argType) return nullopt;
                out.push_back(operation);
                expectValue = false;
            } else if (tokens[i].value == "typesize") {
                auto operation = Operation::Create(tokens[i].codePosition, Operation::Kind::Typesize);
                i += 1;
                if (tokens[i].value != "(") {
                    return errorMessageOpt("expected open bracket '(' symbol of typesize operation, got " 
                        + tokens[i].value, tokens[i].codePosition
                    );
                }
                i += 1;
                auto value = getValue(tokens, i, {")"}, true);
                if (!value) return nullopt;
                operation->arguments.push_back(value);

                out.push_back(operation);
                expectValue = false;
            } else if (tokens[i].value == "error") {
                auto type = MaybeErrorType::Create(Type::Create(Type::Kind::Void));
                if (!type) return nullopt;
                auto constructorOp = ConstructorOperation::Create(tokens[i].codePosition, type, {}, false, true);
                i += 1;
                if (!addArguments(constructorOp, tokens, i)) return nullopt;
                out.push_back(constructorOp);
                expectValue = false;
            } else {
                bool isTemplatedVariable = false;
                if (tokens[i + 1].value == "<") {
                    // either 'less then' operator or templated variable
                    // assume its templated variable and check if it makes sense
                    isTemplatedVariable = true;
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
                                isTemplatedVariable = false;
                                break;
                            }
                        }
                        if (isTemplatedVariable) {
                            auto templateVariable = TemplatedVariable::Create(tokens[i].codePosition, tokens[i].value);
                            templateVariable->templatedTypes = templateTypes;
                            out.push_back(templateVariable);
                            i = k;
                            expectValue = false;
                        }
                    } else {
                        isTemplatedVariable = false;
                    }
                } 
                if (!isTemplatedVariable) {
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
                    bool isNormalOpenBracket = false;
                    if (!canBeFunction && openBracketsCount == 0) {
                        isNormalOpenBracket = true;
                    } else {
                        int j = i + 1;
                        int openparentheses = 1;
                        while (tokens.size() > j && openparentheses > 0) {
                            if (tokens[j].value == "(") openparentheses += 1;
                            else if (tokens[j].value == ")") openparentheses -= 1;
                            j += 1;
                        }
                        if (tokens.size() > j+2 && (tokens[j].value == "{" || tokens[j].value + tokens[j + 1].value == "->")) {
                            // function value (lambda)
                            auto lambda = FunctionValue::Create(tokens[i].codePosition, nullptr, this);
                            auto lambdaType = FunctionType::Create();
                            i += 1;
                            if (tokens[i].value != ")") {
                                while (true) {
                                    if (tokens[i].type != Token::Type::Label) {
                                        return errorMessageOpt("expected function variable name, got " + tokens[i].value, tokens[i].codePosition);
                                    }
                                    int declarationStart = i;
                                    if (tokens[i+1].value == ":") {
                                        i += 1;
                                    }
                                    //lambda->argumentNames.push_back(tokens[i].value);
                                    i += 1;
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
                            isNormalOpenBracket = true;
                        }
                    }
                     if (isNormalOpenBracket) {
                        // normal opening bracket
                        stack.push_back(Operation::Create(tokens[i++].codePosition, Operation::Kind::LeftBracket));
                        openBracketsCount += 1;
                        expectValue = true;
                    }
                }
                else if (tokens[i].value == "<") {
                    // template function value (<T1,T2,..>(x:T,..)->U{}) or type cast (<T>)
                    // assume it's template function value and check if it makes sense
                    bool isTemplateFunctionValue = true;

                    int openTypeBrackets = 1;
                    int j = i+1;
                    while (j < tokens.size() && openTypeBrackets != 0) {
                        if (tokens[j].value == "<") {
                            openTypeBrackets += 1;
                        } else if (tokens[j].value == ">") {
                            openTypeBrackets -= 1;
                        }
                        j += 1;
                    }
                    if (tokens[j].value == "(") {
                        int openBrackets = 1;
                        j += 1;
                        while (j < tokens.size() && openBrackets != 0) {
                            if (tokens[j].value == "(") {
                                openBrackets += 1;
                            } else if (tokens[j].value == ")") {
                                openBrackets -= 1;
                            }
                            j += 1;
                        }
                        if (tokens[j].value != "{" && tokens[j].value + tokens[j + 1].value != "->") {
                            isTemplateFunctionValue = false;
                        }
                    } else {
                        isTemplateFunctionValue = false;
                    }

                    if (isTemplateFunctionValue) {
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
                                int declarationStart = i;
                                if (tokens[i+1].value == ":") {
                                    i += 1;
                                }
                                //templateFunction->argumentNames.push_back(tokens[i].value);
                                i += 1;
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
                    } else {
                        // type cast
                        int startOfCastIndex = i;
                        i += 1;
                        auto type = getType(tokens, i, {">"});
                        if (!type) {
                            return nullopt;
                        }
                        i += 1;
                        auto castOperation = CastOperation::Create(tokens[startOfCastIndex].codePosition, type);
                        appendOperator(stack, out, castOperation);
                        expectValue = true;
                    }
                }
                else if (tokens[i].value == "[") {
                    // static array ([x, y, z, ...]) or dynamic array constructor ([]T) 
                    // or array view constructor ([*]T) or static array constructor ([N]T)
                    if (tokens[i + 1].value == "]") {
                        // dynamic array constructor ([]T or []T(...))
                        addConstructorOperation(out, tokens, i);
                        expectValue = false;
                    } else if (tokens[i + 1].value == "*" && tokens[i + 2].value == "]") {
                        // array viewy constructor ([*]T or [*]T(...))
                        addConstructorOperation(out, tokens, i);
                        expectValue = false;
                    } else {
                        // static array ([x, y, z, ...]) or static array constructor ([N]T)
                        int openBrackets = 1;
                        int j = i + 1;
                        while (j < tokens.size() && openBrackets != 0) {
                            if (tokens[j].value == "[") openBrackets += 1;
                            if (tokens[j].value == "]") openBrackets -= 1;
                            j += 1;
                        }
                        if (tokens[j].type == Token::Type::Label || tokens[j].value == "*" || tokens[j].value == "("
                            || tokens[j].value == "[" || tokens[j].value == "?" || tokens[j].value == "^")
                        {
                            // static array constructor ([N]T or [N]T(...))
                            addConstructorOperation(out, tokens, i);
                            expectValue = false;
                        } else {
                            // static array ([x, y, z, ...])
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
                    }
                } else if (tokens[i].value == "^") {
                    // owner pointer constructor
                    addConstructorOperation(out, tokens, i);
                    expectValue = false;
                } else if (tokens[i].value == "*") {
                    // raw pointer constructor
                    addConstructorOperation(out, tokens, i);
                    expectValue = false;
                } else if (tokens[i].value == "?") {
                    // maybe error constructor
                    addConstructorOperation(out, tokens, i);
                    expectValue = false;
                } else if (tokens[i].value == "-") {
                    if (i + 1 < tokens.size() && tokens[i + 1].type == Token::Type::Integer) {
                        auto getIntegerResult = getInteger(tokens[i].value + tokens[i+1].value, tokens[i+1].codePosition);
                        if (!getIntegerResult) return nullopt;
                        out.push_back(getIntegerResult.value());
                        i += 2;
                        expectValue = false;
                    } else if (i + 1 < tokens.size() && tokens[i + 1].type == Token::Type::Float) {
                        out.push_back(FloatValue::Create(tokens[i+1].codePosition, stod(tokens[i].value+tokens[i+1].value)));
                        i += 2;
                        expectValue = false;
                    } else {
                        appendOperator(stack, out, Operation::Kind::Minus, tokens[i++].codePosition);
                        expectValue = true;
                    }
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
                } else if (i+2 < tokens.size() && tokens[i].value + tokens[i + 1].value == "<<") {
                    appendOperator(stack, out, Operation::Kind::Shl, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+2 < tokens.size() && tokens[i].value + tokens[i + 1].value == ">>") {
                    appendOperator(stack, out, Operation::Kind::Shr, tokens[i].codePosition);
                    i += 2;
                    expectValue = true;
                } else if (i+3 < tokens.size() && tokens[i].value + tokens[i + 1].value + tokens[i + 2].value == "<<<") {
                    appendOperator(stack, out, Operation::Kind::Sal, tokens[i].codePosition);
                    i += 3;
                    expectValue = true;
                } else if (i+3 < tokens.size() && tokens[i].value + tokens[i + 1].value + tokens[i + 2].value == ">>>") {
                    appendOperator(stack, out, Operation::Kind::Sar, tokens[i].codePosition);
                    i += 3;
                    expectValue = true;
                } else if (i+3 < tokens.size() && tokens[i].value + tokens[i + 1].value + tokens[i + 2].value == "<<=") {
                    appendOperator(stack, out, Operation::Kind::ShlAssign, tokens[i].codePosition);
                    i += 3;
                    expectValue = true;
                } else if (i+3 < tokens.size() && tokens[i].value + tokens[i + 1].value + tokens[i + 2].value == ">>=") {
                    appendOperator(stack, out, Operation::Kind::ShrAssign, tokens[i].codePosition);
                    i += 3;
                    expectValue = true;
                } else if (i+4 < tokens.size() && tokens[i].value + tokens[i + 1].value + tokens[i + 2].value + tokens[i + 3].value == "<<<=") {
                    appendOperator(stack, out, Operation::Kind::SalAssign, tokens[i].codePosition);
                    i += 4;
                    expectValue = true;
                } else if (i+4 < tokens.size() && tokens[i].value + tokens[i + 1].value + tokens[i + 2].value + tokens[i + 3].value == ">>>=") {
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
                    appendOperator(stack, out, DotOperation::Create(tokens[i++].codePosition));
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
                    appendOperator(stack, out, AssignOperation::Create(tokens[i++].codePosition));
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
Value* Scope::getValue(const vector<Token>& tokens, int& i, const vector<string>& delimiters, bool skipOnGoodDelimiter, bool canBeFunction) {
    auto reversePolishNotation = getReversePolishNotation(tokens, i, canBeFunction);
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
    if (tokens[i].value == "^") {
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
            case TypeKeyword::Value::Byte:
                type = IntegerType::Create(IntegerType::Size::U8); break;
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
                type = DynamicArrayType::Create(IntegerType::Create(IntegerType::Size::U8)); break;
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
    if (delimiters.empty()) {
        return type;
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
    else {
        declarations[0]->scope = this;
        switch (declarations[0]->status) {
        case Declaration::Status::None:
            return declarations[0];
        case Declaration::Status::InEvaluation:
            if (declarations[0]->variable->isConstexpr && declarations[0]->value->type->kind == Type::Kind::Function) {
                return declarations[0];
            } else {
                return errorMessageNull("recursive variable dependency", variable->position);
            }
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
void CodeScope::templateCopy(CodeScope* scope, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    scope->isGlobalScope = isGlobalScope;
    for (auto statement : statements) {
        scope->statements.push_back(statement->templateCopy(scope, templateToType));
    }
    Scope::templateCopy(scope, parentScope, templateToType);
}
Statement* CodeScope::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto value = Create(position, owner, parentScope);
    templateCopy(value, parentScope, templateToType);
    return value;
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
                if (statementValue.statement->kind != Statement::Kind::Value
                    || ((Value*)statementValue.statement)->valueKind != Value::ValueKind::Empty) 
                {
                    statements.push_back(statementValue.statement);
                }
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
        auto statement = statements[i];
        if (statement->kind == Statement::Kind::Declaration) {
            auto declaration = (Declaration*)statement;
            if (declaration->value && declaration->variable->isConst && declaration->value->valueKind == Value::ValueKind::FunctionValue) {
                if (declaration->variable->type && !declaration->variable->type->interpret(this)) {
                    errorMessageBool("unknown type: " + DeclarationMap::toString(declaration->variable->type), position);
                    wereErrors = true;
                }
                if (!declaration->value->type->interpret(this)) {
                    errorMessageBool("unknown type: " + DeclarationMap::toString(declaration->value->type), position);
                    wereErrors = true;
                }
                if (!declarationMap.addFunctionDeclaration(declaration)) {
                    wereErrors = true;
                }
                declaration->variable->type = declaration->value->type;
                declaration->variable->isConstexpr = true;
                if (declaration->value->type->kind == Type::Kind::TemplateFunction) {
                    declaration->isTemplateFunctionDeclaration = true;
                }
            }
        }
    }
    for (int i = 0; i < statements.size(); ++i) {
        auto& statement = statements[i];
        switch (statement->kind) {
        case Statement::Kind::Declaration: {
            Declaration* declaration = (Declaration*)statement;
            if (declaration->isTemplateFunctionDeclaration) break;
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
                if (((Operation*)declaration->value)->containedErrorResolve) {
                    errorResolveAfterStatement[declaration] = ((Operation*)declaration->value)->containedErrorResolve;
                    maybeUninitializedDeclarations.insert(declaration);
                    declarationDependingOnErrorScope = declaration;
                }
            }
            break;
        }
        case Statement::Kind::ClassDeclaration: {
            ClassDeclaration* declaration = (ClassDeclaration*)statement;
            if (!declaration->interpret({})) {
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
            if (((Value*)statement)->valueKind == Value::ValueKind::Operation) {
                if (((Operation*)statement)->containedErrorResolve) {
                    errorResolveAfterStatement[statement] = ((Operation*)statement)->containedErrorResolve;
                }
            }
            break;
        }
        }
        if (!valuesToDestroyBuffer.empty()) {
            valuesToDestroyAfterStatement[statement] = valuesToDestroyBuffer;
            valuesToDestroyBuffer.clear();
        }

        if (onErrorScopeToInterpret) {
            onErrorScopeToInterpret->hasReturnStatement = hasReturnStatement;
            onErrorScopeToInterpret->parentMaybeUninitializedDeclarations = maybeUninitializedDeclarations;
            onErrorScopeToInterpret->declarationsInitState = declarationsInitState;
            if (declarationDependingOnErrorScope) onErrorScopeToInterpret->declarationsInitState.at(declarationDependingOnErrorScope) = false;
            if (!onErrorScopeToInterpret->interpret()) {
                wereErrors = true;
            }
            if (!onSuccessScopeToInterpret) {
                declarationsInitState = onErrorScopeToInterpret->getDeclarationsInitState();
                if (declarationDependingOnErrorScope) declarationsInitState.at(declarationDependingOnErrorScope) = true;
            }

            if (declarationDependingOnErrorScope) {
                if (onErrorScopeToInterpret->hasReturnStatement) {
                    maybeUninitializedDeclarations.erase(declarationDependingOnErrorScope);
                } else {
                    if (onErrorScopeToInterpret->maybeUninitializedDeclarations.find(declarationDependingOnErrorScope) == onErrorScopeToInterpret->maybeUninitializedDeclarations.end()) {
                        maybeUninitializedDeclarations.erase(declarationDependingOnErrorScope);
                    }
                }
            }
        }

        if (onSuccessScopeToInterpret) {
            onSuccessScopeToInterpret->hasReturnStatement = hasReturnStatement;
            onSuccessScopeToInterpret->parentMaybeUninitializedDeclarations = maybeUninitializedDeclarations;
            if (declarationDependingOnErrorScope) onSuccessScopeToInterpret->parentMaybeUninitializedDeclarations.erase(declarationDependingOnErrorScope);
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
                    && onSuccessScopeToInterpret->declarationsInitState.at(declaration))) {
                    newDeclarationsInitState.at(declaration) = true;
                }
            }
            declarationsInitState = newDeclarationsInitState;
        }

        onErrorScopeToInterpret = nullptr;
        onSuccessScopeToInterpret = nullptr;
        declarationDependingOnErrorScope = nullptr;
    }

    for (auto& statement : statements) {
        if (statement->kind == Kind::ClassDeclaration) {
            if (!((ClassDeclaration*)statement)->interpretAllImplementations()) {
                wereErrors = true;
            }
        }
    }

    if (wereErrors) {
        return false;
    }

    if (!hasReturnStatement) {
        for (int i = declarationsOrder.size() - 1; i >= 0; --i) {
            auto& declaration = declarationsOrder[i];
            auto& variable = declaration->variable;
            if (variable->type->needsDestruction() && declarationsInitState.at(declaration)) {
                if (maybeUninitializedDeclarations.find(declaration) != maybeUninitializedDeclarations.end()) {
                    warningMessage("end of scope destruction of maybe uninitialized variable " + variable->name, position);
                }
                auto destroyOp = Operation::Create(position, Operation::Kind::Destroy);
                destroyOp->arguments.push_back(variable);
                if (!destroyOp->interpret(this)) return false;
                statements.push_back(destroyOp);
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
void CodeScope::allocaAllDeclarationsLlvm(LlvmObject* llvmObj) {
    for (auto statement : statements) {
        if (statement->kind == Statement::Kind::Declaration) {
            ((Declaration*)statement)->createAllocaLlvmIfNeeded(llvmObj);
        } else if (statement->kind == Statement::Kind::Value) {
            ((Value*)statement)->createAllocaLlvmIfNeededForValue(llvmObj);
        }
        else if (statement->kind == Statement::Kind::Scope) {
            ((Scope*)statement)->allocaAllDeclarationsLlvm(llvmObj);
        }
    }
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
            declaration->createLlvmBody(llvmObj);
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
        auto errorResolve = errorResolveAfterStatement.find(statement);
        if (errorResolve != errorResolveAfterStatement.end()) {
            // onsuccess block
            errorResolve->second->createLlvmSuccessDestructor(llvmObj);
            auto iter = valuesToDestroyAfterStatement.find(statement);
            if (iter != valuesToDestroyAfterStatement.end()) {
                for (auto value : iter->second) {
                    if (!value->wasCaptured) {
                        value->createDestructorLlvm(llvmObj);
                    }
                }
            }

            if (errorResolve->second->onSuccessScope) {
                errorResolve->second->onSuccessScope->createLlvm(llvmObj);
            }
            auto lastOnSuccessBlock = llvmObj->block;

            // onerror block
            llvmObj->block = errorResolve->second->llvmErrorBlock;

            if (iter != valuesToDestroyAfterStatement.end()) {
                for (auto value : iter->second) {
                    if (!value->wasCaptured) {
                        value->createDestructorLlvm(llvmObj);
                    }
                }
            }

            auto afterErrorResolveBlock = llvm::BasicBlock::Create(llvmObj->context, "afterErrorResolve", llvmObj->function);

            if (errorResolve->second->onErrorScope) {
                errorResolve->second->onErrorScope->createLlvm(llvmObj);
                if (!errorResolve->second->onErrorScope->getHasReturnStatement()) {
                    llvm::BranchInst::Create(afterErrorResolveBlock, llvmObj->block);
                }
            } else {
                llvm::BranchInst::Create(afterErrorResolveBlock, llvmObj->block);
            }

            if (errorResolve->second->onSuccessScope) {
                if (!errorResolve->second->onSuccessScope->getHasReturnStatement()) {
                    llvm::BranchInst::Create(afterErrorResolveBlock, lastOnSuccessBlock);
                }
            } else {
                llvm::BranchInst::Create(afterErrorResolveBlock, lastOnSuccessBlock);
            }

            llvmObj->block = afterErrorResolveBlock;

        } else {
            auto iter = valuesToDestroyAfterStatement.find(statement);
            if (iter != valuesToDestroyAfterStatement.end()) {
                for (auto value : iter->second) {
                    if (!value->wasCaptured) {
                        value->createDestructorLlvm(llvmObj);
                    }
                }
            }
        }
    }
}


/*
    GlobalScope
*/
GlobalScope::GlobalScope() : CodeScope(CodePosition(nullptr,0,0),Scope::Owner::None,nullptr,true) {}
GlobalScope GlobalScope::Instance = GlobalScope();
bool GlobalScope::setCDeclaration(const CodePosition& codePosition, const string& name, Type* type) {
    if (type->kind != Type::Kind::Function) {
        return errorMessageBool("declaration has to be of function type, got " + DeclarationMap::toString(type), codePosition);
    }
    Declaration* declaration = Declaration::Create(codePosition);
    declaration->status = Declaration::Status::Completed;
    declaration->scope = this;
    declaration->value = FunctionValue::Create(codePosition, type, this);
    declaration->variable = Variable::Create(codePosition, name);
    declaration->variable->type = type;
    declaration->variable->isConstexpr = true;
    declaration->variable->isConst = true;
    auto addToMapStatus = declarationMap.addFunctionDeclaration(declaration);
    if (!addToMapStatus) {
        return errorMessageBool("error including C function " + name, codePosition);
    }
    cDeclarations.push_back(declaration);
    return true;
}
bool GlobalScope::interpret() {
    for (auto& declaration : cDeclarations) {
        if (!declaration->variable->type->interpret(this)) {
            return false;
        }
    }
    return CodeScope::interpret();
}
void GlobalScope::createLlvm(LlvmObject* llvmObj) {
    for (auto declaration : cDeclarations) {
        ((FunctionValue*)declaration->value)->llvmFunction = llvm::cast<llvm::Function>(llvmObj->module->getOrInsertFunction(
            declaration->variable->name, 
            (llvm::FunctionType*)((llvm::PointerType*)declaration->variable->type->createLlvm(llvmObj))->getElementType()
        ));
    }
    for (auto& statement : statements) {
        if (statement->kind == Statement::Kind::Declaration) {
            auto declaration = (Declaration*)statement;
            if (!declaration->variable->isConstexpr) {
                declaration->llvmVariable = new llvm::GlobalVariable(
                    *llvmObj->module, 
                    declaration->variable->type->createLlvm(llvmObj), 
                    false, 
                    llvm::GlobalValue::LinkageTypes::InternalLinkage, 
                    llvm::UndefValue::get(declaration->variable->type->createLlvm(llvmObj)), 
                    declaration->variable->name
                );
            }
        }
    }
    CodeScope::createLlvm(llvmObj);
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
void ClassScope::templateCopy(ClassScope* scope, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    for (auto declaration : declarations) {
        scope->declarations.push_back((Declaration*)declaration->templateCopy(scope, templateToType));
    }
    Scope::templateCopy(scope, parentScope, templateToType);
}
Statement* ClassScope::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto value = Create(position, parentScope);
    templateCopy(value, parentScope, templateToType);
    return value;
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
    if (wasInterpreted) {
        return true;
    }
    wasInterpreted = true;
    auto classType = ClassType::Create(classDeclaration->name);
    classType->declaration = classDeclaration;

    bool wereErrors = false;
    for (auto& declaration : declarations) {
        if (!declaration->variable->isConst || (declaration->value && declaration->value->valueKind != Value::ValueKind::FunctionValue)) {
            if (!declaration->interpret(this)) {
                wereErrors = true;
            }
        }
        if (!declaration->value) {
            maybeUninitializedDeclarations.insert(declaration);
        }
        declarationsInitState.insert({declaration, declaration->value});
    }
    
    /*
        inline constructors
    */
    auto lambdaType = FunctionType::Create();
    auto classPointerType = RawPointerType::Create(classType);
    classPointerType->interpret(this);
    lambdaType->argumentTypes.push_back(classPointerType);
    lambdaType->returnType = Type::Create(Type::Kind::Void);
    inlineConstructors = FunctionValue::Create(position, lambdaType, this);
    inlineConstructors->arguments.push_back(Declaration::Create(position));
    inlineConstructors->arguments.back()->variable->name = "this";
    inlineConstructors->arguments.back()->variable->type = classPointerType;
    for (auto& declaration : declarations) {
        if (declaration->value && !declaration->variable->isConst) {
            auto assignOperation = AssignOperation::Create(declaration->position);
            assignOperation->arguments.push_back(declaration->variable);
            assignOperation->arguments.push_back(declaration->value);
            inlineConstructors->body->statements.push_back(assignOperation);
        }
    }
    if (inlineConstructors->body->statements.empty()) {
        inlineConstructors = nullptr;
    } else {
        if (!inlineConstructors->interpret(this)) {
            internalError("failed interpreting of inline class member constructors", position);
        }
        for (auto& statement : inlineConstructors->body->statements) {
            ((AssignOperation*)statement)->isConstruction = true;
        }
    }


    /*
        inline destructors
    */
    inlineDestructors = FunctionValue::Create(position, lambdaType, this);
    inlineDestructors->arguments.push_back(Declaration::Create(position));
    inlineDestructors->arguments.back()->variable->name = "this";
    inlineDestructors->arguments.back()->variable->type = classPointerType;
    for (int i = declarations.size() - 1; i >= 0; --i) {
        auto& declaration = declarations[i];
        if (!declaration->variable->isConst && declaration->variable->type->needsDestruction()) {
            auto destroyOperation = Operation::Create(declaration->position, Operation::Kind::Destroy);
            destroyOperation->arguments.push_back(declaration->variable);
            inlineDestructors->body->statements.push_back(destroyOperation);
        }
    }
    if (inlineDestructors->body->statements.empty()) {
        inlineDestructors = nullptr;
    } else {
        if (!inlineDestructors->interpret(this)) {
            internalError("failed interpreting of inline class member destructors", position);
        }
    }


    /*
        copy constructor
    */
    auto copyConstructorType = FunctionType::Create();
    auto classReferenceType = ReferenceType::Create(classPointerType->underlyingType);
    copyConstructorType->argumentTypes.push_back(classReferenceType);
    copyConstructorType->argumentTypes.push_back(classPointerType);
    copyConstructorType->returnType = Type::Create(Type::Kind::Void);
    copyConstructor = FunctionValue::Create(position, copyConstructorType, this);
    copyConstructor->arguments.push_back(Declaration::Create(position));
    copyConstructor->arguments.back()->variable->name = "other";
    copyConstructor->arguments.back()->variable->type = classReferenceType;
    copyConstructor->arguments.push_back(Declaration::Create(position));
    copyConstructor->arguments.back()->variable->name = "this";
    copyConstructor->arguments.back()->variable->type = classPointerType;

    for (auto& declaration : declarations) {
        if (!declaration->variable->isConst) {
            auto otherVar = DotOperation::Create(declaration->position);
            otherVar->arguments.push_back(Variable::Create(declaration->position, "other"));
            otherVar->arguments.push_back(Variable::Create(declaration->position, declaration->variable->name));

            auto assignOperation = AssignOperation::Create(declaration->position);
            assignOperation->arguments.push_back(declaration->variable);
            assignOperation->arguments.push_back(otherVar);
            copyConstructor->body->statements.push_back(assignOperation);
        }
    }
    if (!copyConstructor->interpret(this)) {
        internalError("failed interpreting of default copy constructor", position);
    }
    for (auto& statement : copyConstructor->body->statements) {
        ((AssignOperation*)statement)->isConstruction = true;
    }


    /*
        copy operator=
    */
    operatorEq = FunctionValue::Create(position, copyConstructorType, this);
    operatorEq->arguments.push_back(Declaration::Create(position));
    operatorEq->arguments.back()->variable->name = "other";
    operatorEq->arguments.back()->variable->type = classReferenceType;
    operatorEq->arguments.push_back(Declaration::Create(position));
    operatorEq->arguments.back()->variable->name = "this";
    operatorEq->arguments.back()->variable->type = classPointerType;

    for (auto& declaration : declarations) {
        if (!declaration->variable->isConst) {
            auto otherVar = DotOperation::Create(declaration->position);
            otherVar->arguments.push_back(Variable::Create(declaration->position, "other"));
            otherVar->arguments.push_back(Variable::Create(declaration->position, declaration->variable->name));

            auto assignOperation = AssignOperation::Create(declaration->position);
            assignOperation->arguments.push_back(declaration->variable);
            assignOperation->arguments.push_back(otherVar);
            operatorEq->body->statements.push_back(assignOperation);
        }
    }
    if (!operatorEq->interpret(this)) {
        internalError("failed interpreting of default operator=", position);
    }


    for (auto& declaration : declarations) {
        if (declaration->value && declaration->variable->isConst && declaration->value->valueKind == Value::ValueKind::FunctionValue) {
            auto lambda = (FunctionValue*)declaration->value;
            auto lambdaType = (FunctionType*)declaration->value->type;
            lambdaType->argumentTypes.push_back(RawPointerType::Create(classType));
            lambda->arguments.push_back(Declaration::Create(lambda->position));
            lambda->arguments.back()->variable->name = "this";
            lambda->arguments.back()->variable->type = lambdaType->argumentTypes.back();
            if (declaration->variable->name == "constructor") {
                if (lambdaType->returnType->kind != Type::Kind::Void) {
                    return errorMessageBool("cannot specify class constructor return type", declaration->variable->position);
                }
                constructors.push_back(lambda);
            } 
            if (declaration->variable->name == "destructor") {
                if (lambdaType->returnType->kind != Type::Kind::Void) {
                    return errorMessageBool("cannot specify class destructor return type", declaration->variable->position);
                }
                if (lambdaType->argumentTypes.size() > 1) {
                    return errorMessageBool("class destructor cannot have any arguments\n", declaration->variable->position);
                }
                destructor = lambda;
            }
            if (declaration->variable->type && !declaration->variable->type->interpret(this)) {
                errorMessageBool("unknown type: " + DeclarationMap::toString(declaration->variable->type), position);
                wereErrors = true;
            }
            if (!declaration->value->type->interpret(this)) {
                errorMessageBool("unknown type: " + DeclarationMap::toString(declaration->value->type), position);
                wereErrors = true;
            }
            if (!declarationMap.addFunctionDeclaration(declaration)) {
                wereErrors = true;
            }
            declaration->variable->type = declaration->value->type;
            declaration->variable->isConstexpr = true;
            if (declaration->value->type->kind == Type::Kind::TemplateFunction) {
                declaration->isTemplateFunctionDeclaration = true;
            }
        }
    }

    for (auto& declaration : declarations) {
        if (declaration->variable->isConst && declaration->value && declaration->value->valueKind == Value::ValueKind::FunctionValue) {
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
void ClassScope::allocaAllDeclarationsLlvm(LlvmObject* llvmObj) {}
void ClassScope::createLlvm(LlvmObject* llvmObj) {
    if (inlineConstructors) {
        inlineConstructors->createLlvm(llvmObj, classDeclaration->name + "InlineConstructors");
    }
    copyConstructor->createLlvm(llvmObj, classDeclaration->name + "DefaultCopyConstructor");
    operatorEq->createLlvm(llvmObj, classDeclaration->name + "DefaultCopyAssignment");
    if (inlineDestructors) {
        inlineDestructors->createLlvm(llvmObj, classDeclaration->name + "InlineDestructors");
    }
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
void FunctionScope::templateCopy(FunctionScope* scope, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    CodeScope::templateCopy(scope, parentScope, templateToType);
}
Statement* FunctionScope::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto value = Create(position, parentScope, nullptr);
    templateCopy(value, parentScope, templateToType);
    return value;
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
    for (auto funArg : function->arguments) {
        funArg->llvmVariable = funArg->variable->type->allocaLlvm(llvmObj);
        funArg->llvmVariable->setName(funArg->variable->name);
    }
    CodeScope::allocaAllDeclarationsLlvm(llvmObj);
    auto* arg = llvmObj->function->args().begin();
    for (auto funArg : function->arguments) {
        new llvm::StoreInst(arg, funArg->llvmVariable, llvmObj->block);
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
void ForScope::templateCopy(ForScope* scope, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    if (holds_alternative<ForIterData>(data)) {
        auto& forIterData = get<ForIterData>(data);
        ForIterData thisData;
        thisData.iterVariable = (Variable*)forIterData.iterVariable->templateCopy(scope, templateToType);
        thisData.firstValue = (Value*)forIterData.firstValue->templateCopy(scope, templateToType);
        thisData.step = (Value*)forIterData.step->templateCopy(scope, templateToType);
        thisData.lastValue = (Value*)forIterData.lastValue->templateCopy(scope, templateToType);
        thisData.iterDeclaration = nullptr;
        thisData.stepOperation = nullptr;
        thisData.conditionOperation = nullptr;
        scope->data = thisData;
    } else {
        auto& forEachData = get<ForEachData>(data);
        ForEachData thisData;
        thisData.arrayValue = (Value*)forEachData.arrayValue->templateCopy(scope, templateToType);
        thisData.it = (Variable*)forEachData.it->templateCopy(scope, templateToType);
        thisData.index = (Variable*)forEachData.index->templateCopy(scope, templateToType);
        thisData.itDeclaration = nullptr;
        thisData.indexDeclaration = nullptr;
        scope->data = thisData;
    }
    scope->loopForward = loopForward;
    CodeScope::templateCopy(scope, parentScope, templateToType);
}
Statement* ForScope::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto value = Create(position, parentScope);
    templateCopy(value, parentScope, templateToType);
    return value;
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
    ForScopeDeclarationType declarationType;

    if (tokens[i].value == "in") {
        declarationType.isConst = false;
        declarationType.byValue = true;
        i += 1;
    } else {
        if ((tokens[i].value == "&" && (tokens[i+1].value != ":" && tokens[i+1].value != "="))) {
            return errorMessageOpt("expected declaration of for-each array element (:: or := or &: or &= or :)", tokens[i].codePosition);
        }

        declarationType.isConst = tokens[i+1].value == ":";
        declarationType.byValue = tokens[i].value == ":";

        i += 2;
    }

    return declarationType;
}
bool ForScope::setForLoopDirection(const vector<Token>& tokens, int& i) {
    if (tokens[i-1].value == "forward" || tokens[i-1].value == "backwards") {
        loopForward = tokens[i-1].value == "forward";
        if (tokens[i].value != "{" && tokens[i].value != "do") {
            return errorMessageBool("expected for loop opening ('{' or 'do'), got " + tokens[i].value, tokens[i].codePosition);
        }
        i += 1;
    }
    return true;
}
bool ForScope::createCodeTree(const vector<Token>& tokens, int& i) {
    /// Possible legal uses of a for loop:
    // 1. for _range_ {}
    // 2. for var1 _declarationType_ _range_ {} (var1 is int; _declarationType_ is one of {:: := in})
    // 3. for _array_ {}
    // 4. for var1 _declarationType_ _array_ {} (var1 is element of array; _declarationType_ is one of {:: := &: &= in})
    // 5. for var1, var2 _declarationType_ _array_ {} (var2 is index of element var1)
    auto firstValue = getValue(tokens, i, {"{", ":", "&", ",", "in", "do", "forward", "backwards"}, false, false);
    if (!firstValue) { return false; }
    if (tokens[i].value == "{" || tokens[i].value == "do" || tokens[i].value == "forward" || tokens[i].value == "backwards") {
        // 3. for _array_ {}
        ForEachData forEachData;
        forEachData.arrayValue = firstValue;
        forEachData.it = Variable::Create(tokens[i].codePosition);
        forEachData.it->name = "it";
        forEachData.it->isConst = false;
        forEachData.index = Variable::Create(tokens[i].codePosition);
        forEachData.index->name = "index";
        forEachData.index->isConst = false;
        forEachData.index->type = IntegerType::Create(IntegerType::Size::I64);
        data = forEachData;
        i += 1;
    } else if(tokens[i].value == ",") {
        // 5. for var1, var2 _declarationType_ _array_ {}
        if (firstValue->valueKind != Value::ValueKind::Variable) {
            return errorMessageBool("expected a new element iterator variable name", tokens[i-1].codePosition);
        }
        auto var1 = (Variable*)firstValue;
        if (i + 5 >= tokens.size()) {
            return errorMessageBool("unexpected end of a file (tried to interpret a for loop)", tokens[tokens.size()-1].codePosition);
        }
        i += 1; // now is on the start of var2
        auto var2Value = getValue(tokens, i, {":", "&", "in"}, false, false);
        if (var2Value->valueKind != Value::ValueKind::Variable) {
            return errorMessageBool("expected a new index variable name", tokens[i-1].codePosition);
        }
        auto var2 = (Variable*)var2Value;
        // now i shows start of _declarationType_
        auto declarationTypeOpt = readForScopeDeclarationType(tokens, i);
        if (!declarationTypeOpt) {
            return false;
        }
        auto declarationType = declarationTypeOpt.value();
        auto arrayValue = getValue(tokens, i, {"{", "do", "forward", "backwards"}, true, false);
        if (!arrayValue) { return false; }

        ForEachData forEachData;
        forEachData.arrayValue = arrayValue;
        forEachData.it = var1;
        forEachData.it->isConst = declarationType.isConst;
        forEachData.index = var2;
        forEachData.index->isConst = true;
        forEachData.index->type = IntegerType::Create(IntegerType::Size::I64);
        data = forEachData;
    } else if (tokens[i].value == ":" && tokens[i+1].value != ":" && tokens[i+1].value != "=") {
        // 1. for _range_ {}
        ForIterData forIterData;
        forIterData.iterVariable = Variable::Create(position, "index");
        forIterData.iterVariable->type = IntegerType::Create(IntegerType::Size::I64);

        forIterData.firstValue = firstValue;
        i += 1;
        auto secondValue = getValue(tokens, i, {":", "{", "do", "forward", "backwards"}, false, false);
        if (!secondValue) return false;
        if (tokens[i].value == "{" || tokens[i].value == "do" || tokens[i].value == "forward" || tokens[i].value == "backwards") {
            forIterData.step = IntegerValue::Create(tokens[i].codePosition, 1);
            forIterData.lastValue = secondValue;
        } else {
            i += 1;
            forIterData.step = secondValue;
            forIterData.lastValue = getValue(tokens, i, {"{", "do", "forward", "backwards"}, false, false);
            if (!forIterData.lastValue) return false;
        }
        data = forIterData;
        i += 1; // skip '{'
    } else {
        // 2. for var1 _declarationType_ _range_ {} (var1 is int; _declarationType_ is one of {:: := in})
        // 4. for var1 _declarationType_ _array_ {} (var1 is element of array; _declarationType_ is one of {:: := &: &= in})
        if (firstValue->valueKind != Value::ValueKind::Variable) {
            return errorMessageBool("expected a new for loop iterator variable name", tokens[i-1].codePosition);
        }
        auto var1 = (Variable*)firstValue;

        auto declarationTypeOpt = readForScopeDeclarationType(tokens, i);
        if (!declarationTypeOpt) { return false; }
        auto declarationType = declarationTypeOpt.value();

        auto secondValue = getValue(tokens, i, {":", "{", "do", "forward", "backwards"}, false, false);
        if (!secondValue) { return false; }
        if (tokens[i].value == "{" || tokens[i].value == "do" || tokens[i].value == "forward" || tokens[i].value == "backwards") {
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
            auto thirdValue = getValue(tokens, i, {":", "{", "do", "forward", "backwards"}, false, false);
            if (!thirdValue) { return false; }
            ForIterData forIterData;
            forIterData.iterVariable = var1;
            forIterData.iterVariable->isConst = declarationType.isConst;
            forIterData.firstValue = secondValue;
            if (tokens[i].value == "{" || tokens[i].value == "do" || tokens[i].value == "forward" || tokens[i].value == "backwards") {
                forIterData.step = IntegerValue::Create(tokens[i].codePosition, 1);
                forIterData.lastValue = thirdValue;
            } else {
                i += 1;
                forIterData.step = thirdValue;
                forIterData.lastValue = getValue(tokens, i, {"{", "do", "forward", "backwards"}, false, false);
                if (!forIterData.lastValue) { return false; }
            }
            data = forIterData;
            i += 1; // skip '{'
        }
    }

    if (!setForLoopDirection(tokens, i)) return false;

    if (tokens[i-1].value == "{") {
        return CodeScope::createCodeTree(tokens, i);
    } else {
        auto statementValue = readStatement(tokens, i);
        if (statementValue.isScopeEnd) {
            return errorMessageBool("unexpected '}' (trying to close unopened for scope)", tokens[i-1].codePosition);
        } else if (!statementValue.statement) {
            return false;
        } else if (statementValue.statement->kind == Statement::Kind::ClassDeclaration) {
            return errorMessageBool("expected expression, got class declaration", tokens[i-1].codePosition);
        }
        statements.push_back(statementValue.statement);
        return true;
    }
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

        forIterData.iterDeclaration = Declaration::Create(position);
        forIterData.iterDeclaration->scope = this;
        forIterData.iterDeclaration->variable = forIterData.iterVariable;
        forIterData.iterDeclaration->value = forIterData.firstValue;
        forIterData.iterDeclaration->status = Declaration::Status::Completed;
        declarationMap.addVariableDeclaration(forIterData.iterDeclaration);
        declarationsInitState.insert({forIterData.iterDeclaration, true});
        declarationsOrder.push_back(forIterData.iterDeclaration);

        Operation* stepOperation;
        if (loopForward) {
            stepOperation = Operation::Create(position, Operation::Kind::AddAssign);
        } else {
            stepOperation = Operation::Create(position, Operation::Kind::SubAssign);
        }
        stepOperation->arguments.push_back(forIterData.iterVariable);
        stepOperation->arguments.push_back(forIterData.step);
        auto addInterpret = stepOperation->interpret(this);
        if (!addInterpret) internalError("couldn't create for-scope step operation", position);
        if (addInterpret.value()) forIterData.stepOperation = addInterpret.value();
        else forIterData.stepOperation = stepOperation;

        Operation* conditionOperation;
        if (loopForward) {
            conditionOperation = Operation::Create(position, Operation::Kind::Lte);
        } else {
            conditionOperation = Operation::Create(position, Operation::Kind::Gte);
        }
        conditionOperation->arguments.push_back(forIterData.iterVariable);
        conditionOperation->arguments.push_back(forIterData.lastValue);
        auto conditionInterpret = conditionOperation->interpret(this);
        if (!conditionInterpret) internalError("couldn't create for-scope condition operation", position);
        if (conditionInterpret.value()) forIterData.conditionOperation = conditionInterpret.value();
        else forIterData.conditionOperation = conditionOperation;

    } else if (holds_alternative<ForEachData>(data)) {
        auto& forEachData = get<ForEachData>(data);

        auto arrayValue = forEachData.arrayValue->interpret(parentScope);
        if (!arrayValue) return false;
        if (arrayValue.value()) forEachData.arrayValue = arrayValue.value();

        forEachData.itDeclaration = Declaration::Create(position);
        forEachData.itDeclaration->scope = this;
        auto arrayEffectiveType = forEachData.arrayValue->type->getEffectiveType();
        if (arrayEffectiveType->kind == Type::Kind::StaticArray) {
            forEachData.it->type = ReferenceType::Create(((StaticArrayType*)arrayEffectiveType)->elementType);
        } else if (arrayEffectiveType->kind == Type::Kind::DynamicArray) {
            forEachData.it->type = ReferenceType::Create(((DynamicArrayType*)arrayEffectiveType)->elementType);
        } else if (arrayEffectiveType->kind == Type::Kind::ArrayView) {
            forEachData.it->type = ReferenceType::Create(((ArrayViewType*)arrayEffectiveType)->elementType);
        }
        forEachData.it->type->interpret(this);
        forEachData.itDeclaration->variable = forEachData.it;
        forEachData.it->declaration = forEachData.itDeclaration;
        forEachData.itDeclaration->status = Declaration::Status::Completed;
        declarationMap.addVariableDeclaration(forEachData.itDeclaration);
        declarationsInitState.insert({forEachData.itDeclaration, true});
        declarationsOrder.push_back(forEachData.itDeclaration);

        forEachData.indexDeclaration = Declaration::Create(position);
        forEachData.indexDeclaration->scope = this;
        forEachData.index->type = IntegerType::Create(IntegerType::Size::I64);
        forEachData.index->type->interpret(this);
        forEachData.index->declaration = forEachData.indexDeclaration;
        forEachData.indexDeclaration->variable = forEachData.index;
        forEachData.indexDeclaration->status = Declaration::Status::Completed;
        declarationMap.addVariableDeclaration(forEachData.indexDeclaration);
        declarationsInitState.insert({forEachData.indexDeclaration, true});
        declarationsOrder.push_back(forEachData.indexDeclaration);
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
void ForScope::allocaAllDeclarationsLlvm(LlvmObject* llvmObj) {
    if (holds_alternative<ForIterData>(data)) {
        auto& forIterData = get<ForIterData>(data);
        forIterData.iterDeclaration->createAllocaLlvmIfNeeded(llvmObj);
        forIterData.stepOperation->createAllocaLlvmIfNeededForValue(llvmObj);
        forIterData.conditionOperation->createAllocaLlvmIfNeededForValue(llvmObj);
    } else {
        auto& forEachData = get<ForEachData>(data);
        forEachData.indexDeclaration->createAllocaLlvmIfNeeded(llvmObj);
        forEachData.itDeclaration->createAllocaLlvmIfNeeded(llvmObj);

        auto arrayEffType = forEachData.arrayValue->type->getEffectiveType();
        if (arrayEffType->kind == Type::Kind::StaticArray) {
            forEachData.arrayValue->createAllocaLlvmIfNeededForReference(llvmObj);
        } else if (arrayEffType->kind == Type::Kind::DynamicArray) {
            auto dynamicArrayType = (DynamicArrayType*)arrayEffType;
            if (Value::isLvalue(forEachData.arrayValue)) {
                forEachData.arrayValue->createAllocaLlvmIfNeededForReference(llvmObj);
            } else {
                forEachData.arrayValue->createAllocaLlvmIfNeededForValue(llvmObj);
            }
        } else if (arrayEffType->kind == Type::Kind::ArrayView) {
            auto arrayViewType = (ArrayViewType*)arrayEffType;
            if (Value::isLvalue(forEachData.arrayValue)) {
                forEachData.arrayValue->createAllocaLlvmIfNeededForReference(llvmObj);
            } else {
                forEachData.arrayValue->createAllocaLlvmIfNeededForValue(llvmObj);
            }
        }
    }
    CodeScope::allocaAllDeclarationsLlvm(llvmObj);
}
void ForScope::createLlvm(LlvmObject* llvmObj) {
    if (holds_alternative<ForIterData>(data)) {
        auto& forIterData = get<ForIterData>(data);

        auto forStartBlock     = llvm::BasicBlock::Create(llvmObj->context, "forStart",     llvmObj->function);
        auto forStepBlock      = llvm::BasicBlock::Create(llvmObj->context, "forStep",      llvmObj->function);
        auto forConditionBlock = llvm::BasicBlock::Create(llvmObj->context, "forCondition", llvmObj->function);
        auto forBodyBlock      = llvm::BasicBlock::Create(llvmObj->context, "for",          llvmObj->function);
        auto afterForBlock     = llvm::BasicBlock::Create(llvmObj->context, "afterFor",     llvmObj->function);
        llvmStepBlock = forStepBlock;
        llvmAfterBlock = afterForBlock;

        // start block
        llvm::BranchInst::Create(forStartBlock, llvmObj->block);
        llvmObj->block = forStartBlock;
        forIterData.iterDeclaration->createLlvm(llvmObj);
        llvm::BranchInst::Create(forConditionBlock, forStartBlock);
        
        // body block
        llvmObj->block = forBodyBlock;
        CodeScope::createLlvm(llvmObj);

        // after block
        if (!hasReturnStatement) llvm::BranchInst::Create(forStepBlock, llvmObj->block);

        // step block
        llvmObj->block = forStepBlock;
        forIterData.stepOperation->createLlvm(llvmObj);
        llvm::BranchInst::Create(forConditionBlock, forStepBlock);

        // condition block
        llvmObj->block = forConditionBlock;
        llvm::BranchInst::Create(
            forBodyBlock, 
            afterForBlock, 
            new llvm::TruncInst(forIterData.conditionOperation->createLlvm(llvmObj), llvm::Type::getInt1Ty(llvmObj->context), "", llvmObj->block), 
            forConditionBlock
        );

        llvmObj->block = afterForBlock;
    } else {
        auto& forEachData = get<ForEachData>(data);
        
        auto forStartBlock     = llvm::BasicBlock::Create(llvmObj->context, "forEachStart",     llvmObj->function);
        auto forStepBlock      = llvm::BasicBlock::Create(llvmObj->context, "forEachStep",      llvmObj->function);
        auto forConditionBlock = llvm::BasicBlock::Create(llvmObj->context, "forEachCondition", llvmObj->function);
        auto forBodyBlock      = llvm::BasicBlock::Create(llvmObj->context, "forEach",          llvmObj->function);
        auto afterForBlock     = llvm::BasicBlock::Create(llvmObj->context, "afterForEach",     llvmObj->function);
        llvmStepBlock = forStepBlock;
        llvmAfterBlock = afterForBlock;

        // start block
        llvm::BranchInst::Create(forStartBlock, llvmObj->block);
        llvmObj->block = forStartBlock;
        
        forEachData.indexDeclaration->createLlvm(llvmObj);
        forEachData.itDeclaration->createLlvm(llvmObj);
        auto llvmItRef = forEachData.it->getReferenceLlvm(llvmObj, true);
        llvm::Value* itOffset = llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), ((ReferenceType*)forEachData.it->type)->underlyingType->sizeInBytes());
        llvm::Value* size = nullptr;
        llvm::Value* data = nullptr;
        llvm::Value* dynArrayRef = nullptr;
        auto arrayEffType = forEachData.arrayValue->type->getEffectiveType();
        if (arrayEffType->kind == Type::Kind::StaticArray) {
            auto staticArrayType = (StaticArrayType*)arrayEffType;
            auto arrayRef = forEachData.arrayValue->getReferenceLlvm(llvmObj);
            data = new llvm::BitCastInst(arrayRef, forEachData.it->type->createLlvm(llvmObj), "", llvmObj->block);
            size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), staticArrayType->sizeAsInt);
        } else if (arrayEffType->kind == Type::Kind::DynamicArray) {
            auto dynamicArrayType = (DynamicArrayType*)arrayEffType;
            if (Value::isLvalue(forEachData.arrayValue)) {
                dynArrayRef = forEachData.arrayValue->getReferenceLlvm(llvmObj);
                data = new llvm::LoadInst(dynamicArrayType->llvmGepData(llvmObj, dynArrayRef), "", llvmObj->block);
            } else {
                auto arrayVal = forEachData.arrayValue->createLlvm(llvmObj);
                data = dynamicArrayType->llvmExtractData(llvmObj, arrayVal);
                size = dynamicArrayType->llvmExtractSize(llvmObj, arrayVal);
            }
        } else if (arrayEffType->kind == Type::Kind::ArrayView) {
            auto arrayViewType = (ArrayViewType*)arrayEffType;
            if (Value::isLvalue(forEachData.arrayValue)) {
                auto arrayRef = forEachData.arrayValue->getReferenceLlvm(llvmObj);
                data = new llvm::LoadInst(arrayViewType->llvmGepData(llvmObj, arrayRef), "", llvmObj->block);
                size = new llvm::LoadInst(arrayViewType->llvmGepSize(llvmObj, arrayRef), "", llvmObj->block);
            } else {
                auto arrayVal = forEachData.arrayValue->createLlvm(llvmObj);
                data = arrayViewType->llvmExtractData(llvmObj, arrayVal);
                size = arrayViewType->llvmExtractSize(llvmObj, arrayVal);
            }
        }
        auto llvmIndexRef = forEachData.index->getReferenceLlvm(llvmObj);
        if (loopForward) {
            new llvm::StoreInst(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), llvmIndexRef, llvmObj->block);
            new llvm::StoreInst(data, llvmItRef, llvmObj->block);
        } else {
            if (dynArrayRef) {
                auto dynamicArrayType = (DynamicArrayType*)arrayEffType;
                size = new llvm::LoadInst(dynamicArrayType->llvmGepSize(llvmObj, dynArrayRef), "", llvmObj->block);
            }
            auto sizeMinus1 = llvm::BinaryOperator::CreateSub(
                size, 
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 1), 
                "", llvmObj->block
            );
            new llvm::StoreInst(sizeMinus1, llvmIndexRef, llvmObj->block);
            new llvm::StoreInst(
                new llvm::IntToPtrInst(
                    llvm::BinaryOperator::CreateAdd(
                        new llvm::PtrToIntInst(
                            data,
                            IntegerType::Create(IntegerType::Size::I64)->createLlvm(llvmObj),
                            "", llvmObj->block
                        ),
                        llvm::BinaryOperator::CreateMul(itOffset, sizeMinus1, "", llvmObj->block),
                        "", llvmObj->block
                    ),
                    forEachData.it->type->createLlvm(llvmObj),
                    "", llvmObj->block
                ),
                llvmItRef, 
                llvmObj->block
            );
        }
        llvm::BranchInst::Create(forConditionBlock, forStartBlock);

        // body block
        llvmObj->block = forBodyBlock;
        CodeScope::createLlvm(llvmObj);

        // after block
        if (!hasReturnStatement) llvm::BranchInst::Create(forStepBlock, llvmObj->block);

        // step block
        llvmObj->block = forStepBlock;
        if (loopForward) {
            new llvm::StoreInst(
                llvm::BinaryOperator::CreateAdd(
                    new llvm::LoadInst(llvmIndexRef, "", llvmObj->block),
                    llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 1),
                    "", llvmObj->block
                ),
                llvmIndexRef, 
                llvmObj->block
            );
            new llvm::StoreInst(
                new llvm::IntToPtrInst(
                    llvm::BinaryOperator::CreateAdd(
                        new llvm::PtrToIntInst(
                            new llvm::LoadInst(llvmItRef, "", llvmObj->block), 
                            IntegerType::Create(IntegerType::Size::I64)->createLlvm(llvmObj),
                            "", llvmObj->block
                        ),
                        itOffset,
                        "", llvmObj->block
                    ),
                    forEachData.it->type->createLlvm(llvmObj),
                    "", llvmObj->block
                ),
                llvmItRef, 
                llvmObj->block
            );
        } else {
            new llvm::StoreInst(
                llvm::BinaryOperator::CreateSub(
                    new llvm::LoadInst(llvmIndexRef, "", llvmObj->block),
                    llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 1),
                    "", llvmObj->block
                ),
                llvmIndexRef, 
                llvmObj->block
            );
            new llvm::StoreInst(
                new llvm::IntToPtrInst(
                    llvm::BinaryOperator::CreateSub(
                        new llvm::PtrToIntInst(
                            new llvm::LoadInst(llvmItRef, "", llvmObj->block), 
                            IntegerType::Create(IntegerType::Size::I64)->createLlvm(llvmObj),
                            "", llvmObj->block
                        ),
                        itOffset,
                        "", llvmObj->block
                    ),
                    forEachData.it->type->createLlvm(llvmObj),
                    "", llvmObj->block
                ),
                llvmItRef, 
                llvmObj->block
            );
        }
        llvm::BranchInst::Create(forConditionBlock, forStepBlock);

        // condition block
        llvmObj->block = forConditionBlock;
        if (dynArrayRef) {
            auto dynamicArrayType = (DynamicArrayType*)arrayEffType;
            size = new llvm::LoadInst(dynamicArrayType->llvmGepSize(llvmObj, dynArrayRef), "", llvmObj->block);
        }
        llvm::Value* condition;
        if (loopForward) {
            condition = new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SLT, 
                new llvm::LoadInst(llvmIndexRef, "", llvmObj->block), 
                size, 
                ""
            );
        } else {
            condition = new llvm::ICmpInst(*llvmObj->block, llvm::ICmpInst::ICMP_SGE, 
                new llvm::LoadInst(llvmIndexRef, "", llvmObj->block), 
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvmObj->context), 0), 
                ""
            );
        }
        llvm::BranchInst::Create(forBodyBlock, afterForBlock, condition, forConditionBlock);

        llvmObj->block = afterForBlock;
    }
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
void WhileScope::templateCopy(WhileScope* scope, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    scope->conditionExpression = (Value*)conditionExpression->templateCopy(parentScope, templateToType);
    CodeScope::templateCopy(scope, parentScope, templateToType);
}
Statement* WhileScope::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto value = Create(position, parentScope);
    templateCopy(value, parentScope, templateToType);
    return value;
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
    this->conditionExpression = getValue(tokens, i, {"{", "do"}, false, false);
    if (!this->conditionExpression) {
        return false;
    }
    if (tokens[i].value == "{") {
        i += 1;
        return CodeScope::createCodeTree(tokens, i);
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
        return true;
    }
}
bool WhileScope::interpret() {
    auto boolCondition = ConstructorOperation::Create(position, Type::Create(Type::Kind::Bool), {conditionExpression});
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
    return false;
}
void WhileScope::allocaAllDeclarationsLlvm(LlvmObject* llvmObj) {
    conditionExpression->createAllocaLlvmIfNeededForValue(llvmObj);
    CodeScope::allocaAllDeclarationsLlvm(llvmObj);
}
void WhileScope::createLlvm(LlvmObject* llvmObj) {
    auto whileConditionBlock = llvm::BasicBlock::Create(llvmObj->context, "whileCondition", llvmObj->function);
    auto whileBlock          = llvm::BasicBlock::Create(llvmObj->context, "while",          llvmObj->function);
    auto afterWhileBlock     = llvm::BasicBlock::Create(llvmObj->context, "afterWhile",     llvmObj->function);
    llvmConditionBlock = whileConditionBlock;
    llvmAfterBlock = afterWhileBlock;
    llvm::BranchInst::Create(whileConditionBlock, llvmObj->block);
    llvmObj->block = whileBlock;
    CodeScope::createLlvm(llvmObj);
    if (!hasReturnStatement) llvm::BranchInst::Create(whileConditionBlock, llvmObj->block);
    llvmObj->block = whileConditionBlock;
    llvm::BranchInst::Create(
        whileBlock, 
        afterWhileBlock, 
        new llvm::TruncInst(conditionExpression->createLlvm(llvmObj), llvm::Type::getInt1Ty(llvmObj->context), "", llvmObj->block), 
        whileConditionBlock
    );
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
void IfScope::templateCopy(IfScope* scope, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    scope->conditionExpression = (Value*)conditionExpression->templateCopy(parentScope, templateToType);
    if (elseScope) {
        scope->elseScope = (CodeScope*)elseScope->templateCopy(parentScope, templateToType);
    } else {
        scope->elseScope = nullptr;
    }
    CodeScope::templateCopy(scope, parentScope, templateToType);
}
Statement* IfScope::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto value = Create(position, parentScope);
    templateCopy(value, parentScope, templateToType);
    return value;
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
    this->conditionExpression = getValue(tokens, i, {"{", "then"}, false, false);
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
    auto boolCondition = ConstructorOperation::Create(position, Type::Create(Type::Kind::Bool), {conditionExpression});
    auto valueInterpret = boolCondition->interpret(parentScope);
    if (!valueInterpret) return false;
    if (valueInterpret.value()) conditionExpression = valueInterpret.value();
    bool elseScopeErrors = false;
    maybeUninitializedDeclarations = parentScope->maybeUninitializedDeclarations;
    parentMaybeUninitializedDeclarations = maybeUninitializedDeclarations;
    if (elseScope) elseScope->hasReturnStatement = hasReturnStatement;
    if (!interpretNoUnitializedDeclarationsSet()) {
        return false;
    }
    if (elseScope) {
        elseScope->parentMaybeUninitializedDeclarations = parentScope->maybeUninitializedDeclarations;
        elseScope->declarationsInitState = parentScope->declarationsInitState;
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
        return hasReturnStatement && conditionExpression->isConstexpr && ((BoolValue*)conditionExpression)->value;
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
void IfScope::allocaAllDeclarationsLlvm(LlvmObject* llvmObj) {
    conditionExpression->createAllocaLlvmIfNeededForValue(llvmObj);
    CodeScope::allocaAllDeclarationsLlvm(llvmObj);
    if (elseScope) {
        elseScope->allocaAllDeclarationsLlvm(llvmObj);
    }
}
void IfScope::createLlvm(LlvmObject* llvmObj) {
    if (hasReturnStatement && conditionExpression->isConstexpr && ((BoolValue*)conditionExpression)->value) {
        CodeScope::createLlvm(llvmObj);
    }
    else if (elseScope) {
        auto ifBlock = llvm::BasicBlock::Create(llvmObj->context, "if", llvmObj->function);
        auto oldBlock = llvmObj->block;
        llvmObj->block = ifBlock;
        CodeScope::createLlvm(llvmObj);
        auto afterIfBlock = llvmObj->block;
        auto elseBlock = llvm::BasicBlock::Create(llvmObj->context, "else", llvmObj->function);
        llvmObj->block = oldBlock;
        llvm::BranchInst::Create(
            ifBlock, 
            elseBlock, 
            new llvm::TruncInst(conditionExpression->createLlvm(llvmObj), llvm::Type::getInt1Ty(llvmObj->context), "", llvmObj->block), 
            llvmObj->block
        );
        llvmObj->block = elseBlock;
        elseScope->createLlvm(llvmObj);
        if (!getHasReturnStatement()) {
            auto afterIfElseBlock = llvm::BasicBlock::Create(llvmObj->context, "afterIfElse", llvmObj->function);
            if (!hasReturnStatement) llvm::BranchInst::Create(afterIfElseBlock, afterIfBlock);
            if (!elseScope->hasReturnStatement) llvm::BranchInst::Create(afterIfElseBlock, llvmObj->block);
            llvmObj->block = afterIfElseBlock;
        }
    } else {
        auto ifBlock = llvm::BasicBlock::Create(llvmObj->context, "if", llvmObj->function);
        auto oldBlock = llvmObj->block;
        llvmObj->block = ifBlock;
        CodeScope::createLlvm(llvmObj);
        auto afterIfBlock = llvm::BasicBlock::Create(llvmObj->context, "afterIf", llvmObj->function);
        if (!hasReturnStatement) llvm::BranchInst::Create(afterIfBlock, llvmObj->block);
        llvmObj->block = oldBlock;
        llvm::BranchInst::Create(
            ifBlock, 
            afterIfBlock, 
            new llvm::TruncInst(conditionExpression->createLlvm(llvmObj), llvm::Type::getInt1Ty(llvmObj->context), "", llvmObj->block), 
            llvmObj->block
        );
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

