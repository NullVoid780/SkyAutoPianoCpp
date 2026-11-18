#include "ui/editor_window/editor_document.hpp"

#include <algorithm>
#include <array>

#include "model/key_table.hpp"

namespace sky {

namespace {

const std::vector<std::string> kNoKeys;

const std::array<std::string, kSkyKeyCount> &keyNames()
{
    static const std::array<std::string, kSkyKeyCount> names = [] {
        std::array<std::string, kSkyKeyCount> result;
        for (int i = 0; i < kSkyKeyCount; ++i) {
            result[static_cast<std::size_t>(i)] = std::string(kAutoKeys[static_cast<std::size_t>(i)]);
        }
        return result;
    }();
    return names;
}

}  // namespace

EditorDocument::EditorDocument(const KeyMap &keyMap) : m_current(keyMap.entries())
{
    std::stable_sort(m_current.begin(), m_current.end(),
                     [](const KeyMapEntry &a, const KeyMapEntry &b) { return a.time < b.time; });
    m_saved = m_current;
}

double EditorDocument::timeAt(int cell) const
{
    return cell >= 0 && cell < cellCount() ? m_current[static_cast<std::size_t>(cell)].time : 0.0;
}

const std::vector<std::string> &EditorDocument::keysAt(int cell) const
{
    return cell >= 0 && cell < cellCount() ? m_current[static_cast<std::size_t>(cell)].keys : kNoKeys;
}

bool EditorDocument::hasKey(int cell, const std::string &key) const
{
    const auto &keys = keysAt(cell);
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

bool EditorDocument::toggle(int cell, const std::string &key)
{
    if (cell < 0 || cell >= cellCount() || keyIndex(key) < 0) {
        return false;
    }
    auto &keys = m_current[static_cast<std::size_t>(cell)].keys;
    const auto found = std::find(keys.begin(), keys.end(), key);
    if (found != keys.end()) {
        keys.erase(found);
        return false;
    }
    keys.push_back(key);
    return true;
}

int EditorDocument::keyIndex(const std::string &key)
{
    return autoKeyIndex(key);
}

const std::string &EditorDocument::keyAt(int index)
{
    return keyNames()[static_cast<std::size_t>(std::clamp(index, 0, kSkyKeyCount - 1))];
}

}  // namespace sky
