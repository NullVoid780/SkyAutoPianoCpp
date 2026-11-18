#include "skyinput/low_level_hook.hpp"

#include <windows.h>

#include "hook_thread.hpp"

namespace skyinput {

namespace {

void assignMessages(LowLevelHook::State &state, bool down, unsigned downMessage, unsigned upMessage)
{
    if (down) {
        state.message = downMessage;
        state.antagonist = upMessage;
    } else {
        state.message = upMessage;
        state.antagonist = 0;
    }
}

bool deliver(LowLevelHook::State &state, const HookEvent &event)
{
    if (event.injected || event.virtualKey != state.data) {
        return false;
    }
    if (state.antagonist != 0) {
        if (event.message == state.antagonist) {
            state.released.store(true);
        } else if (state.released.load() && event.message == state.message) {
            state.released.store(false);
            if (state.callback) {
                state.callback();
            }
        }
    } else if (event.message == state.message && state.callback) {
        state.callback();
    }
    return false;
}

}  // namespace

LowLevelHook::LowLevelHook(std::shared_ptr<State> state) : m_state(std::move(state)) {}

Result<std::unique_ptr<LowLevelHook>> LowLevelHook::install(bool keyboard, std::shared_ptr<State> state)
{
    std::unique_ptr<LowLevelHook> hook(new LowLevelHook(state));
    auto added = HookThread::instance().addHandler(
        keyboard ? HookKind::Keyboard : HookKind::Mouse, HookGroup::Listener,
        [state](const HookEvent &event) { return deliver(*state, event); });
    if (!added) {
        return added.error();
    }
    hook->m_handlerId = added.value();
    return hook;
}

Result<std::unique_ptr<LowLevelHook>> LowLevelHook::onKey(KeyCode key, bool down, Callback callback)
{
    auto state = std::make_shared<State>();
    assignMessages(*state, down, WM_KEYDOWN, WM_KEYUP);
    state->data = virtualKey(key);
    state->callback = std::move(callback);
    return install(true, std::move(state));
}

Result<std::unique_ptr<LowLevelHook>> LowLevelHook::onMouse(MouseButton button, bool down, Callback callback)
{
    auto state = std::make_shared<State>();
    switch (button) {
    case MouseButton::Left:
        assignMessages(*state, down, WM_LBUTTONDOWN, WM_LBUTTONUP);
        break;
    case MouseButton::Right:
        assignMessages(*state, down, WM_RBUTTONDOWN, WM_RBUTTONUP);
        break;
    case MouseButton::Middle:
        assignMessages(*state, down, WM_MBUTTONDOWN, WM_MBUTTONUP);
        break;
    case MouseButton::X1:
        assignMessages(*state, down, WM_XBUTTONDOWN, WM_XBUTTONUP);
        state->data = XBUTTON1;
        break;
    case MouseButton::X2:
        assignMessages(*state, down, WM_XBUTTONDOWN, WM_XBUTTONUP);
        state->data = XBUTTON2;
        break;
    }
    state->callback = std::move(callback);
    return install(false, std::move(state));
}

Result<std::unique_ptr<LowLevelHook>> LowLevelHook::onWheel(bool forward, Callback callback)
{
    auto state = std::make_shared<State>();
    state->message = WM_MOUSEWHEEL;
    state->antagonist = 0;
    state->data = forward ? WHEEL_DELTA : -WHEEL_DELTA;
    state->callback = std::move(callback);
    return install(false, std::move(state));
}

void LowLevelHook::unregisterAll()
{
    HookThread::instance().removeGroup(HookGroup::Listener);
}

LowLevelHook::~LowLevelHook()
{
    HookThread::instance().removeHandler(m_handlerId);
}

bool LowLevelHook::isPressed() const
{
    return !m_state->released.load();
}

}  // namespace skyinput
