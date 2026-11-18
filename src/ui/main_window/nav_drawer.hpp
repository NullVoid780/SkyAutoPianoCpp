#pragma once

#include <vector>

#include <QWidget>

#include "ui/main_window/library_model.hpp"

class QPropertyAnimation;

namespace sky {

class NavTab;

class NavDrawer final : public QWidget {
    Q_OBJECT

public:
    explicit NavDrawer(QWidget *window);

    void setOpen(bool open);
    bool isOpen() const { return m_open; }
    void setActiveTab(LibraryTab tab);
    void setToggle(QWidget *toggle);
    void syncToWindow();

signals:
    void tabChosen(sky::LibraryTab tab);
    void openChanged(bool open);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    int panelWidth() const;
    int closedX() const;

    QWidget *m_window;
    QWidget *m_toggle = nullptr;
    QWidget *m_panel;
    std::vector<std::pair<LibraryTab, NavTab *>> m_tabs;
    QPropertyAnimation *m_slide;
    bool m_open = false;
};

}  // namespace sky
