#pragma once

#include "FileInfo.h"

struct CodePosition {
    CodePosition(const FileInfo* fileInfo, int lineNumber, int charNumber) :
        fileInfo(fileInfo),
        lineNumber(lineNumber),
        charNumber(charNumber)
    {}

    const FileInfo* fileInfo;
    int lineNumber;
    int charNumber;
};