#include "ui/settings_window/color_picker.hpp"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

#include "core/translator.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/widgets/action_button.hpp"
#include "ui/theme_catalog.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kPanelWidth = 360;
constexpr int kPadding = 24;
constexpr int kFieldHeight = 190;
constexpr int kStripHeight = 14;
constexpr int kGap = 14;
constexpr int kSwatch = 22;
constexpr int kSwatchGap = 8;
constexpr qreal kChecker = 5;

QColor checkerLight()
{
    return QColor(255, 255, 255);
}

QColor checkerDark()
{
    return QColor(204, 204, 204);
}

void paintChecker(QPainter &painter, const QRectF &rect)
{
    painter.fillRect(rect, checkerLight());
    for (qreal y = rect.top(); y < rect.bottom(); y += kChecker) {
        const bool rowOdd = static_cast<int>((y - rect.top()) / kChecker) % 2 == 1;
        for (qreal x = rect.left() + (rowOdd ? kChecker : 0); x < rect.right(); x += kChecker * 2) {
            painter.fillRect(QRectF(x, y, std::min(kChecker, rect.right() - x), std::min(kChecker, rect.bottom() - y)),
                             checkerDark());
        }
    }
}

void paintHandle(QPainter &painter, const QPointF &center, qreal radius, const QColor &fill)
{
    painter.setPen(QPen(QColor(0, 0, 0, 90), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius + 1.5, radius + 1.5);
    painter.setPen(QPen(Qt::white, 2.5));
    painter.setBrush(fill);
    painter.drawEllipse(center, radius, radius);
}

std::vector<QColor> quickColors()
{
    const AccentPalette &a = currentTheme().accent;
    return {a.white,   a.slate400,  a.slate700,  a.ink900,  a.black,    a.red500,
            a.amber500, a.green500, a.emerald500, a.sky500, a.blue600,  a.violet400,
            a.purple400, a.rose400};
}

}  // namespace

