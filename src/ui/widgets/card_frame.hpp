#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <QMargins>
#include <QWidget>

#include "ui/theme.hpp"

namespace sky {

class ElevationLayer;
class HoverTracker;

class CardFrame : public QWidget {
    Q_OBJECT

public:
    CardFrame(qreal lift, qreal shift, bool hoverable, QWidget *parent = nullptr);

    void setPadding(const QMargins &padding);
    void attachShadows(ElevationLayer *layer);
    qreal hoverProgress() const;
    void addChildShadow(QWidget *target, qreal radius, std::function<std::optional<ShadowSpec>()> shadow);

protected:
    virtual void paintCard(QPainter &painter, const QRectF &body, qreal hover) = 0;
    virtual std::vector<ShadowSpec> hoverShadows() const { return {}; }
    virtual qreal cornerRadius() const = 0;

    QRectF bodyRect() const;
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyOffset();

    struct ChildShadow {
        QWidget *target;
        qreal radius;
        std::function<std::optional<ShadowSpec>()> shadow;
    };

    qreal m_lift;
    qreal m_shift;
    QMargins m_padding;
    HoverTracker *m_hover = nullptr;
    ElevationLayer *m_layer = nullptr;
    std::vector<ChildShadow> m_childShadows;
};

}  // namespace sky
