#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "process/Memory.h"
#include "Offsets.h"

struct InstanceInfo {
    uintptr_t address{};
    std::string name;
    std::string className;
};

class InstanceTree {
public:
    explicit InstanceTree(const Memory& mem, uintptr_t baseAddr);

    InstanceInfo Read(uintptr_t addr) const;
    std::string GetName(uintptr_t addr) const;
    std::string GetClassName(uintptr_t addr) const;
    bool IsValid(uintptr_t addr) const;

    std::vector<uintptr_t> GetChildren(uintptr_t addr) const;
    uintptr_t FindChild(uintptr_t parent, const std::string& target) const;
    uintptr_t FindChildByClass(uintptr_t parent, const std::string& target) const;
    std::vector<std::string> GetPath(uintptr_t addr) const;

    // DataModel discovery
    uintptr_t FindDataModel() const;
    uintptr_t FindDataModelViaTS() const;
    uintptr_t FindDataModelViaFDM() const;
    uintptr_t FindDataModelViaVE() const;

    uintptr_t FindModuleScript(const std::string& name) const;

private:
    const Memory& m_mem;
    uintptr_t m_base;
};
