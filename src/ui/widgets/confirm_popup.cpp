#include "ui/widgets/confirm_popup.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kMaxWidth = 450;
constexpr qreal kRadius = 12;

}  // namespace

ConfirmPopup::ConfirmPopup(QWidget *window)
    : Overlay(window),
      m_panel(new QWidget(this)),
      m_message(new QLabel(m_panel)),
      m_confirm(new QPushButton(QStringLiteral("OK"), m_panel)),
      m_cancel(new QPushButton(QStringLiteral("Cancel"), m_panel))
{
    m_message->setObjectName(QStringLiteral("ConfirmMessage"));
    m_message->setAlignment(Qt::AlignCenter);
    m_message->setWordWrap(true);
    m_confirm->setObjectName(QStringLiteral("ConfirmOk"));
    m_cancel->setObjectName(QStringLiteral("ConfirmCancel"));
    for (QPushButton *button : {m_confirm, m_cancel}) {
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    auto *buttons = new QHBoxLayout();
    buttons->setContentsMargins(0, 1, 0, 0);
    buttons->setSpacing(0);
    buttons->addWidget(m_confirm);
    buttons->addWidget(m_cancel);

    auto *column = new QVBoxLayout(m_panel);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(m_message);
    column->addLayout(buttons);

    connect(m_confirm, &QPushButton::clicked, this, [this] {
        auto action = std::move(m_onConfirm);
        m_onConfirm = nullptr;
        close();
        if (action) {
            action();
        }
    });
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        m_onConfirm = nullptr;
        close();
    });
}

void ConfirmPopup::ask(const QString &message, std::function<void()> onConfirm)
{
    m_message->setText(message);
    m_onConfirm = std::move(onConfirm);
    open();
    QTimer::singleShot(0, m_cancel, [this] { m_cancel->setFocus(); });
}

QWidget *ConfirmPopup::panel() const
{
    return m_panel;
}

QColor ConfirmPopup::dimColor() const
{
    return currentTheme().confirmOverlay;
}

ShadowSpec ConfirmPopup::panelShadow() const
{
    return currentTheme().shadowConfirm;
}

qreal ConfirmPopup::panelRadius() const
{
    return kRadius;
}

QRect ConfirmPopup::panelGeometry() const
{
    const int width = std::min(kMaxWidth, static_cast<int>(this->width() * 0.9));
    m_message->setFixedWidth(width);
    const int height = m_panel->sizeHint().height();
    return QRect((this->width() - width) / 2, static_cast<int>(this->height() * 0.1), width, height);
}

void ConfirmPopup::paintPanel(QPainter &painter, const QRectF &rect)
{
    const Theme &theme = currentTheme();
    const QPainterPath path = roundedPath(rect, kRadius);
    painter.fillPath(path, theme.confirmFill);
    painter.save();
    painter.setClipPath(path);
    const QRectF message(rect.left(), rect.top(), rect.width(), m_message->height());
    painter.fillRect(message, theme.confirmMessageFill);
    painter.fillRect(QRectF(rect.left(), message.bottom(), rect.width(), 1), theme.confirmRule);
    painter.restore();
}

}  // namespace sky
