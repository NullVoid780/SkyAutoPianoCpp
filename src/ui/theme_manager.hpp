#pragma once

#include <QObject>

class QWidget;
#include <QString>

#include "model/config.hpp"
#include "ui/theme.hpp"

namespace sky {

class ThemeManager final : public QObject {
    Q_OBJECT

public:
    static ThemeManager &instance();

    void apply(AppTheme kind);
    void apply(const Theme &theme);
    void toggle();
    AppTheme kind() const { return m_theme.kind; }
    const Theme &theme() const { return m_theme; }

    static QString renderStyleSheet(const QString &templateText, const Theme &theme);
    static QStringList unresolvedTokens(const QString &templateText, const Theme &theme);

signals:
    void themeChanged(sky::AppTheme kind);

private:
    ThemeManager() = default;

    QString m_template;
    QString m_renderedSheet;
    Theme m_theme = Theme::light();
};

void applyThemeTitleBar(QWidget *window);

inline const Theme &currentTheme()
{
    return ThemeManager::instance().theme();
}

}  // namespace sky
