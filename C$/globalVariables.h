#pragma once
#include <unordered_set>
#include <memory>

#include "CodeTreeTypes.h"

// this variables hold data, that others can point to. 

extern std::vector<std::unique_ptr<FileInfo>> fileInfos;
extern std::unordered_set<std::unique_ptr<Type>> types;
