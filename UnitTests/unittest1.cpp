#include "stdafx.h"
#include "CppUnitTest.h"

#include <string>
#include <fstream>
#include <optional>
#include "../C$/parsing.cpp"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std;

wstring toWstring(const string& str) {
    wstring result = L"";
    for (char c : str) {
        result += c;
    }
    return result;
}
wstring toWstring(char c) {
    wstring result = L"";
    result += c;
    return result;
}
wstring toWstring(int i) {
    return toWstring(to_string(i));
}
wstring toWstring(const SourceStringLine& sourceStringLine) {
    return toWstring("[\"" + sourceStringLine.line + "\", " + to_string(sourceStringLine.number) + "]");
}
template <typename T> wstring toWstring(const std::optional<T>& obj) {
    if (obj) {
        return toWstring(obj.value());
    } else {
        return L"";
    }
}
template <typename T> wstring toWstring(const vector<T>& vec) {
    wstring str = L"{";
    for (int i = 0; i < vec.size() - 1; ++i) {
        str += toWstring(vec[i]) + L", ";
    }
    str += toWstring(vec[vec.size()-1]) + L"}";
    return str;
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

bool operator==(const SourceStringLine& lhs, const SourceStringLine& rhs) {
    return lhs.line == rhs.line && lhs.number == rhs.number;
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
    template<> wstring ToString<unordered_set<string>>(const unordered_set<string>& set) {
        return toWstringNewLines(set);
    }
}

namespace Parsing {	
    string randomString = "fao478qt4ovywfubdao8q4ygfuaualsdfkasd";

	TEST_CLASS(GetSourceFromFile) {
	public:
		TEST_METHOD(fileDoesntExist) {
            FileInfo fileInfo(randomString);
            unordered_set<string> includedFiles;
            auto [result, _] = getSourceFromFile(fileInfo);
		    Assert::IsFalse(result.has_value(), L"result doesn't have value");
        }

        TEST_METHOD(emptyFile) {
            ofstream newFile(randomString);
            newFile.close();
            FileInfo fileInfo(randomString);
            unordered_set<string> includedFiles;
            auto [result, _] = getSourceFromFile(fileInfo);
            remove(randomString.c_str());

            Assert::IsTrue(result.has_value(), L"result has value");
            Assert::IsTrue(result.value().empty(), L"result is empty");
        }

        TEST_METHOD(fewLinesNoInclude) {
            vector<SourceStringLine> expected = {
                {"// some commented line", 1},
                {"#notInclude", 2},
                {"", 3},
                {"x #include file", 4}
            };

            ofstream newFile(randomString);
            for (auto sourceLine : expected) {
                newFile << sourceLine.line << "\n";
            }
            newFile.close();
            FileInfo fileInfo(randomString);
            unordered_set<string> includedFiles;
            auto [result, _] = getSourceFromFile(fileInfo);
            remove(randomString.c_str());

            Assert::IsTrue(result.has_value(), L"result has value");
            Assert::AreEqual(expected, result.value());
        }

        TEST_METHOD(onlyIncludesInFirstFile) {
            vector<SourceStringLine> expected = {
                {"file 1 line 0", 1},
                {"file 1 line 1", 2},
                {"file 2 line 0", 1}
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

            newFile1 << expected[0].line << '\n';
            newFile1 << expected[1].line << '\n';
            newFile2 << expected[2].line << '\n';

            newFile0.close();
            newFile1.close();
            newFile2.close();
            newFile3.close();

            FileInfo fileInfo(randomString);
            FileInfo fileInfo1(randomString1);
            FileInfo fileInfo2(randomString2);
            FileInfo fileInfo3(randomString3);

            unordered_set<string> includedFiles;
            auto [result, _] = getSourceFromFile(fileInfo);

            remove(randomString.c_str());
            remove((randomString1).c_str());
            remove((randomString2).c_str());
            remove((randomString3).c_str());

            Assert::IsTrue(result.has_value(), L"result has value");
            Assert::AreEqual(expected, result.value());
        }

        TEST_METHOD(recursiveInclude) {
            vector<SourceStringLine> expectedSource = {};
            unordered_set<string> expectedIncludedFiles = {
                {randomString}
            };

            ofstream newFile(randomString);
            for (auto sourceLine : expectedSource) {
                newFile << sourceLine.line << "\n";
            }
            newFile.close();
            FileInfo fileInfo(randomString);
            auto [sourceCode, includedFiles] = getSourceFromFile(fileInfo);
            remove(randomString.c_str());

            Assert::IsTrue(sourceCode.has_value(), L"result has value");
            Assert::AreEqual(expectedSource, sourceCode.value());
            Assert::AreEqual(expectedIncludedFiles, includedFiles);
        }


	};
}