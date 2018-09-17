#include "Statement.h"
#include "Type.h"
#include "Scope.h"

using namespace std;


Statement::Statement(const CodePosition& position, Kind kind) : 
    kind(kind),
    position(position)
{}
vector<unique_ptr<Statement>> Statement::objects;
Statement* Statement::Create(const CodePosition& position, Kind kind) {
    objects.emplace_back(make_unique<Statement>(position, kind));
    return objects.back().get();
}

bool Statement::operator==(const Statement& statement) const {
    if(typeid(statement) == typeid(*this)){
        const auto& other = static_cast<const Statement&>(statement);
        return this->kind == other.kind
            && this->position.charNumber == other.position.charNumber
            && this->position.lineNumber == other.position.lineNumber
            && this->position.fileInfo == other.position.fileInfo;
    } else {
        return false;
    }
}
void Statement::templateCopy(Statement* statement, Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    statement->kind = kind;
    statement->position = position;
    statement->isReachable = isReachable;
}
Statement* Statement::templateCopy(Scope* parentScope, const unordered_map<string, Type*>& templateToType) {
    auto statement = Create(position, kind);
    statement->isReachable = isReachable;
    return statement;
}