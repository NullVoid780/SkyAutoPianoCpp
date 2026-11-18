#include "skyinput/input_blocker.hpp"

#include <windows.h>

#include <set>
#include <tuple>

#include "hook_thread.hpp"

namespace skyinput {

namespace {

constexpr std::size_t kMouseActionCount = 8;

struct BlockRule {
    unsigned down = 0;
    unsigned action = 0;
    std::int32_t virtualKey = 0;

    bool operator<(const BlockRule &other) const
    {
        return std::tie(down, action, virtualKey) < std::tie(other.down, other.action, other.virtualKey);
    }
};

struct RawBlocked {
    unsigned message = 0;
    std::int32_t virtualKey = 0;
};

struct DeviceState {
    std::set<BlockRule> disabled;
    std::set<BlockRule> enabled;
    std::set<std::uint64_t> pressed;
    bool inverted = false;
    int handlerId = 0;
};

struct BlockerState {
    DeviceState keyboard;
    DeviceState mouse;
    std::vector<RawBlocked> blocked;
};

BlockerState &state()
{
    static BlockerState instance;
    return instance;
}

std::optional<unsigned> releaseFor(unsigned message)
{
    switch (message) {
    case WM_LBUTTONDOWN:
        return WM_LBUTTONUP;
    case WM_RBUTTONDOWN:
        return WM_RBUTTONUP;
    case WM_MBUTTONDOWN:
        return WM_MBUTTONUP;
    case WM_XBUTTONDOWN:
        return WM_XBUTTONUP;
    case WM_KEYDOWN:
        return WM_KEYUP;
    default:
        return std::nullopt;
    }
}

std::uint64_t pressKey(unsigned message, std::int32_t virtualKey)
{
    return (static_cast<std::uint64_t>(message) << 32) | static_cast<std::uint32_t>(virtualKey);
}

BlockRule mouseRule(MouseAction action)
{
    BlockRule rule;
    switch (action) {
    case MouseAction::Move:
        rule.action = WM_MOUSEMOVE;
        break;
    case MouseAction::WheelForward:
        rule.action = WM_MOUSEWHEEL;
        rule.virtualKey = WHEEL_DELTA;
        break;
    case MouseAction::WheelBack:
        rule.action = WM_MOUSEWHEEL;
        rule.virtualKey = -WHEEL_DELTA;
        break;
    case MouseAction::Left:
        rule.down = WM_LBUTTONDOWN;
        break;
    case MouseAction::Right:
        rule.down = WM_RBUTTONDOWN;
        break;
    case MouseAction::Middle:
        rule.down = WM_MBUTTONDOWN;
        break;
    case MouseAction::X1:
        rule.down = WM_XBUTTONDOWN;
        rule.virtualKey = XBUTTON1;
        break;
    case MouseAction::X2:
        rule.down = WM_XBUTTONDOWN;
        rule.virtualKey = XBUTTON2;
        break;
    }
    if (rule.down != 0) {
        rule.action = *releaseFor(rule.down);
    }
    return rule;
}

BlockRule keyboardRule(KeyCode key)
{
    return BlockRule{WM_KEYDOWN, WM_KEYUP, virtualKey(key)};
}

bool shouldBlock(DeviceState &device, unsigned message, std::int32_t virtualKey)
{
    std::vector<RawBlocked> &blocked = state().blocked;
    if (device.inverted) {
        for (const BlockRule &rule : device.enabled) {
            if (rule.virtualKey == virtualKey && (message == rule.down || message == rule.action)) {
                return false;
            }
        }
        if (const auto release = releaseFor(message)) {
            if (device.pressed.insert(pressKey(*release, virtualKey)).second) {
                blocked.push_back({message, virtualKey});
            }
        } else if (message != WM_MOUSEMOVE) {
            device.pressed.erase(pressKey(message, virtualKey));
            blocked.push_back({message, virtualKey});
        }
        return true;
    }
    for (const BlockRule &rule : device.disabled) {
        if (rule.virtualKey != virtualKey) {
            continue;
        }
        const bool isDown = message == rule.down;
        if (!isDown && message != rule.action) {
            continue;
        }
        if (isDown) {
            if (device.pressed.insert(pressKey(rule.action, virtualKey)).second) {
                blocked.push_back({message, virtualKey});
            }
        } else if (message != WM_MOUSEMOVE) {
            device.pressed.erase(pressKey(rule.action, virtualKey));
            blocked.push_back({message, virtualKey});
        }
        return true;
    }
    return false;
}

Result<void> ensureHandler(DeviceState &device, HookKind kind)
{
    if (device.handlerId != 0) {
        return {};
    }
    auto added = HookThread::instance().addHandler(kind, HookGroup::Blocker, [&device](const HookEvent &event) {
        return !event.injected && shouldBlock(device, event.message, event.virtualKey);
    });
    if (!added) {
        return added.error();
    }
    device.handlerId = added.value();
    return {};
}

void cleanup(DeviceState &device)
{
    if (device.handlerId != 0) {
        HookThread::instance().removeHandler(device.handlerId);
        device.handlerId = 0;
    }
    device.disabled.clear();
    device.enabled.clear();
    device.inverted = false;
}

template <typename Item, typename ToRule>
Result<void> disableDevice(DeviceState &device, HookKind kind, const std::optional<std::vector<Item>> &items,
                           ToRule toRule)
{
    if (items) {
        if (device.inverted) {
            for (const Item &item : *items) {
                device.enabled.erase(toRule(item));
            }
            return {};
        }
        if (items->empty()) {
            return {};
        }
        for (const Item &item : *items) {
            device.disabled.insert(toRule(item));
        }
        return ensureHandler(device, kind);
    }
    if (device.inverted) {
        device.enabled.clear();
        return {};
    }
    device.inverted = true;
    return ensureHandler(device, kind);
}

template <typename Item, typename ToRule>
void enableDevice(DeviceState &device, const std::optional<std::vector<Item>> &items, ToRule toRule,
                  std::size_t everything)
{
    if (!items) {
        cleanup(device);
        return;
    }
    if (device.inverted) {
        for (const Item &item : *items) {
            device.enabled.insert(toRule(item));
        }
        if (device.enabled.size() == everything) {
            cleanup(device);
        }
        return;
    }
    for (const Item &item : *items) {
        device.disabled.erase(toRule(item));
    }
    if (device.disabled.empty()) {
        cleanup(device);
    }
}

BlockedInput describe(const RawBlocked &raw)
{
    if (raw.message == WM_KEYDOWN || raw.message == WM_KEYUP) {
        const std::string_view name = nameFromKey(keyCodeFromVirtualKey(static_cast<std::uint16_t>(raw.virtualKey)));
        return {BlockedInput::Device::Keyboard, std::string(name), raw.message == WM_KEYDOWN};
    }
    BlockedInput input{BlockedInput::Device::Mouse, {}, false};
    if (raw.message == WM_LBUTTONDOWN || raw.message == WM_LBUTTONUP) {
        input.button = "left";
        input.down = raw.message == WM_LBUTTONDOWN;
    } else if (raw.message == WM_RBUTTONDOWN || raw.message == WM_RBUTTONUP) {
        input.button = "right";
        input.down = raw.message == WM_RBUTTONDOWN;
    } else if (raw.message == WM_MBUTTONDOWN || raw.message == WM_MBUTTONUP) {
        input.button = "middle";
        input.down = raw.message == WM_MBUTTONDOWN;
    } else if (raw.virtualKey == WHEEL_DELTA || raw.virtualKey == -WHEEL_DELTA) {
        input.button = "wheel";
        input.down = raw.virtualKey == WHEEL_DELTA;
    } else {
        input.button = raw.virtualKey == XBUTTON1 ? "x1" : "x2";
        input.down = raw.message == WM_XBUTTONDOWN;
    }
    return input;
}

}  // namespace

Result<void> InputBlocker::disable(const InputSelection &selection)
{
    Result<void> result;
    HookThread::instance().execute([&] {
        BlockerState &s = state();
        if (Result<void> r = disableDevice(s.mouse, HookKind::Mouse, selection.mouse, mouseRule); !r) {
            result = r;
        }
        if (Result<void> r = disableDevice(s.keyboard, HookKind::Keyboard, selection.keyboard, keyboardRule); !r) {
            result = r;
        }
    });
    return result;
}

std::vector<BlockedInput> InputBlocker::enable(const InputSelection &selection)
{
    std::vector<BlockedInput> inputs;
    HookThread::instance().execute([&] {
        BlockerState &s = state();
        enableDevice(s.mouse, selection.mouse, mouseRule, kMouseActionCount);
        enableDevice(s.keyboard, selection.keyboard, keyboardRule, keyNames().size());
        for (const RawBlocked &raw : s.blocked) {
            inputs.push_back(describe(raw));
        }
        s.blocked.clear();
    });
    return inputs;
}

}  // namespace skyinput
