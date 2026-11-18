#include "ui/settings_window/theme_builder.hpp"

#include <QBoxLayout>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>

#include <algorithm>

#include "app/theme_controller.hpp"
#include "services/settings/theme_library_service.hpp"
#include "ui/core/hover_tracker.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/settings_window/action_button.hpp"
#include "ui/settings_window/color_picker.hpp"
#include "ui/settings_window/settings_panel.hpp"
#include "ui/settings_window/theme_preview.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/confirm_popup.hpp"
#include "ui/widgets/icon_button.hpp"
#include "ui/widgets/select_box.hpp"

namespace sky {

namespace {

constexpr int kPanelPadding = 20;
constexpr int kSectionGap = 10;
constexpr int kRowHeight = 44;
constexpr int kSwatchWidth = 38;
constexpr int kSwatchHeight = 26;
constexpr int kResetBox = 26;
constexpr int kPreviewDelayMs = 90;
constexpr int kSectionTile = 36;
constexpr int kToggleHeight = 52;
constexpr int kStickyWidth = 340;
constexpr int kNarrowStickyWidth = 280;
constexpr int kCompactWidth = 250;
constexpr int kWideFromWidth = 980;
constexpr int kTwoColumnsFrom = 600;
constexpr int kStickyGap = 20;
constexpr int kStickyInset = 16;
constexpr int kStickyPadding = 14;
constexpr int kCompactBelowWidth = 640;
constexpr qreal kPreviewAspect = 0.56;
constexpr int kTabsHeight = 26;

QLabel *label(const QString &text, const char *objectName, QWidget *parent)
{
    auto *result = new QLabel(text, parent);
    result->setObjectName(QString::fromLatin1(objectName));
    return result;
}

QString modeName(AppTheme mode)
{
    return mode == AppTheme::Dark ? QStringLiteral("dark") : QStringLiteral("light");
}

QStringList slotIds(const ThemeSection &section)
{
    QStringList ids;
    for (const ThemeSlot &entry : section.entries) {
        ids << entry.id;
    }
    return ids;
}

QStringList targetIds(const QuickColor &quick)
{
    QStringList ids;
    for (const auto &target : quick.targets) {
        ids << target.first;
    }
    return ids;
}

class PreviewTabs final : public QWidget {
public:
    explicit PreviewTabs(QWidget *parent) : QWidget(parent)
    {
        setFixedHeight(kTabsHeight);
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true);
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
    }

    std::function<void(PreviewScene)> onPicked;

    void setCurrent(PreviewScene scene)
    {
        if (scene != m_current) {
            m_current = scene;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Theme &theme = currentTheme();
        QPainter painter(this);
        preparePainter(painter);
        const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        paintPill(painter, frame, theme.settings.subtleFill);
        const QFont font = FontLoader::font(11, QFont::DemiBold);
        painter.setFont(font);
        for (int i = 0; i < kCount; ++i) {
            const QRectF segment = segmentRect(i);
            const bool current = static_cast<int>(m_current) == i;
            if (current) {
                paintPill(painter, segment.adjusted(2, 2, -2, -2), theme.hoverColor);
            } else if (i == m_hover) {
                paintPill(painter, segment.adjusted(2, 2, -2, -2), theme.settings.subtleHoverFill);
            }
            const QColor ink = current ? (theme.hoverColor.lightnessF() > 0.7 ? theme.accent.ink900 : theme.accent.white)
                                       : theme.fontColor;
            painter.setPen(ink);
            painter.drawText(segment, Qt::AlignCenter, label(i));
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        const int hover = indexAt(event->position());
        if (hover != m_hover) {
            m_hover = hover;
            update();
        }
    }

    void leaveEvent(QEvent *) override
    {
        m_hover = -1;
        update();
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        const int index = indexAt(event->position());
        if (index >= 0 && onPicked) {
            onPicked(static_cast<PreviewScene>(index));
        }
    }

private:
    static constexpr int kCount = 5;

    static QString label(int index)
    {
        static const QString labels[kCount] = {QStringLiteral("Songs"), QStringLiteral("Menus"), QStringLiteral("Alerts"),
                                               QStringLiteral("Settings"), QStringLiteral("Editor")};
        return labels[index];
    }

    QRectF segmentRect(int index) const
    {
        const qreal width = static_cast<qreal>(this->width()) / kCount;
        return QRectF(index * width, 0, width, height());
    }

    int indexAt(const QPointF &point) const
    {
        for (int i = 0; i < kCount; ++i) {
            if (segmentRect(i).contains(point)) {
                return i;
            }
        }
        return -1;
    }

    PreviewScene m_current = PreviewScene::Main;
    int m_hover = -1;
};

}  // namespace

class ColorRow final : public QAbstractButton {
public:
    ColorRow(const QString &text, const QString &hint, QWidget *parent)
        : QAbstractButton(parent),
          m_label(text),
          m_hover(new HoverTracker(this, kHoverFastMs)),
          m_reset(new IconButton(QStringLiteral("arrow-clockwise"), 13, QSize(kResetBox, kResetBox),
                                 IconButtonStyle{[] { return currentTheme().settings.cardDescText; },
                                                 [] { return currentTheme().hoverColor; },
                                                 [] { return currentTheme().accent.transparent; }, {}, 7},
                                 this))
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setFixedHeight(kRowHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAccessibleName(text);
        if (!hint.isEmpty()) {
            setToolTip(hint);
        }
        m_reset->setToolTip(QStringLiteral("Undo the change to this color"));
        m_reset->setAccessibleName(QStringLiteral("Undo %1").arg(text));
        m_reset->hide();
        connect(m_hover, &HoverTracker::changed, this, [this] { update(); });
        connect(m_reset, &QAbstractButton::clicked, this, [this] {
            if (onReset) {
                onReset();
            }
        });
    }

