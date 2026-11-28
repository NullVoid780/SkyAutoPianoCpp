#include "ui/main_window/nav_toggle.hpp"

#include "core/translator.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/icon_button.hpp"

namespace sky {

IconButton *createNavToggle(QWidget *parent)
{
    auto *button = new IconButton(QStringLiteral("list"), 26, QSize(42, 42),
                                  IconButtonStyle{[] { return currentTheme().fontColor; },
                                                  [] { return currentTheme().hoverColor; }, {}, {}, 8, 0, kHoverSlowMs},
                                  parent);
    button->setToolTip(tr("Open navigation"));
    button->setAccessibleName(tr("Open navigation"));
    return button;
}

}  // namespace sky
