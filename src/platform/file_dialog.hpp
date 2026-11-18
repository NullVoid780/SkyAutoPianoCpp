#pragma once

#include <filesystem>
#include <optional>
#include <vector>

class QString;
class QWidget;

namespace sky {

std::vector<std::filesystem::path> chooseSheetFiles(QWidget *parent);
std::optional<std::filesystem::path> chooseSettingsExportPath(QWidget *parent, const QString &defaultName);
std::optional<std::filesystem::path> chooseSettingsImportFile(QWidget *parent);
std::optional<std::filesystem::path> chooseThemeExportPath(QWidget *parent, const QString &defaultName);
std::optional<std::filesystem::path> chooseThemeImportFile(QWidget *parent);
std::optional<std::filesystem::path> chooseSheetExportPath(QWidget *parent, const QString &defaultName);
std::optional<std::filesystem::path> chooseFolder(QWidget *parent, const QString &title);

}  // namespace sky
