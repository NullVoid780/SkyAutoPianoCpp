#include "app/issue_recorder.hpp"

#include <windows.h>
#include <psapi.h>

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QCursor>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPixmap>
#include <QScreen>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>
#include <QWindow>

#include "core/paths.hpp"
#include "ui/controllers/theme_controller.hpp"
#include "core/version.hpp"
#include "core/string_utils.hpp"
#include "skyinput/skyinput.hpp"
#include "services/library/library_service.hpp"
#include "services/playback/playback_service.hpp"
#include "services/settings/config_service.hpp"
#include "ui/core/overlay.hpp"
#include "ui/render/frame_diff.hpp"
#include "ui/settings_window/settings_window.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/toast_host.hpp"

namespace sky {

namespace {

constexpr int kBusyFrameMs = 100;
constexpr int kIdleFrameMs = 300;
constexpr int kSoonDelayMs = 120;
constexpr int kMouseIntervalMs = 20;
constexpr qint64 kMaxDurationMs = 10 * 60 * 1000;
constexpr std::size_t kMaxFrames = 6000;
constexpr std::size_t kMaxMouseSamples = 40000;
constexpr std::size_t kMaxEvents = 30000;
constexpr int kMaxImageWidth = 1920;
constexpr int kKeyFrameEvery = 30;
constexpr qint64 kWheelGapMs = 400;
constexpr qint64 kResizeGapMs = 250;
constexpr std::size_t kLogTailBytes = 1024 * 1024;
constexpr qint64 kFreezeMs = 400;

std::function<OrderedJson()> &diagnosticsProvider()
{
    static std::function<OrderedJson()> provider;
    return provider;
}

std::string wide(const std::wstring &text)
{
    return fromWide(text);
}

std::string registryText(HKEY root, const wchar_t *key, const wchar_t *name)
{
    wchar_t buffer[256] = {};
    DWORD size = sizeof(buffer);
    if (RegGetValueW(root, key, name, RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS) {
        return {};
    }
    std::wstring text(buffer);
    while (!text.empty() && text.back() == L' ') {
        text.pop_back();
    }
    return wide(text);
}

OrderedJson hardwareJson()
{
    OrderedJson hardware = OrderedJson::object();
    hardware["cpu"] = registryText(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                                   L"ProcessorNameString");
    SYSTEM_INFO info{};
    GetNativeSystemInfo(&info);
    hardware["threads"] = static_cast<int>(info.dwNumberOfProcessors);
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        hardware["ramMb"] = memory.ullTotalPhys / (1024 * 1024);
        hardware["ramFreeMb"] = memory.ullAvailPhys / (1024 * 1024);
        hardware["ramUsedPercent"] = static_cast<int>(memory.dwMemoryLoad);
    }
    OrderedJson gpus = OrderedJson::array();
    std::set<std::wstring> seen;
    DISPLAY_DEVICEW device{};
    device.cb = sizeof(device);
    for (DWORD index = 0; EnumDisplayDevicesW(nullptr, index, &device, 0); ++index) {
        const std::wstring name(device.DeviceString);
        if (!name.empty() && seen.insert(name).second) {
            gpus.push_back(wide(name));
        }
        device = DISPLAY_DEVICEW{};
        device.cb = sizeof(device);
    }
    hardware["gpus"] = std::move(gpus);
    return hardware;
}

OrderedJson processJson()
{
    OrderedJson process = OrderedJson::object();
    PROCESS_MEMORY_COUNTERS_EX counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),
                             sizeof(counters))) {
        process["memoryMb"] = counters.WorkingSetSize / (1024 * 1024);
        process["peakMemoryMb"] = counters.PeakWorkingSetSize / (1024 * 1024);
        process["privateMb"] = counters.PrivateUsage / (1024 * 1024);
    }
    DWORD handles = 0;
    if (GetProcessHandleCount(GetCurrentProcess(), &handles)) {
        process["handles"] = handles;
    }
    FILETIME created{}, exited{}, kernel{}, user{};
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
        FILETIME now{};
        GetSystemTimeAsFileTime(&now);
        const auto toInt = [](FILETIME time) {
            return (static_cast<unsigned long long>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
        };
        process["uptimeMs"] = (toInt(now) - toInt(created)) / 10000;
        process["cpuTimeMs"] = (toInt(kernel) + toInt(user)) / 10000;
    }
    process["guiObjects"] = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    process["userObjects"] = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    return process;
}

