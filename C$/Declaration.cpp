#include "Declaration.h"
#include "Operation.h"

using namespace std;

Declaration::Declaration(const CodePosition& position) : 
    Statement(position, Statement::Kind::Declaration),
    variable(Variable::Create(position))
{}
vector<unique_ptr<Declaration>> Declaration::objects;
Declaration* Declaration::Create(const CodePosition& position) {
    objects.emplace_back(make_unique<Declaration>(position));
    return objects.back().get();
}

bool Declaration::isFunctionDeclaration() {
    if (!value) {
        if (variable->type->kind == Type::Kind::Function) {
            return true;
        }
        if (variable->type->kind == Type::Kind::TemplateFunction) {
            return true;
        }
        return false;
    }
    return value->type != nullptr && 
        (
            value->type->kind == Type::Kind::Function
            || value->type->kind == Type::Kind::TemplateFunction
        );
}
bool Declaration::interpret(Scope* scope, bool outOfOrder) {
    if (status != Declaration::Status::Evaluated) {
        if (outOfOrder && !variable->isConst) {
            return false;
        }
        bool addToMapStatus = false;
        if (variable->type && !variable->type->interpret(scope)) {
            return errorMessage("unknown type: " + DeclarationMap::toString(variable->type), position);
        }
        if (value && value->type && !value->type->interpret(scope)) {
            return errorMessage("unknown type: " + DeclarationMap::toString(value->type), position);
        }
        if (isFunctionDeclaration()) {
            addToMapStatus = scope->declarationMap.addFunctionDeclaration(this);
        } else {
            addToMapStatus = scope->declarationMap.addVariableDeclaration(this);
        }
        if (!addToMapStatus) {
            return errorMessage("2 same declarations of " + variable->name + ".\n"
                + "1 at line " + to_string(position.lineNumber) + "\n"
                + "2 at line " + to_string(scope->declarationMap.getDeclarations(variable->name)[0]->position.lineNumber),
                position);
        }

        status = Declaration::Status::InEvaluation;
        if (value) {
            if (variable->type) {
                if (!variable->type->interpret(scope)) {
                    return false;
                }
                auto cast = CastOperation::Create(value->position, variable->type);
                cast->arguments.push_back(value);
                value = cast;
            }
            auto valueInterpret = value->interpret(scope);
            if (!valueInterpret) {
                return false;
            } 
            if (valueInterpret.value()) {
                value = valueInterpret.value();
            }
            variable->isConstexpr = variable->isConst && value->isConstexpr;
            variable->type = value->type;
        }
        status = Declaration::Status::Evaluated;
    }

    if (!outOfOrder) {
        status = Declaration::Status::Completed;
    }

    return true;
}

bool Declaration::operator==(const Statement& declaration) const {
    if(typeid(declaration) == typeid(*this)){
        const auto& other = static_cast<const Declaration&>(declaration);
        return cmpPtr(this->variable, other.variable)
            && cmpPtr(this->value, other.value)
            && Statement::operator==(other);
    } else {
        return false;
    }
}