#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QString>

#include "core/json.hpp"
#include "core/result.hpp"
#include "services/report/report_writer.hpp"

class QTemporaryDir;
class QTimer;
class QWidget;

namespace sky {

class ConfigService;
class LibraryService;
class PlaybackService;
class ThemeController;

class IssueRecorder final : public QObject {
    Q_OBJECT

public:
    struct SaveOptions {
        QString description;
        bool screenshots = true;
        bool library = true;
        bool failedFiles = true;
        bool log = true;
    };

    IssueRecorder(ConfigService &config, ThemeController &themes, LibraryService &library, PlaybackService &playback,
                  QObject *parent = nullptr);
    ~IssueRecorder() override;

    bool isRecording() const { return m_recording; }
    bool hasRecording() const { return m_hasRecording; }
    void start();
    void stop();
    void discard();
    void ignore(QWidget *widget);

    qint64 durationMs() const;
    int eventCount() const { return static_cast<int>(m_events.size()); }
    int frameCount() const { return static_cast<int>(m_frames.size()); }
    int failedImportCount() const { return static_cast<int>(m_failedFiles.size()); }

    Result<void> save(const std::filesystem::path &target, const SaveOptions &options);

    static void addEnvironment(ReportPackage &package, ConfigService &config, ThemeController &themes,
                               bool includeLibrary, const std::set<QWidget *> &ignored = {});
    static void addLog(ReportPackage &package);
    static OrderedJson environmentJson(ConfigService &config, ThemeController &themes,
                                       const std::set<QWidget *> &ignored = {});
    static void setDiagnosticsProvider(std::function<OrderedJson()> provider);

signals:
    void started();
    void stopped();
    void tick(qint64 elapsedMs);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void record(const QString &type, QWidget *window, const QString &target, const QPoint &position,
                const QString &detail);
    void captureFrames();
    void captureSoon();
    void sampleMouse();
    QString windowId(QWidget *window);
    bool isIgnored(const QWidget *widget) const;
    qint64 now() const;
    void connectSources();
    bool isRepeat(ulong stamp, int type);

    ConfigService &m_config;
    ThemeController &m_themes;
    LibraryService &m_library;
    PlaybackService &m_playback;
    bool m_recording = false;
    bool m_hasRecording = false;
    QElapsedTimer m_clock;
    qint64 m_stoppedAt = 0;
    std::string m_startedAt;
    std::string m_startedAtLocal;
    OrderedJson m_events = OrderedJson::array();
    OrderedJson m_frames = OrderedJson::array();
    OrderedJson m_startEnvironment = OrderedJson::object();
    std::unique_ptr<QTemporaryDir> m_folder;
    QTimer *m_frameTimer;
    QTimer *m_soonTimer;
    QTimer *m_tickTimer;
    QTimer *m_mouseTimer;
    struct MouseSample {
        qint64 time = -1;
        int window = -1;
        QPoint position;
        bool down = false;
    };
    MouseSample m_lastMouse;
    OrderedJson m_mouse = OrderedJson::array();
    OrderedJson m_mouseWindows = OrderedJson::array();
    QHash<QString, int> m_mouseWindowIndex;
    QHash<QWidget *, QString> m_windowIds;
    QHash<QString, QImage> m_lastFrame;
    QHash<QString, int> m_patchesSinceKey;
    std::set<QWidget *> m_ignored;
    std::vector<QPointer<QWidget>> m_ignoredGuards;
    std::map<QString, QString> m_importPaths;
    std::map<QString, QString> m_failedFiles;
    QHash<QString, int> m_windowCounters;
    OrderedJson m_windowInfo = OrderedJson::object();
    QHash<QWidget *, qint64> m_lastResize;
    ulong m_lastInputStamp = 0;
    int m_lastInputType = 0;
    qint64 m_lastInputAt = -1000;
    qint64 m_lastWheel = -1000;
    qint64 m_lastSampleAt = -1;
    int m_frameIndex = 0;
};

}  // namespace sky
