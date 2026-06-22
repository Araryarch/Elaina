#include "Memory.h"
#include "core/Log.h"
#include <cctype>

Memory::Memory(HANDLE hProcess, uintptr_t baseAddr) : m_process(hProcess), m_base(baseAddr) {}

bool Memory::ReadArray(uintptr_t addr, void* buf, uint32_t count) const {
    SIZE_T read = 0;
    NTSTATUS st = Syscall::ReadMemory(m_process, (void*)addr, buf, count, &read);
    return (NT_SUCCESS(st) && read == count);
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

SectionInfo Memory::GetSection(const char* name) const {
    SectionInfo info{};
    auto dosHdr = Read<IMAGE_DOS_HEADER>(m_base);
    if (!dosHdr || dosHdr.value.e_magic != IMAGE_DOS_SIGNATURE) return info;
    auto ntHdr = Read<IMAGE_NT_HEADERS64>(m_base + dosHdr.value.e_lfanew);
    if (!ntHdr || ntHdr.value.Signature != IMAGE_NT_SIGNATURE) return info;

    uintptr_t secAddr = m_base + dosHdr.value.e_lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
                        ntHdr.value.FileHeader.SizeOfOptionalHeader;
    for (WORD i = 0; i < ntHdr.value.FileHeader.NumberOfSections; i++) {
        IMAGE_SECTION_HEADER sec{};
        if (!ReadArray(secAddr + i * sizeof(sec), &sec, sizeof(sec))) continue;
        char secName[9] = {};
        memcpy(secName, sec.Name, 8);
        if (_stricmp(secName, name) == 0) {
            info.start = m_base + sec.VirtualAddress;
            info.size = sec.SizeOfRawData ? sec.SizeOfRawData : sec.Misc.VirtualSize;
            break;
        }
    }
    return info;
}

std::vector<PatternByte> Memory::ParsePattern(const std::string& pattern) {
    std::vector<PatternByte> bytes;
    size_t i = 0;
    while (i < pattern.size()) {
        if (pattern[i] == ' ' || pattern[i] == '\t') { i++; continue; }
        if (pattern[i] == '?') {
            bytes.push_back({0, true});
            i++;
            if (i < pattern.size() && pattern[i] == '?') i++;
        } else if (isxdigit((unsigned char)pattern[i]) && i + 1 < pattern.size() && isxdigit((unsigned char)pattern[i + 1])) {
            uint8_t b = (uint8_t)strtol(pattern.substr(i, 2).c_str(), nullptr, 16);
            bytes.push_back({b, false});
            i += 2;
        } else {
            i++;
        }
    }
    return bytes;
}

uintptr_t Memory::ScanPattern(uintptr_t start, size_t size, const std::vector<PatternByte>& pattern) const {
    if (pattern.empty() || size == 0) return 0;
    const size_t bufSize = 0x10000;
    std::vector<uint8_t> buffer(bufSize);
    size_t patternSize = pattern.size();

    for (uint64_t off = 0; off + patternSize <= size; off += bufSize - patternSize) {
        uint64_t toRead = (std::min)((uint64_t)bufSize, size - off);
        if (!ReadArray(start + off, buffer.data(), (uint32_t)toRead)) continue;

        for (uint64_t i = 0; i + patternSize <= toRead; i++) {
            bool match = true;
            for (size_t j = 0; j < patternSize && match; j++) {
                if (!pattern[j].wildcard && buffer[i + j] != pattern[j].data)
                    match = false;
            }
            if (match) return start + off + i;
        }
    }
    return 0;
}

uintptr_t Memory::ScanSection(const char* section, const std::string& patternStr) const {
    auto sec = GetSection(section);
    if (!sec.start || !sec.size) return 0;
    auto pattern = ParsePattern(patternStr);
    if (pattern.empty()) return 0;
    auto result = ScanPattern(sec.start, sec.size, pattern);
    if (result) return result;
    return 0;
}

Result<HANDLE> Memory::CreateRemoteThread(void* startAddr, void* param) const {
    HANDLE hThread = nullptr;
    NTSTATUS st = Syscall::CreateThreadEx(&hThread, THREAD_ALL_ACCESS, nullptr,
                                           m_process, startAddr, param);
    if (st != 0 || !hThread)
        return { Err::InjectFailed, "NtCreateThreadEx: " + std::to_string(st) };
    return hThread;
}


