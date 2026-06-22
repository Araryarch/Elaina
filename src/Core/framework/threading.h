#pragma once
#include <Windows.h>
#include <thread>
#include <functional>

namespace threading
{
    inline void CreateDetachedThread(std::function<void()> fn)
    {
        std::thread(fn).detach();
    }

    class ScopedCriticalSection
    {
    public:
        ScopedCriticalSection(CRITICAL_SECTION& cs) : m_cs(cs)
        {
            EnterCriticalSection(&m_cs);
        }
        ~ScopedCriticalSection()
        {
            LeaveCriticalSection(&m_cs);
        }
    private:
        CRITICAL_SECTION& m_cs;
    };
}
