#pragma once

#include <QListView>

class QPropertyAnimation;

namespace sky {

class LibraryModel;
class SheetCardDelegate;

class LibraryView final : public QListView {
    Q_OBJECT

public:
    LibraryView(LibraryModel *model, QWidget *parent = nullptr);

    void setOpenMenuRow(int row);
    void scrollToTopSmoothly();

signals:
    void cardActivated(int libraryIndex);
    void actionMenuRequested(int libraryIndex, const QRect &anchorGlobal);
    void scrolled(int offset);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyboardSearch(const QString &search) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void animateScrollTo(int target, int durationMs);

    LibraryModel *m_model;
    SheetCardDelegate *m_delegate;
    QPropertyAnimation *m_scrollAnimation;
    int m_scrollTarget = 0;
};

}  // namespace sky
