#pragma once

#include <array>

#include <QWidget>

#include "model/config.hpp"
#include "model/key_table.hpp"

class QLabel;

namespace sky {

class KeyCap;
class ToggleSwitch;

class KeyboardTab final : public QWidget {
    Q_OBJECT

public:
    explicit KeyboardTab(QWidget *parent = nullptr);

    void load(const Config &config);
    void collect(Config &config) const;

private:
    void refreshTipIcon();

    std::array<KeyCap *, kSkyKeyCount> m_caps{};
    ToggleSwitch *m_customKeyboard;
    QLabel *m_tipIcon;
};

}  // namespace sky
