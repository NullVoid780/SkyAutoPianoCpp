#include "ui/main_window/library_view.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>

#include "ui/core/easing.hpp"
#include "ui/main_window/library_model.hpp"
#include "ui/main_window/sheet_card_delegate.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kWheelStepPixels = 100;
constexpr int kWheelAnimationMs = 160;

}  // namespace

LibraryView::LibraryView(LibraryModel *model, QWidget *parent)
    : QListView(parent),
      m_model(model),
      m_delegate(new SheetCardDelegate(this)),
      m_scrollAnimation(new QPropertyAnimation(verticalScrollBar(), "value", this))
{
    m_scrollAnimation->setEasingCurve(easingCurve(Easing::EaseOut));
    setObjectName(QStringLiteral("LibraryList"));
    setModel(model);
    setItemDelegate(m_delegate);
    setUniformItemSizes(true);
    setMouseTracking(true);
    setSelectionMode(QAbstractItemView::NoSelection);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    viewport()->setAttribute(Qt::WA_Hover);
    verticalScrollBar()->setSingleStep(40);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &LibraryView::scrolled);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, viewport(), [this] { viewport()->update(); });
}

void LibraryView::setOpenMenuRow(int row)
{
    m_delegate->setOpenMenuRow(row);
    viewport()->update();
}

void LibraryView::animateScrollTo(int target, int durationMs)
{
    QScrollBar *bar = verticalScrollBar();
    m_scrollTarget = std::clamp(target, bar->minimum(), bar->maximum());
    m_scrollAnimation->stop();
    m_scrollAnimation->setDuration(durationMs);
    m_scrollAnimation->setStartValue(bar->value());
    m_scrollAnimation->setEndValue(m_scrollTarget);
    m_scrollAnimation->start();
}

void LibraryView::scrollToTopSmoothly()
{
    animateScrollTo(0, kHoverSlowMs);
}

void LibraryView::wheelEvent(QWheelEvent *event)
{
    const int notches = event->angleDelta().y();
    if (!event->pixelDelta().isNull() || notches == 0 || (event->modifiers() & Qt::ControlModifier)) {
        m_scrollAnimation->stop();
        QListView::wheelEvent(event);
        return;
    }
    const int from = m_scrollAnimation->state() == QAbstractAnimation::Running ? m_scrollTarget
                                                                                 : verticalScrollBar()->value();
    animateScrollTo(from - notches * kWheelStepPixels / 120, kWheelAnimationMs);
    event->accept();
}

void LibraryView::mouseMoveEvent(QMouseEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    const bool onAction = index.isValid() && !m_model->bulkMode() &&
                          SheetCardDelegate::actionRect(visualRect(index)).contains(event->position());
    const bool onCard = index.isValid() && SheetCardDelegate::cardRect(visualRect(index)).contains(event->position());
    m_delegate->setHover(index.isValid() ? index.row() : -1, onAction);
    viewport()->setCursor(onCard ? Qt::PointingHandCursor : Qt::ArrowCursor);
    viewport()->update();
    QListView::mouseMoveEvent(event);
}

void LibraryView::leaveEvent(QEvent *event)
{
    m_delegate->setHover(-1, false);
    viewport()->update();
    QListView::leaveEvent(event);
}

void LibraryView::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    const QModelIndex index = indexAt(event->position().toPoint());
    if (index.isValid()) {
        const bool scrolls = hasAutoScroll();
        setAutoScroll(false);
        setCurrentIndex(index);
        setAutoScroll(scrolls);
    }
    event->accept();
}

void LibraryView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const QModelIndex index = indexAt(event->position().toPoint());
    if (!index.isValid()) {
        return;
    }
    const QRect row = visualRect(index);
    const int libraryIndex = m_model->libraryIndex(index.row());
    const QRectF action = SheetCardDelegate::actionRect(row);
    if (!m_model->bulkMode() && action.contains(event->position())) {
        const QRect anchor(viewport()->mapToGlobal(action.topLeft().toPoint()), action.size().toSize());
        setOpenMenuRow(index.row());
        emit actionMenuRequested(libraryIndex, anchor);
        return;
    }
    if (SheetCardDelegate::cardRect(row).contains(event->position())) {
        emit cardActivated(libraryIndex);
    }
}

void LibraryView::keyboardSearch(const QString &)
{
}

void LibraryView::keyPressEvent(QKeyEvent *event)
{
    const QModelIndex index = currentIndex();
    if (index.isValid()) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
            emit cardActivated(m_model->libraryIndex(index.row()));
            return;
        }
        if (!m_model->bulkMode() &&
            (event->key() == Qt::Key_Menu || (event->key() == Qt::Key_F10 && event->modifiers() & Qt::ShiftModifier))) {
            const QRectF action = SheetCardDelegate::actionRect(visualRect(index));
            setOpenMenuRow(index.row());
            emit actionMenuRequested(m_model->libraryIndex(index.row()),
                                     QRect(viewport()->mapToGlobal(action.topLeft().toPoint()), action.size().toSize()));
            return;
        }
    }
    QListView::keyPressEvent(event);
}

}  // namespace sky
