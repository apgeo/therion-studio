#include "ApplicationStartupBootstrap.h"
#include "ApplicationLanguageOverride.h"

#include <QApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

#include <array>
#include <memory>
#include <vector>

namespace
{
constexpr auto kApplicationLanguageKey = "settings/applicationLanguage";

void applyApplicationIdentity(QApplication &application)
{
    application.setApplicationName(QStringLiteral("Therion Studio"));
    application.setOrganizationName(QStringLiteral("Therion Studio"));
    application.setOrganizationDomain(QStringLiteral("therionstudio.example"));
    application.setWindowIcon(QIcon(QStringLiteral(":/resources/app/app-icon.png")));
}

QLocale preferredApplicationLocale()
{
    QString language = TherionStudio::Platform::applicationLanguageOverride();
    if (language == QStringLiteral("system")) {
        language = QSettings()
            .value(QString::fromLatin1(kApplicationLanguageKey), QStringLiteral("system"))
            .toString();
    }
    language = TherionStudio::Platform::normalizeApplicationLanguageSetting(language);
    if (language == QStringLiteral("cs") || language == QStringLiteral("sk")) {
        return QLocale(language);
    }
    if (language == QStringLiteral("en")) {
        return QLocale::c();
    }
    return QLocale();
}

std::unique_ptr<QTranslator> loadTranslator(const QLocale &locale,
                                            const QString &baseName,
                                            const QString &directory)
{
    auto translator = std::make_unique<QTranslator>();
    if (!translator->load(locale, baseName, QStringLiteral("_"), directory)) {
        return nullptr;
    }
    return translator;
}

std::vector<std::unique_ptr<QTranslator>> installApplicationTranslators(
    QApplication &application)
{
    const QLocale locale = preferredApplicationLocale();
    if (locale.language() == QLocale::C || locale.language() == QLocale::English) {
        return {};
    }

    std::vector<std::unique_ptr<QTranslator>> translators;

    const QString qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    constexpr std::array<const char *, 2> qtTranslationCatalogs = {"qtbase", "qt"};
    for (const char *catalog : qtTranslationCatalogs) {
        auto translator = loadTranslator(locale,
                                         QString::fromLatin1(catalog),
                                         qtTranslationsPath);
        if (translator == nullptr) {
            continue;
        }
        application.installTranslator(translator.get());
        translators.push_back(std::move(translator));
    }

    auto applicationTranslator = loadTranslator(locale,
                                                QStringLiteral("therion_studio"),
                                                QStringLiteral(":/i18n"));
    if (applicationTranslator != nullptr) {
        application.installTranslator(applicationTranslator.get());
        translators.push_back(std::move(applicationTranslator));
    }

    return translators;
}
} // namespace

namespace TherionStudio
{

ApplicationStartupState initializeApplicationStartup(QApplication &application)
{
    applyApplicationIdentity(application);

    ApplicationStartupState state;
    state.translators = installApplicationTranslators(application);
    return state;
}

} // namespace TherionStudio
