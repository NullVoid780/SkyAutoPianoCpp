#include "ui/main_window/scroll_top_button.hpp"

#include "ui/theme_manager.hpp"
#include "ui/widgets/icon_button.hpp"

namespace sky {

IconButton *createScrollTopButton(QWidget *parent)
{
    auto *button = new IconButton(QStringLiteral("arrow-up"), 16, QSize(30, 30),
                                  IconButtonStyle{[] { return currentTheme().fontColor; },
                                                  [] { return currentTheme().hoverColor; },
                                                  [] { return currentTheme().background; },
                                                  [] { return currentTheme().neutralBorder; }, 8, 0, kHoverMs},
                                  parent);
    button->setToolTip(QStringLiteral("Back to top"));
    button->hide();
    return button;
}

}  // namespace sky
