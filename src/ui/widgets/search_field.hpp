#pragma once

#include <QLineEdit>

class QAction;

namespace sky {

class SearchField final : public QLineEdit {
    Q_OBJECT

public:
    explicit SearchField(QWidget *parent = nullptr);

    void applyTheme();

private:
    QAction *m_icon;
};

}  // namespace sky
