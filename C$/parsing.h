#pragma once
#include <vector>
#include <fstream>
#include <optional>
#include <iostream>
#include <unordered_set>

#include "SourceStringLine.h"
#include "FileInfo.h"
#include "Token.h"

std::optional<std::vector<Token>> parseFile(std::string fileName);