#include "core/translator.hpp"

#include <memory>

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

namespace sky {

namespace {

constexpr const char *kContext = "sky";

[[maybe_unused]] constexpr const char *kQtTexts[] = {
    trNoop("&Undo"), trNoop("&Redo"),  trNoop("Cu&t"),       trNoop("&Copy"),
    trNoop("&Paste"), trNoop("Delete"), trNoop("Select All"), trNoop("Copy &Link Location"),
};

std::unique_ptr<Translator> &installed()
{
    static std::unique_ptr<Translator> translator;
    return translator;
}

QString matchLanguage(const QString &tag)
{
    const QString lower = tag.toLower().replace(QLatin1Char('_'), QLatin1Char('-'));
    const QString base = lower.section(QLatin1Char('-'), 0, 0);
    if (base == QLatin1String("zh")) {
        return QStringLiteral("zh_CN");
    }
    for (const LanguageInfo &language : supportedLanguages()) {
        if (base == QLatin1String(language.code)) {
            return QString::fromLatin1(language.code);
        }
    }
    return {};
}

}  // namespace

const std::vector<LanguageInfo> &supportedLanguages()
{
    static const std::vector<LanguageInfo> languages = {
        {"en", "English"},
        {"vi", "Tiếng Việt"},
        {"zh_CN", "简体中文"},
        {"ja", "日本語"},
        {"es", "Español"},
    };
    return languages;
}

QString resolveLanguage(const QString &preference, const QStringList &systemLanguages)
{
    for (const LanguageInfo &language : supportedLanguages()) {
        if (preference == QLatin1String(language.code)) {
            return preference;
        }
    }
    for (const QString &tag : systemLanguages) {
        if (const QString found = matchLanguage(tag); !found.isEmpty()) {
            return found;
        }
    }
    return QStringLiteral("en");
}

QString resolveLanguage(const QString &preference)
{
    return resolveLanguage(preference, QLocale::system().uiLanguages());
}

QString tr(const char *text)
{
    return QCoreApplication::translate(kContext, text);
}

QString trText(const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    return QCoreApplication::translate(kContext, utf8.constData());
}

std::string trUtf8(const char *text)
{
    return tr(text).toStdString();
}

QString trCount(long long count, const char *one, const char *many)
{
    return count == 1 ? tr(one) : tr(many).arg(count);
}

bool Translator::loadLanguage(const QString &language)
{
    if (language == QLatin1String("en")) {
        m_language = language;
        m_table.clear();
        return true;
    }
    QFile file(QStringLiteral(":/i18n/%1.json").arg(language));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    return loadTable(language, file.readAll());
}

bool Translator::loadTable(const QString &language, const QByteArray &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isObject()) {
        return false;
    }
    QHash<QString, QString> table;
    const QJsonObject object = document.object();
    for (auto it = object.begin(); it != object.end(); ++it) {
        const QString value = it.value().toString();
        if (!value.isEmpty()) {
            table.insert(it.key(), value);
        }
    }
    m_language = language;
    m_table = std::move(table);
    return true;
}

QString Translator::lookup(const QString &source) const
{
    return m_table.value(source);
}

bool Translator::isEmpty() const
{
    return m_table.isEmpty();
}

QString Translator::translate(const char *, const char *sourceText, const char *, int) const
{
    if (!sourceText) {
        return {};
    }
    return m_table.value(QString::fromUtf8(sourceText));
}

QString installLanguage(const QString &preference)
{
    const QString language = resolveLanguage(preference);
    auto &translator = installed();
    if (translator) {
        QCoreApplication::removeTranslator(translator.get());
    }
    translator = std::make_unique<Translator>();
    if (!translator->loadLanguage(language)) {
        translator->loadLanguage(QStringLiteral("en"));
    }
    QCoreApplication::installTranslator(translator.get());
    QLocale::setDefault(QLocale(translator->language()));
    return translator->language();
}

QString currentLanguage()
{
    const auto &translator = installed();
    return translator ? translator->language() : QStringLiteral("en");
}

}  // namespace sky
