#pragma once

#include <filesystem>
#include <string_view>

namespace sky {

enum class LogLevel { Debug, Info, Warning, Error };

class Logger {
public:
    static void init(const std::filesystem::path &file);
    static void installQtMessageHandler();
    static void shutdown();

    using BreadcrumbSink = void (*)(const char *category, const char *message);
    static void setBreadcrumbSink(BreadcrumbSink sink);

    static void write(LogLevel level, std::string_view category, std::string_view message);
    static void debug(std::string_view category, std::string_view message);
    static void info(std::string_view category, std::string_view message);
    static void warning(std::string_view category, std::string_view message);
    static void error(std::string_view category, std::string_view message);
};

}  // namespace sky
