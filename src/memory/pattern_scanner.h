#pragma once
#include <Windows.h>
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include "process/scanner.h"

class PatternScanner {
public:
    PatternScanner(HANDLE hProcess, uintptr_t baseAddress)
        : m_process(hProcess), m_base(baseAddress) {}

    struct PatternByte {
        uint8_t value;
        bool isWildcard;
    };

    static std::vector<PatternByte> ParsePattern(const std::string& pattern) {
        std::vector<PatternByte> result;
        std::istringstream stream(pattern);
        std::string token;

        while (stream >> token) {
            if (token == "??" || token == "?") {
                result.push_back({ 0, true });
            } else {
                result.push_back({ static_cast<uint8_t>(std::stoul(token, nullptr, 16)), false });
            }
        }
        return result;
    }

    uintptr_t ScanRegion(uintptr_t start, size_t size, const std::vector<PatternByte>& pattern) {
        if (pattern.empty() || size < pattern.size()) return 0;

        std::vector<uint8_t> buffer(size);
        if (!ProcessScanner::ReadMemory(m_process, start, buffer.data(), size)) return 0;

        for (size_t i = 0; i <= buffer.size() - pattern.size(); i++) {
            bool found = true;
            for (size_t j = 0; j < pattern.size(); j++) {
                if (!pattern[j].isWildcard && buffer[i + j] != pattern[j].value) {
                    found = false;
                    break;
                }
            }
            if (found) return start + i;
        }
        return 0;
    }

    uintptr_t ScanModule(const std::string& patternStr) {
        auto pattern = ParsePattern(patternStr);
        if (pattern.empty()) return 0;

        IMAGE_DOS_HEADER dosHeader{};
        if (!ProcessScanner::ReadMemory(m_process, m_base, &dosHeader, sizeof(dosHeader))) return 0;
        if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) return 0;

        IMAGE_NT_HEADERS64 ntHeaders{};
        if (!ProcessScanner::ReadMemory(m_process, m_base + dosHeader.e_lfanew, &ntHeaders, sizeof(ntHeaders))) return 0;
        if (ntHeaders.Signature != IMAGE_NT_SIGNATURE) return 0;

        WORD numSections = ntHeaders.FileHeader.NumberOfSections;
        uintptr_t sectionHeadersAddr = m_base + dosHeader.e_lfanew + sizeof(IMAGE_NT_HEADERS64);

