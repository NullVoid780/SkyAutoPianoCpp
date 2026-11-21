#include "ui/widgets/select_box.hpp"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

#include <algorithm>
#include <functional>

#include "ui/core/hover_tracker.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kHeight = 40;
constexpr int kPadX = 14;
constexpr int kChevron = 14;
constexpr int kChevronGap = 12;
constexpr qreal kRadius = 10;
constexpr int kMinWidth = 140;
constexpr int kPopupMargin = 14;
constexpr int kPopupGap = 6;
constexpr int kPopupPadding = 6;
constexpr int kPopupMinWidth = 260;
constexpr qreal kPopupRadius = 12;
constexpr int kRowPadX = 12;
constexpr int kRowPadY = 9;
constexpr qreal kRowRadius = 8;
constexpr int kCheck = 16;
constexpr int kLineGap = 2;

QFont labelFont()
{
    return FontLoader::font(14, QFont::Medium);
}

QFont descriptionFont()
{
    return FontLoader::font(12, QFont::Normal);
}

class SelectPopup final : public QWidget {
public:
    SelectPopup(const std::vector<SelectBox::Option> &options, int current, int width,
                std::function<void(int)> chosen, QWidget *owner)
        : QWidget(owner, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint),
          m_options(options),
          m_current(current),
          m_hover(current),
          m_chosen(std::move(chosen))
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        setMouseTracking(true);
        int height = kPopupPadding * 2;
        int content = 0;
        const QFontMetrics labelMetrics(labelFont());
        const QFontMetrics descriptionMetrics(descriptionFont());
        for (const SelectBox::Option &option : m_options) {
            height += rowHeight(option);
            content = std::max({content, labelMetrics.horizontalAdvance(option.label),
                                descriptionMetrics.horizontalAdvance(option.description)});
        }
        const int needed = kPopupPadding * 2 + kRowPadX * 3 + kCheck + content + 4;
        resize(std::max({width, kPopupMinWidth, needed}) + kPopupMargin * 2, height + kPopupMargin * 2);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Theme &theme = currentTheme();
        const SettingsPalette &s = theme.settings;
        QPainter painter(this);
        preparePainter(painter);
        const QRectF panel = QRectF(rect()).adjusted(kPopupMargin, kPopupMargin, -kPopupMargin, -kPopupMargin);
        ShadowPainter::paint(painter, panel, kPopupRadius, theme.shadowPopover);
        const QPainterPath path = roundedPath(panel.adjusted(0.5, 0.5, -0.5, -0.5), kPopupRadius);
        painter.fillPath(path, theme.popoverFill);
        painter.setPen(QPen(theme.popoverBorder, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);

        for (int i = 0; i < static_cast<int>(m_options.size()); ++i) {
            const SelectBox::Option &option = m_options[static_cast<std::size_t>(i)];
            const QRectF row = rowRect(i);
            if (i == m_hover) {
                painter.fillPath(roundedPath(row, kRowRadius), theme.menuHoverFill);
            }
            const bool selected = i == m_current;
            const qreal textRight = row.right() - kRowPadX - kCheck - kRowPadX;
            const QFontMetrics labelMetrics(labelFont());
            qreal top = row.top() + kRowPadY;
            painter.setFont(labelFont());
            painter.setPen(selected ? theme.hoverColor : theme.fontColor);
            painter.drawText(QRectF(row.left() + kRowPadX, top, textRight - row.left() - kRowPadX, labelMetrics.height()),
                             Qt::AlignLeft | Qt::AlignVCenter, option.label);
            if (!option.description.isEmpty()) {
                top += labelMetrics.height() + kLineGap;
                painter.setFont(descriptionFont());
                painter.setPen(s.cardDescText);
                painter.drawText(QRectF(row.left() + kRowPadX, top, textRight - row.left() - kRowPadX,
                                        QFontMetrics(descriptionFont()).height()),
                                 Qt::AlignLeft | Qt::AlignVCenter, option.description);
            }
            if (selected) {
                painter.drawPixmap(QPointF(row.right() - kRowPadX - kCheck, row.center().y() - kCheck / 2.0),
                                   IconCache::pixmap(QStringLiteral("check-lg"), kCheck, theme.hoverColor,
                                                     devicePixelRatioF()));
            }
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        const QRectF panel = QRectF(rect()).adjusted(kPopupMargin, kPopupMargin, -kPopupMargin, -kPopupMargin);
        if (panel.contains(event->position())) {
            QWidget::mousePressEvent(event);
            return;
        }
        if (const QWidget *owner = parentWidget()) {
            const QRect ownerArea(owner->mapToGlobal(QPoint(0, 0)), owner->size());
            if (ownerArea.contains(event->globalPosition().toPoint())) {
                setAttribute(Qt::WA_NoMouseReplay);
            }
        }
        close();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        const int row = rowAt(event->position());
        if (row != m_hover && row >= 0) {
            m_hover = row;
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        const int row = rowAt(event->position());
        if (row >= 0) {
            choose(row);
        }
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        const int count = static_cast<int>(m_options.size());
        switch (event->key()) {
        case Qt::Key_Up:
            m_hover = (m_hover - 1 + count) % count;
            update();
            return;
        case Qt::Key_Down:
            m_hover = (m_hover + 1) % count;
            update();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            choose(m_hover);
            return;
        case Qt::Key_Escape:
            close();
            return;
        default:
            QWidget::keyPressEvent(event);
        }
    }

private:
    static int rowHeight(const SelectBox::Option &option)
    {
        int height = kRowPadY * 2 + QFontMetrics(labelFont()).height();
        if (!option.description.isEmpty()) {
            height += kLineGap + QFontMetrics(descriptionFont()).height();
        }
        return height;
    }

    QRectF rowRect(int index) const
    {
        qreal top = kPopupMargin + kPopupPadding;
        for (int i = 0; i < index; ++i) {
            top += rowHeight(m_options[static_cast<std::size_t>(i)]);
        }
        const qreal left = kPopupMargin + kPopupPadding;
        return QRectF(left, top, width() - left * 2, rowHeight(m_options[static_cast<std::size_t>(index)]));
    }

    int rowAt(const QPointF &point) const
    {
        for (int i = 0; i < static_cast<int>(m_options.size()); ++i) {
            if (rowRect(i).contains(point)) {
                return i;
            }
        }
        return -1;
    }

    void choose(int index)
    {
        auto chosen = m_chosen;
        close();
        if (index >= 0 && chosen) {
            chosen(index);
        }
    }

    std::vector<SelectBox::Option> m_options;
    int m_current;
    int m_hover;
    std::function<void(int)> m_chosen;
};

}  // namespace

SelectBox::SelectBox(QWidget *parent) : QAbstractButton(parent), m_hover(new HoverTracker(this, kHoverMs, Easing::Ease))
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_hover, &HoverTracker::changed, this, [this] { update(); });
    connect(this, &QAbstractButton::clicked, this, &SelectBox::openPopup);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void SelectBox::addOption(const QString &label, const QString &description)
{
    m_options.push_back(Option{label, description});
    if (m_current < 0) {
        m_current = 0;
    }
    updateGeometry();
    update();
}

