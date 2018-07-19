#pragma once
#include <iostream>
#include <vector>
#include <optional>

#include "Token.h"
#include "keywords.h"
#include "CodeTreeTypes.h"
#include "globalVariables.h"

bool errorMessage(std::string message, const CodePosition& codePosition);
std::optional<CodeScope> createCodeTree(std::vector<Token> tokens);
