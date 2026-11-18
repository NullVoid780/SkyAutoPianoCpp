#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "skyinput/error.hpp"
#include "skyinput/geometry.hpp"
#include "skyinput/image.hpp"
#include "skyinput/sender.hpp"
#include "skyinput/window_info.hpp"

namespace skyinput {

class WorkWindow {
public:
    explicit WorkWindow(ISender &sender);

    bool set(WindowHandle handle);
    bool set(std::wstring_view title, std::wstring_view className = {});
    bool setChild(WindowHandle parent, std::wstring_view childClassName, std::wstring_view childTitle = {});
    void clear();
    bool refresh();

    WindowHandle handle() const;
    WindowInfo info() const;
    bool isOpen() const;
    bool isForeground() const;
    void setForeground();
    void close();
    void kill();
    Rect view() const;
    void setView(const Rect &rect);

    Result<Image> capture(std::optional<Rect> part = std::nullopt, PixelFormat format = PixelFormat::Rgba,
                          std::uint8_t threshold = 127) const;
    std::uint32_t colorAt(int x, int y) const;

private:
    bool locate();

    ISender &m_sender;
    std::wstring m_title;
    std::wstring m_className;
    std::wstring m_childTitle;
    std::wstring m_childClassName;
};

}  // namespace skyinput
