#include "parsing.h"
#include <filesystem>

using namespace std;
namespace fs = filesystem;

optional<vector<SourceStringLine>> getSourceFromFile(FileInfo* fileInfo);

struct recursive_directory_range {
    recursive_directory_range(fs::path p) : p(p) {}
    fs::recursive_directory_iterator begin() { return fs::recursive_directory_iterator(p); }
    fs::recursive_directory_iterator end() { return fs::recursive_directory_iterator(); }
private:
    fs::path p;
};

vector<string> splitPath(string_view str) {
    vector<string> result;
    string token = "";
    for (char c : str) {
        if (c == '/') {
            result.push_back(token);
            token = "";
        } else {
            token += c;
        }
    }
    if (!token.empty()) {
        result.push_back(token);
    }
    return result;
}

void printErrorIncorrectPath(FileInfo* fileInfo, string_view fileName, int includeLine) {
    cerr << "Include Error: " << fileName << " does not exist\n";
    while (fileInfo) {
        cerr << "included in " << fileInfo->name() << " at line " << includeLine << '\n';
        includeLine = fileInfo->includeLineNumber;
        fileInfo = fileInfo->parent;
    }
}

optional<fs::path> createIncludePath(FileInfo* fileInfo, string_view str, int includeLine) {
    auto newPath = fileInfo->path.parent_path();
    auto tokens = splitPath(str);
    for (auto& token : tokens) {
        if (token == "..") {
            if (newPath.has_parent_path()) {
                newPath = newPath.parent_path();
            } else {
                printErrorIncorrectPath(fileInfo, str, includeLine);
                return nullopt;
            }
        } else {
            newPath /= fs::path(token);
        }
    }
    if (fs::exists(newPath)) {
        return newPath;
    } else {
        printErrorIncorrectPath(fileInfo, str, includeLine);
        return nullopt;
    }
}


char getCharacter(string_view lineStr, int& i) {
    if (lineStr[i] == '\\') {
        i += 2;
        switch (lineStr[i-1]) {
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'b': return '\b';
        case 'f': return '\f';
        case '0': return '\0';
        default:  return lineStr[i-1];
        }
    }
    return lineStr[i++];
}

optional<vector<Token>> createTokens() {
    vector<Token> tokens;

    for(int lineId = 0; lineId < GVARS.sourceCode.size(); ++lineId){
        FileInfo* fileInfo = GVARS.sourceCode[lineId].file;
        int lineNumber = GVARS.sourceCode[lineId].number;
        string_view lineStr = GVARS.sourceCode[lineId].value;
        int charId = 0;
        while (charId < lineStr.size()) {
            int charNumber = charId+1;
            char c = lineStr[charId];
            if (isalpha(c) || c == '_') {
                string label = string(1, c);
                charId++;
                while (charId < lineStr.size() && (isalpha(lineStr[charId]) || isdigit(lineStr[charId]) || lineStr[charId]=='_')) {
                    label += lineStr[charId];
                    charId++;
                }
                tokens.emplace_back(Token::Type::Label, label, lineNumber, charNumber, fileInfo, lineId);
            }
            else if (c == '`') {
                string stringLiteral = "";
                charId++; // skip opening ` symbol
                while (charId < lineStr.size() && lineStr[charId] != '`') {
                    stringLiteral += getCharacter(lineStr, charId);
                }
                charId++; // skip closing ` symbol
                tokens.emplace_back(Token::Type::StringLiteral, stringLiteral, lineNumber, charNumber, fileInfo, lineId);
            }
            else if (c == '"') {
                string rawStringLiteral = "";
                charId++; // skip opening " symbol
                while (charId < lineStr.size() && lineStr[charId] != '"') {
                    rawStringLiteral += getCharacter(lineStr, charId);
                }
                charId++; // skip closing " symbol
                tokens.emplace_back(Token::Type::RawStringLiteral, rawStringLiteral, lineNumber, charNumber, fileInfo, lineId);
            }
            else if (c == '\'') {
                charId++; // skip opening ' symbol
                if (charId >= lineStr.size()) {
                    cerr << "Parsing error: unexpected end of line at line " << lineNumber << '\n';
                    cerr << "didn't complete the definition of char literal defined at char " << charNumber << '\n';
                    return nullopt;
                }
                string character = string(1, getCharacter(lineStr, charId));
                if (charId >= lineStr.size()) {
                    cerr << "Parsing error: unexpected end of line at line " << lineNumber << '\n';
                    cerr << "missing closing ' symbol for char literal defined at char " << charNumber << '\n';
                    return nullopt;
                }
                if (lineStr[charId] != '\'') {
                    cerr << "Parsing error: missing closing ' symbol for char literal\n";
                    return nullopt;
                }
                charId++; // skip closing ' symbol
                tokens.emplace_back(Token::Type::Char, character, lineNumber, charNumber, fileInfo, lineId);
            }
            else if (isdigit(c)) {
                string strNumber = string(1, c);
                charId++;
                bool haveDot = false;
                while (charId < lineStr.size() && (isdigit(lineStr[charId]) || lineStr[charId]=='.')) {
                    if (lineStr[charId] == '.') {
                        if (haveDot) {
                            cerr << "Parsing error: too many dots in number\n";
                            return nullopt;
                        }
                        haveDot = true;
                    }
                    strNumber += lineStr[charId];
                    charId++;
                }
                if (haveDot) {
                    tokens.emplace_back(Token::Type::Float, strNumber, lineNumber, charNumber, fileInfo, lineId);
                } else {
                    tokens.emplace_back(Token::Type::Integer, strNumber, lineNumber, charNumber, fileInfo, lineId);
                }
            }
            else if (c == '/' && charId < lineStr.size() - 1 && lineStr[charId + 1] == '/') {
                // single line comment
                break;
            }
            else if (c == '/' && charId < lineStr.size() - 1 && lineStr[charId + 1] == '*') {
                // multi-line comment
                charId += 2; // skip '/' and '*' symbols

                // skip everything till appropriate closing comment (*/) string
                // (nested coments work -> /* ... /* ... */ ... */ is corrent syntax)
                int openedComents = 1;
                while (lineId < GVARS.sourceCode.size()) {
                    fileInfo = GVARS.sourceCode[lineId].file;
                    lineNumber = GVARS.sourceCode[lineId].number;
                    lineStr = GVARS.sourceCode[lineId].value;
                    if (lineStr.size() > 0) {
                        while (charId < lineStr.size() - 1) {
                            if (lineStr[charId] == '*' && lineStr[charId+1] == '/') {
                                openedComents -= 1;
                                charId++;
                                if (openedComents <= 0) {
                                    break;
                                }
                            }
                            else if (lineStr[charId] == '/' && lineStr[charId+1] == '*') {
                                charId++;
                                openedComents += 1;
                            }
                            charId++;
                        }
                    }
                    if (openedComents <= 0) {
                        break;
                    }
                    lineId++;
                    charId = 0;
                }

                if (openedComents > 0) {
                    cerr << "Parsing error: missing closing multi-line comment (*/)\n";
                    cerr << "in file " << fileInfo->name() << " starting at line " << lineNumber << '\n';
                    return nullopt;
                }

                charId++; // skip closing / symbol
            }
            else if (!isspace(c)){
                // only whitespace characters don't get saved
                tokens.emplace_back(Token::Type::Symbol, string(1,c), lineNumber, charNumber, fileInfo, lineId);
                charId++;
            } else {
                charId++;
            }
        }
    }

    return tokens;
}

