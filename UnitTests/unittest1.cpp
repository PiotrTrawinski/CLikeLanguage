#include "stdafx.h"
#include "CppUnitTest.h"

#include <string>
#include <fstream>
#include <optional>
#include "../C$/parsing.cpp"
#include "../C$/interpreting.cpp"
#include "../C$/keywords.cpp"
#include "../C$/globalVariables.cpp"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std;

int INDENT_SIZE = 4;

wstring toWstring(Type* type, int indent=0, bool isStart=true, bool firstCall=true);
wstring toWstring(Statement* statement, int indent=0, bool isStart=true, bool firstCall=true);
wstring toWstring(Declaration* declaration, int indent=0, bool isStart=true, bool firstCall=true);
wstring toWstring(Scope* scope, int indent=0, bool isStart=true, bool firstCall=true);
wstring toWstring(Value* value, int indent=0, bool isStart=true, bool firstCall=true);
wstring toWstring(FunctionValue* functionValue, int indent=0, bool isStart=true, bool firstCall=true);
wstring toWstring(Operation* operation, int indent=0, bool isStart=true, bool firstCall=true);
wstring toWstring(StaticArrayValue* staticArrayValue, int indent=0, bool isStart=true, bool firstCall=true);
wstring toWstring(Variable* variable, int indent=0, bool isStart=true, bool firstCall=true);

template <typename T> wstring toWstring(const std::optional<T>& obj) {
    if (obj) {
        return toWstring(obj.value());
    } else {
        return L"";
    }
}
template <typename T> wstring toWstring(const vector<T>& vec, int indent) {
    if (vec.size() == 0) {
        return L"[]";
    }
    indent += INDENT_SIZE;
    wstring str = L"[\n";
    for (int i = 0; i < vec.size() - 1; ++i) {
        str += strIndent(indent) + toWstring(vec[i], indent, false) + L",\n";
    }
    str += strIndent(indent) + toWstring(vec[vec.size()-1], indent, false) + L"\n";
    indent -= INDENT_SIZE;
    str += strIndent(indent) + L"]";
    return str;
}
template <typename T> wstring toWstring(const vector<T>& vec) {
    if (vec.size() == 0) {
        return L"[]";
    }
    wstring str = L"[";
    for (int i = 0; i < vec.size() - 1; ++i) {
        str += toWstring(vec[i]) + L", ";
    }
    str += toWstring(vec[vec.size()-1]) + L"]";
    return str;
}
template <typename T> wstring toWstring(const unique_ptr<T>& ptr, int indent=0, bool isStart=true, bool firstCall=true) {
    if (ptr) {
        return toWstring(ptr.get(), indent, isStart, firstCall);
    } else {
        return L"nullptr";
    }
}
template <typename T> wstring toWstringNewLines(const unordered_set<T>& set) {
    wstring str = L"{";
    for (const auto& value : set) {
        str += toWstring(value) + L"\n";
    }
    str += L"}";
    return str;
}
template <typename T> wstring toWstringNewLines(const vector<T>& vec) {
    wstring str = L"{\n";
    for (int i = 0; i < vec.size(); ++i) {
        str += toWstring(vec[i]) + L"\n";
    }
    str += L"}";
    return str;
}

