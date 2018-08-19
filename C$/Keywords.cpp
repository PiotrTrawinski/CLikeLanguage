#include "keywords.h"

using namespace std;

namespace {
    unordered_map<string, unique_ptr<Keyword>> initializeKeywordLabelsMap() {
        unordered_map<string, unique_ptr<Keyword>> keywordLabels;
        keywordLabels["while"]    = std::make_unique<ScopeStartKeyword>(ScopeStartKeyword::Value::While);
        keywordLabels["for"]      = std::make_unique<ScopeStartKeyword>(ScopeStartKeyword::Value::For);
        keywordLabels["if"]       = std::make_unique<ScopeStartKeyword>(ScopeStartKeyword::Value::If);
        keywordLabels["else"]     = std::make_unique<ScopeStartKeyword>(ScopeStartKeyword::Value::Else);
        keywordLabels["class"]    = std::make_unique<ScopeStartKeyword>(ScopeStartKeyword::Value::Class);
        keywordLabels["defer"]    = std::make_unique<ScopeStartKeyword>(ScopeStartKeyword::Value::Defer);
        keywordLabels["return"]   = std::make_unique<FlowStatementKeyword>(FlowStatementKeyword::Value::Return);
        keywordLabels["break"]    = std::make_unique<FlowStatementKeyword>(FlowStatementKeyword::Value::Break);
        keywordLabels["continue"] = std::make_unique<FlowStatementKeyword>(FlowStatementKeyword::Value::Continue);
        keywordLabels["remove"]   = std::make_unique<FlowStatementKeyword>(FlowStatementKeyword::Value::Remove);
        keywordLabels["int"]      = std::make_unique<TypeKeyword>(TypeKeyword::Value::Int);
        keywordLabels["byte"]     = std::make_unique<TypeKeyword>(TypeKeyword::Value::Byte);
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
    } catch (const std::out_of_range&) {
        return nullptr;
    }
}