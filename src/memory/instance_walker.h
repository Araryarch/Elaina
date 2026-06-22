#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <functional>
#include "process/scanner.h"
#include "offsets.h"

struct RobloxInstance {
    uintptr_t address = 0;
    std::string name;
    std::string className;
};

class InstanceWalker {
public:
    InstanceWalker(HANDLE hProcess, uintptr_t baseAddress)
        : m_process(hProcess), m_base(baseAddress) {}

    std::string GetName(uintptr_t instance) {
        if (!instance) return "";
        return ProcessScanner::ReadString(m_process, instance + offsets::Instance::Name);
    }

    std::string GetClassName(uintptr_t instance) {
        if (!instance) return "";
        uintptr_t classDesc = ProcessScanner::Read<uintptr_t>(m_process, instance + offsets::Instance::ClassDescriptor);
        if (!classDesc) return "";
        return ProcessScanner::ReadString(m_process, classDesc + offsets::Instance::ClassDescriptorToClassName);
    }

    uintptr_t GetParent(uintptr_t instance) {
        if (!instance) return 0;
        return ProcessScanner::Read<uintptr_t>(m_process, instance + offsets::Instance::Parent);
    }

    std::vector<uintptr_t> GetChildren(uintptr_t instance) {
        std::vector<uintptr_t> children;
        if (!instance) return children;

        uintptr_t childrenStart = ProcessScanner::Read<uintptr_t>(m_process, instance + offsets::Instance::Children);
        uintptr_t childrenEnd = ProcessScanner::Read<uintptr_t>(m_process,
            instance + offsets::Instance::Children + offsets::Instance::ChildrenEnd);

        if (!childrenStart || !childrenEnd || childrenEnd <= childrenStart) return children;

        size_t count = (childrenEnd - childrenStart) / sizeof(uintptr_t);
        if (count > 10000) return children;

        std::vector<uintptr_t> ptrs(count);
        if (ProcessScanner::ReadMemory(m_process, childrenStart, ptrs.data(), count * sizeof(uintptr_t))) {
            for (auto ptr : ptrs) {
                if (ptr != 0) children.push_back(ptr);
            }
        }
        return children;
    }

    uintptr_t FindDataModelViaTaskScheduler() {
        uintptr_t tsGlobal = m_base + offsets::Pointer::TaskScheduler;
        uintptr_t ts = ProcessScanner::Read<uintptr_t>(m_process, tsGlobal);
        if (!ts || ts < 0x10000) return 0;

        uintptr_t jobStart = ProcessScanner::Read<uintptr_t>(m_process, ts + offsets::TaskScheduler::JobStart);
        uintptr_t jobEnd = ProcessScanner::Read<uintptr_t>(m_process, ts + offsets::TaskScheduler::JobEnd);

        if (!jobStart || !jobEnd || jobEnd <= jobStart) return 0;

        size_t jobCount = (jobEnd - jobStart) / sizeof(uintptr_t);
        if (jobCount > 500) jobCount = 500;

        for (size_t i = 0; i < jobCount; i++) {
            uintptr_t jobPtr = ProcessScanner::Read<uintptr_t>(m_process, jobStart + i * sizeof(uintptr_t));
            if (!jobPtr) continue;

            std::string jobName = ProcessScanner::ReadString(m_process, jobPtr + offsets::Jobs::JobName);
            uintptr_t dm = ProcessScanner::Read<uintptr_t>(m_process, jobPtr + offsets::RenderJob::DataModel);
            if (dm > 0x10000 && dm < 0x7FFFFFFFFFFF) {
                std::string name = GetName(dm);
                if (name == "Game" || name == "App") return dm;
            }
        }
        return 0;
    }

    uintptr_t FindDataModelViaFakePointer() {
        uintptr_t fakePtr = ProcessScanner::Read<uintptr_t>(m_process, m_base + offsets::Pointer::FakeDataModelPointer);
        if (!fakePtr || fakePtr < 0x10000) return 0;

        std::string name = GetName(fakePtr);
        if (name == "Game" || name == "App") return fakePtr;

        uintptr_t dm = ProcessScanner::Read<uintptr_t>(m_process, fakePtr + offsets::FakeDataModel::DataModel);
        if (dm > 0x10000 && dm < 0x7FFFFFFFFFFF) {
            name = GetName(dm);
            if (name == "Game" || name == "App") return dm;
        }
        return 0;
    }

