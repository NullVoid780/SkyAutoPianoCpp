#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "core/json.hpp"

namespace sky {

struct KeyMapEntry {
    double time = 0.0;
    std::vector<std::string> keys;

    bool operator==(const KeyMapEntry &) const = default;
};

class KeyMap {
public:
    KeyMap() = default;
    explicit KeyMap(std::vector<KeyMapEntry> entries) : m_entries(std::move(entries)) {}

    const std::vector<KeyMapEntry> &entries() const { return m_entries; }
    bool empty() const { return m_entries.empty(); }
    std::size_t size() const { return m_entries.size(); }
    const KeyMapEntry &operator[](std::size_t index) const { return m_entries[index]; }

    double lastTime() const { return m_entries.empty() ? 0.0 : m_entries.back().time; }

    KeyMap filteredFrom(double startMs) const
    {
        std::vector<KeyMapEntry> kept;
        for (const KeyMapEntry &entry : m_entries) {
            if (entry.time >= startMs) {
                kept.push_back(entry);
            }
        }
        return KeyMap(std::move(kept));
    }


    OrderedJson toJson() const
    {
        OrderedJson object = OrderedJson::object();
        auto append = [&object](const KeyMapEntry &entry) {
            OrderedJson keys = OrderedJson::array();
            for (const std::string &key : entry.keys) {
                keys.push_back(key);
            }
            object[formatJsNumber(entry.time)] = std::move(keys);
        };
        for (const KeyMapEntry &entry : m_entries) {
            if (isArrayIndex(entry.time)) {
                append(entry);
            }
        }
        for (const KeyMapEntry &entry : m_entries) {
            if (!isArrayIndex(entry.time)) {
                append(entry);
            }
        }
        return object;
    }

    bool operator==(const KeyMap &) const = default;

private:
    static bool isArrayIndex(double time)
    {
        return time >= 0.0 && time < 4294967295.0 && std::trunc(time) == time;
    }

    std::vector<KeyMapEntry> m_entries;
};

}  // namespace sky
