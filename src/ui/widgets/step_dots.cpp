#include "ui/widgets/step_dots.hpp"

#include <QPainter>

#include <algorithm>

#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

StepDots::StepDots(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(12);
}

void StepDots::set(int count, int current)
{
    m_count = count;
    m_current = current;
    setFixedWidth(std::max(0, count * 22 - 8));
    update();
}

void StepDots::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    qreal x = 0;
    for (int i = 0; i < m_count; ++i) {
        const qreal width = i == m_current ? 22 : 8;
        const QRectF dot(x, 2, width, 8);
        paintPill(painter, dot, i <= m_current ? theme.hoverColor : withAlpha(theme.fontColor, 0.25));
        x += width + 6;
    }
}

}  // namespace sky
