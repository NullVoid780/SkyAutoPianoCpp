#include "crash/crash_channel.hpp"

namespace sky {

CrashChannel::~CrashChannel()
{
    close();
}

std::wstring CrashChannel::baseName(DWORD processId)
{
    return L"Local\\SkyAutoPiano.Crash." + std::to_wstring(processId);
}

bool CrashChannel::create(DWORD processId)
{
    const std::wstring base = baseName(processId);
    m_mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(CrashContext),
                                   (base + L".Context").c_str());
    m_crashSignal = CreateEventW(nullptr, TRUE, FALSE, (base + L".Crash").c_str());
    m_dumpComplete = CreateEventW(nullptr, TRUE, FALSE, (base + L".Done").c_str());
    if (!m_mapping || !m_crashSignal || !m_dumpComplete) {
        close();
        return false;
    }
    m_context = static_cast<CrashContext *>(MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CrashContext)));
    if (!m_context) {
        close();
        return false;
    }
    ZeroMemory(m_context, sizeof(CrashContext));
    m_context->magic = kCrashMagic;
    m_context->layoutVersion = kCrashLayoutVersion;
    m_context->processId = processId;
    m_context->startTick = GetTickCount64();
    return true;
}

bool CrashChannel::open(DWORD processId)
{
    const std::wstring base = baseName(processId);
    m_mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, (base + L".Context").c_str());
    m_crashSignal = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, (base + L".Crash").c_str());
    m_dumpComplete = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, (base + L".Done").c_str());
    if (!m_mapping || !m_crashSignal || !m_dumpComplete) {
        close();
        return false;
    }
    m_context = static_cast<CrashContext *>(MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CrashContext)));
    if (!m_context || m_context->magic != kCrashMagic || m_context->layoutVersion != kCrashLayoutVersion) {
        close();
        return false;
    }
    return true;
}

void CrashChannel::close()
{
    if (m_context) {
        UnmapViewOfFile(m_context);
        m_context = nullptr;
    }
    for (HANDLE *handle : {&m_mapping, &m_crashSignal, &m_dumpComplete}) {
        if (*handle) {
            CloseHandle(*handle);
            *handle = nullptr;
        }
    }
}

}  // namespace sky
