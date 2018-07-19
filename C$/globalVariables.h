#pragma once
#include <unordered_set>
#include <memory>
#include <vector>

#include "FileInfo.h"

// this variables hold data, that others can point to. 
struct GlobalVariables {
    std::vector<std::unique_ptr<FileInfo>> fileInfos;
   // std::unordered_set<std::unique_ptr<Type>> types;
    
    // for testing only
    void clear() {
        fileInfos.clear();
        //types.clear();
    }
};
extern GlobalVariables GVARS;
