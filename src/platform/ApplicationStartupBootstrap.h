#pragma once

#include <memory>
#include <vector>
#include <QTranslator>

class QApplication;

namespace TherionStudio
{
struct ApplicationStartupState
{
    std::vector<std::unique_ptr<QTranslator>> translators;
};

ApplicationStartupState initializeApplicationStartup(QApplication &application);
}
