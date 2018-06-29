#pragma once
#include <iostream>
#include <vector>
#include <optional>

#include "Token.h"
#include "keywords.h"
#include "CodeTreeTypes.h"

std::optional<CodeScope> interpret(std::vector<Token> tokens);
