#include "stdafx.h"
#include "CppUnitTest.h"

#include <string>
#include <fstream>
#include <optional>
#include "../C$/parsing.cpp"
#include "../C$/Scope.cpp"
#include "../C$/Type.cpp"
#include "../C$/Statement.cpp"
#include "../C$/errorMessages.cpp"
#include "../C$/keywords.cpp"
#include "../C$/Value.cpp"
#include "../C$/Operation.cpp"
#include "../C$/Declaration.cpp"
#include "../C$/globalVariables.cpp"
#include "../C$/DeclarationMap.cpp"
#include "../C$/ClassDeclaration.cpp"
#include "../C$/ClassDeclarationMap.cpp"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std;

int INDENT_SIZE = 4;

wstring toWstring(Type* type, int indent=0, bool isStart=true, bool firstCall=true);
wstring toWstring(Statement* statement, int indent=0, bool isStart=true, bool firstCall=true);
wstring toWstring(Declaration* declaration, int indent=0, bool isStart=true, bool firstCall=true);
wstring toWstring(Scope* scope, int indent=0, bool isStart=true, bool firstCall=true);
wstring toWstring(CodeScope* scope, int indent=0, bool isStart=true, bool firstCall=true);
wstring toWstring(ClassScope* scope, int indent=0, bool isStart=true, bool firstCall=true);
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
wstring toWstring(uint64_t i) {
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
    if (!type) {
        return L"nullptr";
    }
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
            str += toWstring(functionType->argumentTypes[i], indent, false);
            if (i != functionType->argumentTypes.size()-1) str += L",";
        }
        return str + L")->" + toWstring(functionType->returnType, indent, false);
    }
    case Type::Kind::RawPointer:
        return L"*" + toWstring(((RawPointerType*)(type))->underlyingType, indent, false);
    case Type::Kind::OwnerPointer:
        return L"!" + toWstring(((OwnerPointerType*)(type))->underlyingType, indent, false);
    case Type::Kind::Reference:
        return L"&" + toWstring(((ReferenceType*)(type))->underlyingType, indent, false);
    case Type::Kind::MaybeError:
        return L"?" + toWstring(((MaybeErrorType*)(type))->underlyingType, indent, false);
    case Type::Kind::ArrayView:
        return L"[*]" + toWstring(((ArrayViewType*)(type))->elementType, indent, false);
    case Type::Kind::DynamicArray:
        return L"[]" + toWstring(((DynamicArrayType*)(type))->elementType, indent, false);
    case Type::Kind::StaticArray:
        return L"[N]" + toWstring(((StaticArrayType*)(type))->elementType, indent, false);
    case Type::Kind::Template:
        return L"T(" + toWstring(((TemplateType*)(type))->name) + L")";
    case Type::Kind::TemplateFunction:{
        auto functionType = (TemplateFunctionType*)type;
        wstring str = L"<";
        for (int i = 0; i < functionType->templateTypes.size(); ++i) {
            str += toWstring(functionType->templateTypes[i]->name);
            if (i != functionType->templateTypes.size()-1) str += L",";
        }
        str += L">(";
        for (int i = 0; i < functionType->argumentTypes.size(); ++i) {
            str += toWstring(functionType->argumentTypes[i], indent, false);
            if (i != functionType->argumentTypes.size()-1) str += L",";
        }
        return str + L")->" + toWstring(functionType->returnType, indent, false);
    }
    case Type::Kind::Class:{
        ClassType* classType = (ClassType*)(type);
        wstring str = L"C(" + toWstring(((ClassType*)(type))->name) + L")";
        if (classType->templateTypes.size() > 0) {
            str += L"<";
            for (int i = 0; i < classType->templateTypes.size(); ++i) {
                str += toWstring(classType->templateTypes[i], indent, false);
                if (i != classType->templateTypes.size()-1) str += L",";
            }
            str += L">";
        }
        return str;
    }
    case Type::Kind::TemplateClass:
        return L"TemplateClass";
    }
}
wstring toWstring(Statement* statement, int indent, bool isStart, bool firstCall) {
    if (!statement) {
        return L"nullptr";
    }
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
    if (!declaration) {
        return L"nullptr";
    }
    if (firstCall) {
        return toWstring((Statement*)declaration, indent, isStart);
    }
    wstring str = L"";
    str += strIndent(indent) + L"variable = " + toWstring(declaration->variable, indent, false) + L"\n";
    str += strIndent(indent) + L"value = " + toWstring(declaration->value, indent, false) + L"\n";
    return str + strIndent(indent-INDENT_SIZE) + L"} #Declaration";
}
wstring toWstring(Scope* scope, int indent, bool isStart, bool firstCall) {
    if (!scope) {
        return L"nullptr";
    }
    if (firstCall) {
        return toWstring((Statement*)scope, indent, isStart);
    }
    wstring str = L"";
    str += strIndent(indent) + L"parentScope = " + toWstring((int)scope->parentScope) + L"\n";
    if (scope->owner == Scope::Owner::Class){
        return str + toWstring((ClassScope*)scope, indent, false, false);
    } else {
        return str + toWstring((CodeScope*)scope, indent, false, false);
    }
}
wstring toWstring(const ForEachData* forEachData, int indent=0) {
    if (!forEachData) {
        return L"nullptr";
    }
    wstring str = L"";
    str += L"ForEachData {\n";
    indent += INDENT_SIZE;
    str += strIndent(indent) + L"arrayValue = " + toWstring(forEachData->arrayValue, indent, false) + L"\n";
    str += strIndent(indent) + L"index = " + toWstring(forEachData->index, indent, false) + L"\n";
    str += strIndent(indent) + L"it = " + toWstring(forEachData->it, indent, false) + L"\n";
    return str + strIndent(indent-INDENT_SIZE) + L"}";
}
wstring toWstring(const ForIterData* forIterData, int indent=0) {
    if (!forIterData) {
        return L"nullptr";
    }
    wstring str = L"";
    str += L"ForIterData {\n";
    indent += INDENT_SIZE;
    str += strIndent(indent) + L"firstValue = " + toWstring(forIterData->firstValue, indent, false) + L"\n";
    str += strIndent(indent) + L"step = " + toWstring(forIterData->step, indent, false) + L"\n";
    str += strIndent(indent) + L"lastValue = " + toWstring(forIterData->lastValue, indent, false) + L"\n";
    str += strIndent(indent) + L"iterVariable = " + toWstring(forIterData->iterVariable, indent, false) + L"\n";
    return str + strIndent(indent-INDENT_SIZE) + L"}";
}
wstring toWstring(CodeScope* scope, int indent, bool isStart, bool firstCall) {
    if (!scope) {
        return L"nullptr";
    }
    if (firstCall) {
        return toWstring((Statement*)scope, indent, isStart);
    }
    wstring str = L"";
    str += strIndent(indent) + L"statements = " + toWstring(scope->statements, indent) + L"\n";
    switch (scope->owner) {
    case Scope::Owner::Defer: return str + strIndent(indent-INDENT_SIZE) + L"} #Defer";
    case Scope::Owner::Else: return str + strIndent(indent-INDENT_SIZE) + L"} #Else";
    case Scope::Owner::None:
        str += strIndent(indent) + L"owner = None\n";
        break;
    case Scope::Owner::Function:
        str += strIndent(indent) + L"owner = Function\n";
        break;
    case Scope::Owner::If:
        str += strIndent(indent) + L"condition = " + toWstring(((IfScope*)(scope))->conditionExpression, indent, false) + L"\n";
        str += strIndent(indent) + L"elseScope = " + toWstring(((IfScope*)(scope))->elseScope, indent) + L"\n";
        return str + strIndent(indent-INDENT_SIZE) + L"} #If";
    case Scope::Owner::While:
        str += strIndent(indent) + L"condition = " + toWstring(((WhileScope*)(scope))->conditionExpression, indent, false) + L"\n";
        return str + strIndent(indent-INDENT_SIZE) + L"} #While";
    case Scope::Owner::For:
        const auto& data = ((ForScope*)(scope))->data;
        if (holds_alternative<ForEachData>(data)) {
            str += strIndent(indent) + L"data = " + toWstring(&get<ForEachData>(data), indent) + L"\n";
        } else {
            str += strIndent(indent) + L"data = " + toWstring(&get<ForIterData>(data), indent) + L"\n";
        }
        return str + strIndent(indent-INDENT_SIZE) + L"} #For";
    }

    return str + strIndent(indent-INDENT_SIZE) + L"} #Scope";
}
wstring toWstring(ClassScope* scope, int indent, bool isStart, bool firstCall) {
    if (!scope) {
        return L"nullptr";
    }
    if (firstCall) {
        return toWstring((Statement*)scope, indent, isStart);
    }
    wstring str = L"";
    /*
    str += strIndent(indent) + L"name = " + toWstring(scope->) + L"\n";
    str += strIndent(indent) + L"templateTypes = " + toWstring(scope->templateTypes, indent) + L"\n";
    */
    str += strIndent(indent) + L"declarations = " + toWstring(scope->declarations, indent) + L"\n";
    return str + strIndent(indent-INDENT_SIZE) + L"} #ClassScope";
}
wstring toWstring(Value* value, int indent, bool isStart, bool firstCall) {
    if (!value) {
        return L"nullptr";
    }
    if (firstCall) {
        return toWstring((Statement*)value, indent, isStart);
    }
    wstring str = L"";
    str += strIndent(indent) + L"isConstexpr = " + toWstring(value->isConstexpr) + L"\n";
    str += strIndent(indent) + L"type = " + toWstring(value->type, indent, false) + L"\n";

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
    if (!value) {
        return L"nullptr";
    }
    if (firstCall) {
        return toWstring((Statement*)value, indent, isStart);
    }
    wstring str = L"";
    str += strIndent(indent) + L"arguments = " + toWstring(value->arguments, indent) + L"\n";
    str += strIndent(indent) + L"body = " + toWstring(value->body, indent, false) + L"\n";
    return str + strIndent(indent-INDENT_SIZE) + L"} #FunctionValue";
}
wstring toWstring(Operation* operation, int indent, bool isStart, bool firstCall) {
    if (!operation) {
        return L"nullptr";
    }
    if (firstCall) {
        return toWstring((Statement*)operation, indent, isStart);
    }
    wstring str = L"";
    str += strIndent(indent) + L"arguments = " + toWstring(operation->arguments, indent) + L"\n";
    if (operation->kind == Operation::Kind::Cast) {
        str += strIndent(indent) + L"argType = " + toWstring(((CastOperation*)(operation))->argType, indent, false) + L"\n";
        return str + strIndent(indent-INDENT_SIZE) + L"} #CastOperation";
    } else if (operation->kind == Operation::Kind::ArrayIndex) {
        str += strIndent(indent) + L"index = " + toWstring(((ArrayIndexOperation*)(operation))->index, indent, false) + L"\n";
        return str + strIndent(indent-INDENT_SIZE) + L"} #ArrayIndexOperation";
    } else if (operation->kind == Operation::Kind::ArraySubArray) {
        str += strIndent(indent) + L"firstIndex = " + toWstring(((ArraySubArrayOperation*)(operation))->firstIndex, indent, false) + L"\n";
        str += strIndent(indent) + L"secondIndex = " + toWstring(((ArraySubArrayOperation*)(operation))->secondIndex, indent, false) + L"\n";
        return str + strIndent(indent-INDENT_SIZE) + L"} #ArraySubArrayOperation";
    } else if (operation->kind == Operation::Kind::FunctionCall) {
        str += strIndent(indent) + L"function = " + toWstring(((FunctionCallOperation*)(operation))->function) + L"\n";
        return str + strIndent(indent-INDENT_SIZE) + L"} #FunctionCallOperation";
    } else if (operation->kind == Operation::Kind::TemplateFunctionCall) {
        str += strIndent(indent) + L"function = " + toWstring(((TemplateFunctionCallOperation*)(operation))->function) + L"\n";
        str += strIndent(indent) + L"templateTypes = " + toWstring(((TemplateFunctionCallOperation*)(operation))->templateTypes, indent) + L"\n";
        return str + strIndent(indent-INDENT_SIZE) + L"} #TemplateFunctionCallOperation";
    } else {
        str += strIndent(indent) + L"operand = ";
        switch(operation->kind){
        case Operation::Kind::Dot: str += L"."; break;
        case Operation::Kind::FunctionCall: str += L"function call"; break;
        case Operation::Kind::ArrayIndex: str += L"[x]"; break;
        case Operation::Kind::ArraySubArray: str += L"[x:y]"; break;
        case Operation::Kind::Address: str += L"@"; break;
        case Operation::Kind::GetValue: str += L"$"; break;
        case Operation::Kind::Allocation: str += L"alloc"; break;
        case Operation::Kind::Deallocation: str += L"dealloc"; break;
        case Operation::Kind::Cast: str += L"[type]"; break;
        case Operation::Kind::BitNeg: str += L"~"; break;
        case Operation::Kind::LogicalNot: str += L"!"; break;
        case Operation::Kind::Minus: str += L"- (unary)"; break;
        case Operation::Kind::Mul: str += L"*"; break;
        case Operation::Kind::Div: str += L"/"; break;
        case Operation::Kind::Mod: str += L"%"; break;
        case Operation::Kind::Add: str += L"+"; break;
        case Operation::Kind::Sub: str += L"-"; break;
        case Operation::Kind::Shl: str += L"<<"; break;
        case Operation::Kind::Shr: str += L">>"; break;
        case Operation::Kind::Sal: str += L"<<<"; break;
        case Operation::Kind::Sar: str += L">>>"; break;
        case Operation::Kind::Gt: str += L">"; break;
        case Operation::Kind::Lt: str += L"<"; break;
        case Operation::Kind::Gte: str += L">="; break;
        case Operation::Kind::Lte: str += L"<="; break;
        case Operation::Kind::Eq: str += L"=="; break;
        case Operation::Kind::Neq: str += L"!="; break;
        case Operation::Kind::BitAnd: str += L"& (bit And)"; break;
        case Operation::Kind::BitXor: str += L"^"; break;
        case Operation::Kind::BitOr: str += L"|"; break;
        case Operation::Kind::LogicalAnd: str += L"&&"; break;
        case Operation::Kind::LogicalOr: str += L"||"; break;
        case Operation::Kind::Assign: str += L"="; break;
        case Operation::Kind::AddAssign: str += L"+="; break;
        case Operation::Kind::SubAssign: str += L"-="; break;
        case Operation::Kind::MulAssign: str += L"*="; break;
        case Operation::Kind::DivAssign: str += L"/="; break;
        case Operation::Kind::ModAssign: str += L"%="; break;
        case Operation::Kind::ShlAssign: str += L"<<="; break;
        case Operation::Kind::ShrAssign: str += L">>="; break;
        case Operation::Kind::SalAssign: str += L"<<<="; break;
        case Operation::Kind::SarAssign: str += L">>>="; break;
        case Operation::Kind::BitNegAssign: str += L"~="; break;
        case Operation::Kind::BitOrAssign: str += L"|="; break;
        case Operation::Kind::BitXorAssign: str += L"^="; break;
        }
        return str + L"\n" + strIndent(indent-INDENT_SIZE) + L"} #Operation";
    }
}
wstring toWstring(StaticArrayValue* value, int indent, bool isStart, bool firstCall) {
    if (!value) {
        return L"nullptr";
    }
    if (firstCall) {
        return toWstring((Statement*)value, indent, isStart);
    }
    wstring str = L"";
    str += strIndent(indent) + L"values = " + toWstring(value->values, indent) + L"\n";
    return str + strIndent(indent-INDENT_SIZE) + L"} #StaticArrayValue";
}
wstring toWstring(Variable* value, int indent, bool isStart, bool firstCall) {
    if (!value) {
        return L"nullptr";
    }
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
    template<> wstring ToString<Value>(Value* value) {
        return toWstring(value);
    }
    template<> wstring ToString<vector<Value*>>(const vector<Value*>& value) {
        return toWstring(value);
    }
    template<> wstring ToString<Type>(Type* type) {
        return toWstring(type);
    }
    template<> wstring ToString<Statement>(Statement* statement) {
        return toWstring(statement);
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

template<typename T> void AssertAreEqual(T* expected, T* actual) {
    if (!cmpPtr(expected, actual)) {
        Assert::AreEqual(expected, actual);
    }
}

namespace codeTreeCreating {
    TEST_CLASS(SolveReversePolishNotation) {
    public:
        TEST_METHOD(singleIntegerValue) {
            CodePosition codePosition(nullptr, 0, 0);
            vector<vector<Value*>> values(2); // 0 -> for function; 1 -> for checking

            for (int i = 0; i < 2; ++i) {
                values[i].push_back(IntegerValue::Create(codePosition, 3));
            }

            auto outValue = solveReversePolishNotation(values[0]);
            AssertAreEqual(values[1][0], outValue);
        }
        TEST_METHOD(singleStaticArrayFloatValue) {
            CodePosition codePosition(nullptr, 0, 0);
            vector<vector<Value*>> values(2); // 0 -> for function; 1 -> for checking

            for (int i = 0; i < 2; ++i) {
                auto arrayValue = StaticArrayValue::Create(codePosition);
                arrayValue->values.push_back(FloatValue::Create(codePosition, 2.5));
                arrayValue->values.push_back(FloatValue::Create(codePosition, 1));
                values[i].push_back(arrayValue);
            }

            auto outValue = solveReversePolishNotation(values[0]);
            AssertAreEqual(values[1][0], outValue);
        }
        TEST_METHOD(singleFunctionValue) {
            CodePosition codePosition(nullptr, 0, 0);
            vector<vector<Value*>> values(2); // 0 -> for function; 1 -> for checking

            for (int i = 0; i < 2; ++i) {
                auto functionType = FunctionType::Create();
                functionType->returnType = Type::Create(Type::Kind::Void);
                functionType->argumentTypes.push_back(IntegerType::Create(IntegerType::Size::U16));
                functionType->argumentTypes.push_back(FloatType::Create(FloatType::Size::F32));
                auto functionValue = FunctionValue::Create(codePosition, functionType, nullptr);
                functionValue->arguments.push_back(Declaration::Create(codePosition));
                functionValue->arguments.back()->variable->name = "a";
                functionValue->arguments.push_back(Declaration::Create(codePosition));
                functionValue->arguments.back()->variable->name = "arg2";
                values[i].push_back(functionValue);
            }

            auto outValue = solveReversePolishNotation(values[0]);
            AssertAreEqual(values[1][0], outValue);
        }
        TEST_METHOD(adding2Integers) {
            // 2 + 3  --->  2 3 +
            CodePosition codePosition(nullptr, 0, 0);

            vector<Value*> values;
            values.push_back(IntegerValue::Create(codePosition, 2));
            values.push_back(IntegerValue::Create(codePosition, 3));
            values.push_back(Operation::Create(codePosition, Operation::Kind::Add));

            auto expectedOperation = Operation::Create(codePosition, Operation::Kind::Add);
            expectedOperation->arguments.push_back(IntegerValue::Create(codePosition, 2));
            expectedOperation->arguments.push_back(IntegerValue::Create(codePosition, 3));
            Value* expected = expectedOperation;

            Value* outValue = solveReversePolishNotation(values);
            AssertAreEqual(expected, outValue);
        }
        TEST_METHOD(getAddressOfVariable) {
            // @var  --->  var @
            CodePosition codePosition(nullptr, 0, 0);

            vector<Value*> values;
            values.push_back(Variable::Create(codePosition));
            ((Variable*)(values.back()))->name = "var";
            values.push_back(Operation::Create(codePosition, Operation::Kind::Address));

            auto expectedOperation = Operation::Create(codePosition, Operation::Kind::Address);
            expectedOperation->arguments.push_back(Variable::Create(codePosition));
            ((Variable*)(expectedOperation->arguments.back()))->name = "var";
            Value* expected = expectedOperation;

            Value* outValue = solveReversePolishNotation(values);
            AssertAreEqual(expected, outValue);
        }
        /*TEST_METHOD(getReferenceOfArrayElement) {
            // &tab[2]  --->  tab [2] &
            CodePosition codePosition(nullptr, 0, 0);

            vector<Value*> values;
            values.push_back(Variable::Create(codePosition));
            ((Variable*)(values.back()))->name = "tab";
            values.push_back(ArrayIndexOperation::Create(codePosition, IntegerValue::Create(codePosition, 2)));
            values.push_back(Operation::Create(codePosition, Operation::Kind::Reference));

            auto expectedOperation = Operation::Create(codePosition, Operation::Kind::Reference);
            expectedOperation->arguments.push_back(ArrayIndexOperation::Create(codePosition, IntegerValue::Create(codePosition, 2)));
            auto variable = Variable::Create(codePosition);
            variable->name = "tab";
            ((Operation*)(expectedOperation->arguments.back()))->arguments.push_back(variable);
            Value* expected = expectedOperation;

            Value* outValue = solveReversePolishNotation(values);
            AssertAreEqual(expected, outValue);
        }*/
    };
    TEST_CLASS(GetReversePolishNotation) {
    public:
        TEST_METHOD(onlySemicolon) {
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol, ";", 1, 1, &fileInfo},
            };
            int i = 0;

            vector<Value*> expected = {};
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getReversePolishNotation(tokens, i);

            Assert::AreEqual(expected, actual.value());
            Assert::AreEqual(i, 0);
        }
        TEST_METHOD(singleFloatValue) {
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Float, "2.5", 1, 1, &fileInfo},
                {Token::Type::Symbol, ";",  1, 4, &fileInfo},
            };
            int i = 0;

            vector<Value*> expected;
            expected.push_back(FloatValue::Create(tokens[0].codePosition, 2.5));

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getReversePolishNotation(tokens, i);

            Assert::AreEqual(expected, actual.value());
            Assert::AreEqual(i, 1);
        }
        TEST_METHOD(castOperation) {
            //[i8]y;
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol, "[", 1, 1, &fileInfo},
                {Token::Type::Label, "i8", 1, 2, &fileInfo},
                {Token::Type::Symbol, "]", 1, 4, &fileInfo},
                {Token::Type::Label,  "y", 1, 5, &fileInfo},
                {Token::Type::Symbol, ";", 1, 6, &fileInfo},
            };
            int i = 0;

            vector<Value*> expected;
            auto castOperation = CastOperation::Create(
                tokens[0].codePosition,
                IntegerType::Create(IntegerType::Size::I8)
            );
            expected.push_back(Variable::Create(tokens[3].codePosition, "y"));
            expected.push_back(castOperation);

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getReversePolishNotation(tokens, i);

            Assert::IsTrue(actual.has_value(), L"has value");
            Assert::AreEqual(expected, actual.value());
            Assert::AreEqual(i, 4);
        }
        TEST_METHOD(arrayValueSingleIntegerElement) {
            //[7];
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol, "[", 1, 1, &fileInfo},
                {Token::Type::Integer,"7", 1, 2, &fileInfo},
                {Token::Type::Symbol, "]", 1, 3, &fileInfo},
                {Token::Type::Symbol, ";", 1, 4, &fileInfo},
            };
            int i = 0;

            vector<Value*> expected;
            auto arrayValue = StaticArrayValue::Create(tokens[0].codePosition);
            arrayValue->values.push_back(IntegerValue::Create(tokens[1].codePosition, 7));
            expected.push_back(arrayValue);

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getReversePolishNotation(tokens, i);

            Assert::IsTrue(actual.has_value(), L"has value");
            Assert::AreEqual(expected, actual.value());
            Assert::AreEqual(i, 3);
        }
        TEST_METHOD(arrayValueMultipleDifferentValues) {
            //[f(), [int](2.5)];
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol, "[",  1, 1,  &fileInfo},
                {Token::Type::Label,  "f",  1, 2,  &fileInfo},
                {Token::Type::Symbol, "(",  1, 3,  &fileInfo},
                {Token::Type::Symbol, ")",  1, 4,  &fileInfo},
                {Token::Type::Symbol, ",",  1, 5,  &fileInfo},
                {Token::Type::Symbol, "[",  1, 6,  &fileInfo},
                {Token::Type::Label,  "i8", 1, 7,  &fileInfo},
                {Token::Type::Symbol, "]",  1, 8,  &fileInfo},
                {Token::Type::Symbol, "(",  1, 9,  &fileInfo},
                {Token::Type::Float, "2.5", 1, 10, &fileInfo},
                {Token::Type::Symbol, ")",  1, 11, &fileInfo},
                {Token::Type::Symbol, "]",  1, 12, &fileInfo},
                {Token::Type::Symbol, ";",  1, 13, &fileInfo},
            };
            int i = 0;

            vector<Value*> expected;
            auto arrayValue = StaticArrayValue::Create(tokens[0].codePosition);
            arrayValue->values.push_back(FunctionCallOperation::Create(tokens[2].codePosition));
            ((FunctionCallOperation*)arrayValue->values.back())->function = Variable::Create(tokens[1].codePosition,"f");
            auto castOperation = CastOperation::Create(
                tokens[5].codePosition,
                IntegerType::Create(IntegerType::Size::I8)
            );
            castOperation->arguments.push_back(FloatValue::Create(tokens[9].codePosition, 2.5));
            arrayValue->values.push_back(castOperation);
            expected.push_back(arrayValue);

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getReversePolishNotation(tokens, i);

            Assert::IsTrue(actual.has_value(), L"has value");
            Assert::AreEqual(expected, actual.value());
            Assert::AreEqual(i, 12);
        }
        TEST_METHOD(functionCallNoArguments) {
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label, "fun", 1, 1, &fileInfo},
                {Token::Type::Symbol, "(",  1, 4, &fileInfo},
                {Token::Type::Symbol, ")",  1, 5, &fileInfo},
                {Token::Type::Symbol, ";",  1, 6, &fileInfo},
            };
            int i = 0;

            vector<Value*> expected;
            expected.push_back(Variable::Create(tokens[0].codePosition, "fun"));
            expected.push_back(FunctionCallOperation::Create(tokens[1].codePosition));

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getReversePolishNotation(tokens, i);

            Assert::AreEqual(expected, actual.value());
            Assert::AreEqual(i, 3);
        }
        TEST_METHOD(functionCallWithIntegerArgument) {
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label,   "fun", 1, 1, &fileInfo},
                {Token::Type::Symbol,  "(",   1, 4, &fileInfo},
                {Token::Type::Integer, "27",  1, 5, &fileInfo},
                {Token::Type::Symbol,  ")",   1, 7, &fileInfo},
                {Token::Type::Symbol,  ";",   1, 8, &fileInfo},
            };
            int i = 0;

            vector<Value*> expected;

            auto functionCall = FunctionCallOperation::Create(tokens[1].codePosition);
            functionCall->arguments.push_back(IntegerValue::Create(tokens[2].codePosition, 27));
            expected.push_back(Variable::Create(tokens[0].codePosition, "fun"));
            expected.push_back(functionCall);

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getReversePolishNotation(tokens, i);

            Assert::IsTrue(actual.has_value(), L"has value");
            Assert::AreEqual(expected, actual.value());
            Assert::AreEqual(i, 4);
        }
        TEST_METHOD(functionCallWithIntegerAndVariableArguments) {
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label,   "fun", 1, 1,  &fileInfo},
                {Token::Type::Symbol,  "(",   1, 4,  &fileInfo},
                {Token::Type::Integer, "27",  1, 5,  &fileInfo},
                {Token::Type::Symbol,  ",",   1, 7,  &fileInfo},
                {Token::Type::Label,   "x",   1, 8,  &fileInfo},
                {Token::Type::Symbol,  ")",   1, 9,  &fileInfo},
                {Token::Type::Symbol,  ";",   1, 10, &fileInfo},
            };
            int i = 0;

            vector<Value*> expected;

            auto functionCall = FunctionCallOperation::Create(tokens[1].codePosition);
            functionCall->arguments.push_back(IntegerValue::Create(tokens[2].codePosition, 27));
            functionCall->arguments.push_back(Variable::Create(tokens[4].codePosition, "x"));
            expected.push_back(Variable::Create(tokens[0].codePosition, "fun"));
            expected.push_back(functionCall);

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getReversePolishNotation(tokens, i);

            Assert::IsTrue(actual.has_value(), L"has value");
            Assert::AreEqual(expected, actual.value());
            Assert::AreEqual(i, 6);
        }
        TEST_METHOD(arithmeticOperation1) {
            FileInfo fileInfo("fileName");
            // x %= 27 + 13.25 * y;  -->  x 27 13.25 y * + %=
            vector<Token> tokens = {
                {Token::Type::Label, "x",     1, 1,  &fileInfo},
                {Token::Type::Symbol, "%",    1, 2,  &fileInfo},
                {Token::Type::Symbol, "=",    1, 3,  &fileInfo},
                {Token::Type::Integer, "27",  1, 4,  &fileInfo},
                {Token::Type::Symbol, "+",    1, 6,  &fileInfo},
                {Token::Type::Float, "13.25", 1, 7,  &fileInfo},
                {Token::Type::Symbol, "*",    1, 12, &fileInfo},
                {Token::Type::Label, "y",     1, 13, &fileInfo},
                {Token::Type::Symbol, ";",    1, 14, &fileInfo},
            };
            int i = 0;

            vector<Value*> expected;
            expected.push_back(Variable::Create(tokens[0].codePosition, "x"));
            expected.push_back(IntegerValue::Create(tokens[3].codePosition, 27));
            expected.push_back(FloatValue::Create(tokens[5].codePosition, 13.25));
            expected.push_back(Variable::Create(tokens[7].codePosition, "y"));
            expected.push_back(Operation::Create(tokens[6].codePosition, Operation::Kind::Mul));
            expected.push_back(Operation::Create(tokens[4].codePosition, Operation::Kind::Add));
            expected.push_back(Operation::Create(tokens[1].codePosition, Operation::Kind::ModAssign));

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getReversePolishNotation(tokens, i);

            Assert::IsTrue(actual.has_value(), L"has value");
            Assert::AreEqual(expected, actual.value());
            Assert::AreEqual(i, 8);
        }
        TEST_METHOD(arithmeticOperation2) {
            FileInfo fileInfo("fileName");
            // x <= 27 / 13.25 - -y;  -->  x 27 13.25 / y - - <=
            //                               unarny minus ^
            vector<Token> tokens = {
                {Token::Type::Label, "x",     1, 1,  &fileInfo},
                {Token::Type::Symbol, "<",    1, 2,  &fileInfo},
                {Token::Type::Symbol, "=",    1, 3,  &fileInfo},
                {Token::Type::Integer, "27",  1, 4,  &fileInfo},
                {Token::Type::Symbol, "/",    1, 6,  &fileInfo},
                {Token::Type::Float, "13.25", 1, 7,  &fileInfo},
                {Token::Type::Symbol, "-",    1, 12, &fileInfo},
                {Token::Type::Symbol, "-",    1, 13, &fileInfo},
                {Token::Type::Label, "y",     1, 14, &fileInfo},
                {Token::Type::Symbol, ";",    1, 15, &fileInfo},
            };
            int i = 0;

            vector<Value*> expected;
            expected.push_back(Variable::Create(tokens[0].codePosition, "x"));
            expected.push_back(IntegerValue::Create(tokens[3].codePosition, 27));
            expected.push_back(FloatValue::Create(tokens[5].codePosition, 13.25));
            expected.push_back(Operation::Create(tokens[4].codePosition, Operation::Kind::Div));
            expected.push_back(Variable::Create(tokens[8].codePosition, "y"));
            expected.push_back(Operation::Create(tokens[7].codePosition, Operation::Kind::Minus));
            expected.push_back(Operation::Create(tokens[6].codePosition, Operation::Kind::Sub));
            expected.push_back(Operation::Create(tokens[1].codePosition, Operation::Kind::Lte));

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getReversePolishNotation(tokens, i);

            Assert::IsTrue(actual.has_value(), L"has value");
            Assert::AreEqual(expected, actual.value());
            Assert::AreEqual(i, 9);
        }
        TEST_METHOD(arithmeticOperationWithBrackets) {
            FileInfo fileInfo("fileName");
            // x = ($y + 3) * $(x);  -->  x y $ 3 + x $ * =
            vector<Token> tokens = {
                {Token::Type::Label,   "x",  1, 1,  &fileInfo},
                {Token::Type::Symbol,  "=",  1, 2,  &fileInfo},
                {Token::Type::Symbol,  "(",  1, 3,  &fileInfo},
                {Token::Type::Symbol,  "$",  1, 4,  &fileInfo},
                {Token::Type::Label,   "y",  1, 5,  &fileInfo},
                {Token::Type::Symbol,  "+",  1, 6,  &fileInfo},
                {Token::Type::Integer, "3",  1, 7,  &fileInfo},
                {Token::Type::Symbol,  ")",  1, 8,  &fileInfo},
                {Token::Type::Symbol,  "*",  1, 9,  &fileInfo},
                {Token::Type::Symbol,  "$",  1, 10, &fileInfo},
                {Token::Type::Symbol,  "(",  1, 11, &fileInfo},
                {Token::Type::Label,   "x",  1, 12, &fileInfo},
                {Token::Type::Symbol,  ")",  1, 13, &fileInfo},
                {Token::Type::Symbol,  ";",  1, 14, &fileInfo},
            };
            int i = 0;

            vector<Value*> expected;
            expected.push_back(Variable::Create(tokens[0].codePosition, "x"));
            expected.push_back(Variable::Create(tokens[4].codePosition, "y"));
            expected.push_back(Operation::Create(tokens[3].codePosition, Operation::Kind::GetValue));
            expected.push_back(IntegerValue::Create(tokens[6].codePosition, 3));
            expected.push_back(Operation::Create(tokens[5].codePosition, Operation::Kind::Add));
            expected.push_back(Variable::Create(tokens[11].codePosition, "x"));
            expected.push_back(Operation::Create(tokens[9].codePosition, Operation::Kind::GetValue));
            expected.push_back(Operation::Create(tokens[8].codePosition, Operation::Kind::Mul));
            expected.push_back(Operation::Create(tokens[1].codePosition, Operation::Kind::Assign));
            
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getReversePolishNotation(tokens, i);

            Assert::IsTrue(actual.has_value(), L"has value");
            Assert::AreEqual(expected, actual.value());
            Assert::AreEqual(i, 13);
        }
        TEST_METHOD(lambdaNoArgumentsImplicitReturnValue) {
            // (){}
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol,  "(",   1, 1, &fileInfo},
                {Token::Type::Symbol,  ")",   1, 2, &fileInfo},
                {Token::Type::Symbol,  "{",   1, 3, &fileInfo},
                {Token::Type::Symbol,  "}",   1, 4, &fileInfo},
            };
            int i = 0;

            vector<Value*> expected;
            auto lambdaType = FunctionType::Create();
            lambdaType->returnType = Type::Create(Type::Kind::Void);
            auto lambda = FunctionValue::Create(tokens[0].codePosition, lambdaType, nullptr);
            expected.push_back(lambda);

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getReversePolishNotation(tokens, i);

            Assert::IsTrue(actual.has_value(), L"has value");
            Assert::AreEqual(expected, actual.value());
            Assert::AreEqual(i, 3);
        }
        TEST_METHOD(lambdaWithArgumentsImplicitReturnValue) {
            // (x: i8, y: u8){}
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol,  "(",  1, 1,  &fileInfo},
                {Token::Type::Label,  "x",   1, 2,  &fileInfo},
                {Token::Type::Symbol,  ":",  1, 3,  &fileInfo},
                {Token::Type::Label,  "i8",  1, 4,  &fileInfo},
                {Token::Type::Symbol,  ",",  1, 5,  &fileInfo},
                {Token::Type::Label,  "y",   1, 6,  &fileInfo},
                {Token::Type::Symbol,  ":",  1, 7,  &fileInfo},
                {Token::Type::Label,  "u8",  1, 8,  &fileInfo},
                {Token::Type::Symbol,  ")",  1, 9,  &fileInfo},
                {Token::Type::Symbol,  "{",  1, 10, &fileInfo},
                {Token::Type::Symbol,  "}",  1, 11, &fileInfo},
            };
            int i = 0;

            vector<Value*> expected;
            auto lambdaType = FunctionType::Create();
            lambdaType->returnType = Type::Create(Type::Kind::Void);
            lambdaType->argumentTypes.push_back(IntegerType::Create(IntegerType::Size::I8));
            lambdaType->argumentTypes.push_back(IntegerType::Create(IntegerType::Size::U8));
            auto lambda = FunctionValue::Create(tokens[0].codePosition, lambdaType, nullptr);
            lambda->arguments.push_back(Declaration::Create(tokens[1].codePosition));
            lambda->arguments.back()->variable->name = "x";
            lambda->arguments.push_back(Declaration::Create(tokens[5].codePosition));
            lambda->arguments.back()->variable->name = "y";
            lambda->arguments[0]->variable->type = lambdaType->argumentTypes[0];
            lambda->arguments[1]->variable->type = lambdaType->argumentTypes[1];
            expected.push_back(lambda);

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getReversePolishNotation(tokens, i);

            Assert::IsTrue(actual.has_value(), L"has value");
            Assert::AreEqual(expected, actual.value());
            Assert::AreEqual(i, 10);
        }
        TEST_METHOD(lambdaNoArgumentsWithReturnValue) {
            // ()->i32{}
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol,  "(",   1, 1, &fileInfo},
                {Token::Type::Symbol,  ")",   1, 2, &fileInfo},
                {Token::Type::Symbol,  "-",   1, 3, &fileInfo},
                {Token::Type::Symbol,  ">",   1, 4, &fileInfo},
                {Token::Type::Label,  "i32",  1, 5, &fileInfo},
                {Token::Type::Symbol,  "{",   1, 6, &fileInfo},
                {Token::Type::Symbol,  "}",   1, 7, &fileInfo},
            };
            int i = 0;

            vector<Value*> expected;
            auto lambdaType = FunctionType::Create();
            lambdaType->returnType = IntegerType::Create(IntegerType::Size::I32);
            auto lambda = FunctionValue::Create(tokens[0].codePosition, lambdaType, nullptr);
            expected.push_back(lambda);

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getReversePolishNotation(tokens, i);

            Assert::IsTrue(actual.has_value(), L"has value");
            Assert::AreEqual(expected, actual.value());
            Assert::AreEqual(i, 6);
        }
    };
    TEST_CLASS(GetType) {
    public:
        TEST_METHOD(voidType) {
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label, "void", 1, 1, &fileInfo},
                {Token::Type::Symbol, ";",   1, 5, &fileInfo},
            };
            int i = 0;

            Type* expected = Type::Create(Type::Kind::Void);

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getType(tokens, i, {";"});

            if (!cmpPtr(expected, actual)) {
                Assert::AreEqual(expected, actual);
            }
            Assert::AreEqual(i, 1);
        }
        TEST_METHOD(pointerType) {
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol, "*",  1, 1, &fileInfo},
                {Token::Type::Label, "u16", 1, 2, &fileInfo},
                {Token::Type::Symbol, ":",  1, 5, &fileInfo},
            };
            int i = 0;

            Type* expected = RawPointerType::Create(
                IntegerType::Create(IntegerType::Size::U16)
            );

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getType(tokens, i, {":"});

            if (!cmpPtr(expected, actual)) {
                Assert::AreEqual(expected, actual);
            }
            Assert::AreEqual(i, 2);
        }
        TEST_METHOD(staticArrayType) {
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol,  "[",   1, 1, &fileInfo},
                {Token::Type::Integer, "5",   1, 2, &fileInfo},
                {Token::Type::Symbol,  "]",   1, 3, &fileInfo},
                {Token::Type::Label,   "int", 1, 4, &fileInfo},
                {Token::Type::Symbol,  ";",   1, 3, &fileInfo},
            };
            int i = 0;

            Type* expected = StaticArrayType::Create(
                IntegerType::Create(IntegerType::Size::I64),
                IntegerValue::Create(tokens[1].codePosition, 5)
            );

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getType(tokens, i, {";"});

            if (!cmpPtr(expected, actual)) {
                Assert::AreEqual(expected, actual);
            }
            Assert::AreEqual(i, 4);
        }
        TEST_METHOD(arrayViewType) {
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol, "[",   1, 1, &fileInfo},
                {Token::Type::Symbol, "*",   1, 2, &fileInfo},
                {Token::Type::Symbol, "]",   1, 3, &fileInfo},
                {Token::Type::Label,  "int", 1, 4, &fileInfo},
                {Token::Type::Symbol, ";",   1, 3, &fileInfo},
            };
            int i = 0;

            Type* expected = ArrayViewType::Create(
                IntegerType::Create(IntegerType::Size::I64)
            );

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getType(tokens, i, {";"});

            if (!cmpPtr(expected, actual)) {
                Assert::AreEqual(expected, actual);
            }
            Assert::AreEqual(i, 4);
        }
        TEST_METHOD(dynamicArrayType) {
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol,  "[",   1, 1, &fileInfo},
                {Token::Type::Symbol,  "]",   1, 2, &fileInfo},
                {Token::Type::Label,   "int", 1, 3, &fileInfo},
                {Token::Type::Symbol,  ";",   1, 4, &fileInfo},
            };
            int i = 0;

            Type* expected = DynamicArrayType::Create(
                IntegerType::Create(IntegerType::Size::I64)
            );

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getType(tokens, i, {";"});

            if (!cmpPtr(expected, actual)) {
                Assert::AreEqual(expected, actual);
            }
            Assert::AreEqual(i, 3);
        }
        TEST_METHOD(functionTypeImplicitVoidReturn) {
            // (?int, &!void);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol, "(",   1, 1,  &fileInfo},
                {Token::Type::Symbol, "?",   1, 2,  &fileInfo},
                {Token::Type::Label, "int",  1, 3,  &fileInfo},
                {Token::Type::Symbol, ",",   1, 6,  &fileInfo},
                {Token::Type::Symbol, "&",   1, 7,  &fileInfo},
                {Token::Type::Symbol, "!",   1, 8,  &fileInfo},
                {Token::Type::Label, "void", 1, 9,  &fileInfo},
                {Token::Type::Symbol, ")",   1, 13, &fileInfo},
                {Token::Type::Symbol, ";",   1, 14, &fileInfo},
            };
            int i = 0;

            auto expectedFunction = FunctionType::Create();
            expectedFunction->returnType = Type::Create(Type::Kind::Void);
            expectedFunction->argumentTypes.push_back(MaybeErrorType::Create(
                IntegerType::Create(IntegerType::Size::I64)
            ));
            expectedFunction->argumentTypes.push_back(ReferenceType::Create(
                OwnerPointerType::Create(
                    Type::Create(Type::Kind::Void)
                )
            ));
            Type* expected = expectedFunction;

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getType(tokens, i, {";"});

            if (!cmpPtr(expected, actual)) {
                Assert::AreEqual(expected, actual);
            }
            Assert::AreEqual(i, 8);
        }
        TEST_METHOD(functionTypeNoArgumentsWithReturnType) {
            // ()->float;
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol, "(",    1, 1,  &fileInfo},
                {Token::Type::Symbol, ")",    1, 2,  &fileInfo},
                {Token::Type::Symbol, "-",    1, 3,  &fileInfo},
                {Token::Type::Symbol, ">",    1, 4,  &fileInfo},
                {Token::Type::Label, "float", 1, 5,  &fileInfo},
                {Token::Type::Symbol, ";",    1, 10, &fileInfo},
            };
            int i = 0;

            auto expectedFunction = FunctionType::Create();
            expectedFunction->returnType = FloatType::Create(FloatType::Size::F64);
            Type* expected = expectedFunction;

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getType(tokens, i, {";"});

            if (!cmpPtr(expected, actual)) {
                Assert::AreEqual(expected, actual);
            }
            Assert::AreEqual(i, 5);
        }
        TEST_METHOD(functionTypeFunctionArgumentImplicitVoidReturns) {
            // ((i8));  aka ((i8)->void)->void
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol, "(", 1, 1, &fileInfo},
                {Token::Type::Symbol, "(", 1, 2, &fileInfo},
                {Token::Type::Label, "i8", 1, 3, &fileInfo},
                {Token::Type::Symbol, ")", 1, 5, &fileInfo},
                {Token::Type::Symbol, ")", 1, 6, &fileInfo},
                {Token::Type::Symbol, ";", 1, 7, &fileInfo},
            };
            int i = 0;

            auto expectedFunction = FunctionType::Create();
            auto funtionArgument = FunctionType::Create();
            funtionArgument->returnType = Type::Create(Type::Kind::Void);
            funtionArgument->argumentTypes.push_back(IntegerType::Create(IntegerType::Size::I8));
            expectedFunction->returnType = Type::Create(Type::Kind::Void);
            expectedFunction->argumentTypes.push_back(funtionArgument);
            Type* expected = expectedFunction;

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getType(tokens, i, {";"});

            if (!cmpPtr(expected, actual)) {
                Assert::AreEqual(expected, actual);
            }
            Assert::AreEqual(i, 5);
        }
        TEST_METHOD(functionTypeReturingFunctionReturningFuntion) {
            // ()->()->(i64)->void;
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol, "(",   1, 1,  &fileInfo},
                {Token::Type::Symbol, ")",   1, 2,  &fileInfo},
                {Token::Type::Symbol, "-",   1, 3,  &fileInfo},
                {Token::Type::Symbol, ">",   1, 4,  &fileInfo},
                {Token::Type::Symbol, "(",   1, 5,  &fileInfo},
                {Token::Type::Symbol, ")",   1, 6,  &fileInfo},
                {Token::Type::Symbol, "-",   1, 7,  &fileInfo},
                {Token::Type::Symbol, ">",   1, 8,  &fileInfo},
                {Token::Type::Symbol, "(",   1, 9,  &fileInfo},
                {Token::Type::Label, "i64",  1, 10, &fileInfo},
                {Token::Type::Symbol, ")",   1, 13, &fileInfo},
                {Token::Type::Symbol, "-",   1, 14, &fileInfo},
                {Token::Type::Symbol, ">",   1, 15, &fileInfo},
                {Token::Type::Label, "void", 1, 16, &fileInfo},
                {Token::Type::Symbol, ";",   1, 20, &fileInfo},
            };
            int i = 0;

            auto expectedFunction = FunctionType::Create();
            auto funtionReturn1 = FunctionType::Create();
            auto funtionReturn2 = FunctionType::Create();
            funtionReturn2->returnType = Type::Create(Type::Kind::Void);
            funtionReturn2->argumentTypes.push_back(IntegerType::Create(IntegerType::Size::I64));
            funtionReturn1->returnType = funtionReturn2;
            expectedFunction->returnType = funtionReturn1;
            Type* expected = expectedFunction;

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getType(tokens, i, {";"});

            if (!cmpPtr(expected, actual)) {
                Assert::AreEqual(expected, actual);
            }
            Assert::AreEqual(i, 14);
        }
        TEST_METHOD(templateFunctionType) {
            // <T,U>(!T,U)->&T<u8>;
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol, "<", 1, 1,  &fileInfo},
                {Token::Type::Label,  "T", 1, 2,  &fileInfo},
                {Token::Type::Symbol, ",", 1, 3,  &fileInfo},
                {Token::Type::Label,  "U", 1, 4,  &fileInfo},
                {Token::Type::Symbol, ">", 1, 5,  &fileInfo},
                {Token::Type::Symbol, "(", 1, 6,  &fileInfo},
                {Token::Type::Symbol, "!", 1, 7,  &fileInfo},
                {Token::Type::Label,  "T", 1, 8,  &fileInfo},
                {Token::Type::Symbol, ",", 1, 9,  &fileInfo},
                {Token::Type::Label,  "U", 1, 10, &fileInfo},
                {Token::Type::Symbol, ")", 1, 11, &fileInfo},
                {Token::Type::Symbol, "-", 1, 12, &fileInfo},
                {Token::Type::Symbol, ">", 1, 13, &fileInfo},
                {Token::Type::Symbol, "&", 1, 14, &fileInfo},
                {Token::Type::Label,  "T", 1, 15, &fileInfo},
                {Token::Type::Symbol, "<", 1, 16, &fileInfo},
                {Token::Type::Label, "u8", 1, 17, &fileInfo},
                {Token::Type::Symbol, ">", 1, 18, &fileInfo},
                {Token::Type::Symbol, ";", 1, 19, &fileInfo},
            };
            int i = 0;

            auto expectedFunction = TemplateFunctionType::Create();
            expectedFunction->templateTypes.push_back(TemplateType::Create("T"));
            expectedFunction->templateTypes.push_back(TemplateType::Create("U"));
            auto returnType = ReferenceType::Create(
                ClassType::Create("T")
            );
            ((ClassType*)(returnType->underlyingType))->templateTypes.push_back(
                IntegerType::Create(IntegerType::Size::U8)
            );
            expectedFunction->returnType = returnType;
            expectedFunction->argumentTypes.push_back(OwnerPointerType::Create(
                ClassType::Create("T")
            ));
            expectedFunction->argumentTypes.push_back(ClassType::Create("U"));
            Type* expected = expectedFunction;

            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            auto actual = scope.getType(tokens, i, {";"});

            if (!cmpPtr(expected, actual)) {
                Assert::AreEqual(expected, actual);
            }
            Assert::AreEqual(i, 18);
        }
    };
    TEST_CLASS(Scope__ReadStatement) {
    public:
        TEST_METHOD(declarationConstValueImplicit) {
            // x :: 3;
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label,   "x", 1, 1, &fileInfo},
                {Token::Type::Symbol,  ":", 1, 2, &fileInfo},
                {Token::Type::Symbol,  ":", 1, 3, &fileInfo},
                {Token::Type::Integer, "3", 1, 4, &fileInfo},
                {Token::Type::Symbol,  ";", 1, 5, &fileInfo}
            };
            int i = 0;

            auto statement = Declaration::Create(tokens[0].codePosition);
            statement->value = IntegerValue::Create(tokens[3].codePosition, 3);
            statement->variable->name = "x";
            statement->variable->isConst = true;
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(5, i);
        }
        TEST_METHOD(declarationConstRefImplicit) {
            // x &: y;
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label,   "x", 1, 1, &fileInfo},
                {Token::Type::Symbol,  "&", 1, 2, &fileInfo},
                {Token::Type::Symbol,  ":", 1, 3, &fileInfo},
                {Token::Type::Label,   "y", 1, 4, &fileInfo},
                {Token::Type::Symbol,  ";", 1, 5, &fileInfo}
            };
            int i = 0;

            auto statement = Declaration::Create(tokens[0].codePosition);
            statement->value = Variable::Create(tokens[3].codePosition, "y");
            statement->variable->name = "x";
            statement->variable->isConst = true;
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(5, i);
        }
        TEST_METHOD(declarationValueImplicit) {
            // x := y;
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label,   "x", 1, 1, &fileInfo},
                {Token::Type::Symbol,  ":", 1, 2, &fileInfo},
                {Token::Type::Symbol,  "=", 1, 3, &fileInfo},
                {Token::Type::Label,   "y", 1, 4, &fileInfo},
                {Token::Type::Symbol,  ";", 1, 5, &fileInfo}
            };
            int i = 0;

            auto statement = Declaration::Create(tokens[0].codePosition);
            statement->value = Variable::Create(tokens[3].codePosition, "y");
            statement->variable->name = "x";
            statement->variable->isConst = false;
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(5, i);
        }
        TEST_METHOD(declarationRefImplicit) {
            // x &= y;
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label,   "x", 1, 1, &fileInfo},
                {Token::Type::Symbol,  "&", 1, 2, &fileInfo},
                {Token::Type::Symbol,  "=", 1, 3, &fileInfo},
                {Token::Type::Label,   "y", 1, 4, &fileInfo},
                {Token::Type::Symbol,  ";", 1, 5, &fileInfo}
            };
            int i = 0;

            auto statement = Declaration::Create(tokens[0].codePosition);
            statement->value = Variable::Create(tokens[3].codePosition, "y");
            statement->variable->name = "x";
            statement->variable->isConst = false;
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(5, i);
        }
        TEST_METHOD(declarationWithTypeNoValue) {
            // x : i8;
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label,  "x",  1, 1, &fileInfo},
                {Token::Type::Symbol, ":",  1, 2, &fileInfo},
                {Token::Type::Label,  "i8", 1, 3, &fileInfo},
                {Token::Type::Symbol, ";",  1, 4, &fileInfo}
            };
            int i = 0;

            auto statement = Declaration::Create(tokens[0].codePosition);
            statement->variable->name = "x";
            statement->variable->isConst = false;
            statement->variable->type = IntegerType::Create(IntegerType::Size::I8);
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(4, i);
        }
        TEST_METHOD(declarationWithTypeAndValue) {
            // x : i8 : 2.5;
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label,  "x",   1, 1, &fileInfo},
                {Token::Type::Symbol, ":",   1, 2, &fileInfo},
                {Token::Type::Label,  "i8",  1, 3, &fileInfo},
                {Token::Type::Symbol, ":",   1, 4, &fileInfo},
                {Token::Type::Float,  "2.5", 1, 5, &fileInfo},
                {Token::Type::Symbol, ";",   1, 6, &fileInfo}
            };
            int i = 0;

            auto statement = Declaration::Create(tokens[0].codePosition);
            statement->variable->name = "x";
            statement->variable->isConst = true;
            statement->variable->type = IntegerType::Create(IntegerType::Size::I8);
            statement->value = FloatValue::Create(tokens[4].codePosition, 2.5);
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(6, i);
        }
        TEST_METHOD(scopeEmpty) {
            // {}
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol, "{",   1, 1, &fileInfo},
                {Token::Type::Symbol, "}",   1, 2, &fileInfo},
            };
            int i = 0;

            auto statement = CodeScope::Create(tokens[0].codePosition, Scope::Owner::None, &scope);
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(2, i);
        }
        TEST_METHOD(scopesInScope) {
            // {{}{}}
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Symbol, "{",   1, 1, &fileInfo},
                {Token::Type::Symbol, "{",   1, 2, &fileInfo},
                {Token::Type::Symbol, "}",   1, 3, &fileInfo},
                {Token::Type::Symbol, "{",   1, 4, &fileInfo},
                {Token::Type::Symbol, "}",   1, 5, &fileInfo},
                {Token::Type::Symbol, "}",   1, 6, &fileInfo},
            };
            int i = 0;

            auto statement = CodeScope::Create(tokens[0].codePosition, Scope::Owner::None, &scope);
            statement->statements.push_back(CodeScope::Create(tokens[1].codePosition, Scope::Owner::None, statement));
            statement->statements.push_back(CodeScope::Create(tokens[3].codePosition, Scope::Owner::None, statement));
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(6, i);
        }
        TEST_METHOD(deferScope) {
            // defer {}
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label, "defer", 1, 1, &fileInfo},
                {Token::Type::Symbol, "{",    1, 6, &fileInfo},
                {Token::Type::Symbol, "}",    1, 7, &fileInfo},
            };
            int i = 0;

            auto statement = DeferScope::Create(tokens[0].codePosition, &scope);
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(3, i);
        }
        TEST_METHOD(ifScope) {
            // if 2 {}
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label,  "if", 1, 1, &fileInfo},
                {Token::Type::Integer, "2", 1, 3, &fileInfo},
                {Token::Type::Symbol,  "{", 1, 4, &fileInfo},
                {Token::Type::Symbol,  "}", 1, 5, &fileInfo},
            };
            int i = 0;

            auto statement = IfScope::Create(tokens[0].codePosition, &scope);
            statement->conditionExpression = IntegerValue::Create(tokens[1].codePosition, 2);
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(4, i);
        }
        TEST_METHOD(forScopeForEachImplicitIteratorAndIndex) {
            // for x {}
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label, "for", 1, 1, &fileInfo},
                {Token::Type::Label,  "x",  1, 4, &fileInfo},
                {Token::Type::Symbol, "{",  1, 5, &fileInfo},
                {Token::Type::Symbol, "}",  1, 6, &fileInfo},
            };
            int i = 0;

            auto statement = ForScope::Create(tokens[0].codePosition, &scope);
            ForEachData data;
            data.arrayValue = Variable::Create(tokens[1].codePosition, "x");
            data.index = Variable::Create(tokens[2].codePosition, "index");
            data.index->isConst = true;
            data.index->type = IntegerType::Create(IntegerType::Size::I64);
            data.it = Variable::Create(tokens[2].codePosition, "it");
            data.it->isConst = true;
            statement->data = data;
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(4, i);
        }
        TEST_METHOD(forScopeForEachImplicitIndex) {
            // for x := y {}
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label, "for", 1, 1, &fileInfo},
                {Token::Type::Label,  "x",  1, 4, &fileInfo},
                {Token::Type::Symbol, ":",  1, 5, &fileInfo},
                {Token::Type::Symbol, "=",  1, 6, &fileInfo},
                {Token::Type::Label,  "y",  1, 7, &fileInfo},
                {Token::Type::Symbol, "{",  1, 8, &fileInfo},
                {Token::Type::Symbol, "}",  1, 9, &fileInfo},
            };
            int i = 0;

            auto statement = ForScope::Create(tokens[0].codePosition, &scope);
            ForEachData data;
            data.arrayValue = Variable::Create(tokens[4].codePosition, "y");
            data.index = Variable::Create(tokens[5].codePosition, "index");
            data.index->isConst = true;
            data.index->type = IntegerType::Create(IntegerType::Size::I64);
            data.it = Variable::Create(tokens[1].codePosition, "x");
            data.it->isConst = false;
            statement->data = data;
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(7, i);
        }
        TEST_METHOD(forScopeForEachAllExplicit) {
            // for x, y :: z {}
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label, "for", 1, 1,  &fileInfo},
                {Token::Type::Label,  "x",  1, 4,  &fileInfo},
                {Token::Type::Symbol, ",",  1, 5,  &fileInfo},
                {Token::Type::Label,  "y",  1, 6,  &fileInfo},
                {Token::Type::Symbol, ":",  1, 7,  &fileInfo},
                {Token::Type::Symbol, ":",  1, 8,  &fileInfo},
                {Token::Type::Label,  "z",  1, 9,  &fileInfo},
                {Token::Type::Symbol, "{",  1, 10, &fileInfo},
                {Token::Type::Symbol, "}",  1, 11, &fileInfo},
            };
            int i = 0;

            auto statement = ForScope::Create(tokens[0].codePosition, &scope);
            ForEachData data;
            data.arrayValue = Variable::Create(tokens[6].codePosition, "z");
            data.index = Variable::Create(tokens[3].codePosition, "y");
            data.index->isConst = true;
            data.index->type = IntegerType::Create(IntegerType::Size::I64);
            data.it = Variable::Create(tokens[1].codePosition, "x");
            data.it->isConst = true;
            statement->data = data;
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(9, i);
        }
        TEST_METHOD(forScopeForIterNoStep) {
            // for x :: 1:5 {}
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label, "for", 1, 1,  &fileInfo},
                {Token::Type::Label,  "x",  1, 4,  &fileInfo},
                {Token::Type::Symbol, ":",  1, 5,  &fileInfo},
                {Token::Type::Symbol, ":",  1, 6,  &fileInfo},
                {Token::Type::Integer,"1",  1, 7,  &fileInfo},
                {Token::Type::Symbol, ":",  1, 8,  &fileInfo},
                {Token::Type::Integer,"5",  1, 9,  &fileInfo},
                {Token::Type::Symbol, "{",  1, 10, &fileInfo},
                {Token::Type::Symbol, "}",  1, 11, &fileInfo},
            };
            int i = 0;

            auto statement = ForScope::Create(tokens[0].codePosition, &scope);
            ForIterData data;
            data.iterVariable = Variable::Create(tokens[1].codePosition, "x");
            data.iterVariable->isConst = true;
            data.firstValue = IntegerValue::Create(tokens[4].codePosition, 1);
            data.step = IntegerValue::Create(tokens[7].codePosition, 1);
            data.lastValue = IntegerValue::Create(tokens[6].codePosition, 5);
            statement->data = data;
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(9, i);
        }
        TEST_METHOD(forScopeForIterWithStep) {
            // for x := 1:2:5 {}
            CodeScope scope(CodePosition(nullptr, 0, 0), Scope::Owner::None, nullptr);
            FileInfo fileInfo("fileName");
            vector<Token> tokens = {
                {Token::Type::Label, "for", 1, 1,  &fileInfo},
                {Token::Type::Label,  "x",  1, 4,  &fileInfo},
                {Token::Type::Symbol, ":",  1, 5,  &fileInfo},
                {Token::Type::Symbol, "=",  1, 6,  &fileInfo},
                {Token::Type::Integer,"1",  1, 7,  &fileInfo},
                {Token::Type::Symbol, ":",  1, 8,  &fileInfo},
                {Token::Type::Integer,"2",  1, 9,  &fileInfo},
                {Token::Type::Symbol, ":",  1, 10, &fileInfo},
                {Token::Type::Integer,"5",  1, 11, &fileInfo},
                {Token::Type::Symbol, "{",  1, 12, &fileInfo},
                {Token::Type::Symbol, "}",  1, 13, &fileInfo},
            };
            int i = 0;

            auto statement = ForScope::Create(tokens[0].codePosition, &scope);
            ForIterData data;
            data.iterVariable = Variable::Create(tokens[1].codePosition, "x");
            data.iterVariable->isConst = false;
            data.firstValue = IntegerValue::Create(tokens[4].codePosition, 1);
            data.step = IntegerValue::Create(tokens[6].codePosition, 2);
            data.lastValue = IntegerValue::Create(tokens[8].codePosition, 5);
            statement->data = data;
            auto expected = Scope::ReadStatementValue(statement);

            auto readStatement = scope.readStatement(tokens, i);

            Assert::AreEqual(true, readStatement.operator bool());
            AssertAreEqual(expected.statement, readStatement.statement);
            Assert::AreEqual(11, i);
        }
    };
}

