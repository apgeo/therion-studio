#pragma once

#include <memory>
#include <QTranslator>

class QApplication;

namespace TherionStudio
{
struct ApplicationStartupState
{
    std::unique_ptr<QTranslator> translator;
};

ApplicationStartupState initializeApplicationStartup(QApplication &application);
}
