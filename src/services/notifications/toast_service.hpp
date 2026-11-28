#pragma once

#include <QObject>

#include "model/toast.hpp"

namespace sky {

class ToastService final : public QObject {
    Q_OBJECT

public:
    explicit ToastService(QObject *parent = nullptr);

    void show(const Toast &toast);
    void show(ToastType type, const std::string &message, int durationMs = kDefaultToastMs);
    void showTitled(ToastType type, const std::string &title, const std::string &message,
                    int durationMs = kDefaultToastMs);

signals:
    void toastRequested(const sky::Toast &toast);
};

}  // namespace sky
