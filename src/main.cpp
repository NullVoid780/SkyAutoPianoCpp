#include <QApplication>
#include <QGuiApplication>
#include <QProcess>
#include <QStyleFactory>

#include <thread>

#include "app/application.hpp"
#include "app/paths.hpp"
#include "app/single_instance.hpp"
#include "app/version.hpp"
#include "core/logger.hpp"
#include "crash/crash_handler.hpp"
#include "crash/pending_reports.hpp"
#include "skyinput/panic_release.hpp"
#include "ui/render/font_loader.hpp"

int main(int argc, char *argv[])
{
    sky::CrashHandler::install({sky::kAppVersion, &skyinput::panicReleaseAll});
    sky::Logger::setBreadcrumbSink(&sky::CrashHandler::breadcrumb);
    sky::Logger::init(sky::Paths::logFile());
    sky::Logger::info("app", std::string("starting version ") + sky::kAppVersion);

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Sky Auto Piano"));
    QApplication::setApplicationVersion(QString::fromLatin1(sky::kAppVersion));
    sky::Logger::installQtMessageHandler();

    sky::SingleInstance instance;
    if (!instance.acquire()) {
        sky::SingleInstance::notifyRunningInstance();
        sky::CrashHandler::markCleanExit();
        return 0;
    }
    sky::CrashHandler::startReporter();
    std::thread([] { sky::pruneCrashReports(sky::crashDirectory()); }).detach();
    if (auto ensured = sky::Paths::ensureDirectories(); !ensured) {
        sky::Logger::error("app", ensured.error().details);
    }

    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    sky::FontLoader::registerBundledFonts();
    QApplication::setFont(sky::FontLoader::applicationFont());

    int code = 0;
    bool relaunch = false;
    {
        sky::Application application;
        instance.listen();
        QObject::connect(&instance, &sky::SingleInstance::activationRequested, &application,
                         &sky::Application::activate);
        application.start(QApplication::arguments().contains(QStringLiteral("--after-crash")));
        code = QApplication::exec();
        const bool installerLaunched = application.launchPendingInstaller();
        relaunch = application.relaunchRequested() && !installerLaunched;
    }
    instance.release();
    if (relaunch) {
        QProcess::startDetached(QApplication::applicationFilePath(), {});
    }
    sky::Logger::shutdown();
    sky::CrashHandler::markCleanExit();
    return code;
}
