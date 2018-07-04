#include "keywords.h"

/*namespace {
    const std::unordered_map<std::string, std::unique_ptr<Keyword>> keywordLabels = {
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
}*/

using namespace std;

namespace {
    unordered_map<string, unique_ptr<Keyword>> initializeKeywordLabelsMap() {
        unordered_map<string, unique_ptr<Keyword>> keywordLabels;
        keywordLabels["while"]    = std::make_unique<ScopeStartKeyword>(ScopeStartKeyword::Value::While);
        keywordLabels["for"]      = std::make_unique<ScopeStartKeyword>(ScopeStartKeyword::Value::For);
        keywordLabels["if"]       = std::make_unique<ScopeStartKeyword>(ScopeStartKeyword::Value::If);
        keywordLabels["elseIf"]   = std::make_unique<ScopeStartKeyword>(ScopeStartKeyword::Value::ElseIf);
        keywordLabels["else"]     = std::make_unique<ScopeStartKeyword>(ScopeStartKeyword::Value::Else);
        keywordLabels["class"]    = std::make_unique<ScopeStartKeyword>(ScopeStartKeyword::Value::Class);
        keywordLabels["defer"]    = std::make_unique<ScopeStartKeyword>(ScopeStartKeyword::Value::Defer);
        keywordLabels["return"]   = std::make_unique<FlowStatementKeyword>(FlowStatementKeyword::Value::Return);
        keywordLabels["break"]    = std::make_unique<FlowStatementKeyword>(FlowStatementKeyword::Value::Break);
        keywordLabels["continue"] = std::make_unique<FlowStatementKeyword>(FlowStatementKeyword::Value::Continue);
        keywordLabels["remove"]   = std::make_unique<FlowStatementKeyword>(FlowStatementKeyword::Value::Remove);
        keywordLabels["int"]      = std::make_unique<TypeKeyword>(TypeKeyword::Value::Int);
        keywordLabels["i8"]       = std::make_unique<TypeKeyword>(TypeKeyword::Value::I8);
        keywordLabels["i16"]      = std::make_unique<TypeKeyword>(TypeKeyword::Value::I16);
        keywordLabels["i32"]      = std::make_unique<TypeKeyword>(TypeKeyword::Value::I32);
        keywordLabels["i64"]      = std::make_unique<TypeKeyword>(TypeKeyword::Value::I64);
        keywordLabels["u8"]       = std::make_unique<TypeKeyword>(TypeKeyword::Value::U8);
        keywordLabels["u16"]      = std::make_unique<TypeKeyword>(TypeKeyword::Value::U16);
        keywordLabels["u32"]      = std::make_unique<TypeKeyword>(TypeKeyword::Value::U32);
        keywordLabels["u64"]      = std::make_unique<TypeKeyword>(TypeKeyword::Value::U64);
        keywordLabels["float"]    = std::make_unique<TypeKeyword>(TypeKeyword::Value::Float);
        keywordLabels["f32"]      = std::make_unique<TypeKeyword>(TypeKeyword::Value::F32);
        keywordLabels["f64"]      = std::make_unique<TypeKeyword>(TypeKeyword::Value::F64);
        keywordLabels["bool"]     = std::make_unique<TypeKeyword>(TypeKeyword::Value::Bool);
        keywordLabels["string"]   = std::make_unique<TypeKeyword>(TypeKeyword::Value::String);
        keywordLabels["void"]     = std::make_unique<TypeKeyword>(TypeKeyword::Value::Void);
        keywordLabels["true"]     = std::make_unique<SpecialValueKeyword>(SpecialValueKeyword::Value::True);
        keywordLabels["false"]    = std::make_unique<SpecialValueKeyword>(SpecialValueKeyword::Value::False);
        return keywordLabels;
    }
}

unordered_map<string, unique_ptr<Keyword>> Keyword::keywordLabels = initializeKeywordLabelsMap();

Keyword* Keyword::get(const std::string& str) {
    try {
        return keywordLabels.at(str).get();
    } catch (const std::out_of_range& oor) {
        return nullptr;
    }
}

/*
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
*/