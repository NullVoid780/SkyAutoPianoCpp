#include "ui/editor_window/editor_setup.hpp"

#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <functional>

#include "model/key_table.hpp"
#include "ui/core/hover_tracker.hpp"
#include "ui/editor_window/key_colors.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/settings_window/action_button.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/step_dots.hpp"

namespace sky {

namespace {

constexpr int kPanelWidth = 780;
constexpr int kPadding = 32;
constexpr int kCardHeight = 262;
constexpr int kCardGap = 16;
constexpr qreal kCardRadius = 16;
constexpr qreal kPreviewHeight = 160;
constexpr qreal kPreviewInset = 12;
constexpr qreal kBadge = 24;
constexpr int kKeys = 15;
constexpr int kKeysPerRow = 5;

using Painter = std::function<void(QPainter &, const QRectF &)>;

QLabel *text(const QString &value, const char *objectName, QWidget *parent, bool wrap = false)
{
    auto *label = new QLabel(value, parent);
    label->setObjectName(QString::fromLatin1(objectName));
    label->setWordWrap(wrap);
    return label;
}

QString keyLabel(int key)
{
    const std::string_view name = kAutoKeys[static_cast<std::size_t>(key)];
    return QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size())).toUpper();
}

void paintNoteTile(QPainter &painter, const QRectF &rect, int key, bool colorful, bool letter)
{
    const QColor color = keyNoteColor(key, colorful);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    const qreal radius = rect.width() * 0.24;
    painter.drawRoundedRect(rect, radius, radius);
    if (letter) {
        painter.setPen(keyInkColor(color));
        painter.setFont(FontLoader::font(std::max(8, static_cast<int>(rect.height() * 0.5)), QFont::Bold));
        painter.drawText(rect, Qt::AlignCenter, keyLabel(key));
    }
}

