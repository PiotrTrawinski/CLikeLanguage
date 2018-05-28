#include "parsing.h"

using namespace std;


optional<vector<Token>> createTokens(vector<SourceStringLine> sourceCode) {
    return vector<Token>();
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
optional<vector<SourceStringLine>> getSourceFromFile(FileInfo fileInfo, unordered_set<string>& includedFiles) {
    auto [_iter, inserted] = includedFiles.insert(fileInfo.name);
    if (!inserted) {
        // no error so just return empty vector
        return vector<SourceStringLine>();
    }
    
    ifstream file(fileInfo.name);
    if (!file) {
        printErrorIncludeStack(fileInfo);
        return nullopt;
    }

    vector<SourceStringLine> sourceCode;
    string line;
    int lineNumber = 1;
    while (getline(file, line)) {
        // if include directive then add source file from it
        int includeStrSize = sizeof("#include")-1;
        if (line.size() > includeStrSize && line.substr(0, includeStrSize) == "#include") {

            FileInfo includedFile(line.substr(includeStrSize+1), &fileInfo, lineNumber);
            auto includedCode = getSourceFromFile(includedFile, includedFiles);
            
            // if reading source from included file failed we do not try to continue without
            if (!includedCode) {
                file.close();
                return nullopt;
            }

            // append includedFile to the rest of sourceCode
            sourceCode.insert(sourceCode.end(), includedCode.value().begin(), includedCode.value().end());
        } else {
            sourceCode.emplace_back(line, lineNumber);
        }

        lineNumber++;
    }

    file.close();
    return sourceCode;
}
pair<optional<vector<SourceStringLine>>, unordered_set<string>> getSourceFromFile(FileInfo fileInfo) {
    unordered_set<string> includedFiles;
    auto sourceCode = getSourceFromFile(fileInfo, includedFiles);
    return {sourceCode, includedFiles};
}

optional<vector<Token>> parseFile(string fileName) {
    auto [sourceCode, _includedFiles] = getSourceFromFile(FileInfo(fileName));
    if (!sourceCode) {
        return nullopt;
    }
    return createTokens(sourceCode.value());
}