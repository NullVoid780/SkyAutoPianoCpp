#include "ui/settings_window/key_capture.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "core/string_utils.hpp"
#include "model/key_table.hpp"

namespace sky {

namespace {

constexpr quint32 kVkA = 0x41;
constexpr quint32 kVkZ = 0x5A;
constexpr quint32 kVk0 = 0x30;
constexpr quint32 kVk9 = 0x39;
constexpr quint32 kVkNumpad0 = 0x60;
constexpr quint32 kVkNumpad9 = 0x69;
constexpr quint32 kVkF1 = 0x70;
constexpr quint32 kVkF24 = 0x87;

struct NamedKey {
    quint32 virtualKey;
    const char *token;
    const char *value;
    const char *label;
};

constexpr std::array<NamedKey, 41> kNamedKeys = {{
    {0x6B, "NumAdd", "num+", "N+"},
    {0x6D, "NumSub", "num-", "N-"},
    {0x6A, "NumMult", "num*", "N*"},
    {0x6F, "NumDiv", "num/", "N/"},
    {0x6E, "NumDec", "num.", "N."},
    {0x6C, "NumComma", nullptr, nullptr},
    {0x20, "Space", "space", "Spc"},
    {0x0D, "Enter", "enter", "Ent"},
    {0x1B, "Escape", "escape", "Esc"},
    {0x08, "Backspace", "backspace", "Bksp"},
    {0x2E, "Delete", "delete", "Del"},
    {0x2D, "Insert", "insert", "Ins"},
    {0x24, "Home", "home", "Home"},
    {0x23, "End", "end", "End"},
    {0x21, "PageUp", "pageUp", "PgU"},
    {0x22, "PageDown", "pageDown", "PgD"},
    {0x26, "Up", "up", "Up"},
    {0x28, "Down", "down", "Dn"},
    {0x25, "Left", "left", "Lt"},
    {0x27, "Right", "right", "Rt"},
    {0xBD, "-", "-", "-"},
    {0xBB, "=", "=", "="},
    {0xDB, "[", "[", "["},
    {0xDD, "]", "]", "]"},
    {0xDC, "\\", "\\", "\\"},
    {0xBA, ";", ";", ";"},
    {0xDE, "'", "'", "'"},
    {0xBC, ",", ",", ","},
    {0xBE, ".", ".", "."},
    {0xBF, "/", "/", "/"},
    {0xC0, "`", "`", "`"},
    {0x09, "Tab", nullptr, nullptr},
    {0x14, "CapsLock", nullptr, nullptr},
    {0x90, "NumLock", nullptr, nullptr},
    {0x91, "ScrollLock", nullptr, nullptr},
    {0x2C, "PrintScreen", nullptr, nullptr},
    {0x13, "Pause", nullptr, nullptr},
    {0x10, "Shift", nullptr, nullptr},
    {0x11, "Ctrl", nullptr, nullptr},
    {0x12, "Alt", nullptr, nullptr},
    {0x5B, "Win", nullptr, nullptr},
}};

constexpr std::array<std::pair<quint32, quint32>, 7> kModifierAliases = {{
    {0xA0, 0x10},
    {0xA1, 0x10},
    {0xA2, 0x11},
    {0xA3, 0x11},
    {0xA4, 0x12},
    {0xA5, 0x12},
    {0x5C, 0x5B},
}};

constexpr std::array<const char *, 4> kModifierOrder = {"Alt", "Ctrl", "Shift", "Win"};

quint32 canonicalVirtualKey(quint32 virtualKey)
{
    for (const auto &[alias, canonical] : kModifierAliases) {
        if (alias == virtualKey) {
            return canonical;
        }
    }
    return virtualKey;
}

const NamedKey *namedKey(quint32 virtualKey)
{
    const quint32 canonical = canonicalVirtualKey(virtualKey);
    for (const NamedKey &key : kNamedKeys) {
        if (key.virtualKey == canonical) {
            return &key;
        }
    }
    return nullptr;
}

int modifierRank(const QString &token)
{
    for (std::size_t i = 0; i < kModifierOrder.size(); ++i) {
        if (token == QLatin1String(kModifierOrder[i])) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace

std::optional<MappedKey> keyboardKeyForVirtualKey(quint32 virtualKey)
{
    if (virtualKey >= kVkA && virtualKey <= kVkZ) {
        const QChar letter(static_cast<char16_t>(u'a' + (virtualKey - kVkA)));
        return MappedKey{QString(letter), QString(letter.toUpper())};
    }
    if (virtualKey >= kVk0 && virtualKey <= kVk9) {
        const QString digit = QString::number(virtualKey - kVk0);
        return MappedKey{digit, digit};
    }
    if (virtualKey >= kVkNumpad0 && virtualKey <= kVkNumpad9) {
        const QString digit = QString::number(virtualKey - kVkNumpad0);
        return MappedKey{QStringLiteral("num") + digit, QStringLiteral("N") + digit};
    }
    const NamedKey *key = namedKey(virtualKey);
    if (!key || !key->value) {
        return std::nullopt;
    }
    return MappedKey{QString::fromLatin1(key->value), QString::fromLatin1(key->label)};
}

QString keyboardDisplayLabel(const QString &value)
{
    const QString normalized = toQString(normalizeKeyboardMappingValue(fromQString(value)));
    if (normalized.isEmpty()) {
        return {};
    }
    for (const NamedKey &key : kNamedKeys) {
        if (key.value && normalized == QLatin1String(key.value)) {
            return QString::fromLatin1(key.label);
        }
    }
    if (normalized.size() == 4 && normalized.startsWith(QLatin1String("num")) && normalized.at(3).isDigit()) {
        return QStringLiteral("N") + normalized.at(3);
    }
    if (normalized.size() == 1) {
        return normalized.toUpper();
    }
    return normalized;
}

QString shortcutTokenForVirtualKey(quint32 virtualKey)
{
    if (virtualKey >= kVkA && virtualKey <= kVkZ) {
        return QString(QChar(static_cast<char16_t>(u'A' + (virtualKey - kVkA))));
    }
    if (virtualKey >= kVk0 && virtualKey <= kVk9) {
        return QString::number(virtualKey - kVk0);
    }
    if (virtualKey >= kVkNumpad0 && virtualKey <= kVkNumpad9) {
        return QStringLiteral("Num") + QString::number(virtualKey - kVkNumpad0);
    }
    if (virtualKey >= kVkF1 && virtualKey <= kVkF24) {
        return QStringLiteral("F") + QString::number(virtualKey - kVkF1 + 1);
    }
    const NamedKey *key = namedKey(virtualKey);
    return key ? QString::fromLatin1(key->token) : QString();
}

bool isModifierToken(const QString &token)
{
    return modifierRank(token) >= 0;
}

bool hasMainKey(const QStringList &tokens)
{
    return std::any_of(tokens.begin(), tokens.end(), [](const QString &token) { return !isModifierToken(token); });
}

QString formatShortcut(const QStringList &tokens)
{
    QStringList sorted = tokens;
    std::stable_sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b) {
        const int rankA = modifierRank(a);
        const int rankB = modifierRank(b);
        if ((rankA >= 0) != (rankB >= 0)) {
            return rankA >= 0;
        }
        if (rankA >= 0) {
            return rankA < rankB;
        }
        if ((a.size() == 1) != (b.size() == 1)) {
            return b.size() == 1;
        }
        return a < b;
    });
    return sorted.join(QLatin1Char('+'));
}

}  // namespace sky
