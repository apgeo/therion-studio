#include "ApplicationBootstrap.h"

#include "ApplicationAppearanceBootstrap.h"
#include "ApplicationStartupBootstrap.h"

#include <QApplication>

namespace TherionStudio
{

ApplicationStartupState initializeApplicationBootstrap(QApplication &application)
{
    ApplicationStartupState state = initializeApplicationStartup(application);
    initializeApplicationAppearance(application);
    return state;
}

} // namespace TherionStudio
