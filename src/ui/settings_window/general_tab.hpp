#pragma once

#include <QWidget>

#include "model/config.hpp"

namespace sky {

class SelectBox;
class ToggleSwitch;

class GeneralTab final : public QWidget {
    Q_OBJECT

public:
    explicit GeneralTab(QWidget *parent = nullptr);

    void load(const Config &config);
    void collect(Config &config) const;

signals:
    void importRequested();
    void exportRequested();
    void importPreviousRequested();
    void quickStartRequested();

private:
    ToggleSwitch *m_autoSave;
    ToggleSwitch *m_minimizeOnPlay;
    SelectBox *m_playbackMode;
};

}  // namespace sky
