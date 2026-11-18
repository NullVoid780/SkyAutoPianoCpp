#include "skyinput/error.hpp"

#include <windows.h>

namespace skyinput {

Error::Error(std::string operation, unsigned long code)
    : m_operation(std::move(operation)), m_code(code)
{
}

Error Error::fromLastError(std::string operation)
{
    return Error(std::move(operation), GetLastError());
}

std::string Error::message() const
{
    wchar_t *buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, m_code, 0, reinterpret_cast<wchar_t *>(&buffer), 0, nullptr);
    std::string text = m_operation + " failed (" + std::to_string(m_code) + ")";
    if (length > 0 && buffer) {
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
        std::string detail(static_cast<std::size_t>(bytes), '\0');
        WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(length), detail.data(), bytes, nullptr, nullptr);
        while (!detail.empty() && (detail.back() == '\n' || detail.back() == '\r' || detail.back() == ' ')) {
            detail.pop_back();
        }
        text += ": " + detail;
    }
    if (buffer) {
        LocalFree(buffer);
    }
    return text;
}

}  // namespace skyinput