    std::function<void()> onReset;
    std::function<void(bool)> onHover;

    void setState(const QColor &color, bool overridden)
    {
        m_color = color;
        m_overridden = overridden;
        m_reset->setVisible(overridden);
        update();
    }

    QSize sizeHint() const override { return {260, kRowHeight}; }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QAbstractButton::resizeEvent(event);
        m_reset->move(width() - 8 - kResetBox, (height() - kResetBox) / 2);
    }

    void focusInEvent(QFocusEvent *event) override
    {
        QAbstractButton::focusInEvent(event);
        m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
        update();
    }

    void focusOutEvent(QFocusEvent *event) override
    {
        QAbstractButton::focusOutEvent(event);
        m_keyboardFocus = false;
        update();
    }

    void enterEvent(QEnterEvent *event) override
    {
        QAbstractButton::enterEvent(event);
        if (onHover) {
            onHover(true);
        }
    }

    void leaveEvent(QEvent *event) override
    {
        QAbstractButton::leaveEvent(event);
        if (onHover) {
            onHover(false);
        }
    }

    void paintEvent(QPaintEvent *) override
    {
        const Theme &theme = currentTheme();
        QPainter painter(this);
        preparePainter(painter);
        const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal hover = m_hover->progress();
        if (hover > 0) {
            painter.fillPath(roundedPath(frame, 10), withAlpha(theme.settings.subtleHoverFill, hover));
        }
        const QRectF swatch(8, (height() - kSwatchHeight) / 2.0, kSwatchWidth, kSwatchHeight);
        paintColorSwatch(painter, swatch, m_color, 7);

        const qreal textLeft = swatch.right() + 12;
        const qreal right = width() - 8 - (m_overridden ? kResetBox + 6 : 0);
        const QFont hexFont = FontLoader::monospace(12);
        const QString hex = colorToText(m_color).toUpper();
        const qreal hexWidth = QFontMetricsF(hexFont).horizontalAdvance(hex);
        painter.setFont(hexFont);
        painter.setPen(theme.settings.cardDescText);
        painter.drawText(QRectF(right - hexWidth, 0, hexWidth, height()), Qt::AlignVCenter | Qt::AlignRight, hex);

        const QFont nameFont = FontLoader::font(14, m_overridden ? QFont::Medium : QFont::Normal);
        const qreal nameWidth = std::max<qreal>(0, right - hexWidth - 10 - textLeft);
        painter.setFont(nameFont);
        painter.setPen(theme.fontColor);
        painter.drawText(QRectF(textLeft, 0, nameWidth, height()), Qt::AlignVCenter | Qt::AlignLeft,
                         QFontMetricsF(nameFont).elidedText(m_label, Qt::ElideRight, nameWidth));
        if (m_overridden) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(theme.hoverColor);
            painter.drawEllipse(QPointF(swatch.right() + 1, swatch.top() + 1), 3.5, 3.5);
        }
        if (m_keyboardFocus && hasFocus()) {
            paintFocusRing(painter, frame.adjusted(1, 1, -1, -1), 9, theme.hoverColor);
        }
    }

private:
    QString m_label;
    HoverTracker *m_hover;
    IconButton *m_reset;
    QColor m_color;
    bool m_overridden = false;
    bool m_keyboardFocus = false;
};

