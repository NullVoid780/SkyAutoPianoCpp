#include "ui/main_window/theme_toggle.hpp"

#include <QPainter>
#include <QVariantAnimation>

#include "ui/core/hover_tracker.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kSize = 20;

}  // namespace

ThemeToggle::ThemeToggle(QWidget *parent)
    : QAbstractButton(parent), m_fade(new QVariantAnimation(this)), m_hover(new HoverTracker(this, kHoverFastMs))
{
    setFixedSize(kSize, kSize);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    m_fade->setDuration(kHoverSlowMs);
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_darkness = value.toReal();
        update();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { syncWithTheme(true); });
    syncWithTheme(false);
}

void ThemeToggle::syncWithTheme(bool animate)
{
    const qreal target = ThemeManager::instance().kind() == AppTheme::Dark ? 1.0 : 0.0;
    m_fade->stop();
    if (!animate) {
        m_darkness = target;
        update();
        return;
    }
    m_fade->setStartValue(m_darkness);
    m_fade->setEndValue(target);
    m_fade->start();
}

void ThemeToggle::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    const qreal hover = m_hover->progress();
    const qreal dpr = devicePixelRatioF();
    painter.setOpacity(1.0 - m_darkness);
    painter.drawPixmap(0, 0, IconCache::pixmap(QStringLiteral("sun"), kSize, blend(theme.sunIcon, theme.hoverColor, hover), dpr));
    painter.setOpacity(m_darkness);
    painter.drawPixmap(0, 0, IconCache::pixmap(QStringLiteral("moon"), kSize, blend(theme.moonIcon, theme.hoverColor, hover), dpr));
}

}  // namespace sky
