#include "platform/file_dialog.hpp"

#include <QFileDialog>
#include <QStringList>

namespace sky {

std::vector<std::filesystem::path> chooseSheetFiles(QWidget *parent)
{
    const QStringList chosen = QFileDialog::getOpenFileNames(
        parent, QString(), QString(), QStringLiteral("Sky sheets (*.json *.txt *.skysheet *.mid *.midi)"));
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
        QFileDialog::getSaveFileName(parent, QString(), defaultName, QStringLiteral("JSON File (*.json)"));
    if (chosen.isEmpty()) {
        return std::nullopt;
    }
    return std::filesystem::path(chosen.toStdWString());
}

std::optional<std::filesystem::path> chooseSettingsImportFile(QWidget *parent)
{
    const QString chosen =
        QFileDialog::getOpenFileName(parent, QString(), QString(), QStringLiteral("JSON File (*.json)"));
    if (chosen.isEmpty()) {
        return std::nullopt;
    }
    return std::filesystem::path(chosen.toStdWString());
}
std::optional<std::filesystem::path> chooseThemeExportPath(QWidget *parent, const QString &defaultName)
{
    const QString chosen = QFileDialog::getSaveFileName(parent, QStringLiteral("Export Theme"), defaultName,
                                                        QStringLiteral("Sky Auto Piano Theme (*.json)"));
    if (chosen.isEmpty()) {
        return std::nullopt;
    }
    return std::filesystem::path(chosen.toStdWString());
}

std::optional<std::filesystem::path> chooseThemeImportFile(QWidget *parent)
{
    const QString chosen = QFileDialog::getOpenFileName(parent, QStringLiteral("Import Theme"), QString(),
                                                        QStringLiteral("Sky Auto Piano Theme (*.json)"));
    if (chosen.isEmpty()) {
        return std::nullopt;
    }
    return std::filesystem::path(chosen.toStdWString());
}

std::optional<std::filesystem::path> chooseSheetExportPath(QWidget *parent, const QString &defaultName)
{
    const QString chosen = QFileDialog::getSaveFileName(
        parent, QString(), defaultName,
        QStringLiteral("Text File (*.txt);;JSON File (*.json);;SkySheet File (*.skysheet)"));
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