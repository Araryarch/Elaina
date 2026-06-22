#pragma once
#include <string>
#include <vector>
#include <Windows.h>
#include <algorithm>
#include "process/scanner.h"

namespace rblx {
    static std::string ReadRobloxString(HANDLE h, uintptr_t addr) {
        if (addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return "";
        struct { union { char buf[16]; char* ptr; }; size_t length; size_t capacity; } s;
        if (!ProcessScanner::ReadMemory(h, addr, &s, sizeof(s))) return "";
        if (s.length == 0 || s.length > 200) return "";
        if (s.length < 16) {
            for (size_t i = 0; i < s.length; i++) if (s.buf[i] < 0x20 || s.buf[i] > 0x7E) return "";
            return std::string(s.buf, s.length);
        }
        uintptr_t dataAddr = reinterpret_cast<uintptr_t>(s.ptr);
        if (dataAddr < 0x10000 || dataAddr > 0x7FFFFFFFFFFF) return "";
        std::vector<char> buffer(s.length + 1, 0);
        if (ProcessScanner::ReadMemory(h, dataAddr, buffer.data(), s.length)) {
            for (size_t i = 0; i < s.length; i++) if (buffer[i] < 0x20 || buffer[i] > 0x7E) return "";
            return std::string(buffer.data(), s.length);
        }
        return "";
    }

    static std::string ReadInstanceName(HANDLE h, uintptr_t inst) {
        uintptr_t ptr = ProcessScanner::Read<uintptr_t>(h, inst + 0xB0);
        std::string s = ReadRobloxString(h, ptr);
        if (!s.empty()) return s;
        for (size_t off : {(size_t)0xA8, (size_t)0x50, (size_t)0x48}) {
            ptr = ProcessScanner::Read<uintptr_t>(h, inst + off);
            s = ReadRobloxString(h, ptr);
            if (!s.empty()) return s;
        }
        return "";
    }

    static std::string ReadClassName(HANDLE h, uintptr_t inst) {
        uintptr_t cd = ProcessScanner::Read<uintptr_t>(h, inst + 0x18);
        if (cd < 0x10000 || cd > 0x7FFFFFFFFFFF) return "";
        uintptr_t namePtr = ProcessScanner::Read<uintptr_t>(h, cd + 0x8);
        return ReadRobloxString(h, namePtr);
    }

    static bool IsValidInstance(HANDLE h, uintptr_t ptr) {
        if (ptr < 0x10000 || ptr > 0x7FFFFFFFFFFF) return false;
        uintptr_t cd = ProcessScanner::Read<uintptr_t>(h, ptr + 0x18);
        return (cd >= 0x10000 && cd <= 0x7FFFFFFFFFFF);
    }

    static std::vector<uintptr_t> GetChildren(HANDLE h, uintptr_t instance) {
        for (size_t off = 0x30; off <= 0x300; off += 8) {
            uintptr_t containerPtr = ProcessScanner::Read<uintptr_t>(h, instance + off);
            if (containerPtr < 0x10000 || containerPtr > 0x7FFFFFFFFFFF) continue;
            if (containerPtr % 8 != 0) continue;
            uintptr_t startNode = ProcessScanner::Read<uintptr_t>(h, containerPtr);
            uintptr_t endNode = ProcessScanner::Read<uintptr_t>(h, containerPtr + 8);
            if (startNode < 0x10000 || startNode > 0x7FFFFFFFFFFF) continue;
            if (endNode < startNode) continue;
            size_t byteRange = (endNode + 1) - startNode;
            if (byteRange > 0xFFFFFF) continue;
            size_t nodeCount = byteRange / 16;
            if (nodeCount == 0 || nodeCount > 2000) continue;
            int validCount = 0;
            int checkCount = (int)std::min(nodeCount, (size_t)4);
            for (int i = 0; i < checkCount; i++) {
                uintptr_t childPtr = ProcessScanner::Read<uintptr_t>(h, startNode + i * 16);
                if (IsValidInstance(h, childPtr)) validCount++;
            }
            if (validCount == 0) continue;
            std::vector<uintptr_t> children;
            for (size_t i = 0; i < nodeCount; i++) {
                uintptr_t childPtr = ProcessScanner::Read<uintptr_t>(h, startNode + i * 16);
                if (childPtr && IsValidInstance(h, childPtr)) children.push_back(childPtr);
            }
            if (!children.empty()) return children;
        }
        return {};
    }

    static uintptr_t FindChildByName(HANDLE h, uintptr_t parent, const std::string& name) {
        for (auto child : GetChildren(h, parent))
            if (ReadInstanceName(h, child) == name) return child;
        return 0;
    }

    static uintptr_t FindChildByClassName(HANDLE h, uintptr_t parent, const std::string& cls) {
        for (auto child : GetChildren(h, parent))
            if (ReadClassName(h, child) == cls) return child;
        return 0;
    }

    static std::vector<std::string> GetFullInstancePath(HANDLE h, uintptr_t inst) {
        std::vector<std::string> path;
        path.push_back(ReadInstanceName(h, inst));
        uintptr_t current = inst;
        for (int i = 0; i < 20; i++) {
            uintptr_t parent = ProcessScanner::Read<uintptr_t>(h, current + 0x68);
            if (!parent || !IsValidInstance(h, parent)) break;
            uintptr_t parentOfParent = ProcessScanner::Read<uintptr_t>(h, parent + 0x68);
            if (!parentOfParent) break;
            std::string name = ReadInstanceName(h, parent);
            if (name == "Game" || name == "game" || name.empty()) break;
            path.insert(path.begin(), name);
            current = parent;
        }
        path.insert(path.begin(), "game");
        return path;
    }
}
