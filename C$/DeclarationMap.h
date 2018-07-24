#pragma once

#include <vector>
#include <memory>
#include <unordered_map>

#include "Type.h"

struct Declaration;

class DeclarationMap {
public:
    bool addVariableDeclaration(Declaration* declaration);
    bool addFunctionDeclaration(Declaration* declaration);
    std::string getIdName(Declaration* declaration);
    Declaration* getDeclaration(std::string idName);

    std::vector<Declaration*> getDeclarations(std::string name);
    static std::string toString(Type* type);
private:
    bool addDeclaration(std::string idName, Declaration* declaration);

    std::unordered_map<Declaration*, std::string> declarationToIdName;
    std::unordered_map<std::string, Declaration*> idNameToDeclaration;
    std::unordered_map<std::string, std::vector<Declaration*>> nameToDeclarations;
};