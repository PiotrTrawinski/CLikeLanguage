#pragma once
#include "LlvmObject.h"
#include "Scope.h"

std::unique_ptr<LlvmObject> createLlvm(CodeScope* globalScope);