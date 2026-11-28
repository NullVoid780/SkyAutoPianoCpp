#pragma once

#include <QPointer>
#include <QWidget>

#include "model/config.hpp"

namespace sky {

class ActionButton;
class ConfirmPopup;
class SelectBox;
class SettingsRow;
class ToggleSwitch;

class GeneralTab final : public QWidget {
    Q_OBJECT

public:
    explicit GeneralTab(QWidget *parent = nullptr);

    void load(const Config &config);
    void collect(Config &config) const;
    void setLanguage(const QString &preference);
    // Legacy cleanup section: the description explains the current state
    // and the Remove action is only enabled when the removal gate is
    // open. Verification of the imported data runs automatically when
    // this page is shown (the application drives it), so there is
    // deliberately no manual verify action here.
    void setLegacyCleanup(const QString &title, const QString &description, bool removeEnabled, bool busy,
                          bool present);

signals:
    void importRequested();
    void exportRequested();
    void importPreviousRequested();
    void legacyRemovalRequested();
    void quickStartRequested();
    void languageChosen(const QString &preference);
    void restartRequested();

private:
    ToggleSwitch *m_autoSave;
    ToggleSwitch *m_minimizeOnPlay;
    ToggleSwitch *m_pauseWhenUnfocused;
    SelectBox *m_playbackMode;
    SelectBox *m_language;
    SettingsRow *m_legacyCleanup;
    ActionButton *m_legacyRemove;
    QPointer<ConfirmPopup> m_confirm;

    void onLanguageChanged(int index);
};

}  // namespace sky
