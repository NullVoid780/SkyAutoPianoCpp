#include "ui/main_window/nav_drawer.hpp"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QVBoxLayout>

#include "core/translator.hpp"
#include "ui/core/easing.hpp"
#include "ui/main_window/nav_tab.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kMaxWidth = 260;
constexpr int kShadowRoom = 34;
constexpr int kPadTop = 76;
constexpr int kPadSide = 10;
constexpr int kListPadding = 4;
constexpr int kItemGap = 6;
constexpr int kHiddenExtra = 16;

}  // namespace

NavDrawer::NavDrawer(QWidget *window)
    : QWidget(window), m_window(window), m_panel(new QWidget(this)), m_slide(new QPropertyAnimation(this, "pos", this))
{
    setAttribute(Qt::WA_NoSystemBackground);
    m_slide->setDuration(kDrawerSlideMs);
    m_slide->setEasingCurve(easingCurve(Easing::Ease));
    connect(m_slide, &QPropertyAnimation::finished, this, [this] {
        if (!m_open) {
            hide();
        }
    });

    auto *list = new QVBoxLayout(m_panel);
    list->setContentsMargins(kPadSide + kListPadding, kPadTop + kListPadding, kPadSide + kListPadding, kPadSide + kListPadding);
    list->setSpacing(kItemGap);
    const std::pair<LibraryTab, std::pair<const char *, const char *>> items[] = {
        {LibraryTab::AllSongs, {"music-note-list", trNoop("All Songs")}},
        {LibraryTab::Favorite, {"heart", trNoop("Favorite")}},
        {LibraryTab::Playlists, {"folder2", trNoop("Playlists")}},
        {LibraryTab::RecentPlay, {"clock", trNoop("Recent Play")}},
        {LibraryTab::Store, {"shop", trNoop("Store")}},
    };
    for (const auto &[tab, labels] : items) {
        auto *button = new NavTab(QString::fromLatin1(labels.first), tr(labels.second), m_panel);
        const LibraryTab chosen = tab;
        connect(button, &QAbstractButton::clicked, this, [this, chosen] {
            emit tabChosen(chosen);
            setOpen(false);
        });
        list->addWidget(button);
        m_tabs.emplace_back(tab, button);
    }
    list->addStretch();
    setActiveTab(LibraryTab::AllSongs);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
    hide();
}

int NavDrawer::panelWidth() const
{
    return std::min(kMaxWidth, m_window->width());
}

int NavDrawer::closedX() const
{
    return -(panelWidth() + kShadowRoom + kHiddenExtra);
}

void NavDrawer::setToggle(QWidget *toggle)
{
    m_toggle = toggle;
}

void NavDrawer::syncToWindow()
{
    resize(panelWidth() + kShadowRoom, m_window->height());
    m_panel->setGeometry(0, 0, panelWidth(), m_window->height());
    if (m_slide->state() != QAbstractAnimation::Running) {
        move(m_open ? 0 : closedX(), 0);
    }
}

void NavDrawer::setActiveTab(LibraryTab tab)
{
    for (const auto &[kind, button] : m_tabs) {
        button->setChecked(kind == tab);
    }
    update();
}

void NavDrawer::setOpen(bool open)
{
    if (open == m_open) {
        return;
    }
    m_open = open;
    syncToWindow();
    if (open) {
        show();
        raise();
        if (m_toggle) {
            m_toggle->raise();
        }
        qApp->installEventFilter(this);
        for (const auto &[kind, button] : m_tabs) {
            if (button->isChecked()) {
                button->setFocus();
            }
        }
    } else {
        qApp->removeEventFilter(this);
    }
    m_slide->stop();
    m_slide->setStartValue(pos());
    m_slide->setEndValue(QPoint(open ? 0 : closedX(), 0));
    m_slide->start();
    emit openChanged(open);
}

bool NavDrawer::eventFilter(QObject *watched, QEvent *event)
{
    if (m_open && event->type() == QEvent::MouseButtonPress) {
        auto *target = qobject_cast<QWidget *>(watched);
        if (target && target->window() == m_window) {
            const QPoint global = static_cast<QMouseEvent *>(event)->globalPosition().toPoint();
            const QRect panel(m_panel->mapToGlobal(QPoint(0, 0)), m_panel->size());
            const bool onToggle = m_toggle && QRect(m_toggle->mapToGlobal(QPoint(0, 0)), m_toggle->size()).contains(global);
            if (!panel.contains(global) && !onToggle) {
                setOpen(false);
            }
        }
    }
    if (m_open && event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        setOpen(false);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void NavDrawer::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        setOpen(false);
        return;
    }
    QWidget::keyPressEvent(event);
}

void NavDrawer::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    const QRectF panel(0, 0, panelWidth(), height());
    ShadowPainter::paint(painter, panel, 0, theme.shadowDrawer);
    painter.fillRect(panel, theme.drawerFill);
}

}  // namespace sky
