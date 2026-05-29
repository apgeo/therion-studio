#include "MainWindowSessionRestoreUiFlowService.h"

#include <QCoreApplication>

namespace TherionStudio
{
namespace
{
QString trMainWindow(const char *sourceText)
{
    return QCoreApplication::translate("MainWindow", sourceText);
}
}

QString MainWindowSessionRestoreUiFlowService::restoredProjectRootConsoleLine(const QString &projectPath)
{
    return trMainWindow("Restored project root %1").arg(projectPath);
}

QString MainWindowSessionRestoreUiFlowService::skippedProtectedProjectConsoleLine(const QString &projectPath)
{
    return trMainWindow("Skipped automatic project restore for protected folder %1").arg(projectPath);
}
}
