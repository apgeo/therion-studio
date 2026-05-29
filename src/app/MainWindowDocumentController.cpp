#include "MainWindowDocumentController.h"

#include <QObject>

namespace
{
void refreshAndPersistDocumentState(const TherionStudio::MainWindowDocumentController::Actions &actions)
{
    if (actions.refreshDocumentStatusWidgets) {
        actions.refreshDocumentStatusWidgets();
    }
    if (actions.persistOpenDocuments) {
        actions.persistOpenDocuments();
    }
}

void ensureWelcomeTabIfNeeded(const TherionStudio::MainWindowDocumentController::Actions &actions)
{
    if (actions.hasNoAttachedTabs && actions.hasNoAttachedTabs() && actions.ensureWelcomeTab) {
        actions.ensureWelcomeTab();
    }
}
}

namespace TherionStudio
{
bool MainWindowDocumentController::closeTab(const CloseTabRequest &request, const Actions &actions)
{
    if (request.tabIndex < 0) {
        return false;
    }

    if (!actions.confirmCloseTab || !actions.confirmCloseTab(request.tabIndex)) {
        return false;
    }

    if (!actions.closeTab || !actions.closeTab(request.tabIndex)) {
        return false;
    }

    ensureWelcomeTabIfNeeded(actions);
    refreshAndPersistDocumentState(actions);
    return true;
}

bool MainWindowDocumentController::closeAll(const CloseAllRequest &request, const Actions &actions)
{
    bool closedAny = false;

    for (const CloseTabEntry &entry : request.tabEntries) {
        if (entry.tabIndex < 0 || entry.documentPath.isEmpty()) {
            continue;
        }

        if (!actions.confirmCloseTab || !actions.confirmCloseTab(entry.tabIndex)) {
            break;
        }

        if (actions.closeTab && actions.closeTab(entry.tabIndex)) {
            closedAny = true;
        }
    }

    for (const QString &documentPath : request.detachedDocumentPaths) {
        if (documentPath.isEmpty()) {
            continue;
        }

        if (!actions.confirmCloseDocumentPath || !actions.confirmCloseDocumentPath(documentPath)) {
            break;
        }

        if (actions.closeDetachedDocumentPath && actions.closeDetachedDocumentPath(documentPath)) {
            closedAny = true;
        }
    }

    ensureWelcomeTabIfNeeded(actions);
    refreshAndPersistDocumentState(actions);

    if (closedAny && actions.showStatusBarMessage) {
        actions.showStatusBarMessage(QObject::tr("Closed all open document tabs"), 2000);
    }

    return closedAny;
}

bool MainWindowDocumentController::saveActive(const SaveActiveRequest &request, const Actions &actions)
{
    if (!request.hasActiveDocument) {
        if (actions.showComingSoon) {
            actions.showComingSoon(QObject::tr("Save"));
        }
        return false;
    }

    if (!actions.saveDocument) {
        return false;
    }

    QString errorMessage;
    if (!actions.saveDocument(request.activeDocumentPath, &errorMessage)) {
        if (actions.showWarningDialog) {
            actions.showWarningDialog(QObject::tr("Save"), errorMessage);
        }
        return false;
    }

    if (actions.updateTabTitle) {
        actions.updateTabTitle(request.activeDocumentPath);
    }

    if (actions.showStatusBarMessage && actions.documentDisplayName) {
        actions.showStatusBarMessage(QObject::tr("Saved %1").arg(actions.documentDisplayName(request.activeDocumentPath)), 2000);
    }

    return true;
}

bool MainWindowDocumentController::saveAll(const SaveAllRequest &request, const Actions &actions)
{
    bool hadFailure = false;

    for (const QString &documentPath : request.documentPaths) {
        if (documentPath.isEmpty()) {
            continue;
        }

        QString errorMessage;
        const bool dirty = actions.documentIsDirty && actions.documentIsDirty(documentPath);
        const bool saved = !dirty || (actions.saveDocument && actions.saveDocument(documentPath, &errorMessage));
        if (!saved) {
            hadFailure = true;
            if (actions.showWarningDialog) {
                actions.showWarningDialog(QObject::tr("Save All"), errorMessage);
            }
            break;
        }

        const bool attachedDocument = actions.isAttachedDocument && actions.isAttachedDocument(documentPath);
        if (attachedDocument && actions.updateTabTitle) {
            actions.updateTabTitle(documentPath);
        }
    }

    if (!hadFailure && actions.showStatusBarMessage) {
        actions.showStatusBarMessage(QObject::tr("All open documents saved"), 2000);
    }

    return !hadFailure;
}
}
