#include "Session.h"
#include "core/Log.h"

bool Session::Attach() {
    if (m_handle.Valid()) {
        Log::Warn("Already attached, detach first");
        return true;
    }

    auto result = Process::AttachFirst();
    if (!result) {
        Log::Error("Session::Attach — %s", result.ErrorMsg().c_str());
        return false;
    }

    m_handle = std::move(result.value);
    m_mem = std::make_unique<Memory>(m_handle.handle, m_handle.base);
    m_tree = std::make_unique<InstanceTree>(*m_mem, m_handle.base);

    Log::Info("Session attached to PID %u", m_handle.pid);
    return true;
}

void Session::Detach() {
    m_tree.reset();
    m_mem.reset();
    m_handle = RobloxHandle{};
    Log::Info("Session detached");
}
