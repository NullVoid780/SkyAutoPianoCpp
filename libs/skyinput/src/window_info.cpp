#include "skyinput/window_info.hpp"

#include <windows.h>

namespace skyinput {

namespace {

struct WindowQuery {
    std::wstring_view title;
    std::wstring_view className;
    bool requireTitle = true;
    HWND found = nullptr;
};

std::wstring readClassName(HWND window)
{
    wchar_t buffer[256] = {};
    const int length = GetClassNameW(window, buffer, 256);
    return std::wstring(buffer, length > 0 ? static_cast<std::size_t>(length) : 0);
}

std::wstring readTitle(HWND window)
{
    const int length = GetWindowTextLengthW(window);
    if (length <= 0) {
        return {};
    }
    std::wstring title(static_cast<std::size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(window, title.data(), length + 1);
    title.resize(copied > 0 ? static_cast<std::size_t>(copied) : 0);
    return title;
}

bool isCandidate(HWND window, bool requireTitle)
{
    if (!IsWindowVisible(window) || !IsWindowEnabled(window)) {
        return false;
    }
    return !requireTitle || GetWindowTextLengthW(window) > 0;
}

BOOL CALLBACK matchWindow(HWND window, LPARAM param)
{
    auto *query = reinterpret_cast<WindowQuery *>(param);
    if (!isCandidate(window, query->requireTitle)) {
        return TRUE;
    }
    if (!query->className.empty() && readClassName(window) != query->className) {
        return TRUE;
    }
    if (!query->title.empty() && readTitle(window) != query->title) {
        return TRUE;
    }
    query->found = window;
    return FALSE;
}

BOOL CALLBACK collectTopLevel(HWND window, LPARAM param)
{
    if (isCandidate(window, true)) {
        reinterpret_cast<std::vector<HWND> *>(param)->push_back(window);
    }
    return TRUE;
}

BOOL CALLBACK collectChildren(HWND window, LPARAM param)
{
    if (isCandidate(window, false)) {
        reinterpret_cast<std::vector<HWND> *>(param)->push_back(window);
    }
    return TRUE;
}

std::vector<WindowInfo> describe(const std::vector<HWND> &handles)
{
    std::vector<WindowInfo> windows;
    windows.reserve(handles.size());
    for (HWND handle : handles) {
        windows.push_back(windowInfo(handle));
    }
    return windows;
}

}  // namespace

WindowInfo windowInfo(WindowHandle handle)
{
    WindowInfo info;
    info.handle = handle;
    if (handle) {
        info.title = readTitle(handle);
        info.className = readClassName(handle);
    }
    return info;
}

std::vector<WindowInfo> allWindows()
{
    std::vector<HWND> handles;
    EnumWindows(collectTopLevel, reinterpret_cast<LPARAM>(&handles));
    return describe(handles);
}

std::vector<WindowInfo> windowChildren(WindowHandle parent)
{
    std::vector<HWND> handles;
    if (parent) {
        EnumChildWindows(parent, collectChildren, reinterpret_cast<LPARAM>(&handles));
    }
    return describe(handles);
}

std::vector<WindowInfo> windowChildren(std::wstring_view title, std::wstring_view className)
{
    return windowChildren(findWindow(title, className));
}

WindowHandle findWindow(std::wstring_view title, std::wstring_view className)
{
    WindowQuery query{title, className, true, nullptr};
    EnumWindows(matchWindow, reinterpret_cast<LPARAM>(&query));
    return query.found;
}

WindowHandle findChildWindow(WindowHandle parent, std::wstring_view childClassName, std::wstring_view childTitle)
{
    WindowQuery query{childTitle, childClassName, false, nullptr};
    EnumChildWindows(parent, matchWindow, reinterpret_cast<LPARAM>(&query));
    return query.found;
}

}  // namespace skyinput
