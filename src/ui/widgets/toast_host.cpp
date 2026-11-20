#include "ui/widgets/toast_host.hpp"

#include <QEvent>
#include <QWidget>

#include <algorithm>

#include "ui/core/elevation_layer.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/toast_item.hpp"

namespace sky {

namespace {

constexpr int kEdge = 12;
constexpr int kGap = 12;

}  // namespace

ToastHost::ToastHost(QWidget *window) : QObject(window), m_window(window), m_shadows(new ElevationLayer(window))
{
    m_shadows->setFloating(true);
    window->installEventFilter(this);
}

bool ToastHost::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event->type() == QEvent::Resize) {
        relayout();
    }
    return false;
}

namespace {

std::function<void(const Toast &)> &toastObserver()
{
    static std::function<void(const Toast &)> observer;
    return observer;
}

}  // namespace

void ToastHost::setObserver(std::function<void(const Toast &)> observer)
{
    toastObserver() = std::move(observer);
}

void ToastHost::show(const Toast &toast)
{
    if (const auto &observer = toastObserver()) {
        observer(toast);
    }
    if (toast.message.empty() && toast.title.empty()) {
        return;
    }
    auto *item = new ToastItem(toast, m_window);
    m_items.push_back(item);
    m_shadows->track(item, 12, [item] {
        ShadowSpec shadow = currentTheme().shadowToast;
        shadow.color = withAlpha(shadow.color, item->presence());
        return std::vector<ShadowSpec>{shadow};
    });
    connect(item, &ToastItem::presenceChanged, this, &ToastHost::relayout);
    connect(item, &ToastItem::finished, this, &ToastHost::remove);
    m_shadows->raise();
    for (ToastItem *existing : m_items) {
        existing->raise();
    }
    item->show();
    relayout();
    item->enter();
}

void ToastHost::remove(ToastItem *item)
{
    m_shadows->untrack(item);
    std::erase(m_items, item);
    item->deleteLater();
    relayout();
}

void ToastHost::relayout()
{
    int y = kEdge;
    for (ToastItem *item : m_items) {
        const int height = item->preferredHeight();
        const qreal slide = (1.0 - item->presence()) * ToastItem::kWidth * 1.2;
        item->setGeometry(m_window->width() - kEdge - ToastItem::kWidth + static_cast<int>(slide), y, ToastItem::kWidth,
                          height);
        y += height + kGap;
        m_shadows->refresh(item);
    }
}

}  // namespace sky
