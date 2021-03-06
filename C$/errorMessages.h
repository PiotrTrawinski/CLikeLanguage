#pragma once

#include <iostream>
#include <optional>

#include "CodePosition.h"
#include "globalVariables.h"
#include "consoleColors.h"

bool errorMessageBool(const std::string& message, const CodePosition& codePosition=CodePosition(nullptr,0,0));
std::nullopt_t errorMessageOpt(const std::string& message, const CodePosition& codePosition=CodePosition(nullptr,0,0));
std::nullptr_t errorMessageNull(const std::string& message, const CodePosition& codePosition=CodePosition(nullptr,0,0));
void internalError(const std::string& message, const CodePosition& codePosition=CodePosition(nullptr,0,0));
void warningMessage(const std::string& message, const CodePosition& codePosition=CodePosition(nullptr,0,0));