#pragma once

#include <string>

#include "core/json.hpp"

namespace sky {


class SheetEntry {
public:
    SheetEntry() : m_row(OrderedJson::object()) {}
    explicit SheetEntry(OrderedJson row) : m_row(row.is_object() ? std::move(row) : OrderedJson::object()) {}

    const OrderedJson &json() const { return m_row; }
    OrderedJson &json() { return m_row; }

    std::string name() const { return text("name"); }
    std::string author() const { return text("author"); }
    std::string transcribedBy() const { return text("transcribedBy"); }
    std::string payloadFile() const { return text("keyMap"); }
    std::string source() const { return text("source"); }
    std::string sourceId() const { return text("sourceId"); }
    std::string sourceUrl() const { return text("sourceUrl"); }

    double bpm() const { return jsMember(m_row, "bpm"); }
    double pitchLevel() const { return jsMember(m_row, "pitchLevel"); }
    double bitsPerPage() const { return jsMember(m_row, "bitsPerPage"); }
    bool hasHoldNotes() const { return truthy("hasHoldNotes"); }
    bool isComposed() const { return truthy("isComposed"); }
    bool isStoreSheet() const { return !source().empty(); }

    const OrderedJson *field(const char *key) const
    {
        const auto it = m_row.find(key);
        return it == m_row.end() ? nullptr : &*it;
    }

    void set(const char *key, OrderedJson value) { m_row[key] = std::move(value); }

private:
    std::string text(const char *key) const
    {
        const OrderedJson *value = field(key);
        if (!value) {
            return {};
        }
        if (value->is_string()) {
            return value->get<std::string>();
        }
        if (value->is_number()) {
            return formatJsNumber(value->get<double>());
        }
        return {};
    }

    bool truthy(const char *key) const
    {
        const OrderedJson *value = field(key);
        return value && jsTruthy(*value);
    }

    OrderedJson m_row;
};

}  // namespace sky
