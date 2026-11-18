#include "skyinput/global_hotkey.hpp"

#include <windows.h>

#include "hotkey_thread.hpp"

namespace skyinput {

namespace {

unsigned toNative(Modifiers modifiers)
{
    unsigned flags = 0;
    if (modifiers.alt) {
        flags |= MOD_ALT;
    }
    if (modifiers.control) {
        flags |= MOD_CONTROL;
    }
    if (modifiers.shift) {
        flags |= MOD_SHIFT;
    }
    if (modifiers.win) {
        flags |= MOD_WIN;
    }
    return flags;
}

}  // namespace

GlobalHotkey::GlobalHotkey(int id, KeyCode key, Modifiers modifiers, Callback callback)
    : m_id(id), m_key(key), m_modifiers(modifiers), m_callback(std::move(callback))
{
}

Result<std::unique_ptr<GlobalHotkey>> GlobalHotkey::create(KeyCode key, Modifiers modifiers, Callback onPress)
{
    HotkeyThread &thread = HotkeyThread::instance();
    std::unique_ptr<GlobalHotkey> hotkey(new GlobalHotkey(thread.allocateId(), key, modifiers, std::move(onPress)));
    if (Result<void> added = thread.add(hotkey->m_id, virtualKey(key), toNative(modifiers), hotkey->m_callback); !added) {
        return added.error();
    }
    return hotkey;
}

GlobalHotkey::~GlobalHotkey()
{
    HotkeyThread::instance().remove(m_id);
}

Result<void> GlobalHotkey::reassign(KeyCode key, Modifiers modifiers)
{
    m_key = key;
    m_modifiers = modifiers;
    return HotkeyThread::instance().add(m_id, virtualKey(key), toNative(modifiers), m_callback);
}

void GlobalHotkey::unregister()
{
    HotkeyThread::instance().deactivate(m_id);
}

bool GlobalHotkey::isRegistered() const
{
    return HotkeyThread::instance().isActive(m_id);
}

bool GlobalHotkey::isDown() const
{
    return GetAsyncKeyState(virtualKey(m_key)) < 0;
}

void GlobalHotkey::unregisterAll()
{
    HotkeyThread::instance().deactivateAll();
}

}  // namespace skyinput