OrderedJson gameJson()
{
    OrderedJson game = OrderedJson::object();
    const skyinput::WindowHandle handle = skyinput::findWindow(L"Sky");
    game["found"] = handle != nullptr;
    if (handle) {
        RECT area{};
        GetClientRect(reinterpret_cast<HWND>(handle), &area);
        game["width"] = area.right - area.left;
        game["height"] = area.bottom - area.top;
        game["minimized"] = IsIconic(reinterpret_cast<HWND>(handle)) != 0;
        game["foreground"] = GetForegroundWindow() == reinterpret_cast<HWND>(handle);
    }
    return game;
}

OrderedJson inputJson()
{
    OrderedJson input = OrderedJson::object();
    wchar_t layout[KL_NAMELENGTH] = {};
    if (GetKeyboardLayoutNameW(layout)) {
        input["keyboardLayout"] = wide(layout);
    }
    wchar_t language[LOCALE_NAME_MAX_LENGTH] = {};
    const LANGID id = LOWORD(reinterpret_cast<UINT_PTR>(GetKeyboardLayout(0)));
    if (LCIDToLocaleName(MAKELCID(id, SORT_DEFAULT), language, LOCALE_NAME_MAX_LENGTH, 0) > 0) {
        input["inputLanguage"] = wide(language);
    }
    return input;
}

std::string utf8(const QString &text)
{
    return fromQString(text);
}

QString className(const QObject *object)
{
    QString name = QString::fromLatin1(object->metaObject()->className());
    if (name.startsWith(QStringLiteral("sky::"))) {
        name.remove(0, 5);
    }
    return name;
}

QString visibleText(const QWidget *widget)
{
    if (const auto *button = qobject_cast<const QAbstractButton *>(widget); button && !button->text().isEmpty()) {
        return button->text();
    }
    if (const auto *label = qobject_cast<const QLabel *>(widget); label && !label->text().isEmpty()) {
        return label->text();
    }
    if (!widget->accessibleName().isEmpty()) {
        return widget->accessibleName();
    }
    return widget->toolTip();
}

QString describe(const QWidget *widget)
{
    QStringList parts;
    int depth = 0;
    for (const QWidget *current = widget; current && depth < 5; current = current->parentWidget(), ++depth) {
        QString part = className(current);
        if (!current->objectName().isEmpty() && !current->objectName().startsWith(QStringLiteral("qt_"))) {
            part += QLatin1Char('#') + current->objectName();
        }
        const QString text = visibleText(current).simplified();
        if (!text.isEmpty()) {
            part += QStringLiteral(" \"%1\"").arg(text.left(60));
        }
        parts.prepend(part);
        if (current->isWindow()) {
            break;
        }
    }
    return parts.join(QStringLiteral(" > "));
}

QString itemDetail(const QWidget *widget, const QPoint &position)
{
    const auto *view = qobject_cast<const QAbstractItemView *>(widget ? widget->parentWidget() : nullptr);
    if (!view || view->viewport() != widget) {
        return {};
    }
    const QModelIndex index = view->indexAt(position);
    if (!index.isValid()) {
        return QStringLiteral("empty space");
    }
    QString text = index.data(Qt::DisplayRole).toString();
    if (text.isEmpty()) {
        text = index.data(Qt::AccessibleTextRole).toString();
    }
    return text.isEmpty() ? QStringLiteral("row %1").arg(index.row())
                          : QStringLiteral("row %1 \"%2\"").arg(index.row()).arg(text.left(60));
}

QString toastTypeName(ToastType type)
{
    switch (type) {
    case ToastType::Success:
        return QStringLiteral("success");
    case ToastType::Warning:
        return QStringLiteral("warning");
    case ToastType::Error:
        return QStringLiteral("error");
    case ToastType::Info:
        return QStringLiteral("info");
    case ToastType::Neutral:
        break;
    }
    return QStringLiteral("neutral");
}

