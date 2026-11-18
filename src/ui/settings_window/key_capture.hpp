#pragma once

#include <optional>

#include <QString>
#include <QStringList>

namespace sky {

struct MappedKey {
    QString value;
    QString label;
};

std::optional<MappedKey> keyboardKeyForVirtualKey(quint32 virtualKey);
QString keyboardDisplayLabel(const QString &value);

QString shortcutTokenForVirtualKey(quint32 virtualKey);
bool isModifierToken(const QString &token);
QString formatShortcut(const QStringList &tokens);
bool hasMainKey(const QStringList &tokens);

}  // namespace sky
