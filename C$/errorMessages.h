#pragma once

#include <iostream>

#include "CodePosition.h"

bool errorMessage(std::string message, const CodePosition& codePosition=CodePosition(nullptr,0,0));
bool internalError(std::string message, const CodePosition& codePosition=CodePosition(nullptr,0,0));
