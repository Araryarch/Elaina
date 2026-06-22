#include "ExecutionService.h"
#include "Session.h"
#include "execution/Compiler.h"
#include "execution/Encoder.h"
#include "execution/Injector.h"
#include "execution/Payload.h"
#include "core/Log.h"

Result<std::vector<uint8_t>> ExecutionService::CompileAndEncode(const std::string& source) {
    Log::Info("Compiling (%zu chars)", source.size());

    auto compiled = Compiler::Compile(source);
    if (!compiled) return { Err::CompileFailed, compiled.message };

    auto encoded = Encoder::Encode(compiled.value);
    if (!encoded) return { Err::EncodeFailed, encoded.message };

    return std::vector<uint8_t>(encoded.value.begin(), encoded.value.end());
}

bool ExecutionService::Execute(Session& session, const std::string& script) {
    if (!session.IsAttached()) {
        Log::Error("Execute: not attached");
        return false;
    }

    auto rsb1 = CompileAndEncode(script);
    if (!rsb1) {
        Log::Error("Execute: %s", rsb1.ErrorMsg().c_str());
        return false;
    }

    if (!Injector::Execute(session.Mem(), session.Tree(), rsb1.value)) {
        Log::Error("Execute: injection failed");
        return false;
    }

    Log::Info("Script executed successfully");
    return true;
}

bool ExecutionService::InjectUnc(Session& session) {
    Log::Info("Injecting UNC API...");
    return Execute(session, kUncPayload);
}
