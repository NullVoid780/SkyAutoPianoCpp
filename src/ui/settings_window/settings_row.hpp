#pragma once

#include <functional>

#include <QColor>
#include <QWidget>

class QLabel;

namespace sky {

class SettingsRow final : public QWidget {
    Q_OBJECT

public:
    SettingsRow(const QString &iconId, const QString &title, const QString &description, QWidget *control,
                QWidget *parent = nullptr);

    void setIconTile(std::function<QColor()> fill, std::function<QColor()> glyph);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_iconId;
    std::function<QColor()> m_tileFill;
    std::function<QColor()> m_tileGlyph;
    QWidget *m_iconSpace;
    bool m_hovered = false;
};

}  // namespace sky
