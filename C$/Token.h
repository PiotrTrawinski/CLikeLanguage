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
    
    virtual Type type()=0;
};

struct TokenLabel : Token {
    TokenLabel(std::string value) : value(value){}
    Type type() override {
        return Token::Type::Label;
    }

    std::string value;
};

struct TokenStringLiteral : Token {
    TokenStringLiteral(std::string value) : value(value){}
    Type type() override {
        return Token::Type::StringLiteral;
    }

    std::string value;
};

struct TokenChar : Token {
    TokenChar(char value) : value(value){}
    Type type() override {
        return Token::Type::Char;
    }

    char value;
};

struct TokenInteger : Token {
    TokenInteger(std::string value) : value(value){}
    Type type() override {
        return Token::Type::Integer;
    }

    std::string value;
};

struct TokenFloat : Token {
    TokenFloat(std::string value) : value(value){}
    Type type() override {
        return Token::Type::Float;
    }

    std::string value;
};

struct TokenSymbol : Token {
    TokenSymbol(char value) : value(value){}
    Type type() override {
        return Token::Type::Symbol;
    }

    char value;
};