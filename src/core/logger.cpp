#include "core/logger.hpp"

#include <windows.h>

#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <mutex>
#include <string>

#include "core/string_utils.hpp"
#include "core/time_utils.hpp"

namespace sky {

namespace {

constexpr std::uintmax_t kMaxFileBytes = 1024 * 1024;
constexpr std::array<std::string_view, 7> kBreadcrumbCategories = {"app", "playback", "shortcut", "http",
                                                                    "config", "update", "editor"};

std::atomic<Logger::BreadcrumbSink> &breadcrumbSink()
{
    static std::atomic<Logger::BreadcrumbSink> sink{nullptr};
    return sink;
}

bool isBreadcrumbCategory(std::string_view category)
{
    return std::find(kBreadcrumbCategories.begin(), kBreadcrumbCategories.end(), category) !=
           kBreadcrumbCategories.end();
}
constexpr int kBackupCount = 3;

struct LoggerState {
    std::mutex mutex;
    std::filesystem::path path;
    std::ofstream stream;
    std::uintmax_t written = 0;
};

LoggerState &state()
{
    static LoggerState instance;
    return instance;
}

std::string_view levelName(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "INFO";
}

std::filesystem::path backupPath(const std::filesystem::path &path, int index)
{
    std::filesystem::path backup = path;
    backup += L"." + std::to_wstring(index);
    return backup;
}

void rotateLocked(LoggerState &s)
{
    s.stream.close();
    std::error_code ec;
    std::filesystem::remove(backupPath(s.path, kBackupCount), ec);
    for (int i = kBackupCount - 1; i >= 1; --i) {
        std::filesystem::rename(backupPath(s.path, i), backupPath(s.path, i + 1), ec);
    }
    std::filesystem::rename(s.path, backupPath(s.path, 1), ec);
    s.stream.open(s.path, std::ios::binary | std::ios::trunc);
    s.written = 0;
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    LogLevel level = LogLevel::Info;
    switch (type) {
    case QtDebugMsg:
        level = LogLevel::Debug;
        break;
    case QtInfoMsg:
        level = LogLevel::Info;
        break;
    case QtWarningMsg:
        level = LogLevel::Warning;
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        level = LogLevel::Error;
        break;
    }
    const std::string category = context.category ? context.category : "qt";
    Logger::write(level, category, fromQString(message));
}

}  // namespace

void Logger::init(const std::filesystem::path &file)
{
    LoggerState &s = state();
    std::lock_guard lock(s.mutex);
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    s.path = file;
    s.stream.open(file, std::ios::binary | std::ios::app);
    s.written = std::filesystem::exists(file, ec) ? std::filesystem::file_size(file, ec) : 0;
}

void Logger::installQtMessageHandler()
{
    qInstallMessageHandler(qtMessageHandler);
}

void Logger::shutdown()
{
    LoggerState &s = state();
    std::lock_guard lock(s.mutex);
    s.stream.flush();
    s.stream.close();
}

void Logger::setBreadcrumbSink(BreadcrumbSink sink)
{
    breadcrumbSink().store(sink);
}

void Logger::write(LogLevel level, std::string_view category, std::string_view message)
{
    if (level != LogLevel::Debug && isBreadcrumbCategory(category)) {
        if (BreadcrumbSink sink = breadcrumbSink().load()) {
            const std::string categoryText(category);
            const std::string messageText(message);
            sink(categoryText.c_str(), messageText.c_str());
        }
    }
    std::string line = isoTimestampUtc();
    line += " [";
    line += levelName(level);
    line += "] ";
    line += category;
    line += ": ";
    line += message;
    line += "\n";

    OutputDebugStringW(toWide(line).c_str());

    LoggerState &s = state();
    std::lock_guard lock(s.mutex);
    if (!s.stream.is_open()) {
        return;
    }
    if (s.written + line.size() > kMaxFileBytes) {
        rotateLocked(s);
    }
    s.stream.write(line.data(), static_cast<std::streamsize>(line.size()));
    s.stream.flush();
    s.written += line.size();
}

void Logger::debug(std::string_view category, std::string_view message)
{
    write(LogLevel::Debug, category, message);
}

void Logger::info(std::string_view category, std::string_view message)
{
    write(LogLevel::Info, category, message);
}

void Logger::warning(std::string_view category, std::string_view message)
{
    write(LogLevel::Warning, category, message);
}

void Logger::error(std::string_view category, std::string_view message)
{
    write(LogLevel::Error, category, message);
}

}  // namespace sky
