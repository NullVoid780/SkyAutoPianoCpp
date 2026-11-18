#pragma once

#include <QWidget>

#include "model/config.hpp"
#include "ui/settings_window/update_overview_card.hpp"

namespace sky {

class ActionButton;
class ToggleSwitch;

class UpdatesTab final : public QWidget {
    Q_OBJECT

public:
    explicit UpdatesTab(const QString &currentVersion, QWidget *parent = nullptr);

    void load(const Config &config);
    void collect(Config &config) const;
    void setBlockUpdates(bool blocked);
    void setChecking(bool checking);
    void showResult(UpdateOverviewCard::State state, const QString &latestVersion = QString());

signals:
    void blockUpdatesToggled(bool blocked);
    void checkRequested();
    void communityRequested();

private:
    UpdateOverviewCard *m_overview;
    ToggleSwitch *m_automatic;
    ActionButton *m_check;
    bool m_checking = false;
};

}  // namespace sky
