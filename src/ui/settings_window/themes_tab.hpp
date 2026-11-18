#pragma once

#include <QPointer>
#include <QWidget>

#include "services/notifications/toast_service.hpp"
#include "ui/settings_window/theme_card.hpp"

class QLabel;

namespace sky {

class ConfirmPopup;
class SelectBox;
class ThemeBuilder;
class ThemeController;
class ThemeGrid;

class ThemesTab final : public QWidget {
    Q_OBJECT

public:
    explicit ThemesTab(ThemeController &themes, QWidget *parent = nullptr);

    bool isEditing() const;
    void leave();

signals:
    void toastRequested(const sky::Toast &toast);
    void editingChanged(bool editing);

private:
    void rebuild();
    void refreshActive();
    void openBuilder(const ThemeDefinition &draft, bool isNew);
    void closeBuilder();
    void onCardAction(const QString &id, ThemeCard::Action action);
    void createTheme();
    void importTheme();
    void exportTheme(const QString &id);
    void deleteTheme(const QString &id);
    void notify(ToastType type, const std::string &message);
    void refreshModeChoice();
    ConfirmPopup *confirm();

    ThemeController &m_themes;
    QWidget *m_gallery;
    ThemeGrid *m_builtIn;
    ThemeGrid *m_custom;
    QLabel *m_emptyHint;
    SelectBox *m_modeChoice;
    ThemeBuilder *m_builder;
    QPointer<ConfirmPopup> m_confirm;
};

}  // namespace sky