        for (WORD i = 0; i < numSections; i++) {
            IMAGE_SECTION_HEADER section{};
            if (!ProcessScanner::ReadMemory(m_process, sectionHeadersAddr + i * sizeof(IMAGE_SECTION_HEADER),
                &section, sizeof(section))) continue;

            if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;

            uintptr_t sectionStart = m_base + section.VirtualAddress;
            size_t sectionSize = section.Misc.VirtualSize;

            const size_t chunkSize = 0x100000;
            for (size_t offset = 0; offset < sectionSize; offset += chunkSize - pattern.size()) {
                size_t currentSize = min(chunkSize, sectionSize - offset);
                uintptr_t result = ScanRegion(sectionStart + offset, currentSize, pattern);
                if (result) return result;
            }
        }
        return 0;
    }

    uintptr_t ScanAllSections(const std::string& patternStr) {
        auto pattern = ParsePattern(patternStr);
        if (pattern.empty()) return 0;

        IMAGE_DOS_HEADER dosHeader{};
        if (!ProcessScanner::ReadMemory(m_process, m_base, &dosHeader, sizeof(dosHeader))) return 0;

        IMAGE_NT_HEADERS64 ntHeaders{};
        if (!ProcessScanner::ReadMemory(m_process, m_base + dosHeader.e_lfanew, &ntHeaders, sizeof(ntHeaders))) return 0;

        WORD numSections = ntHeaders.FileHeader.NumberOfSections;
        uintptr_t sectionHeadersAddr = m_base + dosHeader.e_lfanew + sizeof(IMAGE_NT_HEADERS64);

        for (WORD i = 0; i < numSections; i++) {
            IMAGE_SECTION_HEADER section{};
            if (!ProcessScanner::ReadMemory(m_process, sectionHeadersAddr + i * sizeof(IMAGE_SECTION_HEADER),
                &section, sizeof(section))) continue;

            uintptr_t sectionStart = m_base + section.VirtualAddress;
            size_t sectionSize = section.Misc.VirtualSize;
            if (sectionSize == 0) continue;

            const size_t chunkSize = 0x100000;
            for (size_t offset = 0; offset < sectionSize; offset += chunkSize - pattern.size()) {
                size_t currentSize = min(chunkSize, sectionSize - offset);
                uintptr_t result = ScanRegion(sectionStart + offset, currentSize, pattern);
                if (result) return result;
            }
        }
        return 0;
    }

    uintptr_t ResolveRIPRelative(uintptr_t instrAddr, int instrLength) {
        int32_t displacement = ProcessScanner::Read<int32_t>(m_process, instrAddr + instrLength - 4);
        return instrAddr + instrLength + displacement;
    }

    uintptr_t FindTaskScheduler() {
        std::string searchBytes;
        const char* ts = "TaskScheduler";
        for (int i = 0; ts[i]; i++) {
            char hex[4];
            snprintf(hex, sizeof(hex), "%02X ", (unsigned char)ts[i]);
            searchBytes += hex;
        }

        uintptr_t stringAddr = ScanAllSections(searchBytes);
        if (!stringAddr) return 0;

        IMAGE_DOS_HEADER dosHeader{};
        ProcessScanner::ReadMemory(m_process, m_base, &dosHeader, sizeof(dosHeader));
        IMAGE_NT_HEADERS64 ntHeaders{};
        ProcessScanner::ReadMemory(m_process, m_base + dosHeader.e_lfanew, &ntHeaders, sizeof(ntHeaders));

        WORD numSections = ntHeaders.FileHeader.NumberOfSections;
        uintptr_t sectionHeadersAddr = m_base + dosHeader.e_lfanew + sizeof(IMAGE_NT_HEADERS64);

        for (WORD si = 0; si < numSections; si++) {
            IMAGE_SECTION_HEADER section{};
            ProcessScanner::ReadMemory(m_process, sectionHeadersAddr + si * sizeof(IMAGE_SECTION_HEADER),
                &section, sizeof(section));

            if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;

            uintptr_t sectionStart = m_base + section.VirtualAddress;
            size_t sectionSize = section.Misc.VirtualSize;

            const size_t chunkSize = 0x200000;
            for (size_t offset = 0; offset < sectionSize; offset += chunkSize - 64) {
                size_t currentSize = min(chunkSize, sectionSize - offset);
                std::vector<uint8_t> buffer(currentSize);
                if (!ProcessScanner::ReadMemory(m_process, sectionStart + offset, buffer.data(), currentSize))
                    continue;

                for (size_t i = 0; i + 7 < currentSize; i++) {
                    if ((buffer[i] == 0x48 || buffer[i] == 0x4C) && buffer[i + 1] == 0x8D) {
                        uint8_t modrm = buffer[i + 2];
                        if ((modrm & 0xC7) == 0x05) {
                            int32_t disp = *reinterpret_cast<int32_t*>(&buffer[i + 3]);
                            uintptr_t instrAddr = sectionStart + offset + i;
                            uintptr_t targetAddr = instrAddr + 7 + disp;

                            if (targetAddr == stringAddr) {
                                size_t searchStart = (i > 200) ? i - 200 : 0;
                                for (size_t j = searchStart; j < i; j++) {
                                    if (buffer[j] == 0x48 && buffer[j + 1] == 0x8B) {
                                        uint8_t mod = buffer[j + 2];
                                        if ((mod & 0xC7) == 0x05) {
                                            int32_t d = *reinterpret_cast<int32_t*>(&buffer[j + 3]);
                                            uintptr_t globalAddr = (sectionStart + offset + j) + 7 + d;
                                            uintptr_t tsPtr = ProcessScanner::Read<uintptr_t>(m_process, globalAddr);
                                            if (tsPtr > 0x10000 && tsPtr < 0x7FFFFFFFFFFF) return globalAddr;
                                        }
                                    }
                                }

                                for (size_t j = i + 7; j < i + 100 && j + 7 < currentSize; j++) {
                                    if (buffer[j] == 0x48 && buffer[j + 1] == 0x8B) {
                                        uint8_t mod = buffer[j + 2];
                                        if ((mod & 0xC7) == 0x05) {
                                            int32_t d = *reinterpret_cast<int32_t*>(&buffer[j + 3]);
                                            uintptr_t globalAddr = (sectionStart + offset + j) + 7 + d;
                                            uintptr_t tsPtr = ProcessScanner::Read<uintptr_t>(m_process, globalAddr);
                                            if (tsPtr > 0x10000 && tsPtr < 0x7FFFFFFFFFFF) return globalAddr;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        return 0;
    }

    size_t GetModuleSize() {
        IMAGE_DOS_HEADER dosHeader{};
        if (!ProcessScanner::ReadMemory(m_process, m_base, &dosHeader, sizeof(dosHeader))) return 0;
        IMAGE_NT_HEADERS64 ntHeaders{};
        if (!ProcessScanner::ReadMemory(m_process, m_base + dosHeader.e_lfanew, &ntHeaders, sizeof(ntHeaders))) return 0;
        return ntHeaders.OptionalHeader.SizeOfImage;
    }

private:
    HANDLE m_process;
    uintptr_t m_base;
};
