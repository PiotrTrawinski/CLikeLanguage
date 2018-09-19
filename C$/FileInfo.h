#pragma once
#include <string>
#include <filesystem>

struct FileInfo {
    FileInfo(){}
    FileInfo(std::filesystem::path path, FileInfo* parent=nullptr, int lineNumber=0) :
        path(path),
        parent(parent),
        includeLineNumber(lineNumber)
    {}

    std::string name() const {
        return path.filename().u8string();
    }

    FileInfo* parent;
    std::filesystem::path path;
    int includeLineNumber;
};