class StickyPreview final : public QWidget {
public:
    explicit StickyPreview(QWidget *parent)
        : QWidget(parent),
          m_caption(label(QString(), "SettingsRowDescription", this)),
          m_tabs(new PreviewTabs(this)),
          m_preview(new ThemePreview(this))
    {
        m_caption->setWordWrap(true);
        m_preview->setModeBadges(false);
        auto *legend = label(QStringLiteral("Left half: light mode  ·  Right half: dark mode"), "ColorPickerCaption", this);
        legend->setAlignment(Qt::AlignCenter);
        auto *column = new QVBoxLayout(this);
        column->setContentsMargins(kStickyPadding, kStickyPadding - 2, kStickyPadding, kStickyPadding);
        column->setSpacing(2);
        column->addWidget(label(QStringLiteral("Live Preview"), "ThemeSectionTitle", this));
        column->addWidget(m_caption);
        column->addSpacing(8);
        column->addWidget(m_tabs);
        column->addSpacing(8);
        column->addWidget(m_preview);
        column->addSpacing(6);
        column->addWidget(legend);
        setPreviewWidth(kStickyWidth);
    }

    ThemePreview *preview() const { return m_preview; }
    PreviewTabs *tabs() const { return m_tabs; }

    void setPreviewWidth(int width)
    {
        const int inner = width - kStickyPadding * 2;
        m_preview->setFixedSize(inner, qRound(inner * kPreviewAspect));
        setFixedWidth(width);
        adjustSize();
    }

