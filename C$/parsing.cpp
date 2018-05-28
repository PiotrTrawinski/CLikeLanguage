#include "parsing.h"

using namespace std;


optional<vector<Token>> createTokens(vector<SourceStringLine> sourceCode) {
    return nullopt;
}


void printErrorIncludeStack(FileInfo fileInfo) {
    cerr << "Include Error: Could not open source file \"" << fileInfo.name << "\"\n";
    FileInfo* parent = fileInfo.parent;
    while (parent != nullptr) {
        cerr << "included in " << parent->name << " at line " << parent->includeLineNumber << '\n';
        parent = parent->parent;
    }
}
optional<vector<SourceStringLine>> getSourceFromFile(FileInfo fileInfo) {
    ifstream file(fileInfo.name);
    if (!file) {
        printErrorIncludeStack(fileInfo);
        return nullopt;
    }

    vector<SourceStringLine> sourceCode;
    string line;
    int lineNumber = 0;
    while (getline(file, line)) {
        // if include directive then add source file from it
        int includeStrSize = sizeof("#include")-1;
        if (line.size() > includeStrSize && line.substr(0, includeStrSize) == "#include") {

            FileInfo includedFile(line.substr(includeStrSize+1), &fileInfo, lineNumber);
            auto includedCode = getSourceFromFile(includedFile);
            
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

optional<vector<Token>> parseFile(string fileName) {
    auto sourceCode = getSourceFromFile(FileInfo(fileName));
    if (!sourceCode) {
        return nullopt;
    }
    return createTokens(sourceCode.value());
}