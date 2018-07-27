#include "ClassDeclarationMap.h"
#include "ClassDeclaration.h"

using namespace std;

bool ClassDeclarationMap::add(ClassDeclaration* classDeclaration){
    if (nameToDeclaration.find(classDeclaration->name) != nameToDeclaration.end()) {
        return false;
    }
    nameToDeclaration[classDeclaration->name] = classDeclaration;
}
ClassDeclaration* ClassDeclarationMap::getDeclaration(std::string name) {
    try {
        return nameToDeclaration.at(name);
    } catch (const std::out_of_range&) {
        return nullptr;
    }
}