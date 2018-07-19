#pragma once
#include "Scope.h"
#include "Token.h"

std::optional<CodeScope> createCodeTree(std::vector<Token> tokens);
