#include "skyinput/keyboard.hpp"

#include <thread>

#include "skyinput/panic_release.hpp"

namespace skyinput {

namespace {

void pause(Duration duration)
{
    if (duration > Duration::zero()) {
        std::this_thread::sleep_for(duration);
    }
}

}  // namespace

Keyboard::Keyboard(ISender &sender) : m_sender(sender) {}

Keyboard::~Keyboard()
{
    releaseAll();
}

void Keyboard::markHeld(KeyCode key, bool down)
{
    const unsigned slot = virtualKey(key);
    if (slot >= m_held.size()) {
        return;
    }
    std::lock_guard lock(m_mutex);
    m_held[slot] = down;
}

Result<void> Keyboard::toggleKey(KeyCode key, bool down)
{
    if (down) {
        panicNoteKeyDown(key);
        markHeld(key, true);
        return m_sender.toggleKey(key, true);
    }
    Result<void> result = m_sender.toggleKey(key, false);
    markHeld(key, false);
    panicNoteKeyUp(key);
    return result;
}

Result<void> Keyboard::toggleKeys(std::span<const KeyCode> keys, bool down)
{
    if (keys.empty()) {
        return {};
    }
    const std::size_t last = keys.size() - 1;
    Result<void> status;
    if (down) {
        for (std::size_t i = 0; i < last; ++i) {
            if (Result<void> r = toggleKey(keys[i], true); !r && status) {
                status = r;
            }
            pause(kMicroDelay);
        }
        if (Result<void> r = toggleKey(keys[last], true); !r && status) {
            status = r;
        }
    } else {
        for (std::size_t i = last; i > 0; --i) {
            if (Result<void> r = toggleKey(keys[i], false); !r && status) {
                status = r;
            }
            pause(kMicroDelay);
        }
        if (Result<void> r = toggleKey(keys[0], false); !r && status) {
            status = r;
        }
    }
    return status;
}

Result<void> Keyboard::sendKey(KeyCode key, Duration hold, Duration after)
{
    Result<void> pressed = toggleKey(key, true);
    pause(hold);
    Result<void> released = toggleKey(key, false);
    pause(after);
    return pressed ? released : pressed;
}

Result<void> Keyboard::sendKeys(std::span<const KeyCode> keys, Duration hold, Duration afterRelease,
                                Duration tail)
{
    if (keys.empty()) {
        return {};
    }
    Result<void> status;
    for (std::size_t i = 0; i + 1 < keys.size(); ++i) {
        if (Result<void> r = sendKey(keys[i], hold, afterRelease); !r && status) {
            status = r;
        }
    }
    if (Result<void> r = sendKey(keys.back(), hold, tail); !r && status) {
        status = r;
    }
    return status;
}

Result<void> Keyboard::printText(std::wstring_view text, Duration perChar)
{
    if (text.empty()) {
        return {};
    }
    if (perChar == Duration::zero()) {
        return m_sender.printChars(text);
    }
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t length = 1;
        if (text[pos] >= 0xD800 && text[pos] <= 0xDBFF && pos + 1 < text.size()) {
            length = 2;
        }
        if (Result<void> r = m_sender.printChars(text.substr(pos, length)); !r) {
            return r;
        }
        pos += length;
        if (pos < text.size()) {
            pause(perChar);
        }
    }
    return {};
}

void Keyboard::releaseAll()
{
    for (KeyCode key : held()) {
        (void)toggleKey(key, false);
    }
}

std::vector<KeyCode> Keyboard::held() const
{
    std::vector<KeyCode> keys;
    std::lock_guard lock(m_mutex);
    for (std::size_t slot = 0; slot < m_held.size(); ++slot) {
        if (m_held[slot]) {
            keys.push_back(keyCodeFromVirtualKey(static_cast<std::uint16_t>(slot)));
        }
    }
    return keys;
}

bool Keyboard::isHeld(KeyCode key) const
{
    const unsigned slot = virtualKey(key);
    std::lock_guard lock(m_mutex);
    return slot < m_held.size() && m_held[slot];
}

}  // namespace skyinput
