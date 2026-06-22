#pragma once
#include <string>
#include <utility>
#include "core/Result.h"

class Compiler {
public:
    static Result<std::string> Compile(const std::string& source);
};
