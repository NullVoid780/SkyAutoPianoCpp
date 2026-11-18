#pragma once

#include <QFont>

namespace sky {

class FontLoader {
public:
    static int registerBundledFonts();
    static QFont applicationFont();
    static QFont font(int pixelSize, int weight);
    static QFont monospace(int pixelSize);
};

}  // namespace sky
