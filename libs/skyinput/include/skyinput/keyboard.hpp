#pragma once

#include <array>
#include <chrono>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

#include "skyinput/error.hpp"
#include "skyinput/key_code.hpp"
#include "skyinput/sender.hpp"

namespace skyinput {

using Duration = std::chrono::milliseconds;

inline constexpr Duration kDefaultDelay{35};
inline constexpr Duration kMicroDelay{3};

class Keyboard {
public:
    explicit Keyboard(ISender &sender);
    ~Keyboard();

    Keyboard(const Keyboard &) = delete;
    Keyboard &operator=(const Keyboard &) = delete;

    Result<void> toggleKey(KeyCode key, bool down);
    Result<void> toggleKeys(std::span<const KeyCode> keys, bool down);
    Result<void> sendKey(KeyCode key, Duration hold = kDefaultDelay, Duration after = Duration::zero());
    Result<void> sendKeys(std::span<const KeyCode> keys, Duration hold = kDefaultDelay,
                          Duration afterRelease = kDefaultDelay, Duration tail = Duration::zero());
    Result<void> printText(std::wstring_view text, Duration perChar = Duration::zero());

    void releaseAll();
    std::vector<KeyCode> held() const;
    bool isHeld(KeyCode key) const;

private:
    void markHeld(KeyCode key, bool down);

    ISender &m_sender;
    mutable std::mutex m_mutex;
    std::array<bool, 256> m_held{};
};

}  // namespace skyinput
