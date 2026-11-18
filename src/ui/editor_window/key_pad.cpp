#include "ui/editor_window/key_pad.hpp"

#include <QGridLayout>

#include <algorithm>

#include "ui/editor_window/editor_document.hpp"
#include "ui/editor_window/key_pad_cell.hpp"

namespace sky {

namespace {

constexpr int kMargin = 4;
constexpr int kGap = 5;
constexpr int kColumns = 5;

constexpr std::array<int, 15> kShapes = {
    KeyPadCell::Square | KeyPadCell::Circle, KeyPadCell::Square, KeyPadCell::Circle, KeyPadCell::Square, KeyPadCell::Circle,
    KeyPadCell::Circle, KeyPadCell::Square, KeyPadCell::Square | KeyPadCell::Circle, KeyPadCell::Square, KeyPadCell::Circle,
    KeyPadCell::Circle, KeyPadCell::Square, KeyPadCell::Circle, KeyPadCell::Square, KeyPadCell::Square | KeyPadCell::Circle,
};

}  // namespace

KeyPad::KeyPad(QWidget *parent) : QWidget(parent)
{
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
    grid->setSpacing(kGap + kMargin * 2);
    for (int i = 0; i < 15; ++i) {
        auto *cell = new KeyPadCell(QString::fromStdString(EditorDocument::keyAt(i)), kShapes[static_cast<std::size_t>(i)], this);
        m_cells[static_cast<std::size_t>(i)] = cell;
        grid->addWidget(cell, i / kColumns, i % kColumns);
        connect(cell, &QAbstractButton::clicked, this, [this, i] { emit keyClicked(i); });
    }
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void KeyPad::showKeys(const std::vector<std::string> &keys)
{
    for (int i = 0; i < 15; ++i) {
        const std::string &key = EditorDocument::keyAt(i);
        m_cells[static_cast<std::size_t>(i)]->setActive(std::find(keys.begin(), keys.end(), key) != keys.end());
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