bool includeFile(vector<SourceStringLine>& sourceCode, fs::path includePath, FileInfo* parentFileInfo, int includeLine) {
    bool alreadyInserted = false;
    for (const auto& element : GVARS.fileInfos) {
        if (element.get()->path == includePath) {
            alreadyInserted = true;
            break;
        }
    }

    if (!alreadyInserted) {
        GVARS.fileInfos.emplace_back(make_unique<FileInfo>(includePath, parentFileInfo, includeLine));
        auto includedCode = getSourceFromFile(GVARS.fileInfos.back().get());

        // if reading source from included file failed we do not try to continue without
        if (!includedCode) {
            return false;
        }

        // append includedFile to the rest of sourceCode
        sourceCode.insert(sourceCode.end(), includedCode.value().begin(), includedCode.value().end());
    }

    return true;
}

optional<vector<SourceStringLine>> getSourceFromFile(FileInfo* fileInfo) {
    ifstream file(fileInfo->path.u8string());
    if (!file) {
        return nullopt;
    }

    vector<SourceStringLine> sourceCode;
    string line;
    int lineNumber = 1;
    while (getline(file, line)) {
        // if include directive then add source file from it
        int includeStrSize = sizeof("#include")-1;
        if (line.size() > includeStrSize && line.substr(0, includeStrSize) == "#include") {
            string includeFileName = line.substr(includeStrSize+1);
            auto includePath = createIncludePath(fileInfo, includeFileName, lineNumber);
            if (!includePath) {
                file.close();
                return nullopt;
            }
            if (fs::is_directory(includePath.value())) {
                for (auto& filePath : recursive_directory_range(includePath.value())) {
                    if (filePath.path().extension().u8string() == ".cdr") {
                        if (!includeFile(sourceCode, filePath, fileInfo, lineNumber)) {
                            file.close();
                            return nullopt;
                        }
                    }
                }
            } else {
                if (!includeFile(sourceCode, includePath.value(), fileInfo, lineNumber)) {
                    file.close();
                    return nullopt;
                }
            }
        } else {
            sourceCode.emplace_back(line, fileInfo, lineNumber);
        }
        lineNumber++;
    }
    file.close();
    return sourceCode;
}

optional<vector<Token>> parseFile(string fileName) {
    fs::path directory = fs::current_path();
    fs::path filePath = fs::path(fileName);
    auto path = directory / filePath;

    if (!fs::exists(path)) {
        cerr << "Provided file \"" + fileName + "\" does not exist\n";
        return nullopt;
    }
    if (!fs::is_regular_file(path)) {
        cerr << "Path " << path << " is not a file\n";
        return nullopt;
    }

    GVARS.fileInfos.emplace_back(make_unique<FileInfo>(path));
    auto sourceCode = getSourceFromFile(GVARS.fileInfos.back().get());
    if (!sourceCode) {
        return nullopt;
    }

    GVARS.sourceCode = sourceCode.value();
    return createTokens();
}