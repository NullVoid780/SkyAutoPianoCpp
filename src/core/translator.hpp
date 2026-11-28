#pragma once

#include <string>
#include <vector>

#include <QHash>
#include <QString>
#include <QTranslator>

namespace sky {

struct LanguageInfo {
    const char *code;
    const char *nativeName;
};

const std::vector<LanguageInfo> &supportedLanguages();
QString resolveLanguage(const QString &preference);
QString resolveLanguage(const QString &preference, const QStringList &systemLanguages);

QString tr(const char *text);
QString trText(const QString &text);
std::string trUtf8(const char *text);
QString trCount(long long count, const char *one, const char *many);
constexpr const char *trNoop(const char *text) { return text; }

class Translator final : public QTranslator {
public:
    bool loadLanguage(const QString &language);
    bool loadTable(const QString &language, const QByteArray &json);

    QString language() const { return m_language; }
    QString lookup(const QString &source) const;
    int size() const { return static_cast<int>(m_table.size()); }

    bool isEmpty() const override;
    QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr,
                      int n = -1) const override;

private:
    QString m_language = QStringLiteral("en");
    QHash<QString, QString> m_table;
};

QString installLanguage(const QString &preference);
QString currentLanguage();

}  // namespace sky
