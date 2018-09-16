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
            return errorMessageBool("unknown type: " + DeclarationMap::toString(variable->type), position);
        }
        if (value && value->type && !value->type->interpret(scope)) {
            return errorMessageBool("unknown type: " + DeclarationMap::toString(value->type), position);
        }
        if (isFunctionDeclaration() && variable->isConst && value) {
            addToMapStatus = true;
        } else {
            addToMapStatus = scope->declarationMap.addVariableDeclaration(this);
        }
        if (!addToMapStatus) {
            return errorMessageBool("2 same declarations of " + variable->name + ".\n"
                + "1 at line " + to_string(position.lineNumber) + "\n"
                + "2 at line " + to_string(scope->declarationMap.getDeclarations(variable->name)[0]->position.lineNumber),
                position);
        }

        status = Declaration::Status::InEvaluation;
        if (value) {
            if (variable->isConstexpr) {
                auto valueInterpret = value->interpret(scope);
                if (!valueInterpret) return false;
                if (valueInterpret.value()) value = valueInterpret.value();
            } else {
                if (variable->type) {
                    if (byReference) {
                        value = ConstructorOperation::Create(value->position, ReferenceType::Create(variable->type), {value});
                    } else {
                        value = ConstructorOperation::Create(value->position, variable->type, {value});
                    }
                }
                optional<Value*> valueInterpret = nullopt;
                if (value->valueKind == Value::ValueKind::Operation && ((Operation*)value)->kind == Operation::Kind::Constructor) {
                    valueInterpret = ((ConstructorOperation*)value)->interpret(scope, false, true);
                } else {
                    valueInterpret = value->interpret(scope);
                }
                if (!valueInterpret) return false;
                if (valueInterpret.value()) value = valueInterpret.value();
                if (!variable->type) {
                    if (byReference) {
                        value = ConstructorOperation::Create(value->position, ReferenceType::Create(value->type), {value});
                        auto valueInterpret = value->interpret(scope);
                        if (!valueInterpret) return false;
                        if (valueInterpret.value()) value = valueInterpret.value();
                        variable->type = ReferenceType::Create(value->type->getEffectiveType());
                    } else {
                        variable->type = value->type->getEffectiveType();
                    }
                }
                variable->isConstexpr = variable->isConst && value->isConstexpr;
            }
        } else if (variable->type && variable->type->kind == Type::Kind::Reference) {
            return errorMessageBool("cannot declare reference type variable without value", position);
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
void Declaration::createAllocaLlvmIfNeeded(LlvmObject* llvmObj) {
    if (!value || !variable->isConstexpr || value->valueKind != Value::ValueKind::FunctionValue) {
        llvmVariable = variable->type->allocaLlvm(llvmObj, variable->name);
    }
    if (value && value->valueKind == Value::ValueKind::Operation) {
        ((Operation*)value)->createAllocaLlvmIfNeeded(llvmObj);
    }
}
void Declaration::createLlvm(LlvmObject* llvmObj) {
    if (value && variable->isConstexpr && value->valueKind == Value::ValueKind::FunctionValue) {
        if (variable->name == "main") {
            ((FunctionValue*)value)->createLlvm(llvmObj, "_main");
        } else {
            ((FunctionValue*)value)->createLlvm(llvmObj, variable->name);
        }
    } else {
        if (value) {
            if (variable->type->kind == Type::Kind::Reference) {
                new llvm::StoreInst(value->getReferenceLlvm(llvmObj), llvmVariable, llvmObj->block);
            } else {
                auto assignOperation = AssignOperation::Create(position);
                assignOperation->arguments.push_back(variable);
                assignOperation->arguments.push_back(value);
                assignOperation->interpret(scope);
                assignOperation->isConstruction = true;
                assignOperation->createLlvm(llvmObj);
            }
        }
    }
}