void paintColorSwatch(QPainter &painter, const QRectF &rect, const QColor &color, qreal radius)
{
    painter.save();
    const QPainterPath path = roundedPath(rect, radius);
    painter.setClipPath(path);
    if (color.alpha() < 255) {
        paintChecker(painter, rect);
    }
    painter.fillRect(rect, color);
    painter.setClipping(false);
    painter.setPen(QPen(withAlpha(currentTheme().fontColor, 0.25), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(roundedPath(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius));
    painter.restore();
}

class SaturationValueField final : public QWidget {
public:
    explicit SaturationValueField(QWidget *parent) : QWidget(parent)
    {
        setFixedHeight(kFieldHeight);
        setCursor(Qt::CrossCursor);
    }

    std::function<void(qreal saturation, qreal value)> onChanged;

    void set(qreal hue, qreal saturation, qreal value)
    {
        m_hue = hue;
        m_saturation = saturation;
        m_value = value;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        preparePainter(painter);
        const QRectF area = QRectF(rect());
        painter.save();
        painter.setClipPath(roundedPath(area, 12));
        painter.fillRect(area, QColor::fromHsvF(static_cast<float>(m_hue), 1, 1));
        QLinearGradient white(area.topLeft(), area.topRight());
        white.setColorAt(0, Qt::white);
        white.setColorAt(1, QColor(255, 255, 255, 0));
        painter.fillRect(area, white);
        QLinearGradient black(area.topLeft(), area.bottomLeft());
        black.setColorAt(0, QColor(0, 0, 0, 0));
        black.setColorAt(1, Qt::black);
        painter.fillRect(area, black);
        painter.restore();
        const QPointF point(area.left() + m_saturation * area.width(), area.top() + (1 - m_value) * area.height());
        paintHandle(painter, point, 7,
                    QColor::fromHsvF(static_cast<float>(m_hue), static_cast<float>(m_saturation),
                                     static_cast<float>(m_value)));
    }

    void mousePressEvent(QMouseEvent *event) override { track(event->position()); }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (event->buttons() & Qt::LeftButton) {
            track(event->position());
        }
    }

private:
    void track(const QPointF &point)
    {
        m_saturation = std::clamp(point.x() / std::max(1, width()), 0.0, 1.0);
        m_value = 1.0 - std::clamp(point.y() / std::max(1, height()), 0.0, 1.0);
        update();
        if (onChanged) {
            onChanged(m_saturation, m_value);
        }
    }

    qreal m_hue = 0;
    qreal m_saturation = 0;
    qreal m_value = 0;
};

class ColorStrip final : public QWidget {
public:
    enum class Mode { Hue, Alpha };

    ColorStrip(Mode mode, QWidget *parent) : QWidget(parent), m_mode(mode)
    {
        setFixedHeight(kStripHeight + 8);
        setCursor(Qt::PointingHandCursor);
    }

    std::function<void(qreal)> onChanged;

    void set(qreal position, const QColor &color)
    {
        m_position = position;
        m_color = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        preparePainter(painter);
        const qreal inset = kStripHeight / 2.0 + 2;
        const QRectF track(0, (height() - kStripHeight) / 2.0, width(), kStripHeight);
        painter.save();
        painter.setClipPath(roundedPath(track, kStripHeight / 2.0));
        QLinearGradient gradient(track.topLeft(), track.topRight());
        if (m_mode == Mode::Hue) {
            for (int i = 0; i <= 6; ++i) {
                gradient.setColorAt(i / 6.0, QColor::fromHsvF(i == 6 ? 0.9999f : static_cast<float>(i / 6.0), 1, 1));
            }
        } else {
            paintChecker(painter, track);
            QColor clear = m_color;
            clear.setAlpha(0);
            QColor solid = m_color;
            solid.setAlpha(255);
            gradient.setColorAt(0, clear);
            gradient.setColorAt(1, solid);
        }
        painter.fillRect(track, gradient);
        painter.restore();
        const qreal x = inset + m_position * (width() - inset * 2);
        QColor handle = m_mode == Mode::Hue ? QColor::fromHsvF(static_cast<float>(std::min(m_position, 0.9999)), 1, 1)
                                            : m_color;
        paintHandle(painter, QPointF(x, height() / 2.0), kStripHeight / 2.0 + 1, handle);
    }

    void mousePressEvent(QMouseEvent *event) override { track(event->position().x()); }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (event->buttons() & Qt::LeftButton) {
            track(event->position().x());
        }
    }

private:
    void track(qreal x)
    {
        const qreal inset = kStripHeight / 2.0 + 2;
        m_position = std::clamp((x - inset) / std::max<qreal>(1, width() - inset * 2), 0.0, 1.0);
        update();
        if (onChanged) {
            onChanged(m_position);
        }
    }

    Mode m_mode;
    qreal m_position = 0;
    QColor m_color;
};

class ColorCompare final : public QWidget {
public:
    explicit ColorCompare(QWidget *parent) : QWidget(parent) { setFixedSize(76, 36); }

