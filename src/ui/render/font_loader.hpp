#pragma once

#include <QFont>
#include <QStringList>

namespace sky {

class FontLoader {
public:
    static int registerBundledFonts();
    static QFont applicationFont();
    static QStringList families();
    static QFont font(int pixelSize, int weight);
    static QFont monospace(int pixelSize);
};

}  // namespace sky
