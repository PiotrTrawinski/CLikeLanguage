#pragma once
#include <string>
#include <unordered_map>

enum class Keyword {
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

Keyword getKeyword(const std::string& str);
bool keywordIsType(Keyword keyword);
bool keywordIsScopeStart(Keyword keyword);
bool keywordIsSpecialValue(Keyword keyword);
bool keywordIsFlowStatement(Keyword keyword);