namespace interpreting {
    TEST_CLASS(Operation__interpret_constexpr) {
    public:
        TEST_METHOD(addI64I64) {
            // i64(3) + i64(-5) = i64(-2)
            CodePosition codePosition(nullptr, 0, 0);
            auto operation = Operation::Create(codePosition, Operation::Kind::Add);
            operation->arguments.push_back(IntegerValue::Create(codePosition, 3));
            operation->arguments.push_back(IntegerValue::Create(codePosition, -5));

            Value* expected = IntegerValue::Create(codePosition, -2);

            auto actual = operation->interpret(nullptr);

            Assert::IsTrue(actual.has_value());
            AssertAreEqual(expected, actual.value());
        }
        TEST_METHOD(addU8I8) {
            // u8(200) + i8(-10) = i64(-2)
            CodePosition codePosition(nullptr, 0, 0);
            auto operation = Operation::Create(codePosition, Operation::Kind::Add);
            operation->arguments.push_back(IntegerValue::Create(codePosition, 200));
            operation->arguments.back()->type = IntegerType::Create(IntegerType::Size::U8);
            operation->arguments.push_back(IntegerValue::Create(codePosition, -10));
            operation->arguments.back()->type = IntegerType::Create(IntegerType::Size::I8);

            Value* expected = IntegerValue::Create(codePosition, 190);

            auto actual = operation->interpret(nullptr);

            Assert::IsTrue(actual.has_value());
            AssertAreEqual(expected, actual.value());
        }
        TEST_METHOD(addOverflowU8U8) {
            // u8(200) + u8(100) = u64(300)
            CodePosition codePosition(nullptr, 0, 0);
            auto operation = Operation::Create(codePosition, Operation::Kind::Add);
            operation->arguments.push_back(IntegerValue::Create(codePosition, 200));
            operation->arguments.back()->type = IntegerType::Create(IntegerType::Size::U8);
            operation->arguments.push_back(IntegerValue::Create(codePosition, 100));
            operation->arguments.back()->type = IntegerType::Create(IntegerType::Size::U8);

            Value* expected = IntegerValue::Create(codePosition, 300);
            expected->type = IntegerType::Create(IntegerType::Size::U64);

            auto actual = operation->interpret(nullptr);

            Assert::IsTrue(actual.has_value());
            AssertAreEqual(expected, actual.value());
        }
    };
}
