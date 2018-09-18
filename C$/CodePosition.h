#pragma once

#include "FileInfo.h"

struct CodePosition {
    CodePosition(const FileInfo* fileInfo, int lineNumber, int charNumber, int lineId=-1) :
        fileInfo(fileInfo),
        lineNumber(lineNumber),
        charNumber(charNumber),
        lineId(lineId)
    {}

    const FileInfo* fileInfo;
    int lineNumber;
    int charNumber;
    int lineId;
};