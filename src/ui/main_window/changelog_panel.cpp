#include "ui/main_window/changelog_panel.hpp"

#include <QDesktopServices>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPainter>
#include <QPushButton>
#include <QTextDocument>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/markdown_view.hpp"

namespace sky {

namespace {

constexpr int kMaxWidth = 480;
constexpr qreal kWidthRatio = 0.9;
constexpr qreal kHeightRatio = 0.8;
constexpr int kPadding = 25;
constexpr qreal kRadius = 16;
constexpr int kRuleWidth = 40;
constexpr int kRuleHeight = 2;
constexpr int kRuleGap = 12;
constexpr int kHeaderGap = 25;
constexpr int kBodyGap = 20;
constexpr int kBodyPadding = 15;
constexpr int kBodyMinHeight = 60;
constexpr int kBodyMaxHeight = 320;
constexpr int kNotesRightMargin = 10;
constexpr qreal kPillRadius = 25;

}  // namespace

ChangelogPanel::ChangelogPanel(QWidget *window)
    : Overlay(window),
      m_panel(new QWidget(this)),
      m_title(new QLabel(QStringLiteral("What's New!"), m_panel)),
      m_pill(new QLabel(m_panel)),
      m_notes(new MarkdownView(MarkdownView::Style{14, [] { return currentTheme().fontColor; },
                                                   [] { return currentTheme().hoverColor; },
                                                   [] { return currentTheme().update.codeFill; }},
                               m_panel)),
      m_readMore(new QPushButton(QStringLiteral("Read in browser"), m_panel)),
      m_gotIt(new QPushButton(QStringLiteral("Got it!"), m_panel))
{
    m_readMore->setObjectName(QStringLiteral("ChangelogSecondaryButton"));
    m_readMore->setCursor(Qt::PointingHandCursor);
    m_readMore->hide();
    m_title->setObjectName(QStringLiteral("ChangelogTitle"));
    m_title->setAlignment(Qt::AlignCenter);
    m_pill->setObjectName(QStringLiteral("ChangelogPill"));
    m_pill->setAlignment(Qt::AlignCenter);
    m_notes->setObjectName(QStringLiteral("ChangelogNotes"));
    m_notes->setContentsMargins(0, 0, kNotesRightMargin, 0);
    m_gotIt->setObjectName(QStringLiteral("ChangelogButton"));
    m_gotIt->setCursor(Qt::PointingHandCursor);

    auto *pillRow = new QHBoxLayout();
    pillRow->addStretch();
    pillRow->addWidget(m_pill);
    pillRow->addStretch();
    auto *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(12);
    buttonRow->addStretch();
    buttonRow->addWidget(m_readMore);
    buttonRow->addWidget(m_gotIt);
    buttonRow->addStretch();

    auto *column = new QVBoxLayout(m_panel);
    column->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    column->setSpacing(0);
    column->addWidget(m_title);
    column->addSpacing(kRuleGap + kRuleHeight + kRuleGap);
    column->addLayout(pillRow);
    column->addSpacing(kHeaderGap + kBodyGap + kBodyPadding);
    column->addWidget(m_notes, 1);
    column->addSpacing(kBodyPadding + kBodyGap);
    column->addLayout(buttonRow);

    connect(m_gotIt, &QPushButton::clicked, this, &Overlay::close);
    connect(m_readMore, &QPushButton::clicked, this, [this] {
        const QUrl url(m_pageUrl);
        if (url.isValid() && url.scheme() == QStringLiteral("https")) {
            QDesktopServices::openUrl(url);
        }
    });
}

void ChangelogPanel::present(const QString &version, const QString &notes, const QString &pageUrl)
{
    m_pill->setText(QStringLiteral("V") + version.toUpper());
    m_pageUrl = pageUrl;
    m_readMore->setVisible(!pageUrl.isEmpty());
    m_readMore->setToolTip(pageUrl);
    m_notes->setMarkdownText(notes);
    fitNotes();
    open();
    m_gotIt->setFocus(Qt::OtherFocusReason);
}

int ChangelogPanel::panelWidth() const
{
    return std::min(kMaxWidth, static_cast<int>(parentWidget()->width() * kWidthRatio));
}

void ChangelogPanel::fitNotes()
{
    const int textWidth = std::max(160, panelWidth() - kPadding * 2 - kNotesRightMargin - 8);
    m_notes->document()->setTextWidth(textWidth);
    const int content = static_cast<int>(m_notes->document()->size().height()) + 8;
    m_notes->setFixedHeight(kBodyMinHeight);
    m_panel->layout()->invalidate();
    const int others = m_panel->layout()->totalSizeHint().height() - kBodyMinHeight;
    const int room = static_cast<int>(parentWidget()->height() * kHeightRatio) - others;
    m_notes->setFixedHeight(std::clamp(content, kBodyMinHeight, std::clamp(room, kBodyMinHeight, kBodyMaxHeight)));
    relayout();
}

bool ChangelogPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible()) {
        QMetaObject::invokeMethod(this, [this] { fitNotes(); }, Qt::QueuedConnection);
    }
    return Overlay::eventFilter(watched, event);
}

QColor ChangelogPanel::dimColor() const
{
    return currentTheme().update.changelogDim;
}

ShadowSpec ChangelogPanel::panelShadow() const
{
    return currentTheme().update.changelogShadow;
}

QRect ChangelogPanel::panelGeometry() const
{
    const QWidget *window = parentWidget();
    const int width = panelWidth();
    const QLayout *layout = m_panel->layout();
    const int wanted = layout->hasHeightForWidth() ? layout->totalHeightForWidth(width) : layout->totalSizeHint().height();
    const int height = std::min(static_cast<int>(window->height() * kHeightRatio), wanted);
    return QRect((window->width() - width) / 2, (window->height() - height) / 2, width, height);
}

void ChangelogPanel::paintPanel(QPainter &painter, const QRectF &rect)
{
    const Theme &theme = currentTheme();
    const UpdatePalette &u = theme.update;
    const QPainterPath path = roundedPath(rect.adjusted(0.5, 0.5, -0.5, -0.5), kRadius);
    painter.fillPath(path, theme.background);
    painter.setPen(QPen(u.changelogBorder, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    const QRectF title(m_title->mapTo(this, QPoint(0, 0)), QSizeF(m_title->size()));
    const QRectF rule(title.center().x() - kRuleWidth / 2.0, title.bottom() + kRuleGap, kRuleWidth, kRuleHeight);
    painter.fillRect(rule, withAlpha(theme.fontColor, 0.35));

    const QRectF pill = QRectF(m_pill->mapTo(this, QPoint(0, 0)), QSizeF(m_pill->size()));
    ShadowPainter::paint(painter, pill, kPillRadius, u.pillShadow);
    const QPainterPath pillPath = roundedPath(pill.adjusted(0.5, 0.5, -0.5, -0.5), std::min(kPillRadius, pill.height() / 2));
    painter.fillPath(pillPath, u.pillStart);
    painter.setPen(QPen(u.pillBorder, 1));
    painter.drawPath(pillPath);

    const QRectF notes(m_notes->mapTo(this, QPoint(0, 0)), QSizeF(m_notes->size()));
    painter.fillRect(QRectF(notes.left(), notes.top() - kBodyPadding, notes.width(), 1), u.rule);
    painter.fillRect(QRectF(notes.left(), notes.bottom() + kBodyPadding, notes.width(), 1), u.rule);

    const QRectF button(m_gotIt->mapTo(this, QPoint(0, 0)), QSizeF(m_gotIt->size()));
    ShadowPainter::paint(painter, button, 8, u.gotItShadow);
}

}  // namespace sky
