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
        bool shouldUpdateExecutableText = false;
        QString updatedExecutableText;
        QString consoleMessage;
        QString statusLabelMessage;
    };

    static Presentation present(const TherionRunnerService::StartResult &startResult,
                                const QString &executableInput,
                                const QString &argumentsText,
                                const QString &workingDirectory);
};
}