QString windowStateText(const QWidget *window)
{
    if (window->isMinimized()) {
        return QStringLiteral("minimized");
    }
    if (window->isMaximized()) {
        return QStringLiteral("maximized");
    }
    if (window->isFullScreen()) {
        return QStringLiteral("full screen");
    }
    return QStringLiteral("normal");
}

bool isReportableWindow(const QWidget *window)
{
    const Qt::WindowType type = window->windowType();
    return type != Qt::ToolTip && type != Qt::SplashScreen;
}

}  // namespace

IssueRecorder::IssueRecorder(ConfigService &config, ThemeController &themes, LibraryService &library,
                             PlaybackService &playback, QObject *parent)
    : QObject(parent),
      m_config(config),
      m_themes(themes),
      m_library(library),
      m_playback(playback),
      m_frameTimer(new QTimer(this)),
      m_soonTimer(new QTimer(this)),
      m_tickTimer(new QTimer(this)),
      m_mouseTimer(new QTimer(this))
{
    m_frameTimer->setSingleShot(true);
    m_frameTimer->setInterval(kIdleFrameMs);
    m_mouseTimer->setTimerType(Qt::PreciseTimer);
    m_mouseTimer->setInterval(kMouseIntervalMs);
    m_soonTimer->setSingleShot(true);
    m_soonTimer->setInterval(kSoonDelayMs);
    m_tickTimer->setInterval(1000);
    connect(m_frameTimer, &QTimer::timeout, this, &IssueRecorder::captureFrames);
    connect(m_soonTimer, &QTimer::timeout, this, &IssueRecorder::captureFrames);
    connect(m_mouseTimer, &QTimer::timeout, this, &IssueRecorder::sampleMouse);
    connect(m_tickTimer, &QTimer::timeout, this, [this] {
        emit tick(durationMs());
        if (durationMs() >= kMaxDurationMs) {
            stop();
        }
    });
    connectSources();
}

IssueRecorder::~IssueRecorder()
{
    if (m_recording) {
        qApp->removeEventFilter(this);
        ToastHost::setObserver(nullptr);
    }
}

void IssueRecorder::connectSources()
{
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        if (m_recording) {
            record(QStringLiteral("theme"), nullptr, m_themes.activeId(), {},
                   QStringLiteral("%1, follow Windows %2")
                       .arg(m_themes.kind() == AppTheme::Dark ? QStringLiteral("dark") : QStringLiteral("light"))
                       .arg(m_themes.followsSystem() ? QStringLiteral("on") : QStringLiteral("off")));
            captureSoon();
        }
    });
    connect(&m_library, &LibraryService::importStarted, this, [this](const QStringList &files) {
        if (!m_recording) {
            return;
        }
        QStringList names;
        for (const QString &file : files) {
            const QString name = QFileInfo(file).fileName();
            m_importPaths[name] = file;
            names << name;
        }
        record(QStringLiteral("import"), nullptr, QStringLiteral("%1 file(s)").arg(files.size()), {},
               names.join(QStringLiteral(", ")));
    });
    connect(&m_library, &LibraryService::importFinished, this, [this](const ImportSummary &summary) {
        if (!m_recording) {
            return;
        }
        QStringList failures;
        for (const ImportFailure &failure : summary.failures) {
            const QString name = toQString(failure.fileName);
            failures << QStringLiteral("%1: %2").arg(name, toQString(failure.message));
            if (const auto found = m_importPaths.find(name); found != m_importPaths.end()) {
                m_failedFiles[name] = found->second;
            }
        }
        record(QStringLiteral("import-result"), nullptr,
               QStringLiteral("%1 of %2 imported").arg(summary.succeeded).arg(summary.total), {},
               failures.join(QStringLiteral("\n")));
        captureSoon();
    });
    connect(&m_playback, &PlaybackService::started, this, [this] {
        if (m_recording) {
            record(QStringLiteral("playback"), nullptr, QStringLiteral("started"), {}, {});
        }
    });
    connect(&m_playback, &PlaybackService::stopped, this, [this](bool manual) {
        if (m_recording) {
            record(QStringLiteral("playback"), nullptr, QStringLiteral("stopped"), {},
                   manual ? QStringLiteral("by user") : QStringLiteral("song ended"));
        }
    });
    connect(&m_config, &ConfigService::changed, this, [this] {
        if (m_recording) {
            record(QStringLiteral("settings"), nullptr, QStringLiteral("settings changed"), {}, {});
        }
    });
}

