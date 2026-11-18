#pragma once

#include <memory>

#include <QColor>
#include <QPointer>
#include <QWidget>

#include "ui/theme.hpp"

class QVariantAnimation;

namespace sky {
class BackdropBlur;
}

namespace sky {

class Overlay : public QWidget {
    Q_OBJECT

public:
    explicit Overlay(QWidget *window);
    ~Overlay() override;

    void open();
    void close();
    bool isOpen() const { return m_open; }

signals:
    void opened();
    void closed();

protected:
    virtual QWidget *panel() const = 0;
    virtual QColor dimColor() const = 0;
    virtual ShadowSpec panelShadow() const = 0;
    virtual qreal panelRadius() const = 0;
    virtual QRect panelGeometry() const = 0;
    virtual void paintPanel(QPainter &painter, const QRectF &rect);
    virtual qreal backdropBlurRadius() const { return 0; }

    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    void relayout();
    qreal presence() const { return m_presence; }

private:
    QWidget *m_window;
    QPointer<QWidget> m_previousFocus;
    std::unique_ptr<BackdropBlur> m_backdrop;
    QVariantAnimation *m_fade;
    qreal m_presence = 0;
    bool m_open = false;
};

}  // namespace sky
