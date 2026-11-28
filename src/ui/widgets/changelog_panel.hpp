#pragma once

#include "ui/core/overlay.hpp"

class QLabel;
class QPushButton;

namespace sky {

class MarkdownView;

class ChangelogPanel final : public Overlay {
    Q_OBJECT

public:
    explicit ChangelogPanel(QWidget *window);

    void present(const QString &version, const QString &notes, const QString &pageUrl = QString());

protected:
    QWidget *panel() const override { return m_panel; }
    QColor dimColor() const override;
    ShadowSpec panelShadow() const override;
    qreal panelRadius() const override { return 16; }
    QRect panelGeometry() const override;
    void paintPanel(QPainter &painter, const QRectF &rect) override;
    qreal backdropBlurRadius() const override { return 8; }
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    int panelWidth() const;
    void fitNotes();

    QWidget *m_panel;
    QLabel *m_title;
    QLabel *m_pill;
    MarkdownView *m_notes;
    QPushButton *m_readMore;
    QPushButton *m_gotIt;
    QString m_pageUrl;
};

}  // namespace sky