    void setCaption(AppTheme mode, const QString &focus)
    {
        const QString base = QStringLiteral("Editing %1 mode").arg(modeName(mode));
        m_caption->setText(focus.isEmpty() ? base : QStringLiteral("%1 · %2").arg(base, focus));
        adjustSize();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const SettingsPalette &s = currentTheme().settings;
        QPainter painter(this);
        preparePainter(painter);
        const QPainterPath path = roundedPath(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 16);
        painter.fillPath(path, s.cardFill);
        painter.setPen(QPen(s.cardBorder, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

private:
    QLabel *m_caption;
    PreviewTabs *m_tabs;
    ThemePreview *m_preview;
};

class SectionToggle final : public QAbstractButton {
public:
    SectionToggle(const ThemeSection &section, QWidget *parent)
        : QAbstractButton(parent), m_section(section), m_hover(new HoverTracker(this, kHoverFastMs))
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setFixedHeight(kToggleHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAccessibleName(section.title);
        connect(m_hover, &HoverTracker::changed, this, [this] { update(); });
    }

    void setExpanded(bool expanded)
    {
        m_expanded = expanded;
        update();
    }

    bool isExpanded() const { return m_expanded; }

    void setChangedCount(int count)
    {
        if (m_changed != count) {
            m_changed = count;
            update();
        }
    }

protected:
    void focusInEvent(QFocusEvent *event) override
    {
        QAbstractButton::focusInEvent(event);
        m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
        update();
    }

    void focusOutEvent(QFocusEvent *event) override
    {
        QAbstractButton::focusOutEvent(event);
        m_keyboardFocus = false;
        update();
    }

    void paintEvent(QPaintEvent *) override
    {
        const Theme &theme = currentTheme();
        QPainter painter(this);
        preparePainter(painter);
        const qreal dpr = devicePixelRatioF();
        const QRectF tile(0, (height() - kSectionTile) / 2.0, kSectionTile, kSectionTile);
        painter.fillPath(roundedPath(tile, 10), withAlpha(theme.hoverColor, 0.14 + 0.08 * m_hover->progress()));
        painter.drawPixmap(QPointF(tile.center().x() - 9, tile.center().y() - 9),
                           IconCache::pixmap(m_section.iconId, 18, theme.hoverColor, dpr));

        const QRectF chevron(width() - 22, (height() - 16) / 2.0, 16, 16);
        painter.save();
        painter.translate(chevron.center());
        painter.rotate(m_expanded ? 180 : 0);
        painter.drawPixmap(QPointF(-8, -8), IconCache::pixmap(QStringLiteral("chevron-down"), 16,
                                                               blend(theme.settings.cardDescText, theme.fontColor,
                                                                     m_hover->progress()),
                                                               dpr));
        painter.restore();

        qreal right = chevron.left() - 12;
        if (m_changed > 0) {
            const QString text = m_changed == 1 ? QStringLiteral("1 changed") : QStringLiteral("%1 changed").arg(m_changed);
            const QFont font = FontLoader::font(12, QFont::DemiBold);
            const qreal width = QFontMetricsF(font).horizontalAdvance(text) + 18;
            const QRectF pill(right - width, (height() - 22) / 2.0, width, 22);
            paintPill(painter, pill, withAlpha(theme.hoverColor, 0.16));
            painter.setFont(font);
            painter.setPen(theme.hoverColor);
            painter.drawText(pill, Qt::AlignCenter, text);
            right = pill.left() - 12;
        }

        const qreal left = tile.right() + 14;
        const qreal textWidth = std::max<qreal>(0, right - left);
        const QFont titleFont = FontLoader::font(16, QFont::DemiBold);
        const QFont descFont = FontLoader::font(13, QFont::Normal);
        painter.setFont(titleFont);
        painter.setPen(theme.fontColor);
        painter.drawText(QRectF(left, height() / 2.0 - 21, textWidth, 22), Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetricsF(titleFont).elidedText(m_section.title, Qt::ElideRight, textWidth));
        painter.setFont(descFont);
        painter.setPen(theme.settings.cardDescText);
        painter.drawText(QRectF(left, height() / 2.0 + 1, textWidth, 20), Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetricsF(descFont).elidedText(m_section.description, Qt::ElideRight, textWidth));

        if (m_keyboardFocus && hasFocus()) {
            paintFocusRing(painter, QRectF(rect()).adjusted(-6, -4, 6, 4), 12, theme.hoverColor);
        }
    }

private:
    const ThemeSection &m_section;
    HoverTracker *m_hover;
    bool m_expanded = false;
    bool m_keyboardFocus = false;
    int m_changed = 0;
};

ThemeBuilder::ThemeBuilder(ThemeController &themes, QWidget *parent)
    : QWidget(parent),
      m_themes(themes),
      m_title(label(QString(), "ThemeBuilderTitle", this)),
      m_name(new QLineEdit(this)),
      m_modeSelect(new SelectBox(this)),
      m_sticky(new StickyPreview(this)),
      m_preview(m_sticky->preview()),
      m_previewSpace(new QWidget(this)),
      m_previewTimer(new QTimer(this))
{
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(kPreviewDelayMs);
    connect(m_previewTimer, &QTimer::timeout, this, [this] {
        if (m_picker && m_picker->isVisible() && !m_picker->isOpen()) {
            m_previewTimer->start();
            return;
        }
        m_themes.preview(m_draft, m_mode);
    });

    auto *back = new IconButton(QStringLiteral("arrow-left"), 18, QSize(36, 36),
                                IconButtonStyle{[] { return currentTheme().fontColor; },
                                                [] { return currentTheme().hoverColor; },
                                                [] { return currentTheme().settings.subtleFill; },
                                                [] { return currentTheme().settings.subtleBorder; }, 10},
                                this);
    back->setToolTip(QStringLiteral("Back to themes"));
    back->setAccessibleName(QStringLiteral("Back to themes"));
    auto *resetButton = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("arrow-clockwise"),
                                         QStringLiteral("Undo changes"), this);
    resetButton->setToolTip(QStringLiteral("Undo every change to the mode you are editing"));
    auto *cancelButton = new ActionButton(ActionButton::Kind::Subtle, QString(), QStringLiteral("Cancel"), this);
    auto *saveButton = new ActionButton(ActionButton::Kind::Primary, QStringLiteral("floppy"), QStringLiteral("Save Theme"), this);

    auto *titleCopy = new QVBoxLayout();
    titleCopy->setSpacing(2);
    titleCopy->addWidget(m_title);
    auto *hint = label(QStringLiteral("Changes show across the app as you make them."), "SettingsHeaderDescription", this);
    hint->setWordWrap(true);
    hint->setMinimumWidth(120);
    titleCopy->addWidget(hint);
    auto *top = new QHBoxLayout();
    top->setSpacing(12);
    top->addWidget(back, 0, Qt::AlignVCenter);
    top->addLayout(titleCopy, 1);
    top->addWidget(resetButton, 0, Qt::AlignVCenter);
    top->addWidget(cancelButton, 0, Qt::AlignVCenter);
    top->addWidget(saveButton, 0, Qt::AlignVCenter);

    m_name->setObjectName(QStringLiteral("ThemeNameField"));
    m_name->setMaxLength(ThemeLibraryService::kNameLimit);
    m_name->setPlaceholderText(QStringLiteral("Theme name"));
    m_name->setAccessibleName(QStringLiteral("Theme name"));
    m_modeSelect->addOption(QStringLiteral("Light mode"), QStringLiteral("Colors used when the app is light"));
    m_modeSelect->addOption(QStringLiteral("Dark mode"), QStringLiteral("Colors used when the app is dark"));
    m_modeSelect->setAccessibleName(QStringLiteral("Editing"));

    auto *details = new SettingsPanel(kPanelPadding, this);
    auto *fields = new QGridLayout();
    fields->setHorizontalSpacing(16);
    fields->setVerticalSpacing(6);
    fields->addWidget(label(QStringLiteral("Name"), "ColorPickerCaption", details), 0, 0);
    fields->addWidget(label(QStringLiteral("Editing"), "ColorPickerCaption", details), 0, 1);
    fields->addWidget(m_name, 1, 0);
    fields->addWidget(m_modeSelect, 1, 1, Qt::AlignLeft);
    fields->setColumnStretch(0, 1);

    auto *quickBox = new QWidget(details);
    auto *quickColumn = new QVBoxLayout(quickBox);
    quickColumn->setContentsMargins(0, 0, 0, 0);
    quickColumn->setSpacing(0);
    m_quickTitle = label(QStringLiteral("Quick Colors"), "ThemeSectionTitle", quickBox);
    quickColumn->addWidget(m_quickTitle);
    quickColumn->addSpacing(2);
    m_quickCaption = label(QString(), "SettingsRowDescription", quickBox);
    m_quickCaption->setWordWrap(true);
    quickColumn->addWidget(m_quickCaption);
    quickColumn->addSpacing(10);
    m_quickHost = new QWidget(quickBox);
    m_quickLayout = new QVBoxLayout(m_quickHost);
    m_quickLayout->setContentsMargins(0, 0, 0, 0);
    m_quickLayout->setSpacing(0);
    quickColumn->addWidget(m_quickHost);
    showQuickColors(PreviewScene::Main);

    auto *detailsColumn = new QVBoxLayout(details);
    detailsColumn->setContentsMargins(0, 0, 0, 0);
    detailsColumn->setSpacing(18);
    detailsColumn->addLayout(fields);
    detailsColumn->addWidget(quickBox);

    auto *controls = new QVBoxLayout();
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setSpacing(0);
    controls->addWidget(details);
    controls->addSpacing(28);
    controls->addWidget(label(QStringLiteral("All Colors"), "ThemeGroupTitle", this));
    controls->addSpacing(2);
    controls->addWidget(label(QStringLiteral("Open a section to fine-tune one part of the app."),
                              "SettingsRowDescription", this));
    controls->addSpacing(12);
    for (const ThemeSection &section : themeSections()) {
        controls->addWidget(addSection(section));
        controls->addSpacing(kSectionGap);
    }
    controls->addStretch();

    m_previewSpace->setFixedWidth(kStickyWidth);
    m_previewSpace->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(kStickyGap);
    body->addLayout(controls, 1);
    body->addWidget(m_previewSpace);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addLayout(top);
    column->addSpacing(20);
    column->addLayout(body);
    m_sticky->hide();
    layoutGrids(2);
    m_sticky->tabs()->onPicked = [this](PreviewScene scene) {
        m_hovered.reset();
        pinFocus(Focus{scene, {}, {}});
    };

    connect(back, &QAbstractButton::clicked, this, &ThemeBuilder::requestClose);
    connect(cancelButton, &QAbstractButton::clicked, this, &ThemeBuilder::requestClose);
    connect(resetButton, &QAbstractButton::clicked, this, &ThemeBuilder::resetAll);
    connect(saveButton, &QAbstractButton::clicked, this, &ThemeBuilder::save);
    connect(m_name, &QLineEdit::textEdited, this, [this](const QString &text) { m_draft.name = text; });
    connect(m_name, &QLineEdit::returnPressed, this, &ThemeBuilder::save);
    connect(m_modeSelect, &SelectBox::currentIndexChanged, this, [this](int index) {
        m_mode = index == 1 ? AppTheme::Dark : AppTheme::Light;
        refresh();
        schedulePreview();
    });
}

void ThemeBuilder::showQuickColors(PreviewScene scene)
{
    if (m_quickScene == scene) {
        return;
    }
    m_quickScene = scene;
    for (const RowBinding &binding : m_quickRows) {
        binding.row->hide();
        binding.row->onHover = nullptr;
        binding.row->deleteLater();
    }
    m_quickRows.clear();
    if (m_hovered && m_hovered->scene != scene) {
        m_hovered.reset();
    }
    const QuickColorSet &set = quickColorSet(scene);
    m_quickTitle->setText(scene == PreviewScene::Main ? QStringLiteral("Quick Colors")
                                                      : QStringLiteral("Quick Colors · %1").arg(set.name));
    m_quickCaption->setText(set.description);
    for (const QuickColor &quick : set.colors) {
        addQuickRow(quick, scene);
    }
    if (m_composed.background.isValid()) {
        const ThemeColors &colors = m_draft.colors(m_mode);
        const ThemeColors &start = m_initial.colors(m_mode);
        for (const RowBinding &binding : m_quickRows) {
            binding.row->setState(binding.read(m_composed), binding.changed(colors, start));
        }
    }
}

void ThemeBuilder::addQuickRow(const QuickColor &quick, PreviewScene scene)
{
    auto *row = new ColorRow(quick.label, quick.description, m_quickHost);
    m_quickLayout->addWidget(row);
    const ThemeSlot *primary = findThemeSlot(quick.primary);
    m_quickRows.push_back(RowBinding{row, [primary](const Theme &theme) { return primary->read(theme); },
                                     [&quick](const ThemeColors &colors, const ThemeColors &start) {
                                         return isQuickColorChanged(colors, start, quick);
                                     }});
    const Focus focus{scene, targetIds(quick), quick.label};
    watchRow(row, focus);
    connect(row, &QAbstractButton::clicked, this, [this, &quick, primary, focus] {
        pinFocus(focus);
        pickColor(quick.label, primary->read(m_composed),
                  [&quick](ThemeColors &colors, const QColor &color) { applyQuickColor(colors, quick, color); });
    });
    row->onReset = [this, &quick] {
        const ThemeColors start = m_initial.colors(m_mode);
        changeColors([&quick, &start](ThemeColors &colors) { restoreQuickColor(colors, start, quick); });
    };
}

QWidget *ThemeBuilder::addSection(const ThemeSection &section)
{
    auto *panel = new SettingsPanel(16, this);
    auto *toggle = new SectionToggle(section, panel);
    auto *body = new QWidget(panel);
    auto *grid = new QGridLayout(body);
    grid->setContentsMargins(0, 12, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(4);
    std::vector<ColorRow *> rows;
    for (const ThemeSlot &entry : section.entries) {
        auto *row = new ColorRow(entry.label, QString(), body);
        rows.push_back(row);
        m_rows.push_back(RowBinding{row, entry.read,
                                    [id = entry.id](const ThemeColors &colors, const ThemeColors &start) {
                                        return isSlotChanged(colors, start, id);
                                    }});
        const Focus focus{section.scene, QStringList{entry.id}, entry.label};
        watchRow(row, focus);
        connect(row, &QAbstractButton::clicked, this, [this, &entry, focus] {
            pinFocus(focus);
            pickColor(entry.label, entry.read(m_composed),
                      [id = entry.id](ThemeColors &colors, const QColor &color) { colors[id] = color; });
        });
        row->onReset = [this, id = entry.id] {
            const ThemeColors start = m_initial.colors(m_mode);
            changeColors([&id, &start](ThemeColors &colors) { restoreSlot(colors, start, id); });
        };
    }
    body->hide();

    auto *panelColumn = new QVBoxLayout(panel);
    panelColumn->setContentsMargins(0, 0, 0, 0);
    panelColumn->setSpacing(0);
    panelColumn->addWidget(toggle);
    panelColumn->addWidget(body);
    connect(toggle, &QAbstractButton::clicked, this, [this, toggle, body, &section] {
        const bool expand = !toggle->isExpanded();
        toggle->setExpanded(expand);
        body->setVisible(expand);
        if (expand) {
            pinFocus(Focus{section.scene, slotIds(section), section.title});
        }
    });
    m_sections.push_back(SectionBinding{toggle, &section, grid, std::move(rows)});
    return panel;
}

void ThemeBuilder::watchRow(ColorRow *row, Focus focus)
{
    row->onHover = [this, focus = std::move(focus)](bool inside) {
        if (inside) {
            m_hovered = focus;
        } else {
            m_hovered.reset();
        }
        applyFocus();
    };
}

void ThemeBuilder::pinFocus(Focus focus)
{
    m_pinned = std::move(focus);
    showQuickColors(m_pinned.scene);
    applyFocus();
}

void ThemeBuilder::applyFocus()
{
    const Focus &focus = m_hovered ? *m_hovered : m_pinned;
    m_preview->showFocus(focus.scene, focus.ids);
    m_sticky->tabs()->setCurrent(focus.scene);
    m_sticky->setCaption(m_mode, focus.label);
    QTimer::singleShot(0, this, &ThemeBuilder::placeSticky);
}

void ThemeBuilder::edit(const ThemeDefinition &draft, bool isNew, AppTheme mode)
{
    m_mode = mode;
    m_pinned = Focus{};
    m_hovered.reset();
    showQuickColors(PreviewScene::Main);
    m_draft = draft;
    m_draft.builtIn = false;
    m_initial = m_draft;
    m_title->setText(isNew ? QStringLiteral("New Theme") : QStringLiteral("Edit Theme"));
    m_name->setText(m_draft.name);
    {
        const QSignalBlocker blocker(m_modeSelect);
        m_modeSelect->setCurrentIndex(m_mode == AppTheme::Dark ? 1 : 0);
    }
    refresh();
    schedulePreview();
}

bool ThemeBuilder::isDirty() const
{
    return m_draft.name.simplified() != m_initial.name.simplified() || m_draft.light != m_initial.light ||
           m_draft.dark != m_initial.dark;
}

void ThemeBuilder::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateLayoutMode();
}

void ThemeBuilder::updateLayoutMode()
{
    int available = width();
    if (m_scroll && isVisible()) {
        QWidget *viewport = m_scroll->viewport();
        const int left = std::max(0, mapTo(viewport, QPoint(0, 0)).x() + m_scroll->horizontalScrollBar()->value());
        available = std::min(available, viewport->width() - left * 2);
    }
    const bool compact = available < kCompactBelowWidth;
    const int side = compact ? 0 : (available >= kWideFromWidth ? kStickyWidth : kNarrowStickyWidth);
    if (compact != m_compact || side != m_sideWidth) {
        m_compact = compact;
        m_sideWidth = side;
        m_previewSpace->setVisible(!compact);
        if (!compact) {
            m_previewSpace->setFixedWidth(side);
        }
        m_sticky->setPreviewWidth(compact ? kCompactWidth : side);
    }
    const int controls = compact ? available : available - side - kStickyGap;
    const int columns = controls >= kTwoColumnsFrom ? 2 : 1;
    if (columns != m_columns) {
        layoutGrids(columns);
    }
    QTimer::singleShot(0, this, &ThemeBuilder::placeSticky);
}

void ThemeBuilder::layoutGrids(int columns)
{
    m_columns = columns;
    for (const SectionBinding &binding : m_sections) {
        for (ColorRow *row : binding.rows) {
            binding.grid->removeWidget(row);
        }
        for (int c = 0; c < 2; ++c) {
            binding.grid->setColumnStretch(c, c < columns ? 1 : 0);
        }
        int index = 0;
        for (ColorRow *row : binding.rows) {
            binding.grid->addWidget(row, index / columns, index % columns);
            ++index;
        }
    }
}

void ThemeBuilder::attachScroll()
{
    if (m_scroll) {
        return;
    }
    for (QWidget *ancestor = parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
        if (auto *area = qobject_cast<QScrollArea *>(ancestor)) {
            m_scroll = area;
            break;
        }
    }
    if (!m_scroll) {
        return;
    }
    m_sticky->setParent(m_scroll->viewport());
    m_scroll->viewport()->installEventFilter(this);
    connect(m_scroll->verticalScrollBar(), &QScrollBar::valueChanged, this, &ThemeBuilder::placeSticky);
}

void ThemeBuilder::placeSticky()
{
    if (!m_scroll || !isVisible()) {
        return;
    }
    QWidget *viewport = m_scroll->viewport();
    const QSize size = m_sticky->sizeHint();
    m_sticky->resize(size);
    QPoint position;
    if (m_compact) {
        position = QPoint(viewport->width() - size.width() - kStickyInset,
                          viewport->height() - size.height() - kStickyInset);
    } else {
        const QPoint space = m_previewSpace->mapTo(viewport, QPoint(0, 0));
        const int bottom = mapTo(viewport, QPoint(0, height())).y() - size.height();
        position = QPoint(space.x(), std::min(std::max(space.y(), kStickyInset), std::max(bottom, space.y())));
    }
    m_sticky->move(position);
    m_sticky->show();
    m_sticky->raise();
}

void ThemeBuilder::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    attachScroll();
    QTimer::singleShot(0, this, [this] {
        updateLayoutMode();
        placeSticky();
    });
}

void ThemeBuilder::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_sticky->hide();
}

