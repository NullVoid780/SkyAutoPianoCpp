#pragma once

#include <set>

#include <QLineEdit>
#include <QStringList>

namespace sky {

class ShortcutEdit final : public QLineEdit {
    Q_OBJECT

public:
    explicit ShortcutEdit(QWidget *parent = nullptr);

    void setCaptureEnabled(bool enabled);

signals:
    void edited();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void finish();

    bool m_captureEnabled = true;
    std::set<quint32> m_held;
    QStringList m_tokens;
    QString m_previous;
};

}  // namespace sky
