#pragma once

#include "ApplicationStartupBootstrap.h"

class QApplication;

namespace TherionStudio
{
ApplicationStartupState initializeApplicationBootstrap(QApplication &application);
}
