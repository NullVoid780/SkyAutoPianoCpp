#include "ui/settings_window/update_prompt.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QStyle>
#include <QTextDocument>
#include <QVBoxLayout>

#include <algorithm>

#include "core/translator.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/settings_window/action_button.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/icon_button.hpp"
#include "ui/widgets/markdown_view.hpp"
#include "ui/widgets/progress_bar.hpp"

namespace sky {

namespace {

constexpr int kMaxWidth = 560;
constexpr qreal kWidthRatio = 0.86;
constexpr qreal kHeightRatio = 0.82;
constexpr int kPadding = 22;
constexpr int kCloseBox = 28;
constexpr int kCloseIcon = 14;
constexpr int kHeaderGap = 14;
constexpr int kSectionGap = 14;
constexpr int kMinNotes = 80;
constexpr int kMaxNotes = 340;
constexpr qreal kRise = 30;

}  // namespace

UpdatePrompt::UpdatePrompt(QWidget *window)
    : Overlay(window),
      m_panel(new QWidget(this)),
      m_heading(new QLabel(m_panel)),
      m_versionOld(new QLabel(m_panel)),
      m_arrow(new QLabel(m_panel)),
      m_versionNew(new QLabel(m_panel)),
      m_critical(new QLabel(tr("This update includes an important fix. Please install it."), m_panel)),
      m_notes(new MarkdownView(MarkdownView::Style{13, [] { return currentTheme().update.promptBody; },
                                                   [] { return currentTheme().update.versionNew; },
                                                   [] { return currentTheme().update.codeFill; },
                                                   [] { return currentTheme().update.promptHeading; }},
                               m_panel)),
      m_progress(new ProgressBar(ProgressBar::Palette{[] { return currentTheme().update.progressTrack; },
                                                      [] { return currentTheme().hoverColor; },
                                                      [] { return currentTheme().settings.tileEnd; }},
                                 m_panel)),
      m_status(new QLabel(m_panel)),
      m_primary(new ActionButton(ActionButton::Kind::UpdateNow, QStringLiteral("download"),
                                 tr("Update now"), m_panel)),
      m_later(new ActionButton(ActionButton::Kind::Later, QString(), tr("Not now"), m_panel)),
      m_github(new QPushButton(tr("View on GitHub"), m_panel)),
      m_close(new IconButton(QStringLiteral("x-lg"), kCloseIcon, QSize(kCloseBox, kCloseBox),
                             IconButtonStyle{[] { return currentTheme().update.promptVersion; },
                                             [] { return currentTheme().update.promptHeading; }},
                             m_panel))
{
    m_heading->setObjectName(QStringLiteral("UpdatePromptHeading"));
    m_heading->setAlignment(Qt::AlignCenter);
    m_versionOld->setObjectName(QStringLiteral("UpdateVersionOld"));
    m_versionNew->setObjectName(QStringLiteral("UpdateVersionNew"));
    m_critical->setObjectName(QStringLiteral("UpdatePromptStatus"));
    m_critical->setProperty("error", true);
    m_critical->setAlignment(Qt::AlignCenter);
    m_critical->setWordWrap(true);
    m_notes->setObjectName(QStringLiteral("UpdatePromptNotes"));
    QScrollBar *notesBar = m_notes->verticalScrollBar();
    notesBar->setObjectName(QStringLiteral("UpdateNotesScrollBar"));
    notesBar->style()->unpolish(notesBar);
    notesBar->style()->polish(notesBar);
    m_status->setObjectName(QStringLiteral("UpdatePromptStatus"));
    m_status->setWordWrap(true);
    m_close->setToolTip(tr("Close"));
    m_github->setObjectName(QStringLiteral("UpdatePromptLink"));
    m_github->setCursor(Qt::PointingHandCursor);
    m_github->setFlat(true);

    auto *versionRow = new QHBoxLayout();
    versionRow->setSpacing(10);
    versionRow->addStretch();
    versionRow->addWidget(m_versionOld);
    versionRow->addWidget(m_arrow);
    versionRow->addWidget(m_versionNew);
    versionRow->addStretch();

    auto *progressRow = new QVBoxLayout();
    progressRow->setSpacing(8);
    progressRow->addWidget(m_progress);
    progressRow->addWidget(m_status);

    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(12);
    buttons->addWidget(m_github, 0, Qt::AlignVCenter);
    buttons->addStretch();
    buttons->addWidget(m_later);
    buttons->addWidget(m_primary);

    auto *column = new QVBoxLayout(m_panel);
    column->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    column->setSpacing(0);
    column->addWidget(m_heading);
    column->addSpacing(4);
    column->addLayout(versionRow);
    column->addSpacing(6);
    column->addWidget(m_critical);
    column->addSpacing(kHeaderGap);
    column->addWidget(m_notes, 1);
    column->addSpacing(kSectionGap);
    column->addLayout(progressRow);
    column->addSpacing(kSectionGap);
    column->addLayout(buttons);

    m_panel->installEventFilter(this);
    connect(m_close, &QAbstractButton::clicked, this, &Overlay::close);
    connect(m_github, &QAbstractButton::clicked, this, &UpdatePrompt::releasePageRequested);
    connect(m_later, &QAbstractButton::clicked, this, &Overlay::close);
    connect(m_primary, &QAbstractButton::clicked, this, [this] {
        if (m_state == State::Ready) {
            emit restartRequested();
        } else if (m_state != State::Downloading) {
            emit installRequested();
        }
    });
    connect(this, &Overlay::closed, this, [this] {
        if (m_state == State::Offer) {
            emit dismissed();
        }
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &UpdatePrompt::refreshVersion);
}

void UpdatePrompt::refreshVersion()
{
    m_versionOld->setText(m_currentVersion);
    m_versionNew->setText(m_latestVersion);
    m_arrow->setPixmap(IconCache::pixmap(QStringLiteral("arrow-right"), 15, currentTheme().update.versionOld,
                                         devicePixelRatioF()));
}

void UpdatePrompt::present(const QString &currentVersion, const QString &latestVersion, const QString &markdown,
                           bool critical)
{
    m_currentVersion = currentVersion;
    m_latestVersion = latestVersion;
    m_state = State::Offer;
    m_heading->setText(tr("A new update is available!"));
    refreshVersion();
    m_critical->setVisible(critical);
    m_primary->setBusy(false);
    m_primary->setText(tr("Update now"));
    m_primary->setIconId(QStringLiteral("download"));
    m_later->setText(tr("Not now"));
    m_later->setVisible(true);
    m_progress->setVisible(false);
    m_progress->setValue(0);
    m_status->setVisible(false);
    setNotes(markdown);
    open();
    m_primary->setFocus(Qt::OtherFocusReason);
}

void UpdatePrompt::setNotes(const QString &markdown)
{
    m_notes->setMarkdownText(markdown);
    fitNotes();
}

void UpdatePrompt::fitNotes()
{
    const QWidget *window = parentWidget();
    const int panelWidth = std::min(kMaxWidth, static_cast<int>(window->width() * kWidthRatio));
    m_notes->document()->setTextWidth(std::max(200, panelWidth - kPadding * 2 - 8));
    const int content = static_cast<int>(m_notes->document()->size().height()) + 8;
    m_notes->setFixedHeight(kMinNotes);
    m_panel->layout()->invalidate();
    const int others = m_panel->layout()->totalSizeHint().height() - kMinNotes;
    const int room = static_cast<int>(window->height() * kHeightRatio) - others;
    m_notes->setFixedHeight(std::clamp(content, kMinNotes, std::clamp(room, kMinNotes, kMaxNotes)));
    relayout();
}

void UpdatePrompt::showDownloading(int percent)
{
    m_state = State::Downloading;
    m_primary->setBusy(true, tr("Downloading..."));
    m_later->setVisible(false);
    m_progress->setVisible(true);
    m_progress->setValue(percent);
    setStatus(tr("Downloading update... %1%").arg(percent), false);
}

void UpdatePrompt::showReady()
{
    m_state = State::Ready;
    m_heading->setText(tr("Update ready to install"));
    m_primary->setBusy(false);
    m_primary->setText(tr("Restart now"));
    m_primary->setIconId(QStringLiteral("update"));
    m_later->setText(tr("Later"));
    m_later->setVisible(true);
    m_progress->setVisible(true);
    m_progress->setValue(100);
    setStatus(tr("Downloaded and checked. It will also install automatically when you close the app."),
              false);
    m_primary->setFocus(Qt::OtherFocusReason);
}

void UpdatePrompt::showFailed(const QString &message)
{
    m_state = State::Failed;
    m_primary->setBusy(false);
    m_primary->setText(tr("Try again"));
    m_primary->setIconId(QStringLiteral("update"));
    m_later->setText(tr("Not now"));
    m_later->setVisible(true);
    m_progress->setVisible(false);
    setStatus(message, true);
}

void UpdatePrompt::setStatus(const QString &text, bool error)
{
    m_status->setText(text);
    m_status->setProperty("error", error);
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);
    m_status->setVisible(true);
    fitNotes();
}

bool UpdatePrompt::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible()) {
        QMetaObject::invokeMethod(this, [this] { fitNotes(); }, Qt::QueuedConnection);
    }
    if (watched == m_panel && event->type() == QEvent::Resize) {
        m_close->move(m_panel->width() - kPadding / 2 - kCloseBox, kPadding / 2);
        m_close->raise();
    }
    return Overlay::eventFilter(watched, event);
}

QColor UpdatePrompt::dimColor() const
{
    return currentTheme().settings.modalDim;
}

ShadowSpec UpdatePrompt::panelShadow() const
{
    return currentTheme().settings.modalShadow;
}

QRect UpdatePrompt::panelGeometry() const
{
    const QWidget *window = parentWidget();
    const int width = std::min(kMaxWidth, static_cast<int>(window->width() * kWidthRatio));
    const int maxHeight = static_cast<int>(window->height() * kHeightRatio);
    QLayout *layout = m_panel->layout();
    const int wanted = layout->hasHeightForWidth() ? layout->totalHeightForWidth(width) : layout->totalSizeHint().height();
    const int height = std::min(maxHeight, wanted);
    const int rise = qRound(kRise * (1.0 - presence()));
    return QRect((window->width() - width) / 2, (window->height() - height) / 2 + rise, width, height);
}

void UpdatePrompt::paintPanel(QPainter &painter, const QRectF &rect)
{
    const UpdatePalette &u = currentTheme().update;
    const QPainterPath path = roundedPath(rect.adjusted(0.5, 0.5, -0.5, -0.5), panelRadius());
    painter.fillPath(path, u.promptFill);
    painter.setPen(QPen(u.promptBorder, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

}  // namespace sky