wstring strIndent(int indent) {
    wstring str = L"";
    for (int i = 0; i < indent; ++i) {
        str += L" ";
    }
    return str;
}
wstring toWstring(const string& str) {
    wstring result = L"";
    for (char c : str) {
        result += c;
    }
    return result;
}
wstring toWstring(bool b) {
    if (b) {
        return L"true";
    } else {
        return L"false";
    }
}
wstring toWstring(char c) {
    wstring result = L"";
    result += c;
    return result;
}
wstring toWstring(int i) {
    return toWstring(to_string(i));
}
wstring toWstring(double i) {
    return toWstring(to_string(i));
}
wstring toWstring(const SourceStringLine& sourceStringLine) {
    return toWstring("[\"" + sourceStringLine.value + "\", " + to_string(sourceStringLine.number) + ", " + to_string((size_t)sourceStringLine.file) + "]");
}
wstring toWstring(const Token& token) {
    string typeStr;
    switch (token.type) {
    case Token::Type::Label: typeStr         = "Label";         break;
    case Token::Type::StringLiteral: typeStr = "StringLiteral"; break;
    case Token::Type::Char: typeStr          = "Char";          break;
    case Token::Type::Integer: typeStr       = "Integer";       break;
    case Token::Type::Float: typeStr         = "Float";         break;
    case Token::Type::Symbol: typeStr        = "Symbol";        break;
    }
    string filePosStr = to_string(token.codePosition.lineNumber) + ":" + to_string(token.codePosition.charNumber);
    string filePointer = to_string((size_t)token.codePosition.fileInfo);

    return toWstring("["+typeStr+", "+token.value+", "+filePosStr+", "+filePointer+"]");

}
wstring toWstring(const CodePosition& codePosition, int indent=0) {
    return L"CodePostion("+toWstring((int)codePosition.fileInfo)+L","+toWstring(codePosition.lineNumber)+L","+toWstring(codePosition.charNumber)+ L")";
}
wstring toWstring(Type* type, int indent, bool isStart, bool firstCall) {
    wstring str = L"";
    if (isStart) str += L"\n";
    switch (type->kind) {
    case Type::Kind::Void: return L"void";
    case Type::Kind::Bool: return L"bool";
    case Type::Kind::String: return L"string";
    case Type::Kind::Integer:
        switch (((IntegerType*)type)->size) {
        case IntegerType::Size::I8:  return L"i8";
        case IntegerType::Size::I16: return L"i16";
        case IntegerType::Size::I32: return L"i32";
        case IntegerType::Size::I64: return L"i64";
        case IntegerType::Size::U8:  return L"u8";
        case IntegerType::Size::U16: return L"u16";
        case IntegerType::Size::U32: return L"u32";
        case IntegerType::Size::U64: return L"u64";
        }
    case Type::Kind::Float:
        switch (((FloatType*)type)->size) {
        case FloatType::Size::F32: return L"f32";
        case FloatType::Size::F64: return L"f64";
        }
    case Type::Kind::Function:{
        auto functionType = (FunctionType*)type;
        wstring str = L"(";
        for (int i = 0; i < functionType->argumentTypes.size(); ++i) {
            str += toWstring(functionType->argumentTypes[i], indent, false, false);
            if (i != functionType->argumentTypes.size()-1) str += L",";
        }
        return str + L")->" + toWstring(functionType->returnType, indent, false, false);
    }
    case Type::Kind::RawPointer:
        return L"*" + toWstring(((RawPointerType*)(type))->underlyingType, indent, false, false);
    case Type::Kind::OwnerPointer:
        return L"!" + toWstring(((OwnerPointerType*)(type))->underlyingType, indent, false, false);
    case Type::Kind::Reference:
        return L"&" + toWstring(((ReferenceType*)(type))->underlyingType, indent, false, false);
    case Type::Kind::MaybeError:
        return L"?" + toWstring(((MaybeErrorType*)(type))->underlyingType, indent, false, false);
    case Type::Kind::ArrayView:
        return L"[*]" + toWstring(((ArrayViewType*)(type))->elementType, indent, false, false);
    case Type::Kind::DynamicArray:
        return L"[]" + toWstring(((DynamicArrayType*)(type))->elementType, indent, false, false);
    case Type::Kind::StaticArray:
        return L"[N]" + toWstring(((StaticArrayType*)(type))->elementType, indent, false, false);
    case Type::Kind::Template:
        return L"T(" + toWstring(((TemplateType*)(type))->name) + L")";
    case Type::Kind::TemplateFunction:
        return L"TemplateFunction";
    case Type::Kind::Class:
        return L"Class";
    case Type::Kind::TemplateClass:
        return L"TemplateClass";
    }
}
wstring toWstring(Statement* statement, int indent, bool isStart, bool firstCall) {
    wstring str = L"";
    if (isStart) str += L"\n";
    str += L"Statement {\n";
    indent += INDENT_SIZE;
    str += strIndent(indent) + L"position = " + toWstring(statement->position, indent) + L"\n";
    switch (statement->kind) {
    case Statement::Kind::Declaration: return str + toWstring((Declaration*)statement, indent, false, false);
    case Statement::Kind::Scope: return str + toWstring((Scope*)statement, indent, false, false);
    case Statement::Kind::Value: return str + toWstring((Value*)statement, indent, false, false);
    }
}
wstring toWstring(Declaration* declaration, int indent, bool isStart, bool firstCall) {
    if (firstCall) {
        return toWstring((Declaration*)declaration, indent, isStart);
    }
    wstring str = L"";
    return str + strIndent(indent-INDENT_SIZE) + L"} #Declaration";
}
wstring toWstring(Scope* scope, int indent, bool isStart, bool firstCall) {
    if (firstCall) {
        return toWstring((Statement*)scope, indent, isStart);
    }
    wstring str = L"";
    return str + strIndent(indent-INDENT_SIZE) + L"} #Scope";
}
wstring toWstring(Value* value, int indent, bool isStart, bool firstCall) {
    if (firstCall) {
        return toWstring((Statement*)value, indent, isStart);
    }
    wstring str = L"";
    str += strIndent(indent) + L"isConstexpr = " + toWstring(value->isConstexpr) + L"\n";
    str += strIndent(indent) + L"type = " + toWstring(value->type, indent) + L"\n";

    string typeName = "";
    switch (value->valueKind) {
    case Value::ValueKind::Char: 
        str += strIndent(indent) + L"Char = " + toWstring(((CharValue*)value)->value) + L"\n"; 
        typeName = "CharValue";
        break;
    case Value::ValueKind::Integer: 
        str += strIndent(indent) + L"Integer = " + toWstring(((IntegerValue*)value)->value) + L"\n";
        typeName = "IntegerValue";
        break;
    case Value::ValueKind::Float: 
        str += strIndent(indent) + L"Float = " + toWstring(((FloatValue*)value)->value) + L"\n"; 
        typeName = "FloatValue";
        break;
    case Value::ValueKind::String: 
        str += strIndent(indent) + L"String = " + toWstring(((StringValue*)value)->value) + L"\n"; 
        typeName = "StringValue";
        break;
    case Value::ValueKind::Empty: 
        str += strIndent(indent) + L"valueKind = Empty\n"; 
        typeName = "Value";
        break;
    case Value::ValueKind::FunctionValue: return str + toWstring((FunctionValue*)value, indent, false, false);
    case Value::ValueKind::Operation: return str + toWstring((Operation*)value, indent, false, false);
    case Value::ValueKind::StaticArray: return str + toWstring((StaticArrayValue*)value, indent, false, false);
    case Value::ValueKind::Variable: return str + toWstring((Variable*)value, indent, false, false);
    }
    return str + strIndent(indent-INDENT_SIZE) + L"} #" + toWstring(typeName);
}
wstring toWstring(FunctionValue* value, int indent, bool isStart, bool firstCall) {
    if (firstCall) {
        return toWstring((Statement*)value, indent, isStart);
    }
    wstring str = L"";
    str += strIndent(indent) + L"argumentNames = " + toWstring(value->argumentNames) + L"\n";
    str += strIndent(indent) + L"body = " + toWstring(&value->body, indent, false) + L"\n";
    return str + strIndent(indent-INDENT_SIZE) + L"} #FunctionValue";
}
wstring toWstring(Operation* operation, int indent, bool isStart, bool firstCall) {
    if (firstCall) {
        return toWstring((Statement*)operation, indent, isStart);
    }
    wstring str = L"";
    return str + strIndent(indent-INDENT_SIZE) + L"} #Operation";
}
wstring toWstring(StaticArrayValue* value, int indent, bool isStart, bool firstCall) {
    if (firstCall) {
        return toWstring((Statement*)value, indent, isStart);
    }
    wstring str = L"";
    str += strIndent(indent) + L"values = " + toWstring(value->values, indent) + L"\n";
    return str + strIndent(indent-INDENT_SIZE) + L"} #StaticArrayValue";
}
wstring toWstring(Variable* value, int indent, bool isStart, bool firstCall) {
    if (firstCall) {
        return toWstring((Statement*)value, indent, isStart);
    }
    wstring str = L"";
    str += strIndent(indent) + L"isConst = " + toWstring(value->isConst) + L"\n";
    str += strIndent(indent) + L"name = " + toWstring(value->name) + L"\n";
    return str + strIndent(indent-INDENT_SIZE) + L"} #Variable";
}

