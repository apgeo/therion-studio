#pragma once

#include <QString>

namespace TherionStudio
{
class TherionExecutableSelectionController final
{
public:
    struct SelectionResult
    {
        bool isAccepted = false;
        bool showWarningDialog = false;
        QString warningDialogTitle;
        QString warningDialogMessage;
        bool shouldUpdateExecutableText = false;
        QString updatedExecutableText;
        bool shouldShowStatusBarMessage = false;
        QString statusBarMessage;
        int statusBarTimeoutMs = 0;
    };

    static QString initialBrowsePath(const QString &currentExecutableText);
    static SelectionResult evaluateSelection(const QString &selectedExecutablePath);
};
}
