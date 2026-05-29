#include "app/MainWindow.h"
#include "core/CommandCatalogStore.h"
#include "core/SessionStore.h"
#include "platform/ApplicationAppearanceBootstrap.h"
#include "platform/ApplicationStartupBootstrap.h"

#include <QApplication>

#include <memory>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    const TherionStudio::ApplicationStartupState startupState = TherionStudio::initializeApplicationStartup(application);
    (void)startupState;

    TherionStudio::initializeApplicationAppearance(application);

    auto *window = new MainWindow(std::make_unique<TherionStudio::SessionSettingsStore>(),
                                  TherionStudio::CommandCatalogStore());
    window->show();

    return application.exec();
}