bool operator==(const SourceStringLine& lhs, const SourceStringLine& rhs) {
    return lhs.value == rhs.value && lhs.number == rhs.number && lhs.file == rhs.file;
}
bool operator==(const Token& lhs, const Token& rhs) {
    return lhs.type == rhs.type 
        && lhs.codePosition.lineNumber == rhs.codePosition.lineNumber 
        && lhs.codePosition.charNumber == rhs.codePosition.charNumber 
        && lhs.value == rhs.value 
        && lhs.codePosition.fileInfo == rhs.codePosition.fileInfo;
}
bool operator==(const unique_ptr<Value>& lhs, const unique_ptr<Value>& rhs) {
    return *lhs == *rhs;
}
template<typename T> bool operator==(const vector<T>& lhs, const vector<T>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (int i = 0; i < lhs.size(); ++i) {
        if (!(lhs[i] == rhs[i])) {
            return false;
        }
    }
    return true;
}

namespace Microsoft::VisualStudio::CppUnitTestFramework {
    template<> wstring ToString<vector<SourceStringLine>>(const vector<SourceStringLine>& vec) {
        return toWstringNewLines(vec);
    }
    template<> wstring ToString<vector<Token>>(const vector<Token>& vec) {
        return toWstringNewLines(vec);
    }
    template<> wstring ToString<unordered_set<string>>(const unordered_set<string>& set) {
        return toWstringNewLines(set);
    }
    template<> wstring ToString<unique_ptr<Value>>(const unique_ptr<Value>& value) {
        return toWstring(value);
    }
}

