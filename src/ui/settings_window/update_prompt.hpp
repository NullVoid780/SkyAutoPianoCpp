#pragma once

#include "ui/core/overlay.hpp"

class QLabel;
class QPushButton;

namespace sky {

class ActionButton;
class IconButton;
class MarkdownView;
class ProgressBar;

class UpdatePrompt final : public Overlay {
    Q_OBJECT

public:
    enum class State { Offer, Downloading, Ready, Failed };

    explicit UpdatePrompt(QWidget *window);

    void present(const QString &currentVersion, const QString &latestVersion, const QString &markdown, bool critical);
    void setNotes(const QString &markdown);
    void showDownloading(int percent);
    void showReady();
    void showFailed(const QString &message);
    State state() const { return m_state; }

signals:
    void installRequested();
    void restartRequested();
    void releasePageRequested();
    void dismissed();

protected:
    QWidget *panel() const override { return m_panel; }
    QColor dimColor() const override;
    ShadowSpec panelShadow() const override;
    qreal panelRadius() const override { return 16; }
    QRect panelGeometry() const override;
    void paintPanel(QPainter &painter, const QRectF &rect) override;
    qreal backdropBlurRadius() const override { return 5; }
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setStatus(const QString &text, bool error);
    void fitNotes();
    void refreshVersion();

    QWidget *m_panel;
    QLabel *m_heading;
    QLabel *m_versionOld;
    QLabel *m_arrow;
    QLabel *m_versionNew;
    QLabel *m_critical;
    MarkdownView *m_notes;
    ProgressBar *m_progress;
    QLabel *m_status;
    ActionButton *m_primary;
    ActionButton *m_later;
    QPushButton *m_github;
    IconButton *m_close;
    State m_state = State::Offer;
    QString m_currentVersion;
    QString m_latestVersion;
};

}  // namespace sky
