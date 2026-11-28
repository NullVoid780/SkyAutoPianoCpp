#include "ui/main_window/store_pagination.hpp"

#include "core/translator.hpp"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace sky {

namespace {

constexpr int kGap = 10;
constexpr int kAbove = 12;
constexpr int kBelow = 6;

}  // namespace

StorePagination::StorePagination(QWidget *parent) : QWidget(parent), m_row(new QHBoxLayout())
{
    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, kAbove, 0, kBelow);
    m_row->setSpacing(kGap);
    column->addLayout(m_row);
}

std::pair<int, int> StorePagination::pageWindow(int current, int total)
{
    current = std::min(current, total);
    int start = std::max(1, current - 2);
    const int end = std::min(total, start + 4);
    start = std::max(1, end - 4);
    return {start, end};
}

void StorePagination::setPages(int current, int total, bool busy)
{
    while (QLayoutItem *item = m_row->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    setVisible(total > 1);
    if (total <= 1) {
        return;
    }
    current = std::min(current, total);
    const auto addButton = [this](const QString &label, int page, bool enabled, bool active) {
        auto *button = new QPushButton(label, this);
        button->setObjectName(active ? QStringLiteral("PageButtonActive") : QStringLiteral("PageButton"));
        button->setEnabled(enabled);
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, page] { emit pageRequested(page); });
        m_row->addWidget(button);
    };
    m_row->addStretch();
    addButton(tr("Prev"), current - 1, !busy && current > 1, false);
    const auto [start, end] = pageWindow(current, total);
    for (int page = start; page <= end; ++page) {
        addButton(QString::number(page), page, !busy && page != current, page == current);
    }
    addButton(tr("Next"), current + 1, !busy && current < total, false);
    m_row->addStretch();
}

}  // namespace sky