    void set(const QColor &before, const QColor &after)
    {
        m_before = before;
        m_after = after;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        preparePainter(painter);
        const QRectF area = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.save();
        painter.setClipPath(roundedPath(area, 10));
        paintChecker(painter, area);
        painter.fillRect(QRectF(area.left(), area.top(), area.width() / 2, area.height()), m_before);
        painter.fillRect(QRectF(area.center().x(), area.top(), area.width() / 2, area.height()), m_after);
        painter.restore();
        painter.setPen(QPen(withAlpha(currentTheme().fontColor, 0.25), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(roundedPath(area, 10));
    }

private:
    QColor m_before;
    QColor m_after;
};

class QuickSwatch final : public QWidget {
public:
    QuickSwatch(const QColor &color, QWidget *parent) : QWidget(parent), m_color(color)
    {
        setFixedSize(kSwatch, kSwatch);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        setToolTip(colorToText(color));
    }

    std::function<void(const QColor &)> onPicked;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        preparePainter(painter);
        const QRectF area = QRectF(rect()).adjusted(1, 1, -1, -1);
        if (underMouse()) {
            painter.setPen(QPen(currentTheme().hoverColor, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(area);
        }
        painter.setPen(QPen(withAlpha(currentTheme().fontColor, 0.25), 1));
        painter.setBrush(m_color);
        painter.drawEllipse(area.adjusted(3, 3, -3, -3));
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && onPicked) {
            onPicked(m_color);
        }
    }

private:
    QColor m_color;
};

ColorPicker::ColorPicker(QWidget *window)
    : Overlay(window),
      m_panel(new QWidget(this)),
      m_title(new QLabel(m_panel)),
      m_field(new SaturationValueField(m_panel)),
      m_hue(new ColorStrip(ColorStrip::Mode::Hue, m_panel)),
      m_alpha(new ColorStrip(ColorStrip::Mode::Alpha, m_panel)),
      m_compare(new ColorCompare(m_panel)),
      m_hex(new QLineEdit(m_panel)),
      m_cancel(new ActionButton(ActionButton::Kind::Subtle, QString(), tr("Cancel"), m_panel)),
      m_apply(new ActionButton(ActionButton::Kind::Primary, QStringLiteral("check-lg"), tr("Apply"), m_panel))
{
    m_title->setObjectName(QStringLiteral("ColorPickerTitle"));
    auto *caption = new QLabel(tr("Drag to pick a color, or type a hex code."), m_panel);
    caption->setObjectName(QStringLiteral("SettingsRowDescription"));
    m_hex->setObjectName(QStringLiteral("ThemeHexField"));
    m_hex->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("#?[0-9a-fA-F]{0,8}")), m_hex));
    m_hex->setAccessibleName(tr("Hex color"));

    auto *hueLabel = new QLabel(tr("Hue"), m_panel);
    hueLabel->setObjectName(QStringLiteral("ColorPickerCaption"));
    auto *alphaLabel = new QLabel(tr("Opacity"), m_panel);
    alphaLabel->setObjectName(QStringLiteral("ColorPickerCaption"));

    auto *valueRow = new QHBoxLayout();
    valueRow->setSpacing(12);
    valueRow->addWidget(m_compare);
    valueRow->addWidget(m_hex, 1);

    auto *swatches = new QHBoxLayout();
    swatches->setSpacing(kSwatchGap);
    for (const QColor &color : quickColors()) {
        auto *swatch = new QuickSwatch(color, m_panel);
        swatch->onPicked = [this](const QColor &picked) {
            QColor next = picked;
            next.setAlpha(m_color.alpha());
            setColor(next, false);
        };
        swatches->addWidget(swatch);
    }
    swatches->addStretch();

    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(12);
    buttons->addStretch();
    buttons->addWidget(m_cancel);
    buttons->addWidget(m_apply);

    auto *column = new QVBoxLayout(m_panel);
    column->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    column->setSpacing(0);
    column->addWidget(m_title);
    column->addSpacing(4);
    column->addWidget(caption);
    column->addSpacing(kGap + 2);
    column->addWidget(m_field);
    column->addSpacing(kGap);
    column->addWidget(hueLabel);
    column->addSpacing(4);
    column->addWidget(m_hue);
    column->addSpacing(8);
    column->addWidget(alphaLabel);
    column->addSpacing(4);
    column->addWidget(m_alpha);
    column->addSpacing(kGap);
    column->addLayout(valueRow);
    column->addSpacing(kGap);
    column->addLayout(swatches);
    column->addSpacing(kGap + 6);
    column->addLayout(buttons);

    m_field->onChanged = [this](qreal saturation, qreal value) {
        QColor next = QColor::fromHsvF(static_cast<float>(m_hueValue), static_cast<float>(saturation),
                                       static_cast<float>(value));
        next.setAlpha(m_color.alpha());
        setColor(next, false);
    };
    m_hue->onChanged = [this](qreal position) {
        m_hueValue = std::min(position, 0.9999);
        float saturation = 0;
        float value = 0;
        float alpha = 0;
        float ignored = 0;
        m_color.getHsvF(&ignored, &saturation, &value, &alpha);
        QColor next = QColor::fromHsvF(static_cast<float>(m_hueValue), saturation, value, alpha);
        setColor(next, false);
    };
    m_alpha->onChanged = [this](qreal position) {
        QColor next = m_color;
        next.setAlphaF(static_cast<float>(position));
        setColor(next, false);
    };
    connect(m_hex, &QLineEdit::textEdited, this, [this](const QString &text) {
        if (const auto parsed = colorFromText(text)) {
            setColor(*parsed, true);
        }
    });
    connect(m_hex, &QLineEdit::editingFinished, this, [this] { m_hex->setText(colorToText(m_color)); });
    connect(m_hex, &QLineEdit::returnPressed, this, [this] { finish(true); });
    connect(m_cancel, &QAbstractButton::clicked, this, [this] { finish(false); });
    connect(m_apply, &QAbstractButton::clicked, this, [this] { finish(true); });
    connect(this, &Overlay::closed, this, [this] {
        auto done = std::move(m_done);
        m_done = nullptr;
        m_live = nullptr;
        if (done) {
            done(m_applied ? std::optional<QColor>(m_color) : std::nullopt);
        }
    });
}

