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
void ClassDeclaration::templateCopy(ClassDeclaration* classDeclaration, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    classDeclaration->name = name;
    classDeclaration->body = (ClassScope*)body->templateCopy(parentScope, templateToType);
    for (auto templateType : templateTypes) {
        classDeclaration->templateTypes.push_back((TemplateType*)templateType->templateCopy(parentScope, templateToType));
    }
    Statement::templateCopy(classDeclaration, parentScope, templateToType);
}
Statement* ClassDeclaration::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto classDeclaration = Create(position, name);
    templateCopy(classDeclaration, parentScope, templateToType);
    return classDeclaration;
}
ClassDeclaration* ClassDeclaration::get(const vector<Type*>& classTemplateTypes) {
    if (templateTypes.size() > 0 && classTemplateTypes.size() > 0) {
        for (auto implementation : implementations) {
            bool isMatch = true;
            for (int i = 0; i < implementation.first.size(); ++i) {
                if (!cmpPtr(classTemplateTypes[i], implementation.first[i])) {
                    isMatch = false;
                    break;
                }
            }
            if (isMatch) {
                return implementation.second;
            }
        }
        unordered_map<string, Type*> templateToType;
        for (int i = 0; i < templateTypes.size(); ++i) {
            templateToType[templateTypes[i]->name] = classTemplateTypes[i];
        }
        auto implementation = (ClassDeclaration*)templateCopy(body->parentScope, templateToType);
        implementation->templateTypes = {};
        implementations.push_back({classTemplateTypes, implementation});
        return implementation;
    } else {
        return this;
    }
}
bool ClassDeclaration::shallowInterpret(const vector<Type*>& classTemplateTypes) {
    if (templateTypes.size() > 0) {
        if (classTemplateTypes.size() == 0) return true;
        return get(classTemplateTypes)->shallowInterpret({});
    } else {
        body->classDeclaration = this;
        return body->interpret();
    }
}
bool ClassDeclaration::interpret(const vector<Type*>& classTemplateTypes) {
    if (templateTypes.size() > 0) {
        if (classTemplateTypes.size() == 0) return true;
        return get(classTemplateTypes)->interpret({});
    } else {
        body->classDeclaration = this;
        return body->completeInterpret();
    }
}
bool ClassDeclaration::interpretAllImplementations() {
    if (!interpret({})) {
        return false;
    }
    for (auto& implementation : implementations) {
        if (!implementation.second->interpret({})) {
            return false;
        }
    }
    return true;
}
bool ClassDeclaration::operator==(const Statement& declaration) const {
    if(typeid(declaration) == typeid(*this)){
        const auto& other = static_cast<const ClassDeclaration&>(declaration);
        return this->name == other.name
            && this->templateTypes == other.templateTypes
            && Statement::operator==(other);
    } else {
        return false;
    }
}
llvm::StructType* ClassDeclaration::getLlvmType(LlvmObject* llvmObj) {
    if (!llvmType) {
        llvmType = llvm::StructType::create(llvmObj->context, name);
    }
    return llvmType;
}
void ClassDeclaration::createLlvm(LlvmObject* llvmObj) {
    if (templateTypes.size() > 0) {
        for (auto implementation : implementations) {
            implementation.second->createLlvm(llvmObj);
        }
    } else {
        llvmType = getLlvmType(llvmObj);
        vector<llvm::Type*> types;

        for (auto declaration : body->declarations) {
            if (!declaration->variable->isConstexpr) {
                types.push_back(declaration->variable->type->createLlvm(llvmObj));
            }
        }

        llvmType->setBody(types);
    }
}
void ClassDeclaration::createLlvmBody(LlvmObject* llvmObj) {
    if (templateTypes.size() > 0) {
        for (auto implementation : implementations) {
            implementation.second->createLlvmBody(llvmObj);
        }
    } else {
        body->createLlvm(llvmObj);
    }
}