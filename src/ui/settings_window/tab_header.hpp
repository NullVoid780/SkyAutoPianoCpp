#pragma once

#include <functional>

#include <QColor>
#include <QWidget>

class QBoxLayout;
class QHBoxLayout;
class QLabel;

namespace sky {

class TabHeader final : public QWidget {
    Q_OBJECT

public:
    TabHeader(const QString &iconId, std::function<QColor()> accent, const QString &title, const QString &description,
              QWidget *parent = nullptr);

    void addAction(QWidget *action);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void refreshIcon();
    void updateWrap();

    QString m_iconId;
    std::function<QColor()> m_accent;
    QString m_descriptionText;
    QLabel *m_icon;
    QLabel *m_description;
    QWidget *m_copy;
    QWidget *m_actions;
    QHBoxLayout *m_actionRow;
    QBoxLayout *m_row;
    bool m_wrapped = false;
};

}  // namespace sky
