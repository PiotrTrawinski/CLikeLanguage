#pragma once
#include <string>
#include "FileInfo.h"

struct SourceStringLine {
    SourceStringLine(std::string value, FileInfo* fileInfo, int number) :
        value(value),
        file(fileInfo),
        number(number)
    {}

    std::string value;
    FileInfo* file;
    int number;
};