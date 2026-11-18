#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include "skyinput/error.hpp"
#include "skyinput/key_code.hpp"
#include "skyinput/mouse_button.hpp"

namespace skyinput {

class LowLevelHook {
public:
    using Callback = std::function<void()>;

    static Result<std::unique_ptr<LowLevelHook>> onKey(KeyCode key, bool down, Callback callback);
    static Result<std::unique_ptr<LowLevelHook>> onMouse(MouseButton button, bool down, Callback callback);
    static Result<std::unique_ptr<LowLevelHook>> onWheel(bool forward, Callback callback);
    static void unregisterAll();

    ~LowLevelHook();

    LowLevelHook(const LowLevelHook &) = delete;
    LowLevelHook &operator=(const LowLevelHook &) = delete;

    bool isPressed() const;

    struct State {
        unsigned message = 0;
        unsigned antagonist = 0;
        std::int32_t data = 0;
        std::atomic<bool> released{true};
        Callback callback;
    };

private:
    explicit LowLevelHook(std::shared_ptr<State> state);

    static Result<std::unique_ptr<LowLevelHook>> install(bool keyboard, std::shared_ptr<State> state);

    std::shared_ptr<State> m_state;
    int m_handlerId = 0;
};

}  // namespace skyinput
