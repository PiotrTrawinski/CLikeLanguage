#include "parsing.h"

using namespace std;


optional<vector<Token>> createTokens(const vector<SourceStringLine>& sourceCode) {
    vector<Token> tokens;

    for(int lineId = 0; lineId < sourceCode.size(); ++lineId){
        const FileInfo* fileInfo = sourceCode[lineId].file;
        const int lineNumber = sourceCode[lineId].number;
        const string& lineStr = sourceCode[lineId].value;
        int charId = 0;
        while (charId < lineStr.size()) {
            int charNumber = charId+1;
            char c = lineStr[charId];
            if (isalpha(c)) {
                //while()
            }
            else if (c == '\"') {

            }
            else if (c == '\'') {

            }
            else if (isdigit(c)) {

            }
            else if (c == '/' && charId < lineStr.size() - 1 && lineStr[charId + 1] == '/') {
                // single line comment
                break;
            }
            else if (c == '/' && charId < lineStr.size() - 1 && lineStr[charId + 1] == '*') {
                // multi-line comment
                
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