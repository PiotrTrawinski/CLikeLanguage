#include "DeclarationMap.h"
#include "Declaration.h"

using namespace std;

bool DeclarationMap::addDeclaration(string idName, Declaration* declaration) {
    if (idNameToDeclaration.find(idName) != idNameToDeclaration.end()) {
        return false;
    }
    idNameToDeclaration[idName] = declaration;
    declarationToIdName[declaration] = idName;
    nameToDeclarations[declaration->variable.name].push_back(declaration);
    return true;
}
bool DeclarationMap::addVariableDeclaration(Declaration* declaration) {
    return addDeclaration(declaration->variable.name, declaration);
}
bool DeclarationMap::addFunctionDeclaration(Declaration* declaration) {
    return addDeclaration(
        declaration->variable.name + toString(declaration->value->type.get()), 
        declaration
    );
}
string DeclarationMap::getIdName(Declaration* declaration) {
    return declarationToIdName.at(declaration);
}
Declaration* DeclarationMap::getDeclaration(string idName) {
    return idNameToDeclaration.at(idName);
}
vector<Declaration*> DeclarationMap::getDeclarations(string name) {
    return nameToDeclarations[name];
}
string DeclarationMap::toString(Type* type) {
    switch (type->kind) {
    case Type::Kind::Void: return "void";
    case Type::Kind::Bool: return "bool";
    case Type::Kind::String: return "string";
    case Type::Kind::Integer:
        switch (((IntegerType*)type)->size) {
        case IntegerType::Size::I8:  return "i8";
        case IntegerType::Size::I16: return "i16";
        case IntegerType::Size::I32: return "i32";
        case IntegerType::Size::I64: return "i64";
        case IntegerType::Size::U8:  return "u8";
        case IntegerType::Size::U16: return "u16";
        case IntegerType::Size::U32: return "u32";
        case IntegerType::Size::U64: return "u64";
        }
    case Type::Kind::Float:
        switch (((FloatType*)type)->size) {
        case FloatType::Size::F32: return "f32";
        case FloatType::Size::F64: return "f64";
        }
    case Type::Kind::Function:{
        auto functionType = (FunctionType*)type;
        string str = "(";
        for (int i = 0; i < functionType->argumentTypes.size(); ++i) {
            str += toString(functionType->argumentTypes[i].get());
            if (i != functionType->argumentTypes.size()-1) str += ",";
        }
        return str + ")";
    }
    case Type::Kind::RawPointer:
        return "*" + toString(((RawPointerType*)(type))->underlyingType.get());
    case Type::Kind::OwnerPointer:
        return "!" + toString(((OwnerPointerType*)(type))->underlyingType.get());
    case Type::Kind::Reference:
        return "&" + toString(((ReferenceType*)(type))->underlyingType.get());
    case Type::Kind::MaybeError:
        return "?" + toString(((MaybeErrorType*)(type))->underlyingType.get());
    case Type::Kind::ArrayView:
        return "[*]" + toString(((ArrayViewType*)(type))->elementType.get());
    case Type::Kind::DynamicArray:
        return "[]" + toString(((DynamicArrayType*)(type))->elementType.get());
    case Type::Kind::StaticArray:
        return "[N]" + toString(((StaticArrayType*)(type))->elementType.get());
    case Type::Kind::Template:
        return "T(" + ((TemplateType*)(type))->name + ")";
    case Type::Kind::TemplateFunction:{
        auto functionType = (TemplateFunctionType*)type;
        string str = "<";
        for (int i = 0; i < functionType->templateTypes.size(); ++i) {
            str += functionType->templateTypes[i]->name;
            if (i != functionType->templateTypes.size()-1) str += ",";
        }
        str += ">(";
        for (int i = 0; i < functionType->argumentTypes.size(); ++i) {
            str += toString(functionType->argumentTypes[i].get());
            if (i != functionType->argumentTypes.size()-1) str += ",";
        }
        return str + ")";
    }
    case Type::Kind::Class:{
        ClassType* classType = (ClassType*)(type);
        string str = ((ClassType*)(type))->name;
        if (classType->templateTypes.size() > 0) {
            str += "<";
            for (int i = 0; i < classType->templateTypes.size(); ++i) {
                str += toString(classType->templateTypes[i].get());
                if (i != classType->templateTypes.size()-1) str += ",";
            }
            str += ">";
        }
        return str;
    }
    case Type::Kind::TemplateClass:
        return "TemplateClass";
    }
}