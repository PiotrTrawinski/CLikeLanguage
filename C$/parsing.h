#pragma once
#include <vector>
#include <fstream>
#include <optional>
#include <iostream>
#include <unordered_set>
#include <memory>
#include <tuple>

#include "SourceStringLine.h"
#include "FileInfo.h"
#include "Token.h"
#include "globalVariables.h"
#include "consoleColors.h"

std::optional<std::vector<Token>> parseFile(std::string fileName);