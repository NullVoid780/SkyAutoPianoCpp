#include "platform/file_dialog.hpp"

#include "core/translator.hpp"

#include <QFileDialog>
#include <QStandardPaths>
#include <QStringList>

namespace sky {

std::vector<std::filesystem::path> chooseSheetFiles(QWidget *parent)
{
    const QStringList chosen = QFileDialog::getOpenFileNames(
        parent, QString(), QString(), tr("Sky sheets (*.json *.txt *.skysheet *.mid *.midi)"));
    std::vector<std::filesystem::path> files;
    files.reserve(static_cast<std::size_t>(chosen.size()));
    for (const QString &file : chosen) {
        files.emplace_back(file.toStdWString());
    }
    return files;
}

std::optional<std::filesystem::path> chooseSettingsExportPath(QWidget *parent, const QString &defaultName)
{
    const QString chosen =
        QFileDialog::getSaveFileName(parent, QString(), defaultName, tr("JSON File (*.json)"));
    if (chosen.isEmpty()) {
        return std::nullopt;
    }
    return std::filesystem::path(chosen.toStdWString());
}

std::optional<std::filesystem::path> chooseSettingsImportFile(QWidget *parent)
{
    const QString chosen =
        QFileDialog::getOpenFileName(parent, QString(), QString(), tr("JSON File (*.json)"));
    if (chosen.isEmpty()) {
        return std::nullopt;
    }
    return std::filesystem::path(chosen.toStdWString());
}
std::optional<std::filesystem::path> chooseThemeExportPath(QWidget *parent, const QString &defaultName)
{
    const QString chosen = QFileDialog::getSaveFileName(parent, tr("Export Theme"), defaultName,
                                                        tr("Sky Auto Piano Theme (*.json)"));
    if (chosen.isEmpty()) {
        return std::nullopt;
    }
    return std::filesystem::path(chosen.toStdWString());
}

std::optional<std::filesystem::path> chooseReportSavePath(QWidget *parent, const QString &defaultName, bool crashReport)
{
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    const QString filter = crashReport ? tr("Sky Auto Piano Crash Report (*.sapcr)")
                                       : tr("Sky Auto Piano Report (*.sapr)");
    const QString chosen = QFileDialog::getSaveFileName(
        parent, crashReport ? tr("Save crash report") : tr("Save report"),
        folder + QLatin1Char('/') + defaultName, filter);
    if (chosen.isEmpty()) {
        return std::nullopt;
    }
    return std::filesystem::path(chosen.toStdWString());
}

std::optional<std::filesystem::path> chooseThemeImportFile(QWidget *parent)
{
    const QString chosen = QFileDialog::getOpenFileName(parent, tr("Import Theme"), QString(),
                                                        tr("Sky Auto Piano Theme (*.json)"));
    if (chosen.isEmpty()) {
        return std::nullopt;
    }
    return std::filesystem::path(chosen.toStdWString());
}

std::optional<std::filesystem::path> chooseSheetExportPath(QWidget *parent, const QString &defaultName)
{
    const QString chosen = QFileDialog::getSaveFileName(
        parent, QString(), defaultName,
        tr("Text File (*.txt);;JSON File (*.json);;SkySheet File (*.skysheet)"));
    if (chosen.isEmpty()) {
        return std::nullopt;
    }
    return std::filesystem::path(chosen.toStdWString());
}

std::optional<std::filesystem::path> chooseFolder(QWidget *parent, const QString &title)
{
    const QString chosen = QFileDialog::getExistingDirectory(parent, title);
    if (chosen.isEmpty()) {
        return std::nullopt;
    }
    return std::filesystem::path(chosen.toStdWString());
}

}  // namespace sky