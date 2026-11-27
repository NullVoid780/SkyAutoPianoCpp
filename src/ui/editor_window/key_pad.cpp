#include "ui/editor_window/key_pad.hpp"

#include <QGridLayout>

#include <algorithm>

#include "model/key_table.hpp"
#include "ui/editor_window/key_colors.hpp"
#include "ui/editor_window/key_pad_cell.hpp"

namespace sky {

namespace {

constexpr int kMargin = 4;
constexpr int kGap = 5;
constexpr int kColumns = 5;
constexpr int kRows = 3;
constexpr int kMinCell = 30;

int spacingFor(int cellSize)
{
    return std::max(6, (kGap + kMargin * 2) * cellSize / KeyPadCell::kSize);
}

constexpr std::array<int, 15> kShapes = {
    KeyPadCell::Square | KeyPadCell::Circle, KeyPadCell::Square, KeyPadCell::Circle, KeyPadCell::Square, KeyPadCell::Circle,
    KeyPadCell::Circle, KeyPadCell::Square, KeyPadCell::Square | KeyPadCell::Circle, KeyPadCell::Square, KeyPadCell::Circle,
    KeyPadCell::Circle, KeyPadCell::Square, KeyPadCell::Circle, KeyPadCell::Square, KeyPadCell::Square | KeyPadCell::Circle,
};

}  // namespace

KeyPad::KeyPad(QWidget *parent) : QWidget(parent), m_grid(new QGridLayout(this))
{
    QGridLayout *grid = m_grid;
    grid->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
    grid->setSpacing(kGap + kMargin * 2);
    for (int i = 0; i < 15; ++i) {
        const std::string_view name = kAutoKeys[static_cast<std::size_t>(i)];
        auto *cell = new KeyPadCell(QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size())),
                                    kShapes[static_cast<std::size_t>(i)], this);
        cell->setKeyIndex(i);
        m_cells[static_cast<std::size_t>(i)] = cell;
        grid->addWidget(cell, i / kColumns, i % kColumns);
        connect(cell, &QAbstractButton::clicked, this, [this, i] { emit keyClicked(i); });
    }
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void KeyPad::setCellSize(int size)
{
    size = std::max(kMinCell, size);
    m_grid->setSpacing(spacingFor(size));
    for (KeyPadCell *cell : m_cells) {
        cell->setCellSize(size);
    }
}

int KeyPad::widthFor(int cellSize)
{
    return kMargin * 2 + kColumns * cellSize + (kColumns - 1) * spacingFor(cellSize);
}

int KeyPad::heightFor(int cellSize)
{
    return kMargin * 2 + kRows * cellSize + (kRows - 1) * spacingFor(cellSize);
}

void KeyPad::showKeys(const std::vector<int> &keys)
{
    for (int i = 0; i < 15; ++i) {
        m_cells[static_cast<std::size_t>(i)]->setActive(std::find(keys.begin(), keys.end(), i) != keys.end());
    }
}

void KeyPad::setKeyActive(int index, bool active)
{
    if (index >= 0 && index < 15) {
        m_cells[static_cast<std::size_t>(index)]->setActive(active);
    }
}

void KeyPad::flash(int index)
{
    if (index >= 0 && index < 15) {
        m_cells[static_cast<std::size_t>(index)]->flash();
    }
}

}  // namespace sky
