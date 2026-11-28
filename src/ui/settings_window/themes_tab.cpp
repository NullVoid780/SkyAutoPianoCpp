#include "ui/settings_window/themes_tab.hpp"

#include <QLabel>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

#include "core/translator.hpp"
#include "ui/controllers/theme_controller.hpp"
#include "core/string_utils.hpp"
#include "platform/file_dialog.hpp"
#include "ui/widgets/action_button.hpp"
#include "ui/widgets/settings_row.hpp"
#include "ui/settings_window/tab_header.hpp"
#include "ui/settings_window/theme_builder.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/confirm_popup.hpp"
#include "ui/widgets/select_box.hpp"

namespace sky {

namespace {

constexpr int kHeaderGap = 20;
constexpr int kGroupGap = 28;
constexpr int kTitleGap = 12;
constexpr int kCardGap = 16;

QString fileStem(const QString &name)
{
    static const QRegularExpression unsafe(QStringLiteral("[^A-Za-z0-9 _-]+"));
    QString stem = name;
    stem.replace(unsafe, QString());
    stem = stem.simplified().replace(QLatin1Char(' '), QLatin1Char('-')).toLower();
    return stem.isEmpty() ? QStringLiteral("theme") : stem;
}

}  // namespace

class ThemeGrid final : public QWidget {
public:
    explicit ThemeGrid(QWidget *parent) : QWidget(parent)
    {
        QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        policy.setHeightForWidth(true);
        setSizePolicy(policy);
    }

    void setCards(std::vector<ThemeCard *> cards)
    {
        for (ThemeCard *card : m_cards) {
            card->deleteLater();
        }
        m_cards = std::move(cards);
        for (ThemeCard *card : m_cards) {
            card->setParent(this);
            card->show();
        }
        place();
        updateGeometry();
    }

    const std::vector<ThemeCard *> &cards() const { return m_cards; }
    bool hasHeightForWidth() const override { return true; }

    int heightForWidth(int width) const override
    {
        if (m_cards.empty()) {
            return 0;
        }
        const int columns = columnsFor(width);
        const int rows = static_cast<int>((m_cards.size() + columns - 1) / columns);
        return rows * ThemeCard::kHeight + (rows - 1) * kCardGap;
    }

    QSize sizeHint() const override { return {ThemeCard::kMinWidth * 3, heightForWidth(ThemeCard::kMinWidth * 3)}; }
    QSize minimumSizeHint() const override { return {ThemeCard::kMinWidth, 0}; }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        place();
    }

private:
    static int columnsFor(int width)
    {
        return std::clamp((width + kCardGap) / (ThemeCard::kMinWidth + kCardGap), 1, 4);
    }

    void place()
    {
        const int columns = columnsFor(width());
        const int cardWidth = (width() - kCardGap * (columns - 1)) / columns;
        for (std::size_t i = 0; i < m_cards.size(); ++i) {
            const int column = static_cast<int>(i) % columns;
            const int row = static_cast<int>(i) / columns;
            m_cards[i]->setGeometry(column * (cardWidth + kCardGap), row * (ThemeCard::kHeight + kCardGap), cardWidth,
                                    ThemeCard::kHeight);
        }
    }

    std::vector<ThemeCard *> m_cards;
};

