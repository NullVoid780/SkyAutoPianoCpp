#include "ui/widgets/toast_item.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QTextLayout>
#include <QTimer>
#include <QVariantAnimation>

#include "core/string_utils.hpp"
#include "ui/core/easing.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kMinHeight = 50;
constexpr qreal kBackdropBlur = 35;
constexpr int kBackdropRefreshMs = 250;
constexpr int kRadius = 12;
constexpr int kGlyph = 20;
constexpr int kGlyphMargin = 15;
constexpr int kClose = 20;
constexpr int kCloseRight = 10;
constexpr int kTextRightPadding = 20;
constexpr int kVerticalPadding = 14;
constexpr int kProgressHeight = 4;

QString glyphFor(ToastType type)
{
    switch (type) {
    case ToastType::Success:
        return QStringLiteral("check-lg");
    case ToastType::Warning:
        return QStringLiteral("exclamation-triangle");
    case ToastType::Error:
        return QStringLiteral("x-lg");
    case ToastType::Info:
        return QStringLiteral("info-lg");
    case ToastType::Neutral:
        return {};
    }
    return {};
}

QFont bodyFont()
{
    return FontLoader::font(14, QFont::Medium);
}

}  // namespace

ToastItem::ToastItem(const Toast &toast, QWidget *parent)
    : QWidget(parent),
      m_toast(toast),
      m_animation(new QVariantAnimation(this)),
      m_ticker(new QTimer(this)),
      m_backdropRefresh(new QTimer(this)),
      m_backdrop(this, kBackdropBlur, BackdropBlur::Extent::WindowWidth)
{
    m_backdropRefresh->setInterval(kBackdropRefreshMs);
    connect(m_backdropRefresh, &QTimer::timeout, this, [this] {
        m_backdrop.invalidate();
        update();
    });
    m_backdropRefresh->start();
    setMouseTracking(true);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(kWidth, preferredHeight());
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_presence = value.toReal();
        emit presenceChanged();
        update();
    });
    connect(m_animation, &QVariantAnimation::finished, this, [this] {
        if (m_leaving) {
            emit finished(this);
        }
    });
    m_ticker->setInterval(16);
    connect(m_ticker, &QTimer::timeout, this, [this] {
        update(0, height() - kProgressHeight, width(), kProgressHeight);
        if (m_toast.durationMs > 0 && m_clock.elapsed() >= m_toast.durationMs) {
            dismiss();
        }
    });
}

void ToastItem::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_backdrop.invalidate();
}

QString ToastItem::displayText() const
{
    return toQString(m_toast.message);
}

QRectF ToastItem::textRect(int height) const
{
    const qreal left = glyphFor(m_toast.type).isEmpty() ? kGlyphMargin : kGlyphMargin * 2 + kGlyph;
    const qreal right = kWidth - kCloseRight - kClose - kTextRightPadding;
    return QRectF(left, kVerticalPadding, right - left, height - kVerticalPadding * 2);
}

int ToastItem::preferredHeight() const
{
    if (m_cachedHeight > 0) {
        return m_cachedHeight;
    }
    const QRectF bounds = textRect(1000);
    QFont title = bodyFont();
    title.setBold(true);
    const QFontMetricsF body(bodyFont());
    const QFontMetricsF heading(title);
    qreal height = body.boundingRect(bounds, Qt::TextWordWrap, displayText()).height() * 1.4 / body.lineSpacing() *
                   body.lineSpacing();
    if (!m_toast.title.empty()) {
        height += heading.lineSpacing() * 1.4;
    }
    m_cachedHeight = std::max(kMinHeight, qCeil(height) + kVerticalPadding * 2);
    return m_cachedHeight;
}

void ToastItem::enter()
{
    m_animation->stop();
    m_animation->setDuration(kToastEnterMs);
    m_animation->setEasingCurve(easingCurve(Easing::ToastIn));
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->start();
    m_clock.start();
    m_ticker->start();
}

