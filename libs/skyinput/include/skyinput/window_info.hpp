#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "skyinput/geometry.hpp"

namespace skyinput {

struct WindowInfo {
    WindowHandle handle = nullptr;
    std::wstring title;
    std::wstring className;
};

WindowInfo windowInfo(WindowHandle handle);
std::vector<WindowInfo> allWindows();
std::vector<WindowInfo> windowChildren(WindowHandle parent);
std::vector<WindowInfo> windowChildren(std::wstring_view title, std::wstring_view className = {});
WindowHandle findWindow(std::wstring_view title, std::wstring_view className = {});
WindowHandle findChildWindow(WindowHandle parent, std::wstring_view childClassName,
                             std::wstring_view childTitle = {});

}  // namespace skyinput
