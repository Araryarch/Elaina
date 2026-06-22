#include "Memory.h"
#include "core/Log.h"
#include "instance/InstanceTree.h"
#include "instance/Offsets.h"

Memory::Memory(HANDLE hProcess) : m_process(hProcess) {}

bool Memory::ReadArray(uintptr_t addr, void* buf, uint32_t count) const {
    SIZE_T read = 0;
    NTSTATUS st = Syscall::ReadMemory(m_process, (void*)addr, buf, count, &read);
    return (st == 0 && read == count);
}

bool Memory::WriteArray(uintptr_t addr, const void* buf, uint32_t count) {
    SIZE_T written = 0;
    NTSTATUS st = Syscall::WriteMemory(m_process, (void*)addr, buf, count, &written);
    return (st == 0 && written == count);
}

Result<void> Memory::ReadRaw(uintptr_t addr, void* buf, size_t size) const {
    SIZE_T read = 0;
    NTSTATUS st = Syscall::ReadMemory(m_process, (void*)addr, buf, size, &read);
    if (st != 0 || read != size)
        return { Err::MemoryReadFailed, "addr=0x" + std::to_string(addr) + " size=" + std::to_string(size) };
    return {};
}

Result<void> Memory::WriteRaw(uintptr_t addr, const void* buf, size_t size) {
    SIZE_T written = 0;
    NTSTATUS st = Syscall::WriteMemory(m_process, (void*)addr, buf, size, &written);
    if (st != 0 || written != size)
        return { Err::MemoryWriteFailed, "addr=0x" + std::to_string(addr) + " size=" + std::to_string(size) };
    return {};
}

Result<std::string> Memory::ReadString(uintptr_t addr) const {
    // MSVC SSO string: +0 union, +0x10 length, +0x18 capacity
    struct { union { char buf[16]; uintptr_t ptr; }; size_t len; size_t cap; } s{};
    auto r = ReadRaw(addr, &s, sizeof(s));
    if (!r) return { r.error, r.message };

    if (s.len == 0 || s.len > 4096) return std::string();
    std::string out(s.len, '\0');

    if (s.cap > 15) {
        if (!s.ptr) return std::string();
        auto r2 = ReadRaw(s.ptr, out.data(), s.len);
        if (!r2) return { r2.error, r2.message };
    } else {
        memcpy(out.data(), s.buf, s.len);
    }
    return out;
}

Result<uintptr_t> Memory::Allocate(size_t size, uint32_t protect) {
    void* addr = nullptr;
    SIZE_T reg = size;
    NTSTATUS st = Syscall::AllocateMemory(m_process, &addr, reg, MEM_COMMIT | MEM_RESERVE, protect);
    if (st != 0 || !addr)
        return { Err::AllocFailed, "size=" + std::to_string(size) };
    return (uintptr_t)addr;
}

Result<uintptr_t> Memory::FindModuleScript(const std::string& name) const {
    InstanceTree tree(*this, (uintptr_t)m_process);
    auto dm = tree.FindDataModel();
    if (!dm) return { Err::ProcessRead, "DataModel not found" };

    auto cg = tree.FindChild(dm, "CoreGui");
    if (!cg) return { Err::ProcessRead, "CoreGui not found" };

    for (auto c : tree.GetChildren(cg)) {
        auto cn = tree.GetClassName(c);
        if (cn == "ModuleScript") {
            auto cn2 = tree.GetName(c);
            if (cn2 == name) return c;
        }
    }

    return { Err::ProcessRead, "ModuleScript \"" + name + "\" not found in CoreGui" };
}
