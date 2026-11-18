#pragma once

#include <QWidget>

namespace sky {

class AboutTab final : public QWidget {
    Q_OBJECT

public:
    explicit AboutTab(const QString &version, QWidget *parent = nullptr);

signals:
    void originalProjectRequested();
    void sourceCodeRequested();
    void dataFolderRequested();
    void licensesRequested();
};

}  // namespace sky
