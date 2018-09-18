#pragma once
#include <unordered_set>
#include <memory>
#include <vector>
#include <unordered_map>

#include "SourceStringLine.h"
#include "FileInfo.h"

struct GlobalVariables {
    std::vector<std::unique_ptr<FileInfo>> fileInfos;
    std::vector<SourceStringLine> sourceCode;
    
    // for testing only
    void clear() {
        fileInfos.clear();
        sourceCode.clear();
    }
};
extern GlobalVariables GVARS;
