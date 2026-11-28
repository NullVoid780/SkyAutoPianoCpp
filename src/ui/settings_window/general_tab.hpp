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
    // Phase 2 legacy cleanup section: the description explains the current
    // state, the Remove action is only enabled when the removal gate is
    // open, and the verify action is offered instead while the imported
    // data still needs a fresh verification.
    void setLegacyCleanup(const QString &description, bool removeEnabled, bool verifyVisible, bool busy);

signals:
    void importRequested();
    void exportRequested();
    void importPreviousRequested();
    void legacyRemovalRequested();
    void legacyVerifyRequested();
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
    SettingsRow *m_legacyVerifyRow;
    ActionButton *m_legacyVerify;
    QPointer<ConfirmPopup> m_confirm;

    void onLanguageChanged(int index);
};

}  // namespace sky