void IssueRecorder::ignore(QWidget *widget)
{
    if (widget) {
        m_ignored.insert(widget);
        m_ignoredGuards.emplace_back(widget);
        connect(widget, &QObject::destroyed, this, [this, widget] { m_ignored.erase(widget); });
    }
}

bool IssueRecorder::isIgnored(const QWidget *widget) const
{
    for (const QWidget *current = widget; current; current = current->parentWidget()) {
        if (m_ignored.count(const_cast<QWidget *>(current)) > 0) {
            return true;
        }
    }
    return false;
}

qint64 IssueRecorder::now() const
{
    return m_clock.isValid() ? m_clock.elapsed() : 0;
}

qint64 IssueRecorder::durationMs() const
{
    return m_recording ? now() : m_stoppedAt;
}

void IssueRecorder::start()
{
    if (m_recording) {
        return;
    }
    discard();
    m_folder = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/SkyAutoPiano-recording-XXXXXX"));
    if (!m_folder->isValid()) {
        m_folder.reset();
        return;
    }
    QDir(m_folder->path()).mkpath(QStringLiteral("frames"));
    m_recording = true;
    m_hasRecording = true;
    m_clock.start();
    m_startedAt = utf8(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    m_startedAtLocal = utf8(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")));
    m_startEnvironment = environmentJson(m_config, m_themes, m_ignored);
    qApp->installEventFilter(this);
    ToastHost::setObserver([this](const Toast &toast) {
        if (!m_recording) {
            return;
        }
        QString text = toQString(toast.message);
        if (!toast.title.empty()) {
            text = toQString(toast.title) + QStringLiteral(": ") + text;
        }
        record(QStringLiteral("toast"), QApplication::activeWindow(), toastTypeName(toast.type), {}, text);
        captureSoon();
    });
    record(QStringLiteral("recording"), nullptr, QStringLiteral("started"), {}, {});
    m_tickTimer->start();
    m_mouseTimer->start();
    sampleMouse();
    captureFrames();
    emit started();
}

void IssueRecorder::stop()
{
    if (!m_recording) {
        return;
    }
    captureFrames();
    record(QStringLiteral("recording"), nullptr, QStringLiteral("stopped"), {}, {});
    m_stoppedAt = now();
    m_recording = false;
    m_frameTimer->stop();
    m_soonTimer->stop();
    m_tickTimer->stop();
    m_mouseTimer->stop();
    m_lastFrame.clear();
    qApp->removeEventFilter(this);
    ToastHost::setObserver(nullptr);
    emit stopped();
}

void IssueRecorder::discard()
{
    if (m_recording) {
        m_recording = false;
        m_frameTimer->stop();
        m_soonTimer->stop();
        m_tickTimer->stop();
        m_mouseTimer->stop();
        qApp->removeEventFilter(this);
        ToastHost::setObserver(nullptr);
    }
    m_hasRecording = false;
    m_mouse = OrderedJson::array();
    m_mouseWindows = OrderedJson::array();
    m_mouseWindowIndex.clear();
    m_lastMouse = {};
    m_lastSampleAt = -1;
    m_events = OrderedJson::array();
    m_frames = OrderedJson::array();
    m_windowInfo = OrderedJson::object();
    m_windowIds.clear();
    m_windowCounters.clear();
    m_lastFrame.clear();
    m_patchesSinceKey.clear();
    m_lastResize.clear();
    m_importPaths.clear();
    m_failedFiles.clear();
    m_frameIndex = 0;
    m_stoppedAt = 0;
    m_folder.reset();
}

QString IssueRecorder::windowId(QWidget *window)
{
    if (!window) {
        return {};
    }
    if (const auto found = m_windowIds.constFind(window); found != m_windowIds.constEnd()) {
        return found.value();
    }
    const QString base = className(window);
    const int count = m_windowCounters.value(base, 0);
    m_windowCounters[base] = count + 1;
    const QString id = count == 0 ? base : QStringLiteral("%1-%2").arg(base).arg(count + 1);
    m_windowIds.insert(window, id);
    m_windowInfo[utf8(id)] = OrderedJson{{"class", utf8(base)},
                                         {"objectName", utf8(window->objectName())},
                                         {"title", utf8(window->windowTitle())},
                                         {"firstSeen", now()}};
    return id;
}

void IssueRecorder::record(const QString &type, QWidget *window, const QString &target, const QPoint &position,
                           const QString &detail)
{
    if (m_events.size() >= kMaxEvents) {
        return;
    }
    OrderedJson event = OrderedJson::object();
    event["t"] = now();
    event["type"] = utf8(type);
    if (window) {
        event["window"] = utf8(windowId(window));
    }
    if (!target.isEmpty()) {
        event["target"] = utf8(target);
    }
    if (!position.isNull() || type == QStringLiteral("click")) {
        event["x"] = position.x();
        event["y"] = position.y();
    }
    if (!detail.isEmpty()) {
        event["detail"] = utf8(detail);
    }
    m_events.push_back(std::move(event));
}

void IssueRecorder::captureSoon()
{
    if (m_recording) {
        m_soonTimer->start();
    }
}

void IssueRecorder::captureFrames()
{
    if (!m_recording || !m_folder) {
        return;
    }
    m_soonTimer->stop();
    bool changed = false;
    for (QWidget *window : QApplication::topLevelWidgets()) {
        if (!window->isVisible() || window->isMinimized() || !isReportableWindow(window) || isIgnored(window) ||
            window->width() < 8 || window->height() < 8) {
            continue;
        }
        const QPixmap shot = window->grab();
        QImage image = shot.toImage();
        if (image.isNull()) {
            continue;
        }
        if (image.width() > kMaxImageWidth) {
            image = image.scaledToWidth(kMaxImageWidth, Qt::SmoothTransformation);
        }
        image = image.convertToFormat(QImage::Format_RGB32);
        const QString id = windowId(window);
        const QImage last = m_lastFrame.value(id);
        QRect area = image.rect();
        bool patch = false;
        if (!last.isNull()) {
            area = changedArea(last, image);
            if (area.isEmpty()) {
                continue;
            }
            const qint64 areaPixels = static_cast<qint64>(area.width()) * area.height();
            const qint64 framePixels = static_cast<qint64>(image.width()) * image.height();
            patch = area != image.rect() && m_patchesSinceKey.value(id) < kKeyFrameEvery && areaPixels * 2 < framePixels;
        }
        changed = true;
        const QString name = QStringLiteral("frames/%1.jpg").arg(++m_frameIndex, 6, 10, QLatin1Char('0'));
        if (!writeFrameJpeg(patch ? image.copy(area) : image, m_folder->filePath(name))) {
            continue;
        }
        m_lastFrame[id] = image;
        m_patchesSinceKey[id] = patch ? m_patchesSinceKey.value(id) + 1 : 0;
        OrderedJson frame{{"t", now()},
                          {"window", utf8(id)},
                          {"file", utf8(name)},
                          {"width", window->width()},
                          {"height", window->height()},
                          {"scale", shot.devicePixelRatio()}};
        if (patch) {
            frame["patch"] = OrderedJson{{"x", area.x()}, {"y", area.y()}};
        }
        m_frames.push_back(std::move(frame));
        if (m_frames.size() >= kMaxFrames) {
            QTimer::singleShot(0, this, &IssueRecorder::stop);
            return;
        }
    }
    m_frameTimer->start(changed ? kBusyFrameMs : kIdleFrameMs);
}

void IssueRecorder::sampleMouse()
{
    if (!m_recording) {
        return;
    }
    const qint64 tickAt = now();
    if (m_lastSampleAt >= 0 && tickAt - m_lastSampleAt >= kFreezeMs) {
        record(QStringLiteral("freeze"), QApplication::activeWindow(), QStringLiteral("app stopped responding"), {},
               QStringLiteral("The app did not respond for %1 ms").arg(tickAt - m_lastSampleAt));
        captureSoon();
    }
    m_lastSampleAt = tickAt;
    if (m_mouse.size() >= kMaxMouseSamples) {
        return;
    }
    const QPoint global = QCursor::pos();
    QWidget *window = QApplication::topLevelAt(global);
    while (window && (!isReportableWindow(window) || isIgnored(window)) && window->parentWidget()) {
        window = window->parentWidget()->window();
    }
    if (window && (!isReportableWindow(window) || isIgnored(window))) {
        window = nullptr;
    }
    MouseSample sample;
    sample.time = now();
    sample.down = QApplication::mouseButtons() != Qt::NoButton;
    if (window) {
        const QString id = windowId(window);
        auto found = m_mouseWindowIndex.constFind(id);
        if (found == m_mouseWindowIndex.constEnd()) {
            found = m_mouseWindowIndex.insert(id, static_cast<int>(m_mouseWindows.size()));
            m_mouseWindows.push_back(utf8(id));
        }
        sample.window = found.value();
        sample.position = window->mapFromGlobal(global);
    }
    if (m_lastMouse.time >= 0 && sample.window == m_lastMouse.window && sample.position == m_lastMouse.position &&
        sample.down == m_lastMouse.down) {
        return;
    }
    if (m_lastMouse.time >= 0 && sample.time - m_lastMouse.time > kMouseIntervalMs * 2 &&
        sample.window == m_lastMouse.window) {
        m_mouse.push_back(OrderedJson::array({sample.time - kMouseIntervalMs, m_lastMouse.window,
                                              m_lastMouse.position.x(), m_lastMouse.position.y(),
                                              m_lastMouse.down ? 1 : 0}));
    }
    m_mouse.push_back(OrderedJson::array(
        {sample.time, sample.window, sample.position.x(), sample.position.y(), sample.down ? 1 : 0}));
    m_lastMouse = sample;
}

bool IssueRecorder::isRepeat(ulong stamp, int type)
{
    const bool repeat = stamp == m_lastInputStamp && type == m_lastInputType && now() - m_lastInputAt < 50;
    m_lastInputStamp = stamp;
    m_lastInputType = type;
    m_lastInputAt = now();
    return repeat;
}

bool IssueRecorder::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_recording || !watched->isWidgetType()) {
        return false;
    }
    auto *widget = static_cast<QWidget *>(watched);
    if (isIgnored(widget)) {
        return false;
    }
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (isRepeat(mouse->timestamp(), static_cast<int>(event->type()))) {
            break;
        }
        QWidget *window = widget->window();
        const QPoint local = mouse->position().toPoint();
        const QString button = mouse->button() == Qt::RightButton    ? QStringLiteral("right")
                               : mouse->button() == Qt::MiddleButton ? QStringLiteral("middle")
                                                                     : QStringLiteral("left");
        QString detail = button;
        if (const QString item = itemDetail(widget, local); !item.isEmpty()) {
            detail += QStringLiteral(", ") + item;
        }
        record(event->type() == QEvent::MouseButtonDblClick ? QStringLiteral("double-click") : QStringLiteral("click"),
               window, describe(widget), widget->mapTo(window, local), detail);
        captureSoon();
        break;
    }
    case QEvent::Wheel: {
        if (now() - m_lastWheel < kWheelGapMs) {
            break;
        }
        m_lastWheel = now();
        auto *wheel = static_cast<QWheelEvent *>(event);
        QWidget *window = widget->window();
        record(QStringLiteral("scroll"), window, describe(widget), widget->mapTo(window, wheel->position().toPoint()),
               wheel->angleDelta().y() > 0 ? QStringLiteral("up") : QStringLiteral("down"));
        captureSoon();
        break;
    }
    case QEvent::KeyPress: {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->isAutoRepeat() || isRepeat(key->timestamp(), static_cast<int>(event->type()))) {
            break;
        }
        QString name = QKeySequence(key->keyCombination()).toString(QKeySequence::NativeText);
        if (name.isEmpty()) {
            name = key->text();
        }
        record(QStringLiteral("key"), widget->window(), describe(widget), {}, name);
        captureSoon();
        break;
    }
    case QEvent::Show:
    case QEvent::Hide: {
        const bool popup = qobject_cast<Overlay *>(widget) != nullptr;
        if ((!widget->isWindow() || !isReportableWindow(widget)) && !popup) {
            break;
        }
        const bool shown = event->type() == QEvent::Show;
        record(popup ? (shown ? QStringLiteral("popup-open") : QStringLiteral("popup-close"))
                     : (shown ? QStringLiteral("window-open") : QStringLiteral("window-close")),
               widget->window(), describe(widget), {}, {});
        captureSoon();
        break;
    }
    case QEvent::WindowStateChange:
        if (widget->isWindow() && isReportableWindow(widget)) {
            record(QStringLiteral("window-state"), widget, describe(widget), {}, windowStateText(widget));
            captureSoon();
        }
        break;
    case QEvent::Resize:
        if (widget->isWindow() && isReportableWindow(widget) && widget->isVisible()) {
            const qint64 last = m_lastResize.value(widget, -kResizeGapMs);
            if (now() - last >= kResizeGapMs) {
                m_lastResize[widget] = now();
                record(QStringLiteral("window-resize"), widget, describe(widget), {},
                       QStringLiteral("%1 x %2").arg(widget->width()).arg(widget->height()));
                captureSoon();
            }
        }
        break;
    default:
        break;
    }
    return false;
}

