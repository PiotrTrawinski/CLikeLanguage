#include "ClassDeclaration.h"
#include "Scope.h"
#include "Type.h"
#include "Declaration.h"

using namespace std;

ClassDeclaration::ClassDeclaration(const CodePosition& position, string name) :
    Statement(position, Statement::Kind::ClassDeclaration),
    name(name)
{}
vector<unique_ptr<ClassDeclaration>> ClassDeclaration::objects;
ClassDeclaration* ClassDeclaration::Create(const CodePosition& position, string name) {
    objects.emplace_back(make_unique<ClassDeclaration>(position, name));
    return objects.back().get();
}
bool ClassDeclaration::interpret() {
    if (status == Status::Evaluated) {
        return true;
    }
    if (status == Status::InEvaluation) {
        return errorMessageBool("recursive class declaration", position);
    }
    status = Status::InEvaluation;
    body->classDeclaration = this;
    if (!body->interpret()) {
        return false;
    }
    status = Status::Evaluated;
    return true;
}
bool ClassDeclaration::operator==(const Statement& declaration) const {
    if(typeid(declaration) == typeid(*this)){
        const auto& other = static_cast<const ClassDeclaration&>(declaration);
        return this->name == other.name
            && this->status == other.status
            && this->templateTypes == other.templateTypes
            && Statement::operator==(other);
    } else {
        return false;
    }
}
llvm::StructType* ClassDeclaration::getLlvmType(LlvmObject* llvmObj) {
    if (!llvmType) {
        llvmType = llvm::StructType::create(llvmObj->context, name+to_string(body->id));
    }
    return llvmType;
}
void ClassDeclaration::createLlvm(LlvmObject* llvmObj) {
    llvmType = getLlvmType(llvmObj);
    vector<llvm::Type*> types;

    //body->createLlvm(llvmObj);

    for (auto declaration : body->declarations) {
        if (!declaration->variable->isConstexpr) {
            types.push_back(declaration->variable->type->createLlvm(llvmObj));
        }
    }

    llvmType->setBody(types);
}