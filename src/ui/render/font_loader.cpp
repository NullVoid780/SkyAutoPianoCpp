#include "ui/render/font_loader.hpp"

#include <QFontDatabase>
#include <QStringList>

namespace sky {

int FontLoader::registerBundledFonts()
{
    int loaded = 0;
    for (const char *face : {"Light", "Regular", "Medium", "Bold"}) {
        if (QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Roboto-%1.ttf").arg(QLatin1String(face))) >= 0) {
            ++loaded;
        }
    }
    return loaded;
}

QFont FontLoader::font(int pixelSize, int weight)
{
    QFont font;
    font.setFamilies({QStringLiteral("Roboto"), QStringLiteral("Segoe UI")});
    font.setPixelSize(pixelSize);
    font.setWeight(static_cast<QFont::Weight>(weight));
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

QFont FontLoader::applicationFont()
{
    return font(16, QFont::Light);
}

QFont FontLoader::monospace(int pixelSize)
{
    QFont font(QStringLiteral("Courier New"));
    font.setPixelSize(pixelSize);
    return font;
}

}  // namespace sky