OrderedJson IssueRecorder::environmentJson(ConfigService &config, ThemeController &themes,
                                           const std::set<QWidget *> &ignored)
{
    OrderedJson environment = OrderedJson::object();
    std::error_code installError;
    const std::filesystem::path exeFolder(QCoreApplication::applicationDirPath().toStdWString());
    environment["app"] = OrderedJson{
        {"name", "SkyAutoPiano"},
        {"version", kAppVersion},
        {"install", std::filesystem::exists(exeFolder / "unins000.exe", installError) ? "installed" : "portable"},
        {"testCopy", QCoreApplication::arguments().contains(QStringLiteral("--sandbox"))}};
    environment["system"] = OrderedJson{{"os", utf8(QSysInfo::prettyProductName())},
                                        {"kernel", utf8(QSysInfo::kernelVersion())},
                                        {"arch", utf8(QSysInfo::currentCpuArchitecture())},
                                        {"qt", qVersion()},
                                        {"locale", utf8(QLocale::system().name())}};
    const auto system = ThemeController::systemKind();
    environment["theme"] =
        OrderedJson{{"activeId", utf8(themes.activeId())},
                    {"kind", themes.kind() == AppTheme::Dark ? "dark" : "light"},
                    {"followWindows", themes.followsSystem()},
                    {"windowsMode", system ? (*system == AppTheme::Dark ? "dark" : "light") : "unknown"}};
    const PanelConfig panel = config.panel();
    environment["playback"] = OrderedJson{{"mode", panel.playbackMode == PlaybackMode::Strict ? "strict" : "lite"},
                                          {"speed", config.speed()},
                                          {"longPress", panel.longPressMode},
                                          {"minimizeOnPlay", panel.minimizeOnPlay}};
    OrderedJson screens = OrderedJson::array();
    for (QScreen *screen : QGuiApplication::screens()) {
        const QRect area = screen->geometry();
        screens.push_back(OrderedJson{{"name", utf8(screen->name())},
                                      {"x", area.x()},
                                      {"y", area.y()},
                                      {"width", area.width()},
                                      {"height", area.height()},
                                      {"scale", screen->devicePixelRatio()},
                                      {"refreshRate", qRound(screen->refreshRate())},
                                      {"model", utf8(screen->model())},
                                      {"primary", screen == QGuiApplication::primaryScreen()}});
    }
    environment["screens"] = std::move(screens);
    environment["hardware"] = hardwareJson();
    environment["process"] = processJson();
    environment["input"] = inputJson();
    environment["game"] = gameJson();
    if (diagnosticsProvider()) {
        environment["diagnostics"] = diagnosticsProvider()();
    }
    OrderedJson windows = OrderedJson::array();
    for (QWidget *window : QApplication::topLevelWidgets()) {
        if (!window->isVisible() || !isReportableWindow(window) || ignored.count(window) > 0) {
            continue;
        }
        const QRect area = window->geometry();
        OrderedJson entry{{"class", utf8(className(window))},
                          {"objectName", utf8(window->objectName())},
                          {"title", utf8(window->windowTitle())},
                          {"x", area.x()},
                          {"y", area.y()},
                          {"width", area.width()},
                          {"height", area.height()},
                          {"state", utf8(windowStateText(window))},
                          {"screen", window->screen() ? utf8(window->screen()->name()) : std::string()},
                          {"scale", window->devicePixelRatioF()}};
        if (const auto *settings = qobject_cast<SettingsWindow *>(window)) {
            entry["page"] = settings->currentPage();
        }
        windows.push_back(std::move(entry));
    }
    environment["windows"] = std::move(windows);
    return environment;
}

