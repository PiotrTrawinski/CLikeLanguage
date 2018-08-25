#pragma once
#include <string>
#include "FileInfo.h"
#include "CodePosition.h"

struct Token {
    enum class Type {
        Label,            // anything that starts with letter
        StringLiteral,    // anything that starts with " symbol
        RawStringLiteral, // anything that starts with ` symbol
        Char,             // anything that starts with ' symbol
        Integer,          // starts with digit, has no '.' in the middle
        Float,            // starts with digit, has exactly 1 '.' in the middle
        Symbol            // any 1 symbol that does not match anything above
    };

    Token(Type type, const std::string& value, int lineNumber, int charNumber, const FileInfo* fileInfo) : 
        type(type),
        value(value),
        codePosition(fileInfo, lineNumber, charNumber)
    {}
    
    Type type;
    std::string value;
    CodePosition codePosition;
};