bool ThemeBuilder::eventFilter(QObject *watched, QEvent *event)
{
    if (m_scroll && watched == m_scroll->viewport() && event->type() == QEvent::Resize) {
        QTimer::singleShot(0, this, [this] {
            updateLayoutMode();
            placeSticky();
        });
    }
    return QWidget::eventFilter(watched, event);
}

void ThemeBuilder::refresh()
{
    const Theme light = composeTheme(m_draft, AppTheme::Light);
    const Theme dark = composeTheme(m_draft, AppTheme::Dark);
    m_composed = m_mode == AppTheme::Dark ? dark : light;
    m_preview->setThemes(light, dark, m_mode);
    applyFocus();
    const ThemeColors &colors = m_draft.colors(m_mode);
    const ThemeColors &start = m_initial.colors(m_mode);
    for (const RowBinding &binding : m_rows) {
        binding.row->setState(binding.read(m_composed), binding.changed(colors, start));
    }
    for (const RowBinding &binding : m_quickRows) {
        binding.row->setState(binding.read(m_composed), binding.changed(colors, start));
    }
    for (const SectionBinding &binding : m_sections) {
        const auto changed = std::count_if(
            binding.section->entries.begin(), binding.section->entries.end(),
            [&colors, &start](const ThemeSlot &entry) { return isSlotChanged(colors, start, entry.id); });
        binding.toggle->setChangedCount(static_cast<int>(changed));
    }
}

