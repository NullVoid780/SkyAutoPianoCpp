#pragma once

#include "ui/core/overlay.hpp"

class QLabel;
class QStackedWidget;

namespace sky {

class ActionButton;
class PreviewCard;
class StepDots;

class EditorSetup final : public Overlay {
    Q_OBJECT

public:
    explicit EditorSetup(QWidget *window);

    void start(bool advanced, bool colorful);

signals:
    void chosen(bool advanced, bool colorful);

protected:
    QWidget *panel() const override { return m_panel; }
    QColor dimColor() const override;
    ShadowSpec panelShadow() const override;
    qreal panelRadius() const override { return 20; }
    QRect panelGeometry() const override;
    void paintPanel(QPainter &painter, const QRectF &rect) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QWidget *buildStep(const QString &step, const QString &title, const QString &body, PreviewCard *first,
                       PreviewCard *second);
    void showStep(int index);
    void next();
    void back();
    void finish();

    QWidget *m_panel;
    QStackedWidget *m_stack;
    StepDots *m_dots;
    ActionButton *m_back;
    ActionButton *m_next;
    PreviewCard *m_simple;
    PreviewCard *m_advanced;
    PreviewCard *m_colorful;
    PreviewCard *m_plain;
    int m_step = 0;
    bool m_finished = true;
};

}  // namespace sky
