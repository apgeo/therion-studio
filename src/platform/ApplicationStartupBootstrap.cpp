#include "ApplicationStartupBootstrap.h"

#include <QApplication>
#include <QIcon>
#include <QLocale>
#include <QTranslator>

namespace
{
void applyApplicationIdentity(QApplication &application)
{
    application.setApplicationName(QStringLiteral("Therion Studio"));
    application.setOrganizationName(QStringLiteral("Therion Studio"));
    application.setOrganizationDomain(QStringLiteral("therionstudio.example"));
    application.setWindowIcon(QIcon(QStringLiteral(":/resources/app/app-icon.png")));
}

std::unique_ptr<QTranslator> installApplicationTranslator(QApplication &application)
{
    auto translator = std::make_unique<QTranslator>();
    if (!translator->load(QLocale(),
                          QStringLiteral("therion_studio"),
                          QStringLiteral("_"),
                          QStringLiteral(":/i18n"))) {
        return nullptr;
    }

    application.installTranslator(translator.get());
    return translator;
}
} // namespace

namespace TherionStudio
{

ApplicationStartupState initializeApplicationStartup(QApplication &application)
{
    applyApplicationIdentity(application);

    ApplicationStartupState state;
    state.translator = installApplicationTranslator(application);
    return state;
}

} // namespace TherionStudio
