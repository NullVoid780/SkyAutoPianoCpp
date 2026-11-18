#pragma once

#include <functional>
#include <memory>

#include "skyinput/error.hpp"
#include "skyinput/key_code.hpp"

namespace skyinput {

struct Modifiers {
    bool control = false;
    bool alt = false;
    bool shift = false;
    bool win = false;

    bool operator==(const Modifiers &) const = default;
};

class GlobalHotkey {
public:
    using Callback = std::function<void()>;

    static Result<std::unique_ptr<GlobalHotkey>> create(KeyCode key, Modifiers modifiers, Callback onPress);
    ~GlobalHotkey();

    GlobalHotkey(const GlobalHotkey &) = delete;
    GlobalHotkey &operator=(const GlobalHotkey &) = delete;

    Result<void> reassign(KeyCode key, Modifiers modifiers);
    void unregister();
    bool isRegistered() const;
    bool isDown() const;
    KeyCode key() const { return m_key; }
    Modifiers modifiers() const { return m_modifiers; }

    static void unregisterAll();

private:
    GlobalHotkey(int id, KeyCode key, Modifiers modifiers, Callback callback);

    int m_id;
    KeyCode m_key;
    Modifiers m_modifiers;
    Callback m_callback;
};

}  // namespace skyinput
