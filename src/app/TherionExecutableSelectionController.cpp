#include "TherionExecutableSelectionController.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace
{
QString translate(const char *sourceText)
{
    return QCoreApplication::translate("MainWindow", sourceText);
}
}

namespace TherionStudio
{
QString TherionExecutableSelectionController::initialBrowsePath(const QString &currentExecutableText)
{
    const QString trimmedPath = currentExecutableText.trimmed();
    QFileInfo currentInfo(trimmedPath);
    if (trimmedPath.isEmpty() || !currentInfo.exists()) {
        return QDir::homePath();
    }
    if (currentInfo.isFile()) {
        return currentInfo.absolutePath();
    }
    return currentInfo.absoluteFilePath();
}

TherionExecutableSelectionController::SelectionResult
TherionExecutableSelectionController::evaluateSelection(const QString &selectedExecutablePath)
{
    SelectionResult result;

    if (selectedExecutablePath.isEmpty()) {
        return result;
    }

    QFileInfo selectedInfo(selectedExecutablePath);
    if (!selectedInfo.isExecutable()) {
        result.showWarningDialog = true;
        result.warningDialogTitle = translate("Select Therion Executable");
        result.warningDialogMessage = translate("The selected file is not executable.");
        return result;
    }

    result.isAccepted = true;
    result.shouldUpdateExecutableText = true;
    result.updatedExecutableText = selectedInfo.absoluteFilePath();
    result.shouldShowStatusBarMessage = true;
    result.statusBarMessage = translate("Therion executable updated");
    result.statusBarTimeoutMs = 2000;
    return result;
}
}
