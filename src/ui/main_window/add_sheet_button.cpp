#include "ui/main_window/add_sheet_button.hpp"

#include "ui/theme_manager.hpp"
#include "ui/widgets/icon_button.hpp"

namespace sky {

IconButton *createAddSheetButton(QWidget *parent)
{
    auto *button = new IconButton(QStringLiteral("plus"), 45, QSize(45, 45),
                                  IconButtonStyle{[] { return currentTheme().fontColor; },
                                                  [] { return currentTheme().hoverColor; }, {}, {}, 0, 0, kHoverFastMs},
                                  parent);
    button->setToolTip(QStringLiteral("Add sheet"));
    return button;
}

}  // namespace sky
