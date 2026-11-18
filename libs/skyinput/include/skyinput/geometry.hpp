#pragma once

struct HWND__;

namespace skyinput {

using WindowHandle = HWND__ *;

struct Point {
    int x = 0;
    int y = 0;

    bool operator==(const Point &) const = default;
};

struct Size {
    int width = 0;
    int height = 0;

    bool operator==(const Size &) const = default;
};

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool operator==(const Rect &) const = default;
};

}  // namespace skyinput
