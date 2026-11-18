#include "ui/settings_window/keyboard_tab.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "core/string_utils.hpp"
#include "ui/core/elevation_layer.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/settings_window/key_cap.hpp"
#include "ui/settings_window/setting_card.hpp"
#include "ui/settings_window/settings_panel.hpp"
#include "ui/settings_window/tab_header.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/toggle_switch.hpp"

namespace sky {

namespace {

constexpr int kHeaderGap = 20;
constexpr int kPanelPadding = 20;
constexpr int kCapSpacing = 8;
constexpr int kTipAbove = 12;
constexpr int kTipBelow = 20;
constexpr int kTipPadding = 8;
constexpr int kTipGap = 8;
constexpr int kTipIcon = 14;
constexpr int kColumns = 5;

constexpr std::array<int, kSkyKeyCount> kShapes = {
    KeyCap::Square | KeyCap::Circle, KeyCap::Square, KeyCap::Circle, KeyCap::Square, KeyCap::Circle,
    KeyCap::Circle, KeyCap::Square, KeyCap::Square | KeyCap::Circle, KeyCap::Square, KeyCap::Circle,
    KeyCap::Circle, KeyCap::Square, KeyCap::Circle, KeyCap::Square, KeyCap::Square | KeyCap::Circle,
};

}  // namespace

KeyboardTab::KeyboardTab(QWidget *parent)
    : QWidget(parent), m_customKeyboard(new ToggleSwitch(this)), m_tipIcon(new QLabel(this))
{
    auto *shadows = new ElevationLayer(this);
    auto *header = new TabHeader(QStringLiteral("keyboard"), [] { return currentTheme().settings.navIconKeyboard; },
                                 QStringLiteral("Keyboard Layout"),
                                 QStringLiteral("Customize your keyboard mapping for piano keys"), this);

    auto *panel = new SettingsPanel(kPanelPadding, this);
    auto *grid = new QGridLayout();
    grid->setSpacing(kCapSpacing);
    for (int i = 0; i < kSkyKeyCount; ++i) {
        auto *cap = new KeyCap(kShapes[static_cast<std::size_t>(i)], panel);
        m_caps[static_cast<std::size_t>(i)] = cap;
        grid->addWidget(cap, i / kColumns, i % kColumns);
    }
    auto *panelRow = new QHBoxLayout(panel);
    panelRow->setContentsMargins(0, 0, 0, 0);
    panelRow->addStretch();
    panelRow->addLayout(grid);
    panelRow->addStretch();

    auto *tip = new QLabel(QStringLiteral("Tip: Press Tab to move to the next key input faster."), this);
    tip->setObjectName(QStringLiteral("KeyboardTip"));
    m_tipIcon->setFixedSize(kTipIcon, kTipIcon);
    auto *tipRow = new QHBoxLayout();
    tipRow->setContentsMargins(kTipPadding, 0, kTipPadding, 0);
    tipRow->setSpacing(kTipGap);
    tipRow->addWidget(m_tipIcon);
    tipRow->addWidget(tip, 1);

    auto *card = new SettingCard(QStringLiteral("palette"), QStringLiteral("Custom Keyboard"),
                                 QStringLiteral("Use custom keyboard mapping instead of default layout"),
                                 m_customKeyboard, SettingCard::Variant::Normal, this);
    card->attachShadows(shadows);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(header);
    column->addSpacing(kHeaderGap);
    column->addWidget(panel);
    column->addSpacing(kTipAbove);
    column->addLayout(tipRow);
    column->addSpacing(kTipBelow);
    column->addWidget(card);
    column->addStretch();

    refreshTipIcon();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { refreshTipIcon(); });
}

void KeyboardTab::refreshTipIcon()
{
    m_tipIcon->setPixmap(IconCache::pixmap(QStringLiteral("info-circle"), kTipIcon, currentTheme().hoverColor,
                                           devicePixelRatioF()));
}

void KeyboardTab::load(const Config &config)
{
    for (std::size_t i = 0; i < m_caps.size(); ++i) {
        m_caps[i]->setValue(toQString(config.keyboard.keys[i]));
    }
    m_customKeyboard->setCheckedImmediately(config.keyboard.customKeyboard);
}

void KeyboardTab::collect(Config &config) const
{
    for (std::size_t i = 0; i < m_caps.size(); ++i) {
        config.keyboard.keys[i] = fromQString(m_caps[i]->value());
    }
    config.keyboard.customKeyboard = m_customKeyboard->isChecked();
}

}  // namespace sky
