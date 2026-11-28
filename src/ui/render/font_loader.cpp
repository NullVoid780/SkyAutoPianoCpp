#include "ui/render/font_loader.hpp"

#include <QFontDatabase>
#include <QStringList>

#include "core/translator.hpp"

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

QStringList FontLoader::families()
{
    const QString language = currentLanguage();
    if (language == QLatin1String("ja")) {
        return {QStringLiteral("Roboto"), QStringLiteral("Yu Gothic UI"), QStringLiteral("Meiryo UI"),
                QStringLiteral("Segoe UI")};
    }
    if (language == QLatin1String("zh_CN")) {
        return {QStringLiteral("Roboto"), QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei"),
                QStringLiteral("Segoe UI")};
    }
    return {QStringLiteral("Roboto"), QStringLiteral("Segoe UI")};
}

QFont FontLoader::font(int pixelSize, int weight)
{
    QFont font;
    font.setFamilies(families());
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
