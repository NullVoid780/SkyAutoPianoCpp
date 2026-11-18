#pragma once

#include "skyinput/key_code.hpp"

namespace skyinput {

void panicNoteKeyDown(KeyCode key) noexcept;
void panicNoteKeyUp(KeyCode key) noexcept;
unsigned panicReleaseAll() noexcept;

}  // namespace skyinput
