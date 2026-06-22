#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "core/Result.h"

class Session;

// Layer 3: orchestrates compile → encode → inject
class ExecutionService {
public:
    static Result<std::vector<uint8_t>> CompileAndEncode(const std::string& source);
    static bool Execute(Session& session, const std::string& script);
    static bool InjectUnc(Session& session);
};
