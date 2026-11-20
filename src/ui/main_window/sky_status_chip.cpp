#include "ui/main_window/sky_status_chip.hpp"

#include <QPainter>

#include "ui/render/font_loader.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kHeight = 22;
constexpr int kPadX = 9;
constexpr int kDot = 7;
constexpr int kGap = 6;
constexpr int kFontSize = 12;

}  // namespace

SkyStatusChip::SkyStatusChip(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void SkyStatusChip::setStatus(const QString &text, Tone tone, const QString &tip)
{
    if (text == m_text && tone == m_tone && tip == toolTip()) {
        return;
    }
    m_text = text;
    m_tone = tone;
    setToolTip(tip);
    setAccessibleName(text);
    updateGeometry();
    resize(sizeHint());
    update();
}

QSize SkyStatusChip::sizeHint() const
{
    const QFontMetrics metrics(FontLoader::font(kFontSize, QFont::DemiBold));
    return QSize(kPadX * 2 + kDot + kGap + metrics.horizontalAdvance(m_text), kHeight);
}

void SkyStatusChip::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QColor tone = m_tone == Tone::Good ? theme.successColor
                        : m_tone == Tone::Warning ? theme.toastWarning
                                                  : theme.footerText;
    painter.setPen(QPen(withAlpha(tone, 0.45), 1));
    painter.setBrush(withAlpha(tone, 0.12));
    painter.drawRoundedRect(body, body.height() / 2, body.height() / 2);
    painter.setPen(Qt::NoPen);
    painter.setBrush(tone);
    painter.drawEllipse(QRectF(kPadX, (height() - kDot) / 2.0, kDot, kDot));
    painter.setFont(FontLoader::font(kFontSize, QFont::DemiBold));
    painter.setPen(theme.footerText);
    painter.drawText(QRectF(kPadX + kDot + kGap, 0, width() - kPadX * 2 - kDot - kGap, height()),
                     Qt::AlignVCenter | Qt::AlignLeft, m_text);
}

}  // namespace sky
