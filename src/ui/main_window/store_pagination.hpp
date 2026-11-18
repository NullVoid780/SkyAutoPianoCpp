#pragma once

#include <QWidget>

class QHBoxLayout;

namespace sky {

class StorePagination final : public QWidget {
    Q_OBJECT

public:
    explicit StorePagination(QWidget *parent = nullptr);

    void setPages(int current, int total, bool busy);

    static std::pair<int, int> pageWindow(int current, int total);

signals:
    void pageRequested(int page);

private:
    QHBoxLayout *m_row;
};

}  // namespace sky
