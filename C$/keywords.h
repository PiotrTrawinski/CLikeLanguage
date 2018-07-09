#pragma once
#include <string>
#include <unordered_map>


struct Keyword {
    enum class Kind {
        TypeName,
        ScopeStart,
        SpecialValue,
        FlowStatement
    };

    Keyword(Kind kind) : kind(kind) {}

    static Keyword* get(const std::string& str);

    Kind kind;

private:
    static std::unordered_map<std::string, std::unique_ptr<Keyword>> keywordLabels;
};
struct TypeKeyword : Keyword {
    enum class Value {
        Int, I8, I16, I32, I64, U8, U16, U32, U64,
        Float, F32, F64,
        Bool,
        String,
        Void
    };

    TypeKeyword(Value value) :
        Keyword(Keyword::Kind::TypeName),
        value(value)
    {}

    Value value;
};
struct ScopeStartKeyword : Keyword {
    enum class Value {
        While, For, If, Else, Class, Defer
    };

    ScopeStartKeyword(Value value) :
        Keyword(Keyword::Kind::ScopeStart),
        value(value)
    {}

    Value value;
};
struct FlowStatementKeyword : Keyword {
    enum class Value {
        Return, Break, Continue, Remove,
    };

    FlowStatementKeyword(Value value) :
        Keyword(Keyword::Kind::FlowStatement),
        value(value)
    {}

    Value value;
};
struct SpecialValueKeyword : Keyword {
    enum class Value {
        True, False
    };

    SpecialValueKeyword(Value value) :
        Keyword(Keyword::Kind::SpecialValue),
        value(value)
    {}

    Value value;
};

/*enum class Keyword {
    NOT_KEYWORD,
    While,
    For,
    If,
    ElseIf,
    Else,
    Return,
    Break,
    Continue,
    Remove,
    Class,
    Defer,
    Int, I8, I16, I32, I64, U8, U16, U32, U64,
    Float, F32, F64,
    Bool, True, False,
    String
};
*

/*
Keyword getKeyword(const std::string& str);
bool keywordIsType(Keyword keyword);
bool keywordIsScopeStart(Keyword keyword);
bool keywordIsSpecialValue(Keyword keyword);
bool keywordIsFlowStatement(Keyword keyword);
*/
