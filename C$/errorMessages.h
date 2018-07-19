#pragma once

#include <iostream>

#include "CodePosition.h"

bool errorMessage(std::string message, const CodePosition& codePosition);
bool internalError(std::string message, const CodePosition& codePosition);