void SelectBox::setCurrentIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(m_options.size()) || index == m_current) {
        return;
    }
    m_current = index;
    update();
    emit currentIndexChanged(index);
}

void SelectBox::choose(int index)
{
    setCurrentIndex(index);
    setFocus(Qt::OtherFocusReason);
}

QSize SelectBox::sizeHint() const
{
    int widest = 0;
    const QFontMetrics metrics(labelFont());
    for (const Option &option : m_options) {
        widest = std::max(widest, metrics.horizontalAdvance(option.label));
    }
    return QSize(std::max(kMinWidth, kPadX * 2 + widest + kChevronGap + kChevron), kHeight);
}

void SelectBox::openPopup()
{
    if (m_popup || m_options.empty()) {
        return;
    }
    QPointer<SelectBox> self(this);
    auto *popup = new SelectPopup(m_options, m_current, width(), [self](int index) {
        if (self) {
            self->choose(index);
        }
    }, this);
    QPoint at = mapToGlobal(QPoint(width(), height() + kPopupGap)) - QPoint(popup->width() - kPopupMargin, kPopupMargin);
    if (const QScreen *screen = this->screen()) {
        const QRect area = screen->availableGeometry();
        if (at.y() + popup->height() > area.bottom()) {
            at.setY(mapToGlobal(QPoint(0, 0)).y() - kPopupGap - popup->height() + kPopupMargin);
        }
        at.setX(std::clamp(at.x(), area.left(), std::max(area.left(), area.right() - popup->width())));
    }
    popup->move(at);
    m_popup = popup;
    connect(popup, &QObject::destroyed, this, [this] { update(); });
    popup->show();
    popup->setFocus(Qt::PopupFocusReason);
    update();
}

void SelectBox::keyPressEvent(QKeyEvent *event)
{
    const int count = static_cast<int>(m_options.size());
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        openPopup();
        return;
    case Qt::Key_Up:
        if (count > 0) {
            setCurrentIndex((m_current - 1 + count) % count);
        }
        return;
    case Qt::Key_Down:
        if (count > 0) {
            setCurrentIndex((m_current + 1) % count);
        }
        return;
    default:
        QAbstractButton::keyPressEvent(event);
    }
}

void SelectBox::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    const SettingsPalette &s = theme.settings;
    QPainter painter(this);
    preparePainter(painter);
    const qreal hover = m_hover->progress();
    const bool open = !m_popup.isNull();
    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QPainterPath path = roundedPath(body, kRadius);
    painter.fillPath(path, blend(s.subtleFill, s.subtleHoverFill, open ? 1.0 : hover));
    painter.setPen(QPen(hasFocus() || open ? theme.hoverColor : s.subtleBorder, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    const QString label = m_current >= 0 ? m_options[static_cast<std::size_t>(m_current)].label : QString();
    painter.setFont(labelFont());
    painter.setPen(theme.fontColor);
    painter.drawText(body.adjusted(kPadX, 0, -(kPadX + kChevron + kChevronGap), 0), Qt::AlignLeft | Qt::AlignVCenter,
                     label);
    painter.save();
    painter.translate(body.right() - kPadX - kChevron / 2.0, body.center().y());
    if (open) {
        painter.rotate(180);
    }
    painter.drawPixmap(QPointF(-kChevron / 2.0, -kChevron / 2.0),
                       IconCache::pixmap(QStringLiteral("chevron-down"), kChevron, s.cardDescText, devicePixelRatioF()));
    painter.restore();
}

}  // namespace sky
