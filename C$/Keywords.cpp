#include "keywords.h"

namespace {
    const std::unordered_map<std::string, Keyword> keywordLabels = {
    {"while",    Keyword::While},
    {"for",      Keyword::For},
    {"if",       Keyword::If},
    {"elseIf",   Keyword::ElseIf},
    {"else",     Keyword::Else},
    {"return",   Keyword::Return},
    {"break",    Keyword::Break},
    {"continue", Keyword::Continue},
    {"remove",   Keyword::Remove},
    {"class",    Keyword::Class},
    {"defer",    Keyword::Defer},
    {"int",      Keyword::Int},
    {"i8",       Keyword::I8},
    {"i16",      Keyword::I16},
    {"i32",      Keyword::I32},
    {"i64",      Keyword::I64},
    {"u8",       Keyword::U8},
    {"u16",      Keyword::U16},
    {"u32",      Keyword::U32},
    {"u64",      Keyword::U64},
    {"Float",    Keyword::Float},
    {"f32",      Keyword::F32},
    {"f64",      Keyword::F64},
    {"bool",     Keyword::Bool},
    {"true",     Keyword::True},
    {"false",    Keyword::False},
    {"string",   Keyword::String},
    };
}

Keyword getKeyword(const std::string& str) {
    try {
        return keywordLabels.at(str);
    } catch (const std::out_of_range& oor) {
        return Keyword::NOT_KEYWORD;
    }
}

bool keywordIsType(Keyword keyword) {
    switch (keyword) {
    case Keyword::Int:
    case Keyword::I8:
    case Keyword::I16:
    case Keyword::I32:
    case Keyword::I64:
    case Keyword::U8:
    case Keyword::U16:
    case Keyword::U32:
    case Keyword::U64:
    case Keyword::Float:
    case Keyword::F32:
    case Keyword::F64:
    case Keyword::Bool:
    case Keyword::String:
        return true;
    default:
        return false;
    }
}

bool keywordIsScopeStart(Keyword keyword) {
    switch (keyword) {
    case Keyword::While:
    case Keyword::For:
    case Keyword::If:
    case Keyword::ElseIf:
    case Keyword::Else:
    case Keyword::Class:
    case Keyword::Defer:
        return true;
    default:
        return false;
    }
}

bool keywordIsSpecialValue(Keyword keyword) {
    switch (keyword) {
    case Keyword::True:
    case Keyword::False:
        return true;
    default:
        return false;
    }
}

bool keywordIsFlowStatement(Keyword keyword) {
    switch (keyword) {
    case Keyword::Return:
    case Keyword::Break:
    case Keyword::Continue:
    case Keyword::Remove:
        return true;
    default:
        return false;
    }
}