void paintSimplePreview(QPainter &painter, const QRectF &area, bool colorful)
{
    const Theme &theme = currentTheme();
    const QColor accent = keyAccentColor();
    static const std::array<std::vector<int>, 6> tiles = {
        {{4, 7, 10}, {}, {2, 11}, {6}, {0, 9, 13}, {}}};
    constexpr int columns = 3;
    constexpr qreal gap = 10;
    constexpr qreal caption = 12;
    const qreal width = (area.width() - gap * (columns - 1)) / columns;
    const qreal height = (area.height() - gap - caption * 2) / 2;
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        const int column = static_cast<int>(i) % columns;
        const int row = static_cast<int>(i) / columns;
        const QRectF tile(area.left() + column * (width + gap), area.top() + row * (height + gap + caption), width,
                          height);
        painter.save();
        painter.setOpacity(tiles[i].empty() ? 0.45 : 1.0);
        painter.fillPath(roundedPath(tile, 7), theme.gridColor);
        painter.restore();
        if (i == 0) {
            painter.setPen(QPen(accent, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(roundedPath(tile.adjusted(1, 1, -1, -1), 6));
        }
        const QRectF inner = tile.adjusted(7, 6, -7, -6);
        const qreal slotW = inner.width() / kKeysPerRow;
        const qreal slotH = inner.height() / 3;
        for (int key = 0; key < kKeys; ++key) {
            const QPointF center(inner.left() + (key % kKeysPerRow + 0.5) * slotW,
                                 inner.top() + (key / kKeysPerRow + 0.5) * slotH);
            if (std::find(tiles[i].begin(), tiles[i].end(), key) != tiles[i].end()) {
                const qreal size = std::min(slotW, slotH) * 0.86;
                paintNoteTile(painter, QRectF(center.x() - size / 2, center.y() - size / 2, size, size), key, colorful,
                              size >= 12);
            } else {
                painter.setPen(Qt::NoPen);
                painter.setBrush(theme.dotColor);
                painter.drawEllipse(center, 1.6, 1.6);
            }
        }
        painter.setPen(blend(theme.background, theme.fontColor, 0.6));
        painter.setFont(FontLoader::font(9, i % columns == 0 ? QFont::Bold : QFont::Normal));
        painter.drawText(QRectF(tile.left(), tile.bottom() + 1, tile.width(), caption), Qt::AlignLeft | Qt::AlignVCenter,
                         i % columns == 0 ? QStringLiteral("Bar %1").arg(i / columns + 1)
                                          : QStringLiteral("0:0%1.%2").arg(i / 2).arg(i * 25 % 100, 2, 10, QLatin1Char('0')));
    }
}

void paintAdvancedPreview(QPainter &painter, const QRectF &area, bool colorful)
{
    const Theme &theme = currentTheme();
    const QColor accent = keyAccentColor();
    constexpr int lowestKey = 4;
    constexpr int rows = 7;
    constexpr int columns = 12;
    constexpr int beatsPerBar = 4;
    constexpr int cursor = 6;
    constexpr qreal ruler = 16;
    constexpr qreal gutter = 24;
    constexpr qreal fade = 14;
    const QRectF roll(area.left() + gutter, area.top() + ruler, area.width() - gutter, area.height() - ruler);
    const qreal rowH = roll.height() / rows;
    const qreal colW = roll.width() / columns;
    const auto rowTop = [&](int key) { return roll.top() + (lowestKey + rows - 1 - key) * rowH; };

    painter.setFont(FontLoader::font(9, QFont::Medium));
    for (int column = 0; column < columns; column += beatsPerBar) {
        const qreal x = roll.left() + column * colW;
        painter.setPen(QPen(blend(theme.windowEditor, theme.fontColor, 0.35), 1));
        painter.drawLine(QPointF(x + 0.5, roll.top() - 5), QPointF(x + 0.5, roll.top() - 1));
        painter.setPen(blend(theme.windowEditor, theme.fontColor, 0.7));
        painter.drawText(QRectF(x + 3, area.top(), colW * beatsPerBar, ruler - 4), Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("0:0%1").arg(column / beatsPerBar));
    }
    painter.fillRect(QRectF(roll.left() + cursor * colW, roll.top() - 3, colW, 3), accent);

    painter.save();
    painter.setClipPath(roundedPath(roll, 7));
    painter.fillRect(roll, theme.gridColor);
    for (int column = beatsPerBar; column < beatsPerBar * 2; ++column) {
        painter.fillRect(QRectF(roll.left() + column * colW, roll.top(), colW, roll.height()),
                         blend(theme.gridColor, theme.fontColor, 0.035));
    }
    painter.fillRect(QRectF(roll.left() + cursor * colW, roll.top(), colW, roll.height()), withAlpha(accent, 0.12));
    for (int row = 1; row < rows; ++row) {
        const qreal y = std::round(roll.top() + row * rowH) + 0.5;
        painter.setPen(QPen(blend(theme.gridColor, theme.fontColor, 0.07), 1));
        painter.drawLine(QPointF(roll.left(), y), QPointF(roll.right(), y));
    }
    for (int column = 1; column < columns; ++column) {
        const qreal x = std::round(roll.left() + column * colW) + 0.5;
        painter.setPen(QPen(blend(theme.gridColor, theme.fontColor, column % beatsPerBar == 0 ? 0.3 : 0.08), 1));
        painter.drawLine(QPointF(x, roll.top()), QPointF(x, roll.bottom()));
    }

    static const std::array<std::array<int, 2>, 16> notes = {{{0, 4},
                                                             {0, 7},
                                                             {1, 5},
                                                             {2, 6},
                                                             {3, 8},
                                                             {4, 7},
                                                             {4, 4},
                                                             {5, 9},
                                                             {6, 10},
                                                             {6, 6},
                                                             {7, 8},
                                                             {8, 7},
                                                             {8, 5},
                                                             {9, 6},
                                                             {10, 9},
                                                             {11, 8}}};
    const qreal size = std::min(rowH, colW) * 0.84;
    for (const auto &[column, key] : notes) {
        const QPointF center(roll.left() + (column + 0.5) * colW, rowTop(key) + rowH / 2);
        paintNoteTile(painter, QRectF(center.x() - size / 2, center.y() - size / 2, size, size), key, colorful,
                      size >= 12);
    }
    painter.setPen(QPen(accent, 1.6));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(roll.left() + cursor * colW + 1, roll.top() + 1, colW - 2, roll.height() - 2), 4, 4);

    for (const bool top : {true, false}) {
        const QRectF band(roll.left(), top ? roll.top() : roll.bottom() - fade, roll.width(), fade);
        QLinearGradient gradient(band.topLeft(), band.bottomLeft());
        const QColor solid = withAlpha(theme.windowEditor, 0.85);
        const QColor clear = withAlpha(theme.windowEditor, 0.0);
        gradient.setColorAt(0, top ? solid : clear);
        gradient.setColorAt(1, top ? clear : solid);
        painter.fillRect(band, gradient);
    }
    painter.restore();

    const qreal cap = std::min(rowH - 3, gutter - 6);
    painter.setFont(FontLoader::font(std::max(8, static_cast<int>(cap * 0.55)), QFont::Bold));
    for (int key = lowestKey; key < lowestKey + rows; ++key) {
        const QColor color = keyNoteColor(key, colorful);
        const bool active = std::any_of(notes.begin(), notes.end(), [key](const std::array<int, 2> &note) {
            return note[0] == cursor && note[1] == key;
        });
        const QRectF rect(area.left() + gutter - cap - 5, rowTop(key) + (rowH - cap) / 2, cap, cap);
        painter.setPen(active ? Qt::NoPen : QPen(withAlpha(color, 0.7), 1));
        painter.setBrush(active ? color : withAlpha(color, 0.12));
        painter.drawRoundedRect(rect, 3, 3);
        painter.setPen(active ? keyInkColor(color) : blend(theme.windowEditor, theme.fontColor, 0.85));
        painter.drawText(rect, Qt::AlignCenter, keyLabel(key));
    }
}

