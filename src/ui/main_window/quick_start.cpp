#include "ui/main_window/quick_start.hpp"

#include <QFocusEvent>
#include <QFontMetricsF>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "core/translator.hpp"
#include "app/theme_controller.hpp"
#include "core/json.hpp"
#include "core/string_utils.hpp"
#include "services/settings/config_service.hpp"
#include "ui/core/hover_tracker.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/settings_window/action_button.hpp"
#include "ui/settings_window/theme_preview.hpp"
#include "ui/theme_catalog.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/select_box.hpp"
#include "ui/widgets/step_dots.hpp"
#include "ui/widgets/toggle_switch.hpp"

namespace sky {

namespace {

constexpr int kPanelWidth = 700;
constexpr int kPadding = 32;
constexpr int kTile = 42;
constexpr int kChoiceHeight = 58;
constexpr int kChipWidth = 116;
constexpr int kChipPreview = 62;
constexpr int kChipHeight = kChipPreview + 28;

QLabel *text(const QString &value, const char *objectName, QWidget *parent, bool wrap = false)
{
    auto *label = new QLabel(value, parent);
    label->setObjectName(QString::fromLatin1(objectName));
    label->setWordWrap(wrap);
    return label;
}

QColor inkOn(const QColor &fill)
{
    return fill.lightnessF() > 0.7 ? currentTheme().accent.ink900 : currentTheme().accent.white;
}

// The same language-name mapping the Settings > General tab uses for the
// descriptions under each language's native name.
const char *languageName(const char *code)
{
    const QLatin1String id(code);
    if (id == QLatin1String("en")) {
        return trNoop("English");
    }
    if (id == QLatin1String("vi")) {
        return trNoop("Vietnamese");
    }
    if (id == QLatin1String("zh_CN")) {
        return trNoop("Chinese (Simplified)");
    }
    if (id == QLatin1String("ja")) {
        return trNoop("Japanese");
    }
    return trNoop("Spanish");
}

}  // namespace

class ChoiceTile final : public QAbstractButton {
public:
    ChoiceTile(const QString &iconId, const QString &title, const QString &description, QWidget *parent)
        : QAbstractButton(parent),
          m_iconId(iconId),
          m_title(title),
          m_description(description),
          m_hover(new HoverTracker(this, kHoverFastMs))
    {
        setCheckable(true);
        setAutoExclusive(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setFixedHeight(kChoiceHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAccessibleName(title);
        connect(m_hover, &HoverTracker::changed, this, [this] { update(); });
        connect(this, &QAbstractButton::toggled, this, [this] { update(); });
    }

    QSize sizeHint() const override { return {200, kChoiceHeight}; }

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
        const SettingsPalette &s = theme.settings;
        QPainter painter(this);
        preparePainter(painter);
        const QRectF frame = QRectF(rect()).adjusted(1, 1, -1, -1);
        const QPainterPath path = roundedPath(frame, 14);
        painter.fillPath(path, isChecked() ? blend(s.cardFill, theme.hoverColor, 0.08) : s.cardFill);
        const QColor border = isChecked() ? theme.hoverColor : blend(s.cardBorder, s.cardHoverBorder, m_hover->progress());
        painter.setPen(QPen(border, isChecked() ? 2 : 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);

        const QRectF tile(14, (height() - 38) / 2.0, 38, 38);
        painter.fillPath(roundedPath(tile, 10), isChecked() ? theme.hoverColor : withAlpha(theme.hoverColor, 0.14));
        painter.drawPixmap(QPointF(tile.center().x() - 9, tile.center().y() - 9),
                           IconCache::pixmap(m_iconId, 18, isChecked() ? inkOn(theme.hoverColor) : theme.hoverColor,
                                             devicePixelRatioF()));
        const qreal left = tile.right() + 12;
        const qreal width = std::max<qreal>(0, this->width() - left - 14);
        const QFont titleFont = FontLoader::font(14, QFont::DemiBold);
        const QFont bodyFont = FontLoader::font(12, QFont::Normal);
        painter.setFont(titleFont);
        painter.setPen(theme.fontColor);
        painter.drawText(QRectF(left, height() / 2.0 - 19, width, 20), Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetricsF(titleFont).elidedText(m_title, Qt::ElideRight, width));
        painter.setFont(bodyFont);
        painter.setPen(s.cardDescText);
        painter.drawText(QRectF(left, height() / 2.0 + 1, width, 18), Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetricsF(bodyFont).elidedText(m_description, Qt::ElideRight, width));
        if (m_keyboardFocus && hasFocus()) {
            paintFocusRing(painter, frame.adjusted(2, 2, -2, -2), 12, theme.hoverColor);
        }
    }

private:
    QString m_iconId;
    QString m_title;
    QString m_description;
    HoverTracker *m_hover;
    bool m_keyboardFocus = false;
};

class ThemeChip final : public QAbstractButton {
public:
    ThemeChip(const ThemeDefinition &definition, QWidget *parent)
        : QAbstractButton(parent),
          m_id(definition.id),
          m_name(definition.name),
          m_light(composeTheme(definition, AppTheme::Light)),
          m_dark(composeTheme(definition, AppTheme::Dark)),
          m_hover(new HoverTracker(this, kHoverFastMs))
    {
        setCheckable(true);
        setAutoExclusive(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setFixedSize(kChipWidth, kChipHeight);
        setAccessibleName(definition.name);
        connect(m_hover, &HoverTracker::changed, this, [this] { update(); });
        connect(this, &QAbstractButton::toggled, this, [this] { update(); });
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
    }

    const QString &themeId() const { return m_id; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Theme &theme = currentTheme();
        QPainter painter(this);
        preparePainter(painter);
        const QRectF preview(3, 3, width() - 6, kChipPreview);
        const qreal dpr = devicePixelRatioF();
        const QSize cacheSize = (preview.size() * dpr).toSize();
        if (m_cache.size() != cacheSize) {
            m_cache = QPixmap(cacheSize);
            m_cache.setDevicePixelRatio(dpr);
            m_cache.fill(Qt::transparent);
            QPainter cache(&m_cache);
            preparePainter(cache);
            paintThemeSplitPreview(cache, QRectF(QPointF(0, 0), preview.size()), m_light, m_dark, 9, false,
                                   std::nullopt, PreviewScene::Main, {}, false);
        }
        painter.drawPixmap(preview.topLeft(), m_cache);
        const QRectF ring = preview.adjusted(-2, -2, 2, 2);
        if (isChecked()) {
            painter.setPen(QPen(theme.hoverColor, 2.5));
        } else {
            painter.setPen(QPen(withAlpha(theme.hoverColor, 0.6 * m_hover->progress()), 2));
        }
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(roundedPath(ring.adjusted(0.5, 0.5, -0.5, -0.5), 11));
        const QFont font = FontLoader::font(13, isChecked() ? QFont::DemiBold : QFont::Normal);
        painter.setFont(font);
        painter.setPen(isChecked() ? theme.hoverColor : theme.fontColor);
        painter.drawText(QRectF(0, preview.bottom() + 6, width(), 20), Qt::AlignHCenter | Qt::AlignVCenter,
                         QFontMetricsF(font).elidedText(m_name, Qt::ElideRight, width()));
        if (hasFocus() && !isChecked()) {
            paintFocusRing(painter, ring, 11, theme.hoverColor);
        }
    }

private:
    QString m_id;
    QString m_name;
    Theme m_light;
    Theme m_dark;
    QPixmap m_cache;
    HoverTracker *m_hover;
};

QuickStart::QuickStart(ThemeController &themes, ConfigService &config, QWidget *window)
    : Overlay(window),
      m_themes(themes),
      m_config(config),
      m_panel(new QWidget(this)),
      m_stack(new QStackedWidget(m_panel)),
      m_dots(new StepDots(m_panel)),
      m_back(new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("arrow-left"), tr("Back"), m_panel)),
      m_skip(new ActionButton(ActionButton::Kind::Subtle, QString(), tr("Skip setup"), m_panel)),
      m_next(new ActionButton(ActionButton::Kind::Primary, QString(), tr("Next"), m_panel))
{
    buildSteps();

    auto *nav = new QHBoxLayout();
    nav->setSpacing(12);
    nav->addWidget(m_skip);
    nav->addWidget(m_back);
    nav->addStretch();
    nav->addWidget(m_dots, 0, Qt::AlignVCenter);
    nav->addStretch();
    nav->addWidget(m_next);

    auto *column = new QVBoxLayout(m_panel);
    column->setContentsMargins(kPadding, kPadding, kPadding, kPadding - 4);
    column->setSpacing(0);
    column->addWidget(m_stack, 1);
    column->addSpacing(24);
    column->addLayout(nav);

    connect(m_next, &QAbstractButton::clicked, this, &QuickStart::next);
    connect(m_back, &QAbstractButton::clicked, this, &QuickStart::back);
    connect(m_skip, &QAbstractButton::clicked, this, &QuickStart::finish);
    connect(this, &Overlay::closed, this, [this] {
        if (!m_finished) {
            m_finished = true;
            emit finished();
        }
    });
    connect(&m_themes, &ThemeController::activeChanged, this, &QuickStart::syncLook);
    connect(&m_themes, &ThemeController::followSystemChanged, this, &QuickStart::syncLook);
}

void QuickStart::buildSteps()
{
    m_welcome = buildWelcome();
    m_look = buildLook();
    m_legacy = buildLegacy();
    m_playback = buildPlayback();
    m_controls = buildControls();
    m_done = buildDone();
    for (QWidget *step : {m_welcome, m_look, m_legacy, m_playback, m_controls, m_done}) {
        m_stack->addWidget(step);
    }
    m_steps = {m_welcome, m_look, m_legacy, m_playback, m_controls, m_done};
}

QWidget *QuickStart::page(const QString &iconId, const QString &title, const QString &body)
{
    auto *widget = new QWidget(m_stack);
    auto *column = new QVBoxLayout(widget);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    auto *icon = new QLabel(widget);
    icon->setFixedSize(kTile, kTile);
    icon->setObjectName(QStringLiteral("QuickStartIcon"));
    icon->setProperty("iconId", iconId);
    column->addWidget(icon);
    column->addSpacing(12);
    column->addWidget(text(title, "QuickStartTitle", widget));
    column->addSpacing(4);
    column->addWidget(text(body, "QuickStartBody", widget, true));
    column->addSpacing(16);
    return widget;
}

QWidget *QuickStart::buildWelcome()
{
    QWidget *widget = page(QStringLiteral("music-note-beamed"), tr("Welcome to Sky Auto Piano"),
                           tr("Let's set things up. It takes less than a minute, and you can change "
                                          "everything later in Settings."));
    auto *column = static_cast<QVBoxLayout *>(widget->layout());
    column->addWidget(text(tr("Language"), "ColorPickerCaption", widget));
    column->addSpacing(6);
    m_language = new SelectBox(widget);
    m_language->addOption(tr("Follow Windows"), tr("Use the same language as Windows"));
    for (const LanguageInfo &language : supportedLanguages()) {
        m_language->addOption(QString::fromUtf8(language.nativeName), tr(languageName(language.code)));
    }
    m_language->setAccessibleName(tr("Language"));
    column->addWidget(m_language);
    column->addSpacing(4);
    column->addWidget(text(tr("Pick the language for the app - it switches right away."),
                           "SettingsRowDescription", widget, true));
    column->addSpacing(16);
    setLanguage(m_languagePreference);
    connect(m_language, &SelectBox::currentIndexChanged, this, &QuickStart::onLanguageIndexChanged);
    const QStringList points = {tr("Pick light or dark and a theme you like"),
                                tr("Bring your songs over from the previous version"),
                                tr("Choose how songs play and learn the shortcuts")};
    for (const QString &point : points) {
        auto *row = new QHBoxLayout();
        row->setSpacing(10);
        auto *mark = new QLabel(widget);
        mark->setFixedSize(18, 18);
        mark->setObjectName(QStringLiteral("QuickStartCheck"));
        row->addWidget(mark);
        row->addWidget(text(point, "QuickStartPoint", widget), 1);
        column->addLayout(row);
        column->addSpacing(10);
    }
    column->addStretch();
    return widget;
}

QWidget *QuickStart::buildLook()
{
    QWidget *widget = page(QStringLiteral("palette"), tr("Pick a look"),
                           tr("Changes show right away. You can build your own theme later in "
                                          "Settings → Themes."));
    auto *column = static_cast<QVBoxLayout *>(widget->layout());
    auto *modeBox = new QWidget(widget);
    auto *modes = new QHBoxLayout(modeBox);
    modes->setContentsMargins(0, 0, 0, 0);
    modes->setSpacing(10);
    auto *follow = new ChoiceTile(QStringLiteral("window"), tr("Follow Windows"),
                                  tr("Changes with Windows"), modeBox);
    auto *light = new ChoiceTile(QStringLiteral("sun"), tr("Light"), tr("Always light"), modeBox);
    auto *dark = new ChoiceTile(QStringLiteral("moon"), tr("Dark"), tr("Always dark"), modeBox);
    m_modeTiles = {follow, light, dark};
    for (ChoiceTile *tile : m_modeTiles) {
        modes->addWidget(tile, 1);
    }
    column->addWidget(modeBox);
    column->addSpacing(14);
    column->addWidget(text(tr("Theme"), "ColorPickerCaption", widget));
    column->addSpacing(6);
    auto *chipBox = new QWidget(widget);
    auto *grid = new QGridLayout(chipBox);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(4);
    int index = 0;
    for (const ThemeDefinition &definition : builtInThemes()) {
        auto *chip = new ThemeChip(definition, chipBox);
        grid->addWidget(chip, index / 4, index % 4, Qt::AlignLeft);
        m_themeChips.push_back(chip);
        connect(chip, &QAbstractButton::clicked, this, [this, id = definition.id] { m_themes.select(id); });
        ++index;
    }
    column->addWidget(chipBox);
    column->addStretch();

    connect(follow, &QAbstractButton::clicked, this, [this] { m_themes.setFollowSystem(true); });
    connect(light, &QAbstractButton::clicked, this, [this] { m_themes.setKind(AppTheme::Light); });
    connect(dark, &QAbstractButton::clicked, this, [this] { m_themes.setKind(AppTheme::Dark); });
    return widget;
}

QWidget *QuickStart::buildLegacy()
{
    QWidget *widget = page(QStringLiteral("download"), tr("Bring your songs over"), QString());
    auto *column = static_cast<QVBoxLayout *>(widget->layout());
    m_legacyBody = widget->findChildren<QLabel *>(QStringLiteral("QuickStartBody")).value(0);
    m_legacyChoices = new QWidget(widget);
    auto *choices = new QHBoxLayout(m_legacyChoices);
    choices->setContentsMargins(0, 0, 0, 0);
    choices->setSpacing(10);
    m_importTile = new ChoiceTile(QStringLiteral("download"), tr("Import them"),
                                  tr("Songs, playlists, favorites, settings"), m_legacyChoices);
    auto *skip = new ChoiceTile(QStringLiteral("x-lg"), tr("Not now"),
                                tr("Import later in Settings → General"), m_legacyChoices);
    m_importTile->setChecked(true);
    choices->addWidget(m_importTile, 1);
    choices->addWidget(skip, 1);
    column->addWidget(m_legacyChoices);
    m_folderButton = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("folder2"),
                                      tr("Choose folder"), widget);
    column->addWidget(m_folderButton, 0, Qt::AlignLeft);
    column->addSpacing(14);
    m_legacyNote = text(tr("Nothing in the previous version is changed. Your songs are copied."),
                        "SettingsRowDescription", widget, true);
    column->addWidget(m_legacyNote);
    column->addSpacing(18);
    column->addWidget(text(tr("Remove Old Electron Version"), "SettingsRowTitle", widget));
    column->addSpacing(2);
    m_cleanupDescription = text(tr("Checking the old version..."), "SettingsRowDescription", widget, true);
    column->addWidget(m_cleanupDescription);
    column->addSpacing(8);
    auto *cleanupActions = new QHBoxLayout();
    cleanupActions->setSpacing(10);
    m_cleanupVerify = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("shield-check"), tr("Verify"),
                                       widget);
    m_cleanupRemove =
        new ActionButton(ActionButton::Kind::Amber, QStringLiteral("trash3"), tr("Remove"), widget);
    cleanupActions->addWidget(m_cleanupVerify);
    cleanupActions->addWidget(m_cleanupRemove);
    cleanupActions->addStretch();
    column->addLayout(cleanupActions);
    column->addStretch();
    connect(m_folderButton, &QAbstractButton::clicked, this, &QuickStart::legacyFolderRequested);
    connect(m_cleanupVerify, &QAbstractButton::clicked, this, &QuickStart::legacyVerifyRequested);
    connect(m_cleanupRemove, &QAbstractButton::clicked, this, &QuickStart::legacyRemovalRequested);
    applyCleanup();
    return widget;
}

QWidget *QuickStart::buildPlayback()
{
    QWidget *widget = page(QStringLiteral("music-note-beamed"), tr("How should songs play?"),
                           tr("Lite is right for most PCs. Pick Strict if notes sound out of time."));
    auto *column = static_cast<QVBoxLayout *>(widget->layout());
    auto *modeBox = new QWidget(widget);
    auto *modes = new QHBoxLayout(modeBox);
    modes->setContentsMargins(0, 0, 0, 0);
    modes->setSpacing(10);
    m_liteTile = new ChoiceTile(QStringLiteral("lightning"), tr("Lite"),
                                tr("Lighter on weak PCs, may drift a few ms"), modeBox);
    m_strictTile = new ChoiceTile(QStringLiteral("metronome"), tr("Strict"),
                                  tr("Most accurate timing, uses more CPU"), modeBox);
    modes->addWidget(m_liteTile, 1);
    modes->addWidget(m_strictTile, 1);
    column->addWidget(modeBox);
    column->addSpacing(16);

    auto *row = new QHBoxLayout();
    row->setSpacing(12);
    auto *copy = new QVBoxLayout();
    copy->setSpacing(2);
    copy->addWidget(text(tr("Minimize on Play"), "SettingsRowTitle", widget));
    copy->addWidget(text(tr("Hide this window when a song starts, so you can see Sky."),
                         "SettingsRowDescription", widget, true));
    m_minimize = new ToggleSwitch(widget);
    m_minimize->setAccessibleName(tr("Minimize on Play"));
    row->addLayout(copy, 1);
    row->addWidget(m_minimize, 0, Qt::AlignVCenter);
    column->addLayout(row);
    column->addStretch();

    connect(m_liteTile, &QAbstractButton::clicked, this, [this] { m_config.updatePanel({{"playbackMode", "lite"}}); });
    connect(m_strictTile, &QAbstractButton::clicked, this,
            [this] { m_config.updatePanel({{"playbackMode", "strict"}}); });
    connect(m_minimize, &QAbstractButton::toggled, this,
            [this](bool checked) { m_config.updatePanel({{"minimizeOnPlay", checked}}); });
    return widget;
}

QWidget *QuickStart::buildControls()
{
    QWidget *widget = page(QStringLiteral("keyboard"), tr("Play from anywhere"),
                           tr("These shortcuts work even while Sky is in front. Change them in "
                                          "Settings → Shortcuts."));
    auto *column = static_cast<QVBoxLayout *>(widget->layout());
    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(10);
    const QStringList actions = {tr("Play / Pause"), tr("Previous song"),
                                 tr("Next song"), tr("Faster"), tr("Slower")};
    for (int i = 0; i < actions.size(); ++i) {
        grid->addWidget(text(actions[i], "QuickStartPoint", widget), i, 0);
        auto *keys = text(QString(), "QuickStartKeys", widget);
        grid->addWidget(keys, i, 1, Qt::AlignLeft);
        m_shortcutKeys.push_back(keys);
    }
    grid->setColumnStretch(1, 1);
    column->addLayout(grid);
    column->addSpacing(16);
    column->addWidget(text(tr("Changed Sky's own keys? Set the same 15 keys in Settings → Keyboard."),
                           "SettingsRowDescription", widget, true));
    column->addStretch();
    return widget;
}

QWidget *QuickStart::buildDone()
{
    QWidget *widget = page(QStringLiteral("check-circle-fill"), tr("You're all set"),
                           tr("A few tips before you start:"));
    auto *column = static_cast<QVBoxLayout *>(widget->layout());
    const QStringList tips = {tr("Press + at the bottom right to add song files."),
                              tr("Open the menu at the top left to find songs in the Store."),
                              tr("Open Sky first, then press Play. The app types the notes for you."),
                              tr("Run this setup again any time from Settings → General.")};
    for (const QString &tip : tips) {
        auto *row = new QHBoxLayout();
        row->setSpacing(10);
        auto *mark = new QLabel(widget);
        mark->setFixedSize(18, 18);
        mark->setObjectName(QStringLiteral("QuickStartCheck"));
        row->addWidget(mark);
        row->addWidget(text(tip, "QuickStartPoint", widget, true), 1);
        column->addLayout(row);
        column->addSpacing(10);
    }
    column->addStretch();
    return widget;
}

void QuickStart::start()
{
    m_finished = false;
    m_current = 0;
    setLegacyState(LegacyState::Searching);
    syncLook();
    syncControls();
    showStep(0);
    open();
    m_next->setFocus(Qt::OtherFocusReason);
}

void QuickStart::syncControls()
{
    const PanelConfig panel = m_config.panel();
    {
        const QSignalBlocker liteBlocker(m_liteTile);
        const QSignalBlocker strictBlocker(m_strictTile);
        (panel.playbackMode == PlaybackMode::Strict ? m_strictTile : m_liteTile)->setChecked(true);
    }
    {
        const QSignalBlocker blocker(m_minimize);
        m_minimize->setCheckedImmediately(panel.minimizeOnPlay);
    }
    loadControls();
}

void QuickStart::setLegacyState(LegacyState state, int songs, int playlists, const QString &detail)
{
    m_legacyState = state;
    m_legacySongs = songs;
    m_legacyPlaylists = playlists;
    m_legacyDetail = detail;
    QString body;
    switch (state) {
    case LegacyState::Searching:
        body = tr("Looking for the previous version of Sky Auto Piano on this PC…");
        break;
    case LegacyState::Found: {
        const QString songText = songs == 1 ? tr("1 song") : tr("%1 songs").arg(songs);
        const QString playlistText =
            playlists == 1 ? tr("1 playlist") : tr("%1 playlists").arg(playlists);
        body = playlists > 0 ? tr("We found %1 and %2 from the previous version of Sky Auto Piano.")
                                   .arg(songText, playlistText)
                             : tr("We found %1 from the previous version of Sky Auto Piano.").arg(songText);
        m_importTile->setChecked(true);
        break;
    }
    case LegacyState::NotFound:
        body = detail.isEmpty() ? tr("No previous version of Sky Auto Piano was found. If you used it "
                                                 "before, choose its folder.")
                                : detail;
        break;
    case LegacyState::UpToDate:
        body = tr("All songs from the previous version are already in your library.");
        break;
    case LegacyState::Importing:
        body = tr("Copying your songs from the previous version…");
        break;
    case LegacyState::Imported:
        body = detail;
        break;
    }
    m_legacyBody->setText(body);
    m_legacyChoices->setVisible(state == LegacyState::Found);
    m_legacyNote->setVisible(state == LegacyState::Found);
    m_folderButton->setVisible(state == LegacyState::Searching || state == LegacyState::NotFound ||
                               state == LegacyState::UpToDate);
    if (isOpen()) {
        relayout();
        update();
    }
}

void QuickStart::setLegacyCleanup(const QString &description, bool removeEnabled, bool verifyVisible, bool busy)
{
    m_cleanupText = description;
    m_cleanupKnown = true;
    m_cleanupRemoveEnabled = removeEnabled;
    m_cleanupVerifyVisible = verifyVisible;
    m_cleanupBusy = busy;
    applyCleanup();
}

void QuickStart::applyCleanup()
{
    if (!m_cleanupDescription) {
        return;
    }
    if (m_cleanupKnown) {
        m_cleanupDescription->setText(m_cleanupText);
    }
    m_cleanupVerify->setVisible(m_cleanupVerifyVisible);
    m_cleanupRemove->setEnabled(m_cleanupRemoveEnabled && !m_cleanupBusy);
    m_cleanupRemove->setBusy(m_cleanupBusy, tr("Uninstalling..."));
    if (isOpen()) {
        relayout();
        update();
    }
}

void QuickStart::setLanguage(const QString &preference)
{
    m_languagePreference = preference;
    if (!m_language) {
        return;
    }
    int index = 0;
    const auto &languages = supportedLanguages();
    for (std::size_t i = 0; i < languages.size(); ++i) {
        if (preference == QLatin1String(languages[i].code)) {
            index = static_cast<int>(i) + 1;
        }
    }
    const QSignalBlocker blocker(m_language);
    m_language->setCurrentIndex(index);
}

void QuickStart::onLanguageIndexChanged(int index)
{
    const auto &languages = supportedLanguages();
    const QString preference = index <= 0 || index > static_cast<int>(languages.size())
                                   ? QString()
                                   : QString::fromLatin1(languages[static_cast<std::size_t>(index - 1)].code);
    m_languagePreference = preference;
    emit languageChosen(preference);
    if (resolveLanguage(preference) == currentLanguage()) {
        return;
    }
    installLanguage(preference);
    rebuildSteps();
}

// The step pages hold text set at build time, so a language switch rebuilds
// them in the new language and restores every piece of state they showed:
// the current step, the look and playback choices, the shortcut list, the
// legacy import state and the legacy cleanup state.
void QuickStart::rebuildSteps()
{
    const int step = m_current;
    for (QWidget *page : m_steps) {
        m_stack->removeWidget(page);
        delete page;
    }
    m_steps.clear();
    m_modeTiles.clear();
    m_themeChips.clear();
    m_shortcutKeys.clear();
    buildSteps();
    syncLook();
    syncControls();
    setLegacyState(m_legacyState, m_legacySongs, m_legacyPlaylists, m_legacyDetail);
    retranslateChrome();
    showStep(step);
}

void QuickStart::retranslateChrome()
{
    m_back->setText(tr("Back"));
    m_skip->setText(tr("Skip setup"));
}

void QuickStart::loadControls()
{
    const ShortcutConfig shortcut = m_config.value().shortcut;
    const std::string values[] = {shortcut.play, shortcut.pre, shortcut.next, shortcut.increaseSpeed,
                                  shortcut.decreaseSpeed};
    for (std::size_t i = 0; i < m_shortcutKeys.size(); ++i) {
        QString keys = toQString(values[i]);
        m_shortcutKeys[i]->setText(keys.replace(QLatin1Char('+'), QStringLiteral("  +  ")));
    }
}

void QuickStart::syncLook()
{
    const int mode = m_themes.followsSystem() ? 0 : (m_themes.kind() == AppTheme::Dark ? 2 : 1);
    for (int i = 0; i < static_cast<int>(m_modeTiles.size()); ++i) {
        const QSignalBlocker blocker(m_modeTiles[static_cast<std::size_t>(i)]);
        if (i == mode) {
            m_modeTiles[static_cast<std::size_t>(i)]->setChecked(true);
        }
    }
    const QString active = m_themes.activeId();
    for (ThemeChip *chip : m_themeChips) {
        const QSignalBlocker blocker(chip);
        if (chip->themeId() == active) {
            chip->setChecked(true);
        }
    }
    if (std::none_of(m_themeChips.begin(), m_themeChips.end(), [](ThemeChip *chip) { return chip->isChecked(); })) {
        for (ThemeChip *chip : m_themeChips) {
            chip->setAutoExclusive(false);
            chip->setChecked(false);
            chip->setAutoExclusive(true);
        }
    }
}

void QuickStart::showStep(int index)
{
    m_current = std::clamp(index, 0, static_cast<int>(m_steps.size()) - 1);
    m_stack->setCurrentWidget(m_steps[static_cast<std::size_t>(m_current)]);
    const bool first = m_current == 0;
    const bool last = m_current == static_cast<int>(m_steps.size()) - 1;
    m_skip->setVisible(first);
    m_back->setVisible(!first);
    m_next->setText(first ? tr("Get started") : (last ? tr("Start playing") : tr("Next")));
    m_next->updateGeometry();
    m_dots->set(static_cast<int>(m_steps.size()), m_current);
    relayout();
    update();
}

void QuickStart::next()
{
    QWidget *step = m_steps[static_cast<std::size_t>(m_current)];
    if (step == m_legacy && m_legacyState == LegacyState::Found && m_importTile->isChecked()) {
        setLegacyState(LegacyState::Importing);
        emit legacyImportRequested();
    }
    if (m_current >= static_cast<int>(m_steps.size()) - 1) {
        finish();
        return;
    }
    showStep(m_current + 1);
}

void QuickStart::back()
{
    showStep(m_current - 1);
}

void QuickStart::finish()
{
    if (!m_finished) {
        m_finished = true;
        emit finished();
    }
    close();
}

QColor QuickStart::dimColor() const
{
    return currentTheme().settings.modalDim;
}

ShadowSpec QuickStart::panelShadow() const
{
    return currentTheme().settings.modalShadow;
}

QRect QuickStart::panelGeometry() const
{
    const int width = std::min(kPanelWidth, parentWidget()->width() - 32);
    const int wanted = m_panel->layout()->totalSizeHint().height();
    const int height = std::min(wanted, parentWidget()->height() - 24);
    const int rise = qRound(16 * (1.0 - presence()));
    return QRect((parentWidget()->width() - width) / 2, std::max(12, (parentWidget()->height() - height) / 2 + rise),
                 width, height);
}

void QuickStart::paintPanel(QPainter &painter, const QRectF &rect)
{
    const Theme &theme = currentTheme();
    const SettingsPalette &s = theme.settings;
    const QPainterPath path = roundedPath(rect.adjusted(0.5, 0.5, -0.5, -0.5), 20);
    painter.fillPath(path, s.modalFill);
    painter.setPen(QPen(s.modalBorder, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    const qreal dpr = painter.device()->devicePixelRatioF();
    QWidget *current = m_stack->currentWidget();
    for (QLabel *icon : current->findChildren<QLabel *>(QStringLiteral("QuickStartIcon"))) {
        const QRectF tile(QRectF(icon->geometry()).translated(QPointF(current->mapTo(m_panel, QPoint(0, 0)))).translated(
            rect.topLeft()));
        painter.fillPath(roundedPath(tile, 12), withAlpha(theme.hoverColor, 0.14));
        painter.drawPixmap(QPointF(tile.center().x() - 11, tile.center().y() - 11),
                           IconCache::pixmap(icon->property("iconId").toString(), 22, theme.hoverColor, dpr));
    }
    for (QLabel *mark : current->findChildren<QLabel *>(QStringLiteral("QuickStartCheck"))) {
        const QRectF box(QRectF(mark->geometry()).translated(QPointF(mark->parentWidget()->mapTo(m_panel, QPoint(0, 0))))
                             .translated(rect.topLeft()));
        painter.drawPixmap(QPointF(box.left() + 1, box.top() + 1),
                           IconCache::pixmap(QStringLiteral("check-circle-fill"), 16, theme.hoverColor, dpr));
    }
}

}  // namespace sky
