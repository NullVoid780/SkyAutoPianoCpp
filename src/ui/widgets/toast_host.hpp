#pragma once

#include <functional>

#include <vector>

#include <QObject>

#include "model/toast.hpp"

class QWidget;

namespace sky {

class ElevationLayer;
class ToastItem;

class ToastHost final : public QObject {
    Q_OBJECT

public:
    explicit ToastHost(QWidget *window);

    void show(const Toast &toast);
    void relayout();

    static void setObserver(std::function<void(const Toast &)> observer);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void remove(ToastItem *item);

    QWidget *m_window;
    ElevationLayer *m_shadows;
    std::vector<ToastItem *> m_items;
};

}  // namespace sky
