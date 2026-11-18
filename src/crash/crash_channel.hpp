#pragma once

#include <string>

#include "crash/crash_context.hpp"

namespace sky {

class CrashChannel {
public:
    CrashChannel() = default;
    ~CrashChannel();
    CrashChannel(const CrashChannel &) = delete;
    CrashChannel &operator=(const CrashChannel &) = delete;

    bool create(DWORD processId);
    bool open(DWORD processId);

    CrashContext *context() const { return m_context; }
    HANDLE crashSignal() const { return m_crashSignal; }
    HANDLE dumpComplete() const { return m_dumpComplete; }

    static std::wstring baseName(DWORD processId);

private:
    void close();

    HANDLE m_mapping = nullptr;
    HANDLE m_crashSignal = nullptr;
    HANDLE m_dumpComplete = nullptr;
    CrashContext *m_context = nullptr;
};

}  // namespace sky
