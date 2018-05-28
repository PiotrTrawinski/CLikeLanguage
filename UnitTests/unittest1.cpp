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
    return toWstring("[\"" + sourceStringLine.value + "\", " + to_string(sourceStringLine.number) + ", " + to_string((size_t)sourceStringLine.file) + "]");
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
    return lhs.value == rhs.value && lhs.number == rhs.number && lhs.file == rhs.file;
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
            auto [result, _] = getSourceFromFile(randomString);
		    Assert::IsFalse(result.has_value(), L"result doesn't have value");
        }

        TEST_METHOD(emptyFile) {
            ofstream newFile(randomString);
            newFile.close();
            unordered_set<string> includedFiles;
            auto [result, _] = getSourceFromFile(randomString);
            remove(randomString.c_str());

            Assert::IsTrue(result.has_value(), L"result has value");
            Assert::IsTrue(result.value().empty(), L"result is empty");
        }

        TEST_METHOD(fewLinesNoInclude) {
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
            auto [sourceCode, fileInfos] = getSourceFromFile(randomString);
            remove(randomString.c_str());

            vector<SourceStringLine> expected;
            for (int i = 0; i < fileContent.size(); ++i) {
                expected.emplace_back(fileContent[i], fileInfos[0].get(), i+1);
            }

            Assert::IsTrue(sourceCode.has_value(), L"result has value");
            Assert::AreEqual(expected, sourceCode.value());
        }

        TEST_METHOD(onlyIncludesInFirstFile) {
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
            auto [result, fileInfos] = getSourceFromFile(randomString);

            remove(randomString.c_str());
            remove((randomString1).c_str());
            remove((randomString2).c_str());
            remove((randomString3).c_str());

            vector<SourceStringLine> expected;
            for (int i = 0; i < file1Content.size(); ++i) {
                expected.emplace_back(file1Content[i], fileInfos[1].get(), i+1);
            }
            for (int i = 0; i < file2Content.size(); ++i) {
                expected.emplace_back(file2Content[i], fileInfos[2].get(), i+1);
            }

            Assert::IsTrue(result.has_value(), L"result has value");
            Assert::AreEqual(expected, result.value());
        }

        TEST_METHOD(recursiveInclude) {
            vector<SourceStringLine> expectedSource = {};
            unordered_set<string> expectedIncludedFiles = {
                {randomString}
            };

            ofstream newFile(randomString);
            newFile.close();
            auto [sourceCode, fileInfos] = getSourceFromFile(randomString);
            remove(randomString.c_str());

            Assert::IsTrue(sourceCode.has_value(), L"result has value");
            Assert::AreEqual(expectedSource, sourceCode.value());
            Assert::AreEqual(fileInfos.size(), (size_t)1);
        }


	};
}