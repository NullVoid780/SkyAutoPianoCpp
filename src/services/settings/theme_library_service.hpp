#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <QObject>

#include "core/json.hpp"
#include "core/result.hpp"
#include "model/config.hpp"

namespace sky {

using ThemeColorList = std::vector<std::pair<std::string, std::string>>;

struct CustomTheme {
    std::string id;
    std::string name;
    ThemeColorList light;
    ThemeColorList dark;
};

class ThemeLibraryService final : public QObject {
    Q_OBJECT

public:
    static constexpr int kNameLimit = 40;
    static constexpr std::size_t kColorLimit = 256;
    static constexpr std::size_t kThemeLimit = 100;

    explicit ThemeLibraryService(std::filesystem::path dataDirectory, QObject *parent = nullptr);

    void load();

    const std::vector<CustomTheme> &themes() const { return m_themes; }
    const CustomTheme *find(const std::string &id) const;
    Result<std::string> save(CustomTheme theme);
    bool remove(const std::string &id);
    std::string uniqueName(const std::string &wanted, const std::string &ignoreId = {}) const;

    const std::filesystem::path &filePath() const { return m_path; }

    static Result<CustomTheme> fromJson(const OrderedJson &json);
    static OrderedJson toJson(const CustomTheme &theme, bool withId);
    static Result<CustomTheme> readThemeFile(const std::filesystem::path &file);
    static Result<void> writeThemeFile(const std::filesystem::path &file, const CustomTheme &theme);
    static std::string cleanName(const std::string &name);
    static bool isColorText(const std::string &text);

signals:
    void changed();

private:
    Result<void> write() const;

    std::filesystem::path m_path;
    std::vector<CustomTheme> m_themes;
};

}  // namespace sky
