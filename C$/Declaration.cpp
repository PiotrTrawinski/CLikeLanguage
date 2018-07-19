#include "Declaration.h"

using namespace std;

Declaration::Declaration(const CodePosition& position) : 
    Statement(position, Statement::Kind::Declaration),
    variable(position)
{}

bool Declaration::isFunctionDeclaration() {
    return value->type != nullptr && 
        (
            value->type->kind == Type::Kind::Function
            || value->type->kind == Type::Kind::TemplateFunction
        );
}
bool Declaration::interpret(Scope* scope, bool outOfOrder) {
    if (status != Declaration::Status::Evaluated) {
        bool addToMapStatus = false;
        if (isFunctionDeclaration()) {
            addToMapStatus = scope->declarationMap.addFunctionDeclaration(this);
        } else {
            addToMapStatus = scope->declarationMap.addVariableDeclaration(this);
        }
        if (!addToMapStatus) {
            return errorMessage("2 same declarations of " + variable.name + ".\n"
                + "1 at line " + to_string(position.lineNumber) + "\n"
                + "2 at line " + to_string(scope->declarationMap.getDeclarations(variable.name)[0]->position.lineNumber),
                position);
        }

        status = Declaration::Status::InEvaluation;
        auto valueInterpret = value->interpret(scope);
        if (!valueInterpret) {
            return false;
        } else if (valueInterpret.value()) {
            value = move(valueInterpret.value());
        }
        status = Declaration::Status::Evaluated;
    }

    variable.isConstexpr = value->isConstexpr;
    variable.type = value->type->copy();

    if (!outOfOrder) {
        status = Declaration::Status::Completed;
    }

    return true;
}

bool Declaration::operator==(const Statement& declaration) const {
    if(typeid(declaration) == typeid(*this)){
        const auto& other = static_cast<const Declaration&>(declaration);
        return this->variable == other.variable
            && this->value == other.value
            && Statement::operator==(other);
    } else {
        return false;
    }
}