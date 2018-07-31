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
    this->scope = scope;
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
            if (!valueInterpret) return false;
            if (valueInterpret.value()) value = valueInterpret.value();
            if (byReference && value->type->kind != Type::Kind::Reference) {
                auto refCast = CastOperation::Create(value->position, ReferenceType::Create(value->type));
                refCast->arguments.push_back(value);
                auto refCastInterpret = refCast->interpret(scope);
                if (!refCastInterpret) return false;
                if (refCastInterpret.value()) value = refCastInterpret.value();
                else value = refCastInterpret.value();
                variable->type = value->type;
            }
            else if (!byReference && value->type->kind == Type::Kind::Reference) {
                variable->type = ((ReferenceType*)value->type)->underlyingType;
            } else {
                variable->type = value->type;
            }
            variable->isConstexpr = variable->isConst && value->isConstexpr;
            
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

void Declaration::createLlvm(LlvmObject* llvmObj) {
    if (value && value->isConstexpr && value->valueKind == Value::ValueKind::FunctionValue) {
        auto functionValue = value->createLlvm(llvmObj);
    } else {
        llvmVariable = new llvm::AllocaInst(variable->type->createLlvm(llvmObj), 0, variable->name, llvmObj->block);
        if (value) {
            auto assignOperation = Operation::Create(position, Operation::Kind::Assign);
            assignOperation->arguments.push_back(variable);
            assignOperation->arguments.push_back(value);
            assignOperation->interpret(scope);
            assignOperation->createLlvm(llvmObj);
        }
    }
}