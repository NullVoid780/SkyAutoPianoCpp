#include "skyinput/panic_release.hpp"

#include <windows.h>

#include <atomic>

namespace skyinput {

namespace {

constexpr unsigned kKeySlots = 256;

std::atomic<std::uint16_t> g_downCounts[kKeySlots];
std::atomic_flag g_releasing = ATOMIC_FLAG_INIT;
INPUT g_releaseBatch[kKeySlots];

}  // namespace

void panicNoteKeyDown(KeyCode key) noexcept
{
    const unsigned slot = virtualKey(key);
    if (slot < kKeySlots) {
        g_downCounts[slot].fetch_add(1, std::memory_order_relaxed);
    }
}

void panicNoteKeyUp(KeyCode key) noexcept
{
    const unsigned slot = virtualKey(key);
    if (slot >= kKeySlots) {
        return;
    }
    std::uint16_t current = g_downCounts[slot].load(std::memory_order_relaxed);
    while (current > 0 &&
           !g_downCounts[slot].compare_exchange_weak(current, static_cast<std::uint16_t>(current - 1),
                                                     std::memory_order_relaxed)) {
    }
}

unsigned panicReleaseAll() noexcept
{
    if (g_releasing.test_and_set(std::memory_order_acquire)) {
        return 0;
    }
    unsigned count = 0;
    for (unsigned slot = 0; slot < kKeySlots; ++slot) {
        if (g_downCounts[slot].exchange(0, std::memory_order_relaxed) == 0) {
            continue;
        }
        const KeyCode key = keyCodeFromVirtualKey(static_cast<std::uint16_t>(slot));
        INPUT &input = g_releaseBatch[count++];
        input = INPUT{};
        input.type = INPUT_KEYBOARD;
        input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(slot, MAPVK_VK_TO_VSC));
        input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | (isExtendedKey(key) ? KEYEVENTF_EXTENDEDKEY : 0);
    }
    if (count > 0) {
        SendInput(count, g_releaseBatch, sizeof(INPUT));
    }
    g_releasing.clear(std::memory_order_release);
    return count;
}

}  // namespace skyinput