namespace Parsing {	
    string randomString = "fao478qt4ovywfubdao8q4ygfuaualsdfkasd";

	TEST_CLASS(GetSourceFromFile) {
	public:
		TEST_METHOD(fileDoesntExist) {
            GVARS.clear();
            auto result = getSourceFromFile(randomString);
		    Assert::IsFalse(result.has_value(), L"result doesn't have value");
        }

        TEST_METHOD(emptyFile) {
            GVARS.clear();
            ofstream newFile(randomString);
            newFile.close();
            unordered_set<string> includedFiles;
            auto result = getSourceFromFile(randomString);
            remove(randomString.c_str());

            Assert::IsTrue(result.has_value(), L"result has value");
            Assert::IsTrue(result.value().empty(), L"result is empty");
        }

        TEST_METHOD(fewLinesNoInclude) {
            GVARS.clear();
            vector<string> fileContent = {
                "// some commented line",
                "#notInclude",
                "",
                "x #include file"
            };

            ofstream newFile(randomString);
            for (auto sourceLine : fileContent) {
                newFile << sourceLine << "\n";
            }
            newFile.close();
            unordered_set<string> includedFiles;
            auto sourceCode = getSourceFromFile(randomString);
            remove(randomString.c_str());

            vector<SourceStringLine> expected;
            for (int i = 0; i < fileContent.size(); ++i) {
                expected.emplace_back(fileContent[i], GVARS.fileInfos[0].get(), i+1);
            }

            Assert::IsTrue(sourceCode.has_value(), L"result has value");
            Assert::AreEqual(expected, sourceCode.value());
        }

        TEST_METHOD(onlyIncludesInFirstFile) {
            GVARS.clear();
            vector<string> file1Content = {
                "file 1 line 0",
                "file 1 line 1",
            };
            vector<string> file2Content = {
                "file 2 line 0"
            };

            string randomString1 = randomString+"1";
            string randomString2 = randomString+"2";
            string randomString3 = randomString+"3";

            ofstream newFile0(randomString);
            ofstream newFile1(randomString1);
            ofstream newFile2(randomString2);
            ofstream newFile3(randomString3);

            newFile0 << "#include " << randomString1 << '\n';
            newFile0 << "#include " << randomString2 << '\n';

            newFile1 << file1Content[0] << '\n';
            newFile1 << file1Content[1] << '\n';
            newFile2 << file2Content[0] << '\n';

            newFile0.close();
            newFile1.close();
            newFile2.close();
            newFile3.close();

            unordered_set<string> includedFiles;
            auto result = getSourceFromFile(randomString);

            remove(randomString.c_str());
            remove((randomString1).c_str());
            remove((randomString2).c_str());
            remove((randomString3).c_str());

            vector<SourceStringLine> expected;
            for (int i = 0; i < file1Content.size(); ++i) {
                expected.emplace_back(file1Content[i], GVARS.fileInfos[1].get(), i+1);
            }
            for (int i = 0; i < file2Content.size(); ++i) {
                expected.emplace_back(file2Content[i], GVARS.fileInfos[2].get(), i+1);
            }

            Assert::IsTrue(result.has_value(), L"result has value");
            Assert::AreEqual(expected, result.value());
        }

        TEST_METHOD(recursiveInclude) {
            GVARS.clear();
            vector<SourceStringLine> expectedSource = {};
            unordered_set<string> expectedIncludedFiles = {
                {randomString}
            };

            ofstream newFile(randomString);
            newFile.close();
            auto sourceCode = getSourceFromFile(randomString);
            remove(randomString.c_str());

            Assert::IsTrue(sourceCode.has_value(), L"result has value");
            Assert::AreEqual(expectedSource, sourceCode.value());
            Assert::AreEqual(GVARS.fileInfos.size(), (size_t)1);
        }


	};