void ThemeBuilder::schedulePreview()
{
    m_previewTimer->start();
}

ColorPicker *ThemeBuilder::picker()
{
    if (!m_picker) {
        m_picker = new ColorPicker(window());
    }
    return m_picker;
}

ConfirmPopup *ThemeBuilder::confirm()
{
    if (!m_confirm) {
        m_confirm = new ConfirmPopup(window());
    }
    return m_confirm;
}

void ThemeBuilder::changeColors(const std::function<void(ThemeColors &)> &change)
{
    change(m_draft.colors(m_mode));
    refresh();
    schedulePreview();
}

void ThemeBuilder::pickColor(const QString &title, const QColor &initial,
                             std::function<void(ThemeColors &, const QColor &)> apply)
{
    const AppTheme mode = m_mode;
    const ThemeColors before = m_draft.colors(mode);
    QPointer<ThemeBuilder> self(this);
    picker()->pick(
        QStringLiteral("%1 (%2)").arg(title, modeName(mode)), initial,
        [self, mode, before, apply](const QColor &color) {
            if (!self) {
                return;
            }
            ThemeColors next = before;
            apply(next, color);
            if (next != self->m_draft.colors(mode)) {
                self->m_draft.colors(mode) = std::move(next);
                self->refresh();
                self->schedulePreview();
            }
        },
        [self, mode, before, apply](std::optional<QColor> chosen) {
            if (!self) {
                return;
            }
            ThemeColors next = before;
            if (chosen) {
                apply(next, *chosen);
            }
            if (next != self->m_draft.colors(mode)) {
                self->m_draft.colors(mode) = std::move(next);
                self->refresh();
                self->schedulePreview();
            }
        });
}

