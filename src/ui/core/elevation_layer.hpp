#pragma once

#include <functional>
#include <vector>

#include <QPointer>
#include <QWidget>

#include "ui/theme.hpp"

namespace sky {

class ElevationLayer final : public QWidget {
    Q_OBJECT

public:
    using ShadowSource = std::function<std::vector<ShadowSpec>()>;
    using BodySource = std::function<QRectF()>;

    explicit ElevationLayer(QWidget *host);

    void track(QWidget *widget, qreal cornerRadius, ShadowSource shadows, BodySource body = {});
    void untrack(QWidget *widget);
    void refresh(QWidget *widget);
    void setFloating(bool floating);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    struct Entry {
        QPointer<QWidget> widget;
        qreal radius = 0;
        ShadowSource shadows;
        BodySource body;
        QRect painted;
    };

    void syncGeometry();
    QRect shadowBounds(const Entry &entry) const;
    QRectF bodyRect(const Entry &entry) const;

    QWidget *m_host;
    std::vector<Entry> m_entries;
    bool m_floating = false;
};

}  // namespace sky