    TEST_CLASS(CreateTokens) {
    public:
        TEST_METHOD(emptyCode) {
            GVARS.clear();
            vector<SourceStringLine> sourceCode = {};
            auto tokens = createTokens(sourceCode);
            Assert::IsTrue(tokens.has_value() && tokens.value().empty());
        }

        TEST_METHOD(justLabels) {
            GVARS.clear();
            FileInfo fileInfo("fileName");
            vector<SourceStringLine> sourceCode = {
                {"label1 label_2",   &fileInfo, 1},
                {"   label3 lAbEl4", &fileInfo, 2},
                {"\tlabel5",         &fileInfo, 3}
            };
            vector<Token> expected = {
                {Token::Type::Label, "label1",  1, 1,  &fileInfo},
                {Token::Type::Label, "label_2", 1, 8,  &fileInfo},
                {Token::Type::Label, "label3",  2, 4,  &fileInfo},
                {Token::Type::Label, "lAbEl4",  2, 11, &fileInfo},
                {Token::Type::Label, "label5",  3, 2,  &fileInfo}
            };

            auto tokens = createTokens(sourceCode);
            Assert::IsTrue(tokens.has_value(), L"result has value");
            Assert::AreEqual(expected, tokens.value());
        }

        TEST_METHOD(justNumbers) {
            GVARS.clear();
            FileInfo fileInfo("fileName");
            vector<SourceStringLine> sourceCode = {
                {"3 4.0",  &fileInfo, 1},
                {"251.53", &fileInfo, 2},
                {"   0",   &fileInfo, 3}
            };
            vector<Token> expected = {
                {Token::Type::Integer, "3",      1, 1, &fileInfo},
                {Token::Type::Float,   "4.0",    1, 3, &fileInfo},
                {Token::Type::Float,   "251.53", 2, 1, &fileInfo},
                {Token::Type::Integer, "0",      3, 4, &fileInfo},
            };

            auto tokens = createTokens(sourceCode);
            Assert::IsTrue(tokens.has_value(), L"result has value");
            Assert::AreEqual(expected, tokens.value());
        }

        TEST_METHOD(incorrectNumberMultipleDots) {
            GVARS.clear();
            FileInfo fileInfo("fileName");
            vector<SourceStringLine> sourceCode = {
                {"3.4.0",  &fileInfo, 1}
            };

            auto tokens = createTokens(sourceCode);
            Assert::IsFalse(tokens.has_value(), L"result has no value");
        }

        TEST_METHOD(justCharsAndStringLiterals) {
            GVARS.clear();
            FileInfo fileInfo("fileName");
            vector<SourceStringLine> sourceCode = {
                {"\"str ing\"",                  &fileInfo, 1},
                {"'c'",                          &fileInfo, 2},
                {"  \"str\"'1''2'\"3\" '4'\t\t", &fileInfo, 3},
                {"\"'char'?\"",                  &fileInfo, 4}
            };
            vector<Token> expected = {
                {Token::Type::StringLiteral, "str ing", 1, 1, &fileInfo},
                {Token::Type::Char,          "c",       2, 1, &fileInfo},
                {Token::Type::StringLiteral, "str",     3, 3, &fileInfo},
                {Token::Type::Char,          "1",       3, 8, &fileInfo},
                {Token::Type::Char,          "2",       3, 11, &fileInfo},
                {Token::Type::StringLiteral, "3",       3, 14, &fileInfo},
                {Token::Type::Char,          "4",       3, 18, &fileInfo},
                {Token::Type::StringLiteral, "'char'?", 4, 1, &fileInfo}
            };

            auto tokens = createTokens(sourceCode);
            Assert::IsTrue(tokens.has_value(), L"result has value");
            Assert::AreEqual(expected, tokens.value());
        }

        TEST_METHOD(justSymbols) {
            GVARS.clear();
            FileInfo fileInfo("fileName");
            vector<SourceStringLine> sourceCode = {
                {"{ <\t\t%\t! }", &fileInfo, 1},
            };
            vector<Token> expected = {
                {Token::Type::Symbol, "{", 1, 1,  &fileInfo},
                {Token::Type::Symbol, "<", 1, 3,  &fileInfo},
                {Token::Type::Symbol, "%", 1, 6,  &fileInfo},
                {Token::Type::Symbol, "!", 1, 8,  &fileInfo},
                {Token::Type::Symbol, "}", 1, 10, &fileInfo},
            };

            auto tokens = createTokens(sourceCode);
            Assert::IsTrue(tokens.has_value(), L"result has value");
            Assert::AreEqual(expected, tokens.value());
        }

        TEST_METHOD(comments) {
            GVARS.clear();
            FileInfo fileInfo("fileName");
            vector<SourceStringLine> sourceCode = {
                {"label1 //label2", &fileInfo, 1},
                {"a /*/ */",         &fileInfo, 2},
            };
            vector<Token> expected = {
                {Token::Type::Label, "label1", 1, 1,  &fileInfo},
                {Token::Type::Label, "a",      2, 1,  &fileInfo},
            };

            auto tokens = createTokens(sourceCode);
            Assert::IsTrue(tokens.has_value(), L"result has value");
            Assert::AreEqual(expected, tokens.value());
        }

        TEST_METHOD(nestedComments) {
            GVARS.clear();
            FileInfo fileInfo("fileName");
            vector<SourceStringLine> sourceCode = {
                {"a/*/**/",  &fileInfo, 1},
                {"b*//*",    &fileInfo, 2},
                {"/*c/*",    &fileInfo, 3},
                {"*/ */*/d", &fileInfo, 4},
            };
            vector<Token> expected = {
                {Token::Type::Label, "a", 1, 1, &fileInfo},
                {Token::Type::Label, "d", 4, 8, &fileInfo},
            };

            auto tokens = createTokens(sourceCode);
            Assert::IsTrue(tokens.has_value(), L"result has value");
            Assert::AreEqual(expected, tokens.value());
        }

        TEST_METHOD(incorrectNestedComments) {
            GVARS.clear();
            FileInfo fileInfo("fileName");
            vector<SourceStringLine> sourceCode = {
                {"a/*/**/", &fileInfo, 1},
                {"b*//*",   &fileInfo, 2},
                {"/*c/*",   &fileInfo, 3},
                {"*/ */d",  &fileInfo, 4},
            };

            auto tokens = createTokens(sourceCode);
            Assert::IsFalse(tokens.has_value(), L"result has no value");
        }
    };
}

