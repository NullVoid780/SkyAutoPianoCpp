#pragma once

#include <QWidget>

namespace sky {

class ActionButton;
class SettingsRow;

// The legacy cleanup on its own Settings page: the description explains the
// current state and the Remove action is only enabled when the removal gate
// is open. Verification of the imported data runs automatically when this
// page is shown (the application drives it), so there is deliberately no
// manual verify action here.
class LegacyCleanupTab final : public QWidget {
    Q_OBJECT

public:
    explicit LegacyCleanupTab(QWidget *parent = nullptr);

    void setLegacyCleanup(const QString &description, bool removeEnabled, bool busy);

signals:
    void legacyRemovalRequested();

private:
    SettingsRow *m_cleanup;
    ActionButton *m_remove;
};

}  // namespace sky
