#include "ui/core/overlay.hpp"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QVariantAnimation>

#include "ui/core/easing.hpp"
#include "ui/render/backdrop_blur.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"

namespace sky {

Overlay::Overlay(QWidget *window) : QWidget(window), m_window(window), m_fade(new QVariantAnimation(this))
{
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    m_fade->setDuration(kHoverSlowMs);
    m_fade->setEasingCurve(easingCurve(Easing::Ease));
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_presence = value.toReal();
        relayout();
        update();
    });
    connect(m_fade, &QVariantAnimation::finished, this, [this] {
        if (!m_open) {
            hide();
        }
    });
    window->installEventFilter(this);
    hide();
}

Overlay::~Overlay() = default;

void Overlay::open()
{
    if (m_open) {
        return;
    }
    m_open = true;
    m_previousFocus = QApplication::focusWidget();
    setGeometry(m_window->rect());
    if (backdropBlurRadius() > 0) {
        m_backdrop = std::make_unique<BackdropBlur>(this, backdropBlurRadius());
        m_backdrop->capture();
    }
    relayout();
    show();
    raise();
    m_fade->stop();
    m_fade->setStartValue(m_presence);
    m_fade->setEndValue(1.0);
    m_fade->start();
    emit opened();
}

void Overlay::close()
{
    if (!m_open) {
        return;
    }
    m_open = false;
    if (m_previousFocus && m_previousFocus->isVisible() && !isAncestorOf(m_previousFocus)) {
        m_previousFocus->setFocus(Qt::OtherFocusReason);
    } else {
        m_window->setFocus(Qt::OtherFocusReason);
    }
    m_fade->stop();
    m_fade->setStartValue(m_presence);
    m_fade->setEndValue(0.0);
    m_fade->start();
    emit closed();
}

void Overlay::relayout()
{
    if (QWidget *content = panel()) {
        content->setGeometry(panelGeometry());
    }
}

bool Overlay::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event->type() == QEvent::Resize && isVisible()) {
        if (m_backdrop) {
            m_backdrop->invalidate();
        }
        setGeometry(m_window->rect());
        relayout();
        QMetaObject::invokeMethod(
            this,
            [this] {
                if (isVisible()) {
                    raise();
                }
            },
            Qt::QueuedConnection);
    }
    return QWidget::eventFilter(watched, event);
}

void Overlay::mousePressEvent(QMouseEvent *event)
{
    if (QWidget *content = panel(); content && !content->geometry().contains(event->position().toPoint())) {
        close();
        return;
    }
    QWidget::mousePressEvent(event);
}

void Overlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}

void Overlay::paintPanel(QPainter &painter, const QRectF &rect)
{
    Q_UNUSED(painter);
    Q_UNUSED(rect);
}

void Overlay::paintEvent(QPaintEvent *)
{
    if (BackdropBlur::isCapturing()) {
        return;
    }
    QPainter painter(this);
    preparePainter(painter);
    painter.setOpacity(m_presence);
    if (m_backdrop) {
        painter.drawPixmap(0, 0, m_backdrop->pixmap());
    }
    painter.fillRect(rect(), dimColor());
    if (QWidget *content = panel()) {
        const QRectF box(content->geometry());
        ShadowPainter::paint(painter, box, panelRadius(), panelShadow());
        paintPanel(painter, box);
    }
}

}  // namespace sky
