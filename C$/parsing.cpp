#include "parsing.h"

using namespace std;


optional<vector<Token>> createTokens(const vector<SourceStringLine>& sourceCode) {
    vector<Token> tokens;

    for(int lineId = 0; lineId < sourceCode.size(); ++lineId){
        FileInfo* fileInfo = sourceCode[lineId].file;
        int lineNumber = sourceCode[lineId].number;
        string_view lineStr = sourceCode[lineId].value;
        int charId = 0;
        while (charId < lineStr.size()) {
            int charNumber = charId+1;
            char c = lineStr[charId];
            if (isalpha(c)) {
                string label = string(1, c);
                charId++;
                while (charId < lineStr.size() && (isalpha(lineStr[charId]) || isdigit(lineStr[charId]) || lineStr[charId]=='_')) {
                    label += lineStr[charId];
                    charId++;
                }
                tokens.emplace_back(Token::Type::Label, label, lineNumber, charNumber, fileInfo);
            }
            else if (c == '\"') {
                // for now there is no support of escape characters (like \" \n \t)
                string stringLiteral = "";
                charId++; // skip opening " symbol
                while (charId < lineStr.size() && lineStr[charId] != '\"') {
                    stringLiteral += lineStr[charId];
                    charId++;
                }
                charId++; // skip closing " symbol
                tokens.emplace_back(Token::Type::StringLiteral, stringLiteral, lineNumber, charNumber, fileInfo);
            }
            else if (c == '\'') {
                // for now there is no support of escape characters (like \" \n \t)
                charId++; // skip opening ' symbol
                if (charId >= lineStr.size()) {
                    cerr << "Parsing error: unexpected end of line at line " << lineNumber << '\n';
                    cerr << "didn't complete the definition of char literal defined at char " << charNumber << '\n';
                    return nullopt;
                }
                string character = string(1, lineStr[charId]);
                charId++; // skip char enclosed in ' symbols
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
                tokens.emplace_back(Token::Type::Char, character, lineNumber, charNumber, fileInfo);
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
                    tokens.emplace_back(Token::Type::Float, strNumber, lineNumber, charNumber, fileInfo);
                } else {
                    tokens.emplace_back(Token::Type::Integer, strNumber, lineNumber, charNumber, fileInfo);
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
                while (lineId < sourceCode.size()) {
                    fileInfo = sourceCode[lineId].file;
                    lineNumber = sourceCode[lineId].number;
                    lineStr = sourceCode[lineId].value;
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
                    if (openedComents <= 0) {
                        break;
                    }
                    lineId++;
                    charId = 0;
                }

                if (openedComents > 0) {
                    cerr << "Parsing error: missing closing multi-line comment (*/)\n";
                    cerr << "in file " << fileInfo->name << " starting at line " << lineNumber << '\n';
                    return nullopt;
                }

                charId++; // skip closing / symbol
            }
            else if (!isspace(c)){
                // only whitespace characters don't get saved
                tokens.emplace_back(Token::Type::Symbol, string(1,c), lineNumber, charNumber, fileInfo);
                charId++;
            } else {
                charId++;
            }
        }
    }

    return tokens;
}


void printErrorIncludeStack(FileInfo fileInfo) {
    cerr << "Include Error: Could not open source file \"" << fileInfo.name << "\"\n";
    FileInfo* parent = fileInfo.parent;
    FileInfo* child = &fileInfo;
    while (parent != nullptr) {
        cerr << "included in " << parent->name << " at line " << child->includeLineNumber << '\n';
        parent = parent->parent;
        child = child->parent;
    }
}
optional<vector<SourceStringLine>> getSourceFromFile(FileInfo* fileInfo, vector<unique_ptr<FileInfo>>& fileInfos) {
    ifstream file(fileInfo->name);
    if (!file) {
        printErrorIncludeStack(*fileInfo);
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
            bool alreadyInserted = false;
            for (const auto& element : fileInfos) {
                if (element.get()->name == includeFileName) {
                    alreadyInserted = true;
                    break;
                }
            }
            if (!alreadyInserted) {
                fileInfos.emplace_back(make_unique<FileInfo>(includeFileName, fileInfo, lineNumber));
                //FileInfo includedFile(line.substr(includeStrSize+1), fileInfo, lineNumber);
                auto includedCode = getSourceFromFile(fileInfos.back().get(), fileInfos);

                // if reading source from included file failed we do not try to continue without
                if (!includedCode) {
                    file.close();
                    return nullopt;
                }

                // append includedFile to the rest of sourceCode
                sourceCode.insert(sourceCode.end(), includedCode.value().begin(), includedCode.value().end());
            }
        } else {
            sourceCode.emplace_back(line, fileInfo, lineNumber);
        }

        lineNumber++;
    }

    file.close();
    return sourceCode;
}
tuple<optional<vector<SourceStringLine>>, vector<unique_ptr<FileInfo>>> getSourceFromFile(string fileName) {
    vector<unique_ptr<FileInfo>> fileInfos;
    fileInfos.emplace_back(make_unique<FileInfo>(fileName));
    auto sourceCode = getSourceFromFile(fileInfos.back().get(), fileInfos);
    return {sourceCode, move(fileInfos)};
}

optional<vector<Token>> parseFile(string fileName) {
    auto [sourceCode, fileInfos] = getSourceFromFile(fileName);
    if (!sourceCode) {
        return nullopt;
    }
    return createTokens(sourceCode.value());
}