namespace interpreting {
    TEST_CLASS(SolveReversePolishNotation) {
    public:
        TEST_METHOD(singleIntegerValue) {
            CodePosition codePosition(nullptr, 0, 0);
            vector<vector<unique_ptr<Value>>> values(2); // 0 -> for function; 1 -> for checking

            for (int i = 0; i < 2; ++i) {
                values[i].push_back(make_unique<IntegerValue>(codePosition, 3));
            }

            unique_ptr<Value> outValue = solveReversePolishNotation(move(values[0]));
            Assert::AreEqual(values[1][0], outValue);
        }
        TEST_METHOD(singleStaticArrayFloatValue) {
            CodePosition codePosition(nullptr, 0, 0);
            vector<vector<unique_ptr<Value>>> values(2); // 0 -> for function; 1 -> for checking

            for (int i = 0; i < 2; ++i) {
                auto arrayValue = make_unique<StaticArrayValue>(codePosition);
                arrayValue->values.push_back(make_unique<FloatValue>(codePosition, 2.5));
                arrayValue->values.push_back(make_unique<FloatValue>(codePosition, 1));
                values[i].push_back(move(arrayValue));
            }

            unique_ptr<Value> outValue = solveReversePolishNotation(move(values[0]));
            Assert::AreEqual(values[1][0], outValue);
        }
        TEST_METHOD(singleFunctionValue) {
            CodePosition codePosition(nullptr, 0, 0);
            vector<vector<unique_ptr<Value>>> values(2); // 0 -> for function; 1 -> for checking

            for (int i = 0; i < 2; ++i) {
                auto functionType = make_unique<FunctionType>();
                functionType->returnType = make_unique<Type>(Type::Kind::Void);
                functionType->argumentTypes.push_back(make_unique<IntegerType>(IntegerType::Size::U16));
                functionType->argumentTypes.push_back(make_unique<FloatType>(FloatType::Size::F32));
                auto functionValue = make_unique<FunctionValue>(codePosition, move(functionType), nullptr);
                functionValue->argumentNames.push_back("a");
                functionValue->argumentNames.push_back("arg2");
                values[i].push_back(move(functionValue));
            }

            unique_ptr<Value> outValue = solveReversePolishNotation(move(values[0]));
            Assert::AreEqual(values[1][0], outValue);
        }
    };
}
