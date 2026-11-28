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

signals:
    void importRequested();
    void exportRequested();
    void importPreviousRequested();
    void quickStartRequested();
    void languageChosen(const QString &preference);
    void restartRequested();

private:
    ToggleSwitch *m_autoSave;
    ToggleSwitch *m_minimizeOnPlay;
    ToggleSwitch *m_pauseWhenUnfocused;
    SelectBox *m_playbackMode;
    SelectBox *m_language;
    QPointer<ConfirmPopup> m_confirm;

    void onLanguageChanged(int index);
};

}  // namespace sky
