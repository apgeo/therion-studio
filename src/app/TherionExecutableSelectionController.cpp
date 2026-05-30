#include "TherionExecutableSelectionController.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

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
        result.warningDialogTitle = QCoreApplication::translate("MainWindow", "Select Therion Executable");
        result.warningDialogMessage = QCoreApplication::translate("MainWindow", "The selected file is not executable.");
        return result;
    }

    result.isAccepted = true;
    result.shouldUpdateExecutableText = true;
    result.updatedExecutableText = selectedInfo.absoluteFilePath();
    result.shouldShowStatusBarMessage = true;
    result.statusBarMessage = QCoreApplication::translate("MainWindow", "Therion executable updated");
    result.statusBarTimeoutMs = 2000;
    return result;
}
}