    uintptr_t FindDataModelViaVisualEngine() {
        uintptr_t veGlobal = ProcessScanner::Read<uintptr_t>(m_process, m_base + offsets::Pointer::VisualEnginePointer);
        if (!veGlobal || veGlobal < 0x10000) return 0;

        uintptr_t step1 = ProcessScanner::Read<uintptr_t>(m_process, veGlobal + offsets::VisualEngine::VisualEngineToDataModel1);
        if (step1 > 0x10000 && step1 < 0x7FFFFFFFFFFF) {
            uintptr_t dm = ProcessScanner::Read<uintptr_t>(m_process, step1 + offsets::VisualEngine::VisualEngineToDataModel2);
            if (dm > 0x10000 && dm < 0x7FFFFFFFFFFF) {
                std::string name = GetName(dm);
                if (name == "Game" || name == "App") return dm;
            }
        }
        return 0;
    }

    uintptr_t FindDataModelBruteForce() {
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

            if (!(section.Characteristics & IMAGE_SCN_MEM_WRITE)) continue;

            char secName[9] = {};
            memcpy(secName, section.Name, 8);

            uintptr_t sectionStart = m_base + section.VirtualAddress;
            size_t sectionSize = section.Misc.VirtualSize;

            const size_t chunkSize = 0x100000;
            for (size_t offset = 0; offset < sectionSize; offset += chunkSize) {
                size_t currentSize = min(chunkSize, sectionSize - offset);
                std::vector<uint8_t> buffer(currentSize);
                if (!ProcessScanner::ReadMemory(m_process, sectionStart + offset, buffer.data(), currentSize))
                    continue;

                for (size_t i = 0; i + 8 <= currentSize; i += 8) {
                    uintptr_t ptr = *reinterpret_cast<uintptr_t*>(&buffer[i]);
                    if (ptr < 0x10000 || ptr > 0x7FFFFFFFFFFF) continue;

                    std::string name = GetName(ptr);
                    if (name == "Game") {
                        auto children = GetChildren(ptr);
                        if (children.size() > 5) return ptr;
                    }
                }
            }
        }
        return 0;
    }

    uintptr_t FindDataModel() {
        uintptr_t dm = 0;

        std::cout << "[*] Method 1: TaskScheduler..." << std::endl;
        dm = FindDataModelViaTaskScheduler();
        if (dm) return dm;

        std::cout << "[*] Method 2: FakeDataModel pointer..." << std::endl;
        dm = FindDataModelViaFakePointer();
        if (dm) return dm;

        std::cout << "[*] Method 3: VisualEngine chain..." << std::endl;
        dm = FindDataModelViaVisualEngine();
        if (dm) return dm;

        std::cout << "[*] Method 4: Brute-force .data scan..." << std::endl;
        dm = FindDataModelBruteForce();
        if (dm) return dm;

        return 0;
    }

    uintptr_t FindChild(uintptr_t parent, const std::string& name) {
        for (auto child : GetChildren(parent)) {
            if (GetName(child) == name) return child;
        }
        return 0;
    }

    uintptr_t FindChildByClass(uintptr_t parent, const std::string& className) {
        for (auto child : GetChildren(parent)) {
            if (GetClassName(child) == className) return child;
        }
        return 0;
    }

    uintptr_t FindDescendant(uintptr_t root, const std::string& name, int maxDepth = 10) {
        if (maxDepth <= 0) return 0;
        for (auto child : GetChildren(root)) {
            if (GetName(child) == name) return child;
            uintptr_t found = FindDescendant(child, name, maxDepth - 1);
            if (found) return found;
        }
        return 0;
    }

    std::vector<uintptr_t> FindModuleScripts(uintptr_t root, int maxDepth = 5) {
        std::vector<uintptr_t> modules;
        FindModuleScriptsRecursive(root, modules, maxDepth);
        return modules;
    }

    std::string GetFullName(uintptr_t instance) {
        if (!instance) return "";
        std::string path = GetName(instance);
        uintptr_t parent = GetParent(instance);
        int depth = 0;
        while (parent && depth < 20) {
            std::string pName = GetName(parent);
            if (pName.empty()) break;
            path = pName + "." + path;
            parent = GetParent(parent);
            depth++;
        }
        return path;
    }

private:
    HANDLE m_process;
    uintptr_t m_base;

    void FindModuleScriptsRecursive(uintptr_t instance, std::vector<uintptr_t>& out, int depth) {
        if (depth <= 0) return;
        for (auto child : GetChildren(instance)) {
            if (GetClassName(child) == "ModuleScript") out.push_back(child);
            FindModuleScriptsRecursive(child, out, depth - 1);
        }
    }
};
