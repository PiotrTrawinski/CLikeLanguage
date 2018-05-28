#pragma once
#include <string>

struct Token {
    enum class Type {
        Label,          // anything that starts with letter
        StringLiteral,  // anything that starts with " symbol
        Char,           // anything that starts with ' symbol
        Integer,        // starts with digit, has no '.' in the middle
        Float,          // starts with digit, has exactly 1 '.' in the middle
        Symbol          // any 1 symbol that does not match anything above
    };

    Token(Type type, std::string value) : 
        type(type),
        value(value)
    {}
    
    Type type;
    std::string value;
};
