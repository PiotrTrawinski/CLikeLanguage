#pragma once
#include <unordered_map>
#include <string>

struct ClassDeclaration;

class ClassDeclarationMap {
public:
    bool add(ClassDeclaration* classDeclaration);
    ClassDeclaration* getDeclaration(std::string name);
private:
    std::unordered_map<std::string, ClassDeclaration*> nameToDeclaration;
};