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