#include "app/MainWindow.h"
#include "core/CommandCatalogStore.h"
#include "core/SessionStore.h"
#include "platform/ApplicationAppearanceBootstrap.h"

#include <QApplication>
#include <QIcon>
#include <QLocale>
#include <QTranslator>

#include <memory>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Therion Studio"));
    application.setOrganizationName(QStringLiteral("Therion Studio"));
    application.setOrganizationDomain(QStringLiteral("therionstudio.example"));
    application.setWindowIcon(QIcon(QStringLiteral(":/resources/app/app-icon.png")));

    QTranslator applicationTranslator;
    if (applicationTranslator.load(QLocale(),
                                   QStringLiteral("therion_studio"),
                                   QStringLiteral("_"),
                                   QStringLiteral(":/i18n"))) {
        application.installTranslator(&applicationTranslator);
    }

    TherionStudio::initializeApplicationAppearance(application);

    auto *window = new MainWindow(std::make_unique<TherionStudio::SessionSettingsStore>(),
                                  TherionStudio::CommandCatalogStore());
    window->show();

    return application.exec();
}
