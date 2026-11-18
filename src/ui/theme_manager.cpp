#include "ui/theme_manager.hpp"

#include <QApplication>
#include <QFile>
#include <QRegularExpression>
#include <QStyle>
#include <QWidget>

#include "platform/window_attributes.hpp"

namespace sky {

namespace {

const QRegularExpression &tokenPattern()
{
    static const QRegularExpression pattern(QStringLiteral("@([A-Za-z][A-Za-z0-9]*)"));
    return pattern;
}

}  // namespace

ThemeManager &ThemeManager::instance()
{
    static ThemeManager manager;
    return manager;
}

QString ThemeManager::renderStyleSheet(const QString &templateText, const Theme &theme)
{
    const QMap<QString, QString> tokens = theme.qssTokens();
    QString result;
    result.reserve(templateText.size());
    qsizetype last = 0;
    auto it = tokenPattern().globalMatch(templateText);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        result += templateText.mid(last, match.capturedStart() - last);
        result += tokens.value(match.captured(1), match.captured(0));
        last = match.capturedEnd();
    }
    result += templateText.mid(last);
    return result;
}

QStringList ThemeManager::unresolvedTokens(const QString &templateText, const Theme &theme)
{
    const QMap<QString, QString> tokens = theme.qssTokens();
    QStringList missing;
    auto it = tokenPattern().globalMatch(templateText);
    while (it.hasNext()) {
        const QString name = it.next().captured(1);
        if (!tokens.contains(name) && !missing.contains(name)) {
            missing << name;
        }
    }
    return missing;
}

void ThemeManager::apply(AppTheme kind)
{
    apply(Theme::forKind(kind));
}

void ThemeManager::apply(const Theme &theme)
{
    if (m_template.isEmpty()) {
        QFile file(QStringLiteral(":/styles/app.qss"));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_template = QString::fromUtf8(file.readAll());
        }
    }
    m_theme = theme;
    QString sheet = renderStyleSheet(m_template, m_theme);
    if (sheet == m_renderedSheet) {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            widget->update();
        }
        emit themeChanged(m_theme.kind);
        return;
    }
    m_renderedSheet = std::move(sheet);
    qApp->setStyleSheet(m_renderedSheet);
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }
    emit themeChanged(m_theme.kind);
}

void applyThemeTitleBar(QWidget *window)
{
    const Theme &theme = ThemeManager::instance().theme();
    applyTitleBarColors(window, theme.titleBarFill, theme.titleBarText, theme.windowBorder);
}

void ThemeManager::toggle()
{
    apply(m_theme.kind == AppTheme::Dark ? AppTheme::Light : AppTheme::Dark);
}

}  // namespace sky
