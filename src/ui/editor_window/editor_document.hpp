#pragma once

#include <string>
#include <vector>

#include "model/key_map.hpp"

namespace sky {

class EditorDocument {
public:
    EditorDocument() = default;
    explicit EditorDocument(const KeyMap &keyMap);

    int cellCount() const { return static_cast<int>(m_current.size()); }
    double timeAt(int cell) const;
    const std::vector<std::string> &keysAt(int cell) const;
    bool hasKey(int cell, const std::string &key) const;
    bool toggle(int cell, const std::string &key);

    bool isDirty() const { return m_current != m_saved; }
    void discard() { m_current = m_saved; }
    void markSaved() { m_saved = m_current; }
    KeyMap keyMap() const { return KeyMap(m_current); }

    static int keyIndex(const std::string &key);
    static const std::string &keyAt(int index);

private:
    std::vector<KeyMapEntry> m_current;
    std::vector<KeyMapEntry> m_saved;
};

}  // namespace sky