void paintColorPreview(QPainter &painter, const QRectF &area, bool colorful)
{
    constexpr qreal gap = 8;
    const qreal size = std::min((area.width() - gap * (kKeysPerRow - 1)) / kKeysPerRow, (area.height() - gap * 2) / 3);
    const qreal width = size * kKeysPerRow + gap * (kKeysPerRow - 1);
    const qreal left = area.center().x() - width / 2;
    const qreal top = area.center().y() - (size * 3 + gap * 2) / 2;
    for (int key = 0; key < kKeys; ++key) {
        const QRectF rect(left + (key % kKeysPerRow) * (size + gap), top + (key / kKeysPerRow) * (size + gap), size, size);
        paintNoteTile(painter, rect, key, colorful, true);
    }
}

}  // namespace

class PreviewCard final : public QAbstractButton {
public:
    PreviewCard(const QString &title, const QString &description, Painter preview, QWidget *parent)
        : QAbstractButton(parent),
          m_title(title),
          m_description(description),
          m_preview(std::move(preview)),
          m_hover(new HoverTracker(this, kHoverFastMs))
    {
        setCheckable(true);
        setAutoExclusive(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setFixedHeight(kCardHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAccessibleName(title);
        connect(m_hover, &HoverTracker::changed, this, [this] { update(); });
        connect(this, &QAbstractButton::toggled, this, [this] { update(); });
    }

    std::function<void()> onDoubleClick;

    QSize sizeHint() const override { return {320, kCardHeight}; }

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        QAbstractButton::mouseDoubleClickEvent(event);
        setChecked(true);
        if (onDoubleClick) {
            onDoubleClick();
        }
    }

    void paintEvent(QPaintEvent *) override
    {
        const Theme &theme = currentTheme();
        const SettingsPalette &s = theme.settings;
        QPainter painter(this);
        preparePainter(painter);
        const QRectF frame = QRectF(rect()).adjusted(1, 1, -1, -1);
        const QPainterPath path = roundedPath(frame, kCardRadius);
        const qreal hover = m_hover->progress();
        painter.fillPath(path, isChecked() ? blend(s.cardFill, theme.hoverColor, 0.07) : s.cardFill);
        painter.setPen(QPen(isChecked() ? theme.hoverColor : blend(s.cardBorder, s.cardHoverBorder, hover),
                            isChecked() ? 2 : 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);

        const QRectF preview(frame.left() + kPreviewInset, frame.top() + kPreviewInset,
                             frame.width() - kPreviewInset * 2, kPreviewHeight);
        painter.fillPath(roundedPath(preview, 10), theme.windowEditor);
        painter.setPen(QPen(withAlpha(theme.fontColor, 0.08), 1));
        painter.drawPath(roundedPath(preview, 10));
        painter.save();
        painter.setClipPath(roundedPath(preview, 10));
        m_preview(painter, preview.adjusted(14, 14, -14, -14));
        painter.restore();

        if (isChecked()) {
            const QRectF badge(preview.right() - kBadge - 8, preview.top() + 8, kBadge, kBadge);
            painter.setPen(Qt::NoPen);
            painter.setBrush(theme.hoverColor);
            painter.drawEllipse(badge);
            painter.drawPixmap(QPointF(badge.center().x() - 7, badge.center().y() - 7),
                               IconCache::pixmap(QStringLiteral("check-lg"), 14, keyInkColor(theme.hoverColor),
                                                 devicePixelRatioF()));
        }

        const qreal left = preview.left() + 4;
        const qreal width = preview.width() - 8;
        const QFont titleFont = FontLoader::font(16, QFont::DemiBold);
        const QFont bodyFont = FontLoader::font(12, QFont::Normal);
        painter.setFont(titleFont);
        painter.setPen(theme.fontColor);
        painter.drawText(QRectF(left, preview.bottom() + 14, width, 22), Qt::AlignLeft | Qt::AlignVCenter, m_title);
        painter.setFont(bodyFont);
        painter.setPen(s.cardDescText);
        painter.drawText(QRectF(left, preview.bottom() + 38, width, frame.bottom() - preview.bottom() - 44),
                         Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_description);
        if (hasFocus()) {
            paintFocusRing(painter, frame.adjusted(3, 3, -3, -3), kCardRadius - 2, withAlpha(theme.hoverColor, 0.6));
        }
    }

private:
    QString m_title;
    QString m_description;
    Painter m_preview;
    HoverTracker *m_hover;
};

EditorSetup::EditorSetup(QWidget *window)
    : Overlay(window),
      m_panel(new QWidget(this)),
      m_stack(new QStackedWidget(m_panel)),
      m_dots(new StepDots(m_panel)),
      m_back(new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("arrow-left"), QStringLiteral("Back"), m_panel)),
      m_next(new ActionButton(ActionButton::Kind::Primary, QString(), QStringLiteral("Next"), m_panel))
{
    m_simple = new PreviewCard(QStringLiteral("Simple"),
                               QStringLiteral("One tile for each moment, laid out like Sky's keyboard. Easy to read and "
                                              "great for quick fixes."),
                               [this](QPainter &painter, const QRectF &area) {
                                   paintSimplePreview(painter, area, m_colorful->isChecked());
                               },
                               nullptr);
    m_advanced = new PreviewCard(QStringLiteral("Advanced"),
                                 QStringLiteral("A timeline with every key on its own row. Best for precise timing and "
                                                "long songs."),
                                 [this](QPainter &painter, const QRectF &area) {
                                     paintAdvancedPreview(painter, area, m_colorful->isChecked());
                                 },
                                 nullptr);
    m_colorful = new PreviewCard(QStringLiteral("Colorful"),
                                 QStringLiteral("Each keyboard row has its own color, so you can tell notes apart at a "
                                                "glance."),
                                 [](QPainter &painter, const QRectF &area) { paintColorPreview(painter, area, true); },
                                 nullptr);
    m_plain = new PreviewCard(QStringLiteral("Plain"),
                              QStringLiteral("Every note in one calm color. Clean and easy on the eyes, the letter tells "
                                             "you the key."),
                              [](QPainter &painter, const QRectF &area) { paintColorPreview(painter, area, false); },
                              nullptr);
    m_stack->addWidget(buildStep(QStringLiteral("STEP 1 OF 2"), QStringLiteral("How do you want to edit?"),
                                 QStringLiteral("Pick the view that feels right. You can switch any time with the "
                                                "Simple / Advanced buttons at the top."),
                                 m_simple, m_advanced));
    m_stack->addWidget(buildStep(QStringLiteral("STEP 2 OF 2"), QStringLiteral("How should notes look?"),
                                 QStringLiteral("This changes the note colors in both views and on the key pad."),
                                 m_colorful, m_plain));
    for (PreviewCard *card : {m_simple, m_advanced, m_colorful, m_plain}) {
        card->onDoubleClick = [this] { next(); };
    }
    connect(m_colorful, &QAbstractButton::toggled, this, [this] {
        m_simple->update();
        m_advanced->update();
    });

    auto *nav = new QHBoxLayout();
    nav->setSpacing(12);
    nav->addWidget(m_back);
    nav->addStretch();
    nav->addWidget(m_dots, 0, Qt::AlignVCenter);
    nav->addStretch();
    nav->addWidget(m_next);

    auto *column = new QVBoxLayout(m_panel);
    column->setContentsMargins(kPadding, kPadding - 4, kPadding, kPadding - 8);
    column->setSpacing(0);
    column->addWidget(m_stack, 1);
    column->addSpacing(24);
    column->addLayout(nav);

    connect(m_next, &QAbstractButton::clicked, this, &EditorSetup::next);
    connect(m_back, &QAbstractButton::clicked, this, &EditorSetup::back);
    connect(this, &Overlay::closed, this, [this] {
        if (!m_finished) {
            m_finished = true;
            emit chosen(m_advanced->isChecked(), m_colorful->isChecked());
        }
    });
}

QWidget *EditorSetup::buildStep(const QString &step, const QString &title, const QString &body, PreviewCard *first,
                                PreviewCard *second)
{
    auto *widget = new QWidget(m_stack);
    auto *column = new QVBoxLayout(widget);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(text(step, "EditorCaption", widget));
    column->addSpacing(8);
    column->addWidget(text(title, "QuickStartTitle", widget));
    column->addSpacing(6);
    column->addWidget(text(body, "QuickStartBody", widget, true));
    column->addSpacing(22);
    auto *cards = new QWidget(widget);
    auto *row = new QHBoxLayout(cards);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(kCardGap);
    first->setParent(cards);
    second->setParent(cards);
    row->addWidget(first);
    row->addWidget(second);
    column->addWidget(cards);
    return widget;
}

void EditorSetup::start(bool advanced, bool colorful)
{
    m_finished = false;
    (advanced ? m_advanced : m_simple)->setChecked(true);
    (colorful ? m_colorful : m_plain)->setChecked(true);
    showStep(0);
    open();
    (advanced ? m_advanced : m_simple)->setFocus(Qt::OtherFocusReason);
}

void EditorSetup::showStep(int index)
{
    m_step = std::clamp(index, 0, m_stack->count() - 1);
    m_stack->setCurrentIndex(m_step);
    const bool last = m_step == m_stack->count() - 1;
    m_back->setVisible(m_step > 0);
    m_next->setText(last ? QStringLiteral("Start editing") : QStringLiteral("Next"));
    m_next->updateGeometry();
    m_dots->set(m_stack->count(), m_step);
    relayout();
    update();
}

void EditorSetup::next()
{
    if (m_step < m_stack->count() - 1) {
        showStep(m_step + 1);
        (m_colorful->isChecked() ? m_colorful : m_plain)->setFocus(Qt::OtherFocusReason);
        return;
    }
    finish();
}

void EditorSetup::back()
{
    showStep(m_step - 1);
}

void EditorSetup::finish()
{
    if (!m_finished) {
        m_finished = true;
        emit chosen(m_advanced->isChecked(), m_colorful->isChecked());
    }
    close();
}

void EditorSetup::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        next();
        return;
    case Qt::Key_Left:
    case Qt::Key_Right: {
        const bool right = event->key() == Qt::Key_Right;
        PreviewCard *card = m_step == 0 ? (right ? m_advanced : m_simple) : (right ? m_plain : m_colorful);
        card->setChecked(true);
        card->setFocus(Qt::OtherFocusReason);
        return;
    }
    case Qt::Key_Backspace:
        if (m_step > 0) {
            back();
        }
        return;
    default:
        Overlay::keyPressEvent(event);
    }
}

QColor EditorSetup::dimColor() const
{
    return currentTheme().settings.modalDim;
}

ShadowSpec EditorSetup::panelShadow() const
{
    return currentTheme().settings.modalShadow;
}

QRect EditorSetup::panelGeometry() const
{
    const int width = std::min(kPanelWidth, parentWidget()->width() - 32);
    const int wanted = m_panel->layout()->totalSizeHint().height();
    const int height = std::min(wanted, parentWidget()->height() - 24);
    const int rise = qRound(16 * (1.0 - presence()));
    return QRect((parentWidget()->width() - width) / 2, std::max(12, (parentWidget()->height() - height) / 2 + rise),
                 width, height);
}

void EditorSetup::paintPanel(QPainter &painter, const QRectF &rect)
{
    const SettingsPalette &s = currentTheme().settings;
    const QPainterPath path = roundedPath(rect.adjusted(0.5, 0.5, -0.5, -0.5), 20);
    painter.fillPath(path, s.modalFill);
    painter.setPen(QPen(s.modalBorder, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

}  // namespace sky
