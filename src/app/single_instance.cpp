#include "app/single_instance.hpp"

#include <windows.h>

#include <QLocalServer>
#include <QLocalSocket>

namespace sky {

namespace {

constexpr const wchar_t *kMutexName = L"Global\\SkyAutoPiano.SingleInstance";
constexpr const wchar_t *kFallbackMutexName = L"Local\\SkyAutoPiano.SingleInstance";
constexpr const char *kServerName = "SkyAutoPiano.SingleInstance";
constexpr int kConnectTimeoutMs = 1000;

}  // namespace

SingleInstance::SingleInstance(QObject *parent) : QObject(parent) {}

SingleInstance::~SingleInstance()
{
    release();
}

void SingleInstance::release()
{
    if (m_server) {
        m_server->close();
    }
    if (m_mutex) {
        ReleaseMutex(static_cast<HANDLE>(m_mutex));
        CloseHandle(static_cast<HANDLE>(m_mutex));
        m_mutex = nullptr;
    }
}

bool SingleInstance::acquire()
{
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (!mutex) {
        mutex = CreateMutexW(nullptr, TRUE, kFallbackMutexName);
    }
    if (!mutex) {
        return true;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return false;
    }
    m_mutex = mutex;
    return true;
}

void SingleInstance::listen()
{
    m_server = new QLocalServer(this);
    QLocalServer::removeServer(QString::fromLatin1(kServerName));
    m_server->listen(QString::fromLatin1(kServerName));
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            socket->deleteLater();
            emit activationRequested();
        }
    });
}

void SingleInstance::notifyRunningInstance()
{
    QLocalSocket socket;
    socket.connectToServer(QString::fromLatin1(kServerName));
    if (socket.waitForConnected(kConnectTimeoutMs)) {
        socket.write("focus");
        socket.waitForBytesWritten(kConnectTimeoutMs);
        socket.disconnectFromServer();
    }
}

}  // namespace sky
