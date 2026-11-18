#include "ui/widgets/card_frame.hpp"

#include <QEvent>
#include <QLayout>
#include <QPainter>

#include "ui/core/elevation_layer.hpp"
#include "ui/core/hover_tracker.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

CardFrame::CardFrame(qreal lift, qreal shift, bool hoverable, QWidget *parent)
    : QWidget(parent), m_lift(lift), m_shift(shift)
{
    if (hoverable) {
        m_hover = new HoverTracker(this, kHoverSlowMs, Easing::Ease);
        connect(m_hover, &HoverTracker::changed, this, [this] {
            applyOffset();
            update();
            if (m_layer) {
                m_layer->refresh(this);
            }
        });
    }
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
    applyOffset();
}

qreal CardFrame::hoverProgress() const
{
    return m_hover ? m_hover->progress() : 0.0;
}

void CardFrame::setPadding(const QMargins &padding)
{
    m_padding = padding;
    applyOffset();
}

void CardFrame::attachShadows(ElevationLayer *layer)
{
    m_layer = layer;
    layer->track(this, cornerRadius(), [this] { return hoverShadows(); }, [this] { return bodyRect(); });
}

void CardFrame::applyOffset()
{
    const qreal p = hoverProgress();
    const int dx = qRound(m_shift * p);
    const int dy = qRound(m_lift * p);
    const int lift = qRound(m_lift);
    const int shift = qRound(m_shift);
    setContentsMargins(m_padding.left() + dx, m_padding.top() + lift - dy, m_padding.right() + shift - dx,
                       m_padding.bottom() + dy);
}

QRectF CardFrame::bodyRect() const
{
    const qreal p = hoverProgress();
    return QRectF(rect()).adjusted(m_shift * p, m_lift - m_lift * p, -m_shift + m_shift * p, -m_lift * p);
}

void CardFrame::addChildShadow(QWidget *target, qreal radius, std::function<std::optional<ShadowSpec>()> shadow)
{
    m_childShadows.push_back(ChildShadow{target, radius, std::move(shadow)});
    target->installEventFilter(this);
}

bool CardFrame::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::EnabledChange:
    case QEvent::Show:
    case QEvent::Hide:
        update();
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

void CardFrame::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    preparePainter(painter);
    paintCard(painter, bodyRect(), hoverProgress());
    for (const ChildShadow &child : m_childShadows) {
        if (!child.target->isVisible()) {
            continue;
        }
        if (const std::optional<ShadowSpec> shadow = child.shadow()) {
            ShadowPainter::paint(painter, QRectF(child.target->geometry()), child.radius, *shadow);
        }
    }
}

}  // namespace sky
