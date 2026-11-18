#include "skyinput/work_window.hpp"

#include <windows.h>

#include <cstring>

namespace skyinput {

namespace {

class WindowDc {
public:
    explicit WindowDc(HWND window) : m_window(window), m_dc(GetDC(window)) {}
    ~WindowDc()
    {
        if (m_dc) {
            ReleaseDC(m_window, m_dc);
        }
    }
    WindowDc(const WindowDc &) = delete;
    WindowDc &operator=(const WindowDc &) = delete;
    HDC get() const { return m_dc; }

private:
    HWND m_window;
    HDC m_dc;
};

class MemoryDc {
public:
    explicit MemoryDc(HDC compatible) : m_dc(CreateCompatibleDC(compatible)) {}
    ~MemoryDc()
    {
        if (m_dc) {
            DeleteDC(m_dc);
        }
    }
    MemoryDc(const MemoryDc &) = delete;
    MemoryDc &operator=(const MemoryDc &) = delete;
    HDC get() const { return m_dc; }

private:
    HDC m_dc;
};

class GdiObject {
public:
    explicit GdiObject(HGDIOBJ object) : m_object(object) {}
    ~GdiObject()
    {
        if (m_object) {
            DeleteObject(m_object);
        }
    }
    GdiObject(const GdiObject &) = delete;
    GdiObject &operator=(const GdiObject &) = delete;
    HGDIOBJ get() const { return m_object; }

private:
    HGDIOBJ m_object;
};

}  // namespace

WorkWindow::WorkWindow(ISender &sender) : m_sender(sender) {}

bool WorkWindow::locate()
{
    HWND found = m_sender.target();
    if (!found) {
        found = findWindow(m_title, m_className);
    }
    if (!m_childClassName.empty() || !m_childTitle.empty()) {
        found = findChildWindow(found, m_childClassName, m_childTitle);
    }
    m_sender.setTarget(found);
    return found != nullptr;
}

bool WorkWindow::set(WindowHandle handle)
{
    m_title.clear();
    m_className.clear();
    m_childTitle.clear();
    m_childClassName.clear();
    m_sender.setTarget(handle);
    return handle != nullptr;
}

bool WorkWindow::set(std::wstring_view title, std::wstring_view className)
{
    m_title = title;
    m_className = className;
    m_childTitle.clear();
    m_childClassName.clear();
    m_sender.setTarget(nullptr);
    return locate();
}

bool WorkWindow::setChild(WindowHandle parent, std::wstring_view childClassName, std::wstring_view childTitle)
{
    m_title.clear();
    m_className.clear();
    m_childClassName = childClassName;
    m_childTitle = childTitle;
    m_sender.setTarget(parent);
    return locate();
}

void WorkWindow::clear()
{
    set(WindowHandle{nullptr});
}

bool WorkWindow::refresh()
{
    m_sender.setTarget(nullptr);
    return locate();
}

WindowHandle WorkWindow::handle() const
{
    return m_sender.target();
}

WindowInfo WorkWindow::info() const
{
    return windowInfo(handle());
}

bool WorkWindow::isOpen() const
{
    return IsWindow(handle()) != FALSE;
}

bool WorkWindow::isForeground() const
{
    return handle() == GetForegroundWindow();
}

void WorkWindow::setForeground()
{
    SetForegroundWindow(handle());
}

void WorkWindow::close()
{
    SendMessageW(handle(), WM_CLOSE, 0, 0);
}

void WorkWindow::kill()
{
    DWORD processId = 0;
    GetWindowThreadProcessId(handle(), &processId);
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, TRUE, processId);
    if (process) {
        TerminateProcess(process, 0);
        CloseHandle(process);
    }
}

Rect WorkWindow::view() const
{
    const HWND window = handle();
    if (!window) {
        return {};
    }
    RECT client{};
    POINT origin{0, 0};
    GetClientRect(window, &client);
    ClientToScreen(window, &origin);
    return Rect{origin.x, origin.y, client.right, client.bottom};
}

void WorkWindow::setView(const Rect &rect)
{
    SetWindowPos(handle(), nullptr, rect.x, rect.y, rect.width, rect.height, 0);
}

Result<Image> WorkWindow::capture(std::optional<Rect> part, PixelFormat format, std::uint8_t threshold) const
{
    const HWND window = handle();
    Rect area;
    if (part) {
        area = *part;
    } else if (window) {
        RECT client{};
        GetClientRect(window, &client);
        area = Rect{client.left, client.top, client.right - client.left, client.bottom - client.top};
    } else {
        area = Rect{0, 0, GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN)};
    }
    if (area.width <= 0 || area.height <= 0) {
        return Error("capture", ERROR_INVALID_PARAMETER);
    }

    WindowDc source(window);
    if (!source.get()) {
        return Error::fromLastError("GetDC");
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = area.width;
    info.bmiHeader.biHeight = -area.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    GdiObject section(CreateDIBSection(source.get(), &info, DIB_RGB_COLORS, &bits, nullptr, 0));
    if (!section.get() || !bits) {
        return Error::fromLastError("CreateDIBSection");
    }
    MemoryDc memory(source.get());
    const HGDIOBJ previous = SelectObject(memory.get(), section.get());
    const BOOL copied = BitBlt(memory.get(), 0, 0, area.width, area.height, source.get(), area.x, area.y, SRCCOPY);
    SelectObject(memory.get(), previous);
    if (!copied) {
        return Error::fromLastError("BitBlt");
    }

    Image image;
    image.width = area.width;
    image.height = area.height;
    image.format = format;
    const std::size_t size = static_cast<std::size_t>(area.width) * static_cast<std::size_t>(area.height) * 4;
    image.data.resize(size);
    std::memcpy(image.data.data(), bits, size);
    convertBgraPixels(image.data, format, threshold);
    return image;
}

std::uint32_t WorkWindow::colorAt(int x, int y) const
{
    WindowDc dc(handle());
    return static_cast<std::uint32_t>(GetPixel(dc.get(), x, y));
}

}  // namespace skyinput
