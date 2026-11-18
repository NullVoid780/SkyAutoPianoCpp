#pragma once

#include <array>
#include <string>
#include <vector>

#include <QWidget>

namespace sky {

class KeyPadCell;

class KeyPad final : public QWidget {
    Q_OBJECT

public:
    explicit KeyPad(QWidget *parent = nullptr);

    void showKeys(const std::vector<std::string> &keys);
    void setKeyActive(int index, bool active);
    void flash(int index);

signals:
    void keyClicked(int index);

private:
    std::array<KeyPadCell *, 15> m_cells{};
};

}  // namespace sky
