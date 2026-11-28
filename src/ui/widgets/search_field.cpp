#include "ui/widgets/search_field.hpp"

#include <QAction>

#include "core/translator.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

SearchField::SearchField(QWidget *parent) : QLineEdit(parent), m_icon(new QAction(this))
{
    setObjectName(QStringLiteral("SearchField"));
    setPlaceholderText(tr("Search the music sheet or author name..."));
    setClearButtonEnabled(false);
    setAccessibleName(tr("Search"));
    m_icon->setText(tr("Search"));
    addAction(m_icon, QLineEdit::TrailingPosition);
    applyTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &SearchField::applyTheme);
}

void SearchField::applyTheme()
{
    const Theme &theme = currentTheme();
    m_icon->setIcon(IconCache::icon(QStringLiteral("search"), 20, theme.searchIcon));
    QPalette palette = this->palette();
    palette.setColor(QPalette::PlaceholderText, theme.searchPlaceholder);
    palette.setColor(QPalette::Text, theme.searchText);
    setPalette(palette);
}

}  // namespace sky
