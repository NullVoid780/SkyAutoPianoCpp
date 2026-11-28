#include "ui/settings_window/confirm_dialog.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include <algorithm>

#include "core/translator.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/icon_button.hpp"

namespace sky {

namespace {

constexpr int kMaxWidth = 460;
constexpr qreal kWidthRatio = 0.9;
constexpr qreal kRadius = 20;
constexpr int kPadding = 28;
constexpr int kIconCircle = 72;
constexpr int kIconGlyph = 30;
constexpr int kIconGap = 18;
constexpr int kTitleGap = 10;
constexpr int kButtonsGap = 26;
constexpr int kButtonGap = 12;
constexpr int kCloseBox = 30;
constexpr int kCloseIcon = 14;
constexpr int kCloseInset = 14;
constexpr qreal kRise = 20;

}  // namespace

ConfirmDialog::ConfirmDialog(QWidget *window, Content content)
    : Overlay(window),
      m_iconId(content.iconId),
      m_panel(new QWidget(this)),
      m_iconSpace(new QWidget(m_panel)),
      m_secondary(new ActionButton(ActionButton::Kind::Subtle, content.secondaryIconId, content.secondaryLabel, m_panel)),
      m_primary(new ActionButton(content.primaryKind, content.primaryIconId, content.primaryLabel, m_panel)),
      m_close(new IconButton(QStringLiteral("x-lg"), kCloseIcon, QSize(kCloseBox, kCloseBox),
                             IconButtonStyle{[] { return currentTheme().settings.cardDescText; },
                                             [] { return currentTheme().fontColor; }},
                             m_panel))
{
    m_iconSpace->setFixedSize(kIconCircle, kIconCircle);
    m_iconSpace->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_close->setToolTip(tr("Close"));
    m_close->setAccessibleName(tr("Close"));
    m_secondary->setMinimumWidth(m_primary->sizeHint().width());

    auto *title = new QLabel(content.title, m_panel);
    title->setObjectName(QStringLiteral("ConfirmTitle"));
    title->setAlignment(Qt::AlignCenter);
    auto *body = new QLabel(content.body, m_panel);
    body->setObjectName(QStringLiteral("ConfirmBody"));
    body->setAlignment(Qt::AlignCenter);
    body->setWordWrap(true);

    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(kButtonGap);
    buttons->addStretch();
    buttons->addWidget(m_secondary);
    buttons->addWidget(m_primary);
    buttons->addStretch();

    auto *column = new QVBoxLayout(m_panel);
    column->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    column->setSpacing(0);
    column->addWidget(m_iconSpace, 0, Qt::AlignHCenter);
    column->addSpacing(kIconGap);
    column->addWidget(title);
    column->addSpacing(kTitleGap);
    column->addWidget(body);
    column->addSpacing(kButtonsGap);
    column->addLayout(buttons);

    m_panel->installEventFilter(this);
    connect(m_close, &QAbstractButton::clicked, this, [this] { finish(Choice::Dismissed); });
    connect(m_secondary, &QAbstractButton::clicked, this, [this] { finish(Choice::Secondary); });
    connect(m_primary, &QAbstractButton::clicked, this, [this] { finish(Choice::Primary); });
    connect(this, &Overlay::closed, this, [this] {
        auto done = std::move(m_done);
        m_done = nullptr;
        if (done) {
            done(m_choice);
        }
    });
}

void ConfirmDialog::ask(std::function<void(Choice)> done)
{
    m_done = std::move(done);
    m_choice = Choice::Dismissed;
    open();
    m_secondary->setFocus(Qt::OtherFocusReason);
}

void ConfirmDialog::finish(Choice choice)
{
    m_choice = choice;
    close();
}

bool ConfirmDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_panel && event->type() == QEvent::Resize) {
        m_close->move(m_panel->width() - kCloseInset - kCloseBox, kCloseInset);
        m_close->raise();
    }
    return Overlay::eventFilter(watched, event);
}

QColor ConfirmDialog::dimColor() const
{
    return currentTheme().settings.modalDim;
}

ShadowSpec ConfirmDialog::panelShadow() const
{
    return currentTheme().settings.modalShadow;
}

QRect ConfirmDialog::panelGeometry() const
{
    const int width = std::min(kMaxWidth, static_cast<int>(parentWidget()->width() * kWidthRatio));
    QLayout *layout = m_panel->layout();
    const int height = layout->hasHeightForWidth() ? layout->totalHeightForWidth(width) : layout->totalSizeHint().height();
    const int rise = qRound(kRise * (1.0 - presence()));
    return QRect((parentWidget()->width() - width) / 2, (parentWidget()->height() - height) / 2 + rise, width, height);
}

void ConfirmDialog::paintPanel(QPainter &painter, const QRectF &rect)
{
    const SettingsPalette &s = currentTheme().settings;
    const QPainterPath path = roundedPath(rect.adjusted(0.5, 0.5, -0.5, -0.5), kRadius);
    painter.fillPath(path, s.modalFill);
    painter.setPen(QPen(s.modalBorder, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    const QRectF circle(QRectF(m_iconSpace->geometry()).translated(rect.topLeft()));
    painter.setPen(QPen(withAlpha(s.amberSolid, 0.35), 1));
    painter.setBrush(s.amberSoft);
    painter.drawEllipse(circle.adjusted(0.5, 0.5, -0.5, -0.5));
    painter.drawPixmap(QPointF(circle.center().x() - kIconGlyph / 2.0, circle.center().y() - kIconGlyph / 2.0),
                       IconCache::pixmap(m_iconId, kIconGlyph, s.amberSolidHover,
                                         painter.device()->devicePixelRatioF()));
}

}  // namespace sky
