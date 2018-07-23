#include "Statement.h"

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