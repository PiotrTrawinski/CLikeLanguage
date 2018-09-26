#include "parsing.h"
#include <filesystem>

using namespace std;
namespace fs = filesystem;

bool createTokens(FileInfo* fileInfo, vector<Token>& tokens, vector<SourceStringLine>& sourceCode);

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

void printErrorIncorrectPath(FileInfo* fileInfo, const std::string& fileName, int includeLine) {
    printfColorErr(COLOR::RED, "Include Error");
    printfColorErr(COLOR::GREY, ": ");
    printfColorErr(COLOR::MAGENTA, "%s", fileName.c_str());
    printfColorErr(COLOR::GREY, " does not exist\n");
    while (fileInfo) {
        printfColorErr(COLOR::GREY,    "included in");
        printfColorErr(COLOR::MAGENTA, " %s ", fileInfo->name().c_str());
        printfColorErr(COLOR::GREY,    "at line %d\n", includeLine);
        includeLine = fileInfo->includeLineNumber;
        fileInfo = fileInfo->parent;
    }
}

optional<fs::path> createIncludePath(FileInfo* fileInfo, const string& str, int includeLine) {
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

bool includeFile(vector<SourceStringLine>& sourceCode, vector<Token>& tokens, fs::path includePath, FileInfo* parentFileInfo, int includeLine) {
    bool alreadyInserted = false;
    for (const auto& element : GVARS.fileInfos) {
        if (element.get()->path == includePath) {
            alreadyInserted = true;
            break;
        }
    }

    if (!alreadyInserted) {
        GVARS.fileInfos.emplace_back(make_unique<FileInfo>(includePath, parentFileInfo, includeLine));

        if (!createTokens(GVARS.fileInfos.back().get(), tokens, sourceCode)) {
            return false;
        }
    }

    return true;
}

void printErrorCodePosition(FileInfo* fileInfo, int lineNumber, int charNumber) {
    printfColorErr(COLOR::MAGENTA, "%s", fileInfo->name().c_str());
    printfColorErr(COLOR::GREY, " (%d:%d) ", lineNumber, charNumber);
}

void printErrorCodeLine(const string& line, int charNumber) {
    printfColorErr(COLOR::WHITE, "%s\n", line.c_str());
    for (int i = 1; i < charNumber; ++i) {
        printfColorErr(COLOR::GREY, "-");
    }
    printfColorErr(COLOR::GREY, "^\n");
}

void printParsingError(const string& message, FileInfo* fileInfo, const string& line, int lineNumber, int charNumber) {
    printErrorCodePosition(fileInfo, lineNumber, charNumber);
    printfColorErr(COLOR::RED, "Parsing Error");
    printfColorErr(COLOR::GREY, ": %s\n", message.c_str());
    printErrorCodeLine(line, charNumber);
}

bool createTokens(FileInfo* fileInfo, vector<Token>& tokens, vector<SourceStringLine>& sourceCode) {
    ifstream file(fileInfo->path.u8string());
    if (!file) return false;

    string line;
    int lineNumber = 1;
    while (getline(file, line)) {
        int lineId = sourceCode.size();
        sourceCode.emplace_back(line, fileInfo, lineNumber);

        int charId = 0;
        while (charId < line.size()) {
            int charNumber = charId+1;
            char c = line[charId];
            if (c == '#' && charId + sizeof("#include") < line.size() && !line.compare(charId, sizeof("#include") - 1, "#include")) {
                string includeFileName = line.substr(charId+sizeof("#include"));
                auto includePath = createIncludePath(fileInfo, includeFileName, lineNumber);
                if (!includePath) {
                    file.close();
                    return false;
                }
                if (fs::is_directory(includePath.value())) {
                    for (auto& filePath : recursive_directory_range(includePath.value())) {
                        if (filePath.path().extension().u8string() == ".cdr") {
                            if (!includeFile(sourceCode, tokens, filePath, fileInfo, lineNumber)) {
                                file.close();
                                return false;
                            }
                        }
                    }
                } else {
                    if (!includeFile(sourceCode, tokens, includePath.value(), fileInfo, lineNumber)) {
                        file.close();
                        return false;
                    }
                }
                break;
            }
            else if (isalpha(c) || c == '_') {
                string label = string(1, c);
                charId++;
                while (charId < line.size() && (isalpha(line[charId]) || isdigit(line[charId]) || line[charId]=='_')) {
                    label += line[charId];
                    charId++;
                }
                tokens.emplace_back(Token::Type::Label, label, lineNumber, charNumber, fileInfo, lineId);
            }
            else if (c == '`') {
                string stringLiteral = "`";
                charId++; // skip opening ` symbol
                while (charId < line.size() && line[charId] != '`') {
                    stringLiteral += getCharacter(line, charId);
                }
                charId++; // skip closing ` symbol
                tokens.emplace_back(Token::Type::StringLiteral, stringLiteral, lineNumber, charNumber, fileInfo, lineId);
            }
            else if (c == '"') {
                string rawStringLiteral = "\"";
                charId++; // skip opening " symbol
                while (charId < line.size() && line[charId] != '"') {
                    rawStringLiteral += getCharacter(line, charId);
                }
                charId++; // skip closing " symbol
                tokens.emplace_back(Token::Type::RawStringLiteral, rawStringLiteral, lineNumber, charNumber, fileInfo, lineId);
            }
            else if (c == '\'') {
                charId++; // skip opening ' symbol
                if (charId >= line.size()) {
                    printParsingError("didn't complete the definition of char literal", fileInfo, line, lineNumber, charNumber);
                    return false;
                }
                string character = "'"s + getCharacter(line, charId);
                if (charId >= line.size()) {
                    printParsingError("missing closing ' symbol for char literal", fileInfo, line, lineNumber, charNumber);
                    return false;
                }
                if (line[charId] != '\'') {
                    printParsingError("missing closing ' symbol for char literal", fileInfo, line, lineNumber, charNumber);
                    return false;
                }
                charId++; // skip closing ' symbol
                tokens.emplace_back(Token::Type::Char, character, lineNumber, charNumber, fileInfo, lineId);
            }
            else if (isdigit(c)) {
                string strNumber = string(1, c);
                charId++;
                bool haveDot = false;
                while (charId < line.size() && (isdigit(line[charId]) || line[charId]=='.')) {
                    if (line[charId] == '.') {
                        if (haveDot) {
                            printParsingError("too many dots in number", fileInfo, line, lineNumber, charNumber);
                            return false;
                        }
                        haveDot = true;
                    }
                    strNumber += line[charId];
                    charId++;
                }
                if (haveDot) {
                    tokens.emplace_back(Token::Type::Float, strNumber, lineNumber, charNumber, fileInfo, lineId);
                } else {
                    tokens.emplace_back(Token::Type::Integer, strNumber, lineNumber, charNumber, fileInfo, lineId);
                }
            }
            else if (c == '/' && charId < line.size() - 1 && line[charId + 1] == '/') {
                // single line comment
                break;
            }
            else if (c == '/' && charId < line.size() - 1 && line[charId + 1] == '*') {
                // multi-line comment
                auto oldLine = line;
                charId += 2; // skip '/' and '*' symbols
                // skip everything till appropriate closing comment (*/) string
                // (nested coments work -> /* ... /* ... */ ... */ is corrent syntax)
                int openedComents = 1;
                do {
                    while (charId+1 < line.size()) {
                        if (line[charId] == '*' && line[charId+1] == '/') {
                            openedComents -= 1;
                            charId++;
                            if (openedComents <= 0) {
                                charId += 1;
                                break;
                            }
                        }
                        else if (line[charId] == '/' && line[charId+1] == '*') {
                            charId++;
                            openedComents += 1;
                        }
                        charId++;
                    }
                    if (openedComents <= 0) break;
                    if (!getline(file, line)) break;
                    lineId = sourceCode.size();
                    sourceCode.emplace_back(line, fileInfo, lineNumber);
                    charId = 0;
                } while(true);
                if (openedComents > 0) {
                    printParsingError("missing closing multi-line comment (*/)", fileInfo, oldLine, lineNumber, charNumber);
                    return false;
                }
            }
            else if (!isspace(c)) {
                // only whitespace characters don't get saved
                tokens.emplace_back(Token::Type::Symbol, string(1,c), lineNumber, charNumber, fileInfo, lineId);
                charId++;
            } else {
                charId++;
            }
        }
        lineNumber += 1;
    }
    file.close();

    return true;
}

optional<vector<Token>> createTokens(FileInfo* fileInfo) {
    vector<Token> tokens;
    if(createTokens(fileInfo, tokens, GVARS.sourceCode)) {
        return tokens;
    } else {
        return nullopt;
    }
}

optional<vector<Token>> parseFile(string fileName) {
    fs::path directory = fs::current_path();
    fs::path filePath = fs::path(fileName);
    auto path = directory / filePath;

    if (!fs::exists(path)) {
        printfColorErr(COLOR::RED, "Argument Error");
        printfColorErr(COLOR::GREY, ": provided file ");
        printfColorErr(COLOR::MAGENTA, "%s", fileName.c_str());
        printfColorErr(COLOR::GREY, " does not exist\n");
        return nullopt;
    }
    if (!fs::is_regular_file(path)) {
        printfColorErr(COLOR::RED, "Argument Error");
        printfColorErr(COLOR::GREY, ": path ");
        printfColorErr(COLOR::MAGENTA, "%s", fileName.c_str());
        printfColorErr(COLOR::GREY, " is not a file\n");
        return nullopt;
    }

    GVARS.fileInfos.emplace_back(make_unique<FileInfo>(path));
    return createTokens(GVARS.fileInfos.back().get());
}