#include "app/MainWindow.h"
#include "core/CommandCatalogStore.h"
#include "core/SessionStore.h"
#include "platform/ApplicationBootstrap.h"

#include <QApplication>

#include <memory>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    const TherionStudio::ApplicationStartupState startupState = TherionStudio::initializeApplicationBootstrap(application);
    (void)startupState;

    auto *window = new MainWindow(std::make_unique<TherionStudio::SessionSettingsStore>(),
                                  TherionStudio::CommandCatalogStore());
    window->show();

    return application.exec();
}
