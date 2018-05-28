#pragma once
#include <string>

struct FileInfo {
    FileInfo(std::string name, FileInfo* parent=nullptr, int lineNumber=0) :
        name(name),
        parent(parent),
        includeLineNumber(lineNumber)
    {}

    FileInfo* parent;
    std::string name;
    int includeLineNumber;
};