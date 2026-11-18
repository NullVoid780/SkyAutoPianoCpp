#include "ui/settings_window/shortcuts_tab.hpp"

#include <QVBoxLayout>

#include "core/string_utils.hpp"
#include "ui/settings_window/shortcut_edit.hpp"
#include "ui/settings_window/shortcut_row.hpp"
#include "ui/settings_window/tab_header.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kHeaderGap = 20;
constexpr int kRowGap = 12;
constexpr int kBottomGap = 20;
constexpr qreal kFieldRadius = 8;

}  // namespace

ShortcutsTab::ShortcutsTab(QWidget *parent) : QWidget(parent)
{
    auto *header = new TabHeader(QStringLiteral("lightning"), [] { return currentTheme().settings.navIconLightning; },
                                 QStringLiteral("Keyboard Shortcuts"),
                                 QStringLiteral("Configure global shortcuts for quick access to features"), this);

    struct RowSpec {
        const char *icon;
        const char *label;
        int field;
    };
    constexpr std::array<RowSpec, 6> kRows = {{
        {"skip-backward", "Previous Song", 0},
        {"play-circle", "Play Current Song", 1},
        {"skip-forward", "Next Song", 2},
        {"eye", "Show/Hide App", -1},
        {"speedometer2", "Increase Speed", 3},
        {"speedometer", "Decrease Speed", 4},
    }};

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(header);
    column->addSpacing(kHeaderGap);
    for (std::size_t i = 0; i < kRows.size(); ++i) {
        const RowSpec &spec = kRows[i];
        auto *row = new ShortcutRow(QString::fromLatin1(spec.icon), QString::fromLatin1(spec.label), this);
        ShortcutEdit *field = row->field();
        if (spec.field < 0) {
            field->setText(QStringLiteral("Alt+Tab"));
            field->setCaptureEnabled(false);
        } else {
            m_fields[static_cast<std::size_t>(spec.field)] = field;
            row->addChildShadow(field, kFieldRadius, [field]() -> std::optional<ShadowSpec> {
                if (!field->hasFocus()) {
                    return std::nullopt;
                }
                return ShadowSpec{3, {0, 0}, currentTheme().settings.shortcutRing};
            });
        }
        column->addWidget(row);
        if (i + 1 < kRows.size()) {
            column->addSpacing(kRowGap);
        }
    }
    column->addSpacing(kBottomGap);
    column->addStretch();
}

void ShortcutsTab::load(const Config &config)
{
    const ShortcutConfig &s = config.shortcut;
    const std::array<const std::string *, 5> values = {&s.pre, &s.play, &s.next, &s.increaseSpeed, &s.decreaseSpeed};
    for (std::size_t i = 0; i < m_fields.size(); ++i) {
        m_fields[i]->setText(toQString(*values[i]));
    }
}

void ShortcutsTab::collect(Config &config) const
{
    ShortcutConfig &s = config.shortcut;
    const std::array<std::string *, 5> values = {&s.pre, &s.play, &s.next, &s.increaseSpeed, &s.decreaseSpeed};
    for (std::size_t i = 0; i < m_fields.size(); ++i) {
        *values[i] = std::string(trimJs(fromQString(m_fields[i]->text())));
    }
}

}  // namespace sky
