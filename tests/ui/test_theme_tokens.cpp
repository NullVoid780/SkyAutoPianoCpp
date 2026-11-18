#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QRegularExpression>

#include "ui/theme_manager.hpp"

using namespace sky;

namespace {

QString loadTemplate()
{
    QFile file(QStringLiteral(SKY_QSS_PATH));
    REQUIRE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    return QString::fromUtf8(file.readAll());
}

}  // namespace

TEST_CASE("every @token in app.qss resolves in both themes")
{
    const QString text = loadTemplate();
    CHECK(ThemeManager::unresolvedTokens(text, Theme::light()).isEmpty());
    CHECK(ThemeManager::unresolvedTokens(text, Theme::dark()).isEmpty());
    CHECK_FALSE(ThemeManager::renderStyleSheet(text, Theme::dark()).contains(QLatin1Char('@')));
}

TEST_CASE("the stylesheet template carries no literal colors")
{
    const QString text = loadTemplate();
    static const QRegularExpression literal(QStringLiteral("#[0-9a-fA-F]{3,8}\\b|rgba?\\("));
    CHECK_FALSE(literal.match(text).hasMatch());
}

TEST_CASE("theme tokens match the design spec")
{
    CHECK(Theme::light().fontColor.name() == QStringLiteral("#2c3e50"));
    CHECK(Theme::dark().hoverColor.name() == QStringLiteral("#588e97"));
    CHECK(Theme::dark().windowMain.name() == QStringLiteral("#1b1d1e"));
    CHECK(Theme::light().prePaint.name() == QStringLiteral("#0a1930"));
    CHECK(Theme::light().cardTopEdgeHeight == 5);
    CHECK(Theme::dark().cardTopEdgeHeight == 1);
    CHECK(cssColor(Theme::dark().scrollHandle) == QStringLiteral("rgba(255, 255, 255, 51)"));
}