ThemesTab::ThemesTab(ThemeController &themes, QWidget *parent)
    : QWidget(parent),
      m_themes(themes),
      m_gallery(new QWidget(this)),
      m_builtIn(new ThemeGrid(m_gallery)),
      m_custom(new ThemeGrid(m_gallery)),
      m_emptyHint(new QLabel(tr("Themes you create or import show up here. Press New Theme to start "
                                            "from the current look, or the pencil on any theme to customize a copy."),
                             m_gallery)),
      m_modeChoice(new SelectBox(m_gallery)),
      m_builder(new ThemeBuilder(themes, this))
{
    auto *header = new TabHeader(QStringLiteral("palette"), [] { return currentTheme().hoverColor; },
                                 tr("Themes"), tr("Pick a look for the app, or build your own."),
                                 m_gallery);
    auto *importButton = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("upload"), tr("Import"));
    auto *newButton = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("plus"), tr("New Theme"));
    header->addAction(importButton);
    header->addAction(newButton);

    auto *builtInTitle = new QLabel(tr("Built-in Themes"), m_gallery);
    builtInTitle->setObjectName(QStringLiteral("ThemeGroupTitle"));
    auto *customTitle = new QLabel(tr("Your Themes"), m_gallery);
    customTitle->setObjectName(QStringLiteral("ThemeGroupTitle"));
    m_emptyHint->setObjectName(QStringLiteral("SettingsRowDescription"));
    m_emptyHint->setWordWrap(true);

    auto *galleryColumn = new QVBoxLayout(m_gallery);
    galleryColumn->setContentsMargins(0, 0, 0, 0);
    galleryColumn->setSpacing(0);
    m_modeChoice->addOption(tr("Follow Windows"), tr("Matches the light or dark mode of Windows"));
    m_modeChoice->addOption(tr("Always light"), tr("Uses the light version of your theme"));
    m_modeChoice->addOption(tr("Always dark"), tr("Uses the dark version of your theme"));
    m_modeChoice->setAccessibleName(tr("Light or Dark"));
    auto *modeRow = new SettingsRow(QStringLiteral("sun"), tr("Light or Dark"),
                                    tr("Follow Windows, or always use one. The sun and moon button also "
                                                   "switches it."),
                                    m_modeChoice, m_gallery);

    galleryColumn->addWidget(header);
    galleryColumn->addSpacing(kHeaderGap);
    galleryColumn->addWidget(modeRow);
    galleryColumn->addSpacing(kGroupGap);
    galleryColumn->addWidget(builtInTitle);
    galleryColumn->addSpacing(kTitleGap);
    galleryColumn->addWidget(m_builtIn);
    galleryColumn->addSpacing(kGroupGap);
    galleryColumn->addWidget(customTitle);
    galleryColumn->addSpacing(kTitleGap);
    galleryColumn->addWidget(m_custom);
    galleryColumn->addWidget(m_emptyHint);
    galleryColumn->addStretch();

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(m_gallery);
    column->addWidget(m_builder);
    m_builder->hide();

    connect(importButton, &QAbstractButton::clicked, this, &ThemesTab::importTheme);
    connect(newButton, &QAbstractButton::clicked, this, &ThemesTab::createTheme);
    connect(m_builder, &ThemeBuilder::finished, this, &ThemesTab::closeBuilder);
    connect(m_builder, &ThemeBuilder::toastRequested, this, &ThemesTab::toastRequested);
    connect(&m_themes, &ThemeController::themesChanged, this, &ThemesTab::rebuild);
    connect(&m_themes, &ThemeController::activeChanged, this, &ThemesTab::refreshActive);
    connect(&m_themes, &ThemeController::activeChanged, this, &ThemesTab::refreshModeChoice);
    connect(&m_themes, &ThemeController::followSystemChanged, this, &ThemesTab::refreshModeChoice);
    connect(m_modeChoice, &SelectBox::currentIndexChanged, this, [this](int index) {
        if (index == 0) {
            m_themes.setFollowSystem(true);
        } else {
            m_themes.setKind(index == 2 ? AppTheme::Dark : AppTheme::Light);
        }
        refreshModeChoice();
    });
    refreshModeChoice();

    std::vector<ThemeCard *> builtInCards;
    for (const ThemeDefinition &definition : builtInThemes()) {
        auto *card = new ThemeCard(definition);
        connect(card, &QAbstractButton::clicked, this, [this, id = definition.id] { m_themes.select(id); });
        connect(card, &ThemeCard::actionRequested, this, &ThemesTab::onCardAction);
        builtInCards.push_back(card);
    }
    m_builtIn->setCards(std::move(builtInCards));
    rebuild();
}

void ThemesTab::rebuild()
{
    std::vector<ThemeCard *> cards;
    for (const ThemeDefinition &definition : m_themes.customThemes()) {
        auto *card = new ThemeCard(definition);
        connect(card, &QAbstractButton::clicked, this, [this, id = definition.id] { m_themes.select(id); });
        connect(card, &ThemeCard::actionRequested, this, &ThemesTab::onCardAction);
        cards.push_back(card);
    }
    m_emptyHint->setVisible(cards.empty());
    m_custom->setVisible(!cards.empty());
    m_custom->setCards(std::move(cards));
    refreshActive();
}

