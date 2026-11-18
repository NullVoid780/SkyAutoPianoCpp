#pragma once

class QWidget;

namespace sky {

class IconButton;

inline constexpr int kScrollTopThreshold = 280;

IconButton *createScrollTopButton(QWidget *parent);

}  // namespace sky
