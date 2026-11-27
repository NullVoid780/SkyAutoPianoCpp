#pragma once

#include <array>
#include <vector>

#include <QWidget>

class QGridLayout;

namespace sky {

class KeyPadCell;

class KeyPad final : public QWidget {
    Q_OBJECT

public:
    explicit KeyPad(QWidget *parent = nullptr);

    void showKeys(const std::vector<int> &keys);
    void setKeyActive(int index, bool active);
    void flash(int index);
    void setCellSize(int size);
    static int widthFor(int cellSize);
    static int heightFor(int cellSize);

signals:
    void keyClicked(int index);

private:
    std::array<KeyPadCell *, 15> m_cells{};
    QGridLayout *m_grid;
};

}  // namespace sky