void ColorPicker::pick(const QString &title, const QColor &initial, std::function<void(const QColor &)> live,
                       std::function<void(std::optional<QColor>)> done)
{
    m_title->setText(title);
    m_original = initial;
    m_live = nullptr;
    m_done = std::move(done);
    m_applied = false;
    float hue = initial.hsvHueF();
    m_hueValue = hue < 0 ? 0 : hue;
    setColor(initial, false);
    m_live = std::move(live);
    open();
    m_hex->setFocus(Qt::OtherFocusReason);
    m_hex->selectAll();
}

void ColorPicker::setColor(const QColor &color, bool fromText)
{
    m_color = color;
    const float hue = color.hsvHueF();
    if (hue >= 0 && color.hsvSaturationF() > 0) {
        m_hueValue = hue;
    }
    m_field->set(m_hueValue, color.hsvSaturationF(), color.valueF());
    m_hue->set(m_hueValue, color);
    m_alpha->set(color.alphaF(), color);
    m_compare->set(m_original, color);
    if (!fromText) {
        m_hex->setText(colorToText(color));
    }
    if (m_live) {
        m_live(color);
    }
}

void ColorPicker::finish(bool applied)
{
    m_applied = applied;
    close();
}

void ColorPicker::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        finish(true);
        return;
    }
    Overlay::keyPressEvent(event);
}

QColor ColorPicker::dimColor() const
{
    return withAlpha(currentTheme().settings.modalDim, 0.35);
}

ShadowSpec ColorPicker::panelShadow() const
{
    return currentTheme().settings.modalShadow;
}

QRect ColorPicker::panelGeometry() const
{
    const int width = std::min(kPanelWidth, parentWidget()->width() - 32);
    QLayout *layout = m_panel->layout();
    const int height = layout->hasHeightForWidth() ? layout->totalHeightForWidth(width) : layout->totalSizeHint().height();
    const int rise = qRound(16 * (1.0 - presence()));
    return QRect((parentWidget()->width() - width) / 2, std::max(8, (parentWidget()->height() - height) / 2 + rise),
                 width, height);
}

void ColorPicker::paintPanel(QPainter &painter, const QRectF &rect)
{
    const SettingsPalette &s = currentTheme().settings;
    const QPainterPath path = roundedPath(rect.adjusted(0.5, 0.5, -0.5, -0.5), 20);
    painter.fillPath(path, s.modalFill);
    painter.setPen(QPen(s.modalBorder, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

}  // namespace sky