void ThemesTab::refreshActive()
{
    const QString active = m_themes.activeId();
    for (ThemeGrid *grid : {m_builtIn, m_custom}) {
        for (ThemeCard *card : grid->cards()) {
            card->setActive(card->themeId() == active);
        }
    }
}

bool ThemesTab::isEditing() const
{
    return !m_builder->isHidden();
}

void ThemesTab::leave()
{
    if (!m_builder->isHidden()) {
        m_builder->discard();
        closeBuilder();
    }
}

void ThemesTab::openBuilder(const ThemeDefinition &draft, bool isNew)
{
    m_builder->edit(draft, isNew, m_themes.kind());
    m_gallery->hide();
    m_builder->show();
    emit editingChanged(true);
}

void ThemesTab::closeBuilder()
{
    m_builder->hide();
    m_gallery->show();
    refreshActive();
    emit editingChanged(false);
}

void ThemesTab::createTheme()
{
    const auto active = m_themes.definition(m_themes.activeId());
    ThemeDefinition draft = active ? *active : *findBuiltInTheme(defaultThemeId());
    draft.id.clear();
    draft.builtIn = false;
    draft.name = m_themes.suggestName(tr("My Theme"));
    openBuilder(draft, true);
}

void ThemesTab::onCardAction(const QString &id, ThemeCard::Action action)
{
    const auto found = m_themes.definition(id);
    if (!found) {
        return;
    }
    switch (action) {
    case ThemeCard::Action::Edit:
        openBuilder(*found, false);
        break;
    case ThemeCard::Action::Duplicate: {
        ThemeDefinition draft = *found;
        draft.id.clear();
        draft.builtIn = false;
        draft.name = m_themes.suggestName(tr("%1 Copy").arg(found->name));
        openBuilder(draft, true);
        break;
    }
    case ThemeCard::Action::Export:
        exportTheme(id);
        break;
    case ThemeCard::Action::Delete:
        deleteTheme(id);
        break;
    }
}

void ThemesTab::importTheme()
{
    const auto path = chooseThemeImportFile(this);
    if (!path) {
        return;
    }
    auto imported = m_themes.importFile(*path);
    if (!imported) {
        notify(ToastType::Error, imported.error().userMessage);
        return;
    }
    m_themes.select(imported.value());
    notify(ToastType::Success, trUtf8("Theme imported and applied."));
}

void ThemesTab::exportTheme(const QString &id)
{
    const auto found = m_themes.definition(id);
    if (!found) {
        return;
    }
    const auto path = chooseThemeExportPath(this, fileStem(found->name) + QStringLiteral("-theme.json"));
    if (!path) {
        return;
    }
    if (auto written = m_themes.exportFile(id, *path); !written) {
        notify(ToastType::Error, written.error().userMessage);
        return;
    }
    notify(ToastType::Success, trUtf8("Theme exported successfully."));
}

void ThemesTab::deleteTheme(const QString &id)
{
    QPointer<ThemesTab> self(this);
    confirm()->ask(tr("Delete this theme? This cannot be undone."), [self, id] {
        if (!self) {
            return;
        }
        if (self->m_themes.remove(id)) {
            self->notify(ToastType::Success, trUtf8("Theme deleted."));
        } else {
            self->notify(ToastType::Error, trUtf8("The theme could not be deleted."));
        }
    });
}

ConfirmPopup *ThemesTab::confirm()
{
    if (!m_confirm) {
        m_confirm = new ConfirmPopup(window());
    }
    return m_confirm;
}

void ThemesTab::refreshModeChoice()
{
    const QSignalBlocker blocker(m_modeChoice);
    m_modeChoice->setCurrentIndex(m_themes.followsSystem() ? 0 : (m_themes.kind() == AppTheme::Dark ? 2 : 1));
}

void ThemesTab::notify(ToastType type, const std::string &message)
{
    emit toastRequested(Toast{type, {}, message, kDefaultToastMs});
}

}  // namespace sky
