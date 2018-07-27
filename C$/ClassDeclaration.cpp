#include "ClassDeclaration.h"
#include "Scope.h"
#include "Type.h"

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