void IssueRecorder::setDiagnosticsProvider(std::function<OrderedJson()> provider)
{
    diagnosticsProvider() = std::move(provider);
}

void IssueRecorder::addEnvironment(ReportPackage &package, ConfigService &config, ThemeController &themes,
                                   bool includeLibrary, const std::set<QWidget *> &ignored)
{
    package.manifest["environment"] = environmentJson(config, themes, ignored);
    package.files.emplace_back("env/config.json", Paths::configDirectory() / "config.json");
    package.files.emplace_back("env/ui-state.json", Paths::dataDirectory() / "ui-state.json");
    package.files.emplace_back("env/themes.json", Paths::dataDirectory() / "themes.json");
    package.files.emplace_back("env/playlists.json", Paths::dataDirectory() / "playlists.json");
    if (includeLibrary) {
        package.files.emplace_back("env/listSheet.json", Paths::dataDirectory() / "listSheet.json");
    }
}

void IssueRecorder::addLog(ReportPackage &package)
{
    package.blobs.emplace_back("log.txt", readFileTail(Paths::logFile(), kLogTailBytes));
}

Result<void> IssueRecorder::save(const std::filesystem::path &target, const SaveOptions &options)
{
    ReportPackage package;
    OrderedJson &manifest = package.manifest;
    manifest["schema"] = kReportSchema;
    manifest["kind"] = "issue";
    manifest["createdAt"] = m_startedAt;
    manifest["createdAtLocal"] = m_startedAtLocal;
    manifest["durationMs"] = durationMs();
    manifest["description"] = utf8(options.description.trimmed());
    addEnvironment(package, m_config, m_themes, options.library, m_ignored);
    manifest["environmentAtStart"] = m_startEnvironment;
    manifest["windows"] = m_windowInfo;
    manifest["events"] = m_events;
    manifest["frames"] = options.screenshots ? m_frames : OrderedJson::array();
    manifest["mouseWindows"] = m_mouseWindows;
    manifest["mouse"] = m_mouse;
    if (options.screenshots && m_folder) {
        for (const OrderedJson &frame : m_frames) {
            const std::string name = frame.value("file", std::string());
            package.files.emplace_back(name, std::filesystem::path(m_folder->filePath(toQString(name)).toStdWString()));
        }
    }
    OrderedJson attachments = OrderedJson::array();
    if (options.failedFiles) {
        for (const auto &[name, path] : m_failedFiles) {
            const std::string archived = "attachments/" + safeArchiveName(utf8(name));
            package.files.emplace_back(archived, std::filesystem::path(path.toStdWString()));
            attachments.push_back(OrderedJson{{"name", utf8(name)}, {"file", archived}, {"reason", "import failed"}});
        }
    }
    manifest["attachments"] = std::move(attachments);
    if (options.log) {
        addLog(package);
    }
    return writeReportPackage(target, package);
}

}  // namespace sky