void ToastItem::dismiss()
{
    if (m_leaving) {
        return;
    }
    m_leaving = true;
    m_ticker->stop();
    m_animation->stop();
    m_animation->setDuration(kToastExitMs);
    m_animation->setEasingCurve(easingCurve(Easing::ToastOut));
    m_animation->setStartValue(m_presence);
    m_animation->setEndValue(0.0);
    m_animation->start();
}

QRectF ToastItem::closeRect() const
{
    return QRectF(width() - kCloseRight - kClose, (height() - kClose) / 2.0, kClose, kClose);
}

void ToastItem::mouseMoveEvent(QMouseEvent *event)
{
    const bool hovered = closeRect().contains(event->position());
    if (hovered != m_closeHovered) {
        m_closeHovered = hovered;
        setCursor(hovered ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void ToastItem::leaveEvent(QEvent *)
{
    m_closeHovered = false;
    update();
}

void ToastItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dismiss();
    }
}

void ToastItem::paintEvent(QPaintEvent *)
{
    if (BackdropBlur::isCapturing()) {
        return;
    }
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    painter.setOpacity(m_presence);

    const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QPainterPath path = roundedPath(card, kRadius);
    const QColor base = theme.toastBase(static_cast<int>(m_toast.type));
    painter.save();
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, m_backdrop.pixmap());
    painter.restore();
    painter.fillPath(path, withAlpha(base, 0.18));
    painter.setPen(QPen(withAlpha(base, 0.35), 1));
    painter.drawPath(path);
    painter.setPen(QPen(theme.toastBorderHighlight, 1));
    painter.drawLine(QPointF(card.left() + kRadius, card.top() + 1), QPointF(card.right() - kRadius, card.top() + 1));

    const QString glyph = glyphFor(m_toast.type);
    if (!glyph.isEmpty()) {
        painter.drawPixmap(QPointF(kGlyphMargin, (height() - kGlyph) / 2.0),
                           IconCache::pixmap(glyph, kGlyph, theme.toastText, devicePixelRatioF()));
    }

    QRectF text = textRect(height());
    const auto drawText = [&](const QRectF &area, const QFont &font, const QString &value) {
        painter.setFont(font);
        if (theme.toastTextShadow.alpha() > 0) {
            painter.setPen(theme.toastTextShadow);
            painter.drawText(area.translated(1, 1), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignVCenter, value);
        }
        painter.setPen(theme.toastText);
        painter.drawText(area, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignVCenter, value);
    };
    if (!m_toast.title.empty()) {
        QFont title = bodyFont();
        title.setBold(true);
        const qreal titleHeight = QFontMetricsF(title).lineSpacing() * 1.4;
        const qreal bodyHeight = text.height() - titleHeight;
        drawText(QRectF(text.left(), text.top(), text.width(), titleHeight), title, toQString(m_toast.title));
        text = QRectF(text.left(), text.top() + titleHeight, text.width(), bodyHeight);
    }
    drawText(text, bodyFont(), displayText());

    const QRectF close = closeRect();
    painter.save();
    painter.setOpacity(m_presence * (m_closeHovered ? 1.0 : 0.7));
    painter.translate(close.center());
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme.toastClose);
    for (int angle : {45, -45}) {
        painter.save();
        painter.rotate(angle);
        painter.drawRect(QRectF(-8, -1, 16, 2));
        painter.restore();
    }
    painter.restore();

    if (m_toast.durationMs > 0 && !m_leaving) {
        const qreal remaining = 1.0 - std::min<qreal>(1.0, static_cast<qreal>(m_clock.elapsed()) / m_toast.durationMs);
        painter.save();
        painter.setClipPath(path);
        painter.fillRect(QRectF(0, height() - kProgressHeight, width() * remaining, kProgressHeight), theme.toastProgress);
        painter.restore();
    }
}

}  // namespace sky
