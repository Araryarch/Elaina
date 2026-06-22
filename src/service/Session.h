#pragma once
#include <memory>
#include "process/Process.h"
#include "process/Memory.h"
#include "instance/InstanceTree.h"

// Layer 3: owns platform + roblox state together
class Session {
public:
    bool Attach();
    void Detach();
    bool IsAttached() const { return m_handle.Valid(); }

    Memory& Mem() { return *m_mem; }
    InstanceTree& Tree() { return *m_tree; }
    RobloxHandle& Handle() { return m_handle; }

private:
    RobloxHandle m_handle;
    std::unique_ptr<Memory> m_mem;
    std::unique_ptr<InstanceTree> m_tree;
};
