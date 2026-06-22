#include "InstanceTree.h"
#include "core/Log.h"
#include <algorithm>

InstanceTree::InstanceTree(const Memory& mem, uintptr_t baseAddr)
    : m_mem(mem), m_base(baseAddr) {}

InstanceInfo InstanceTree::Read(uintptr_t addr) const {
    return { addr, GetName(addr), GetClassName(addr) };
}

std::string InstanceTree::GetName(uintptr_t addr) const {
    if (!addr) return {};
    if (auto r = m_mem.ReadString(addr + offsets::Instance::Name))
        return r.value;
    return {};
}

std::string InstanceTree::GetClassName(uintptr_t addr) const {
    if (!addr) return {};
    auto cd = m_mem.Read<uintptr_t>(addr + offsets::Instance::ClassDescriptor);
    if (!cd || !cd.value) return {};
    if (auto r = m_mem.ReadString(cd.value + offsets::Instance::ClassDescriptorToClassName))
        return r.value;
    return {};
}

bool InstanceTree::IsValid(uintptr_t addr) const {
    if (addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return false;
    auto cd = m_mem.Read<uintptr_t>(addr + offsets::Instance::ClassDescriptor);
    return cd && cd.value >= 0x10000 && cd.value <= 0x7FFFFFFFFFFF;
}

std::vector<uintptr_t> InstanceTree::GetChildren(uintptr_t addr) const {
    std::vector<uintptr_t> out;
    if (!addr) return out;

    auto start = m_mem.Read<uintptr_t>(addr + offsets::Instance::Children);
    auto end = m_mem.Read<uintptr_t>(addr + offsets::Instance::Children + offsets::Instance::ChildrenEnd);
    if (!start || !end || end.value <= start.value) return out;

    size_t count = (end.value - start.value) / sizeof(uintptr_t);
    if (count > 10000) return out;

    std::vector<uintptr_t> raw(count);
    if (m_mem.ReadRaw(start.value, raw.data(), count * sizeof(uintptr_t))) {
        for (auto p : raw)
            if (p && IsValid(p)) out.push_back(p);
    }
    return out;
}

uintptr_t InstanceTree::FindChild(uintptr_t parent, const std::string& target) const {
    for (auto c : GetChildren(parent))
        if (GetName(c) == target) return c;
    return 0;
}

uintptr_t InstanceTree::FindChildByClass(uintptr_t parent, const std::string& target) const {
    for (auto c : GetChildren(parent))
        if (GetClassName(c) == target) return c;
    return 0;
}

std::vector<std::string> InstanceTree::GetPath(uintptr_t addr) const {
    std::vector<std::string> path{GetName(addr)};
    uintptr_t cur = addr;
    for (int i = 0; i < 20; i++) {
        auto p = m_mem.Read<uintptr_t>(cur + offsets::Instance::Parent);
        if (!p || !IsValid(p.value)) break;
        auto gp = m_mem.Read<uintptr_t>(p.value + offsets::Instance::Parent);
        if (!gp.value) break;
        std::string name = GetName(p.value);
        if (name == "Game" || name == "game" || name.empty()) break;
        path.insert(path.begin(), name);
        cur = p.value;
    }
    path.insert(path.begin(), "Game");
    return path;
}

// --- DataModel discovery ---

uintptr_t InstanceTree::FindDataModelViaTS() const {
    auto tsPtr = m_mem.Read<uintptr_t>(m_base + offsets::Pointer::TaskScheduler);
    if (!tsPtr || tsPtr.value < 0x10000) return 0;

    auto start = m_mem.Read<uintptr_t>(tsPtr.value + offsets::TaskScheduler::JobStart);
    auto end = m_mem.Read<uintptr_t>(tsPtr.value + offsets::TaskScheduler::JobEnd);
    if (!start || !end || end.value <= start.value) return 0;

    size_t cnt = (end.value - start.value) / sizeof(uintptr_t);
    if (cnt > 500) cnt = 500;

    for (size_t i = 0; i < cnt; i++) {
        auto job = m_mem.Read<uintptr_t>(start.value + i * sizeof(uintptr_t));
        if (!job || !job.value) continue;
        auto dm = m_mem.Read<uintptr_t>(job.value + offsets::RenderJob::DataModel);
        if (!dm || dm.value < 0x10000) continue;
        auto n = GetName(dm.value);
        if (n == "Game" || n == "App") return dm.value;
    }
    return 0;
}

uintptr_t InstanceTree::FindDataModelViaFDM() const {
    auto fdm = m_mem.Read<uintptr_t>(m_base + offsets::Pointer::FakeDataModelPointer);
    if (!fdm || fdm.value < 0x10000) return 0;
    if (auto n = GetName(fdm.value); n == "Game" || n == "App") return fdm.value;

    auto dm = m_mem.Read<uintptr_t>(fdm.value + offsets::FakeDataModel::DataModel);
    if (dm && dm.value > 0x10000) {
        auto n = GetName(dm.value);
        if (n == "Game" || n == "App") return dm.value;
    }
    return 0;
}

uintptr_t InstanceTree::FindDataModelViaVE() const {
    auto ve = m_mem.Read<uintptr_t>(m_base + offsets::Pointer::VisualEnginePointer);
    if (!ve || ve.value < 0x10000) return 0;

    auto s1 = m_mem.Read<uintptr_t>(ve.value + offsets::VisualEngine::VisualEngineToDataModel1);
    if (s1 && s1.value > 0x10000) {
        auto dm = m_mem.Read<uintptr_t>(s1.value + offsets::VisualEngine::VisualEngineToDataModel2);
        if (dm && dm.value > 0x10000) {
            auto n = GetName(dm.value);
            if (n == "Game" || n == "App") return dm.value;
        }
    }
    return 0;
}

uintptr_t InstanceTree::FindDataModel() const {
    if (auto dm = FindDataModelViaTS()) { Log::Info("DM found via TaskScheduler"); return dm; }
    if (auto dm = FindDataModelViaFDM()) { Log::Info("DM found via FakeDataModel"); return dm; }
    if (auto dm = FindDataModelViaVE()) { Log::Info("DM found via VisualEngine"); return dm; }
    Log::Warn("DataModel not found via any method");
    return 0;
}
