#pragma once

#include <QString>

#include "TherionRunnerService.h"

namespace TherionStudio
{
class TherionRunnerStartSuccessPresenter final
{
public:
    struct Presentation
    {
        QString consoleMessage;
        QString statusLabelMessage;
    };

    static Presentation present(const TherionRunnerService::StartResult &startResult,
                                const QString &argumentsText,
                                const QString &workingDirectory);
};
}
