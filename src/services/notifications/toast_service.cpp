#include "services/notifications/toast_service.hpp"

#include <QMetaObject>
#include <QThread>

namespace sky {

ToastService::ToastService(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<sky::Toast>();
}

void ToastService::show(const Toast &toast)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, toast] { emit toastRequested(toast); }, Qt::QueuedConnection);
        return;
    }
    emit toastRequested(toast);
}

void ToastService::show(ToastType type, const std::string &message, int durationMs)
{
    show(Toast{type, {}, message, durationMs});
}

void ToastService::showTitled(ToastType type, const std::string &title, const std::string &message, int durationMs)
{
    show(Toast{type, title, message, durationMs});
}

}  // namespace sky