void ThemeBuilder::resetAll()
{
    const ThemeColors start = m_initial.colors(m_mode);
    if (m_draft.colors(m_mode) == start) {
        return;
    }
    changeColors([&start](ThemeColors &colors) { colors = start; });
}

void ThemeBuilder::discard()
{
    m_previewTimer->stop();
    if (m_picker && m_picker->isOpen()) {
        m_picker->close();
    }
    m_themes.endPreview();
}

void ThemeBuilder::requestClose()
{
    if (!isDirty()) {
        discard();
        emit finished(QString());
        return;
    }
    QPointer<ThemeBuilder> self(this);
    confirm()->ask(QStringLiteral("Discard your changes to this theme?"), [self] {
        if (self) {
            self->discard();
            emit self->finished(QString());
        }
    });
}

void ThemeBuilder::save()
{
    m_draft.name = m_name->text().simplified();
    if (m_draft.name.isEmpty()) {
        emit toastRequested(Toast{ToastType::Warning, {}, "Enter a theme name.", kDefaultToastMs});
        m_name->setFocus(Qt::OtherFocusReason);
        return;
    }
    m_previewTimer->stop();
    auto saved = m_themes.save(m_draft);
    if (!saved) {
        emit toastRequested(Toast{ToastType::Error, {}, saved.error().userMessage, kDefaultToastMs});
        return;
    }
    m_themes.select(saved.value());
    emit toastRequested(Toast{ToastType::Success, {}, "Theme saved and applied.", kDefaultToastMs});
    emit finished(saved.value());
}

}  // namespace sky
