#include "../src/app/MainWindowDocumentController.h"

#include <QCoreApplication>
#include <QStringList>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int runSaveActiveWithoutDocumentShowsComingSoonTest()
{
    QStringList calls;

    MainWindowDocumentController::Actions actions;
    actions.showComingSoon = [&calls](const QString &featureName) {
        calls.append(QStringLiteral("coming_soon:%1").arg(featureName));
    };

    MainWindowDocumentController::SaveActiveRequest request;
    request.hasActiveDocument = false;

    const bool saved = MainWindowDocumentController::saveActive(request, actions);

    const QStringList expected = {
        QStringLiteral("coming_soon:Save")};
    if (!expect(!saved,
                "Saving without active document should return false.")) {
        return 1;
    }
    if (!expect(calls == expected,
                "Saving without active document should only trigger coming-soon flow.")) {
        return 1;
    }

    return 0;
}

int runSaveActiveFailureShowsWarningTest()
{
    QStringList calls;

    MainWindowDocumentController::Actions actions;
    actions.saveDocument = [&calls](const QString &documentPath, QString *errorMessage) {
        calls.append(QStringLiteral("save:%1").arg(documentPath));
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("disk full");
        }
        return false;
    };
    actions.showWarningDialog = [&calls](const QString &title, const QString &message) {
        calls.append(QStringLiteral("warning:%1:%2").arg(title, message));
    };

    MainWindowDocumentController::SaveActiveRequest request;
    request.hasActiveDocument = true;
    request.activeDocumentPath = QStringLiteral("/tmp/map.th2");

    const bool saved = MainWindowDocumentController::saveActive(request, actions);

    const QStringList expected = {
        QStringLiteral("save:/tmp/map.th2"),
        QStringLiteral("warning:Save:disk full")};
    if (!expect(!saved,
                "Save-active failure should return false.")) {
        return 1;
    }
    if (!expect(calls == expected,
                "Save-active failure flow changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runSaveActiveEmptyPathStillAttemptsSaveTest()
{
    QStringList calls;

    MainWindowDocumentController::Actions actions;
    actions.saveDocument = [&calls](const QString &documentPath, QString *errorMessage) {
        calls.append(QStringLiteral("save:%1").arg(documentPath));
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The current document cannot be saved.");
        }
        return false;
    };
    actions.showWarningDialog = [&calls](const QString &title, const QString &message) {
        calls.append(QStringLiteral("warning:%1:%2").arg(title, message));
    };

    MainWindowDocumentController::SaveActiveRequest request;
    request.hasActiveDocument = true;
    request.activeDocumentPath = QString();

    const bool saved = MainWindowDocumentController::saveActive(request, actions);

    const QStringList expected = {
        QStringLiteral("save:"),
        QStringLiteral("warning:Save:The current document cannot be saved.")};
    if (!expect(!saved,
                "Save-active empty path should return false when save operation fails.")) {
        return 1;
    }
    if (!expect(calls == expected,
                "Save-active empty-path flow changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runSaveAllStopsOnFailureTest()
{
    QStringList calls;

    MainWindowDocumentController::Actions actions;
    actions.documentIsDirty = [&calls](const QString &documentPath) {
        calls.append(QStringLiteral("dirty:%1").arg(documentPath));
        return true;
    };
    actions.saveDocument = [&calls](const QString &documentPath, QString *errorMessage) {
        calls.append(QStringLiteral("save:%1").arg(documentPath));
        if (documentPath == QStringLiteral("/tmp/b.th2")) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("permission denied");
            }
            return false;
        }
        return true;
    };
    actions.isAttachedDocument = [&calls](const QString &documentPath) {
        calls.append(QStringLiteral("attached:%1").arg(documentPath));
        return true;
    };
    actions.updateTabTitle = [&calls](const QString &documentPath) {
        calls.append(QStringLiteral("title:%1").arg(documentPath));
    };
    actions.showWarningDialog = [&calls](const QString &title, const QString &message) {
        calls.append(QStringLiteral("warning:%1:%2").arg(title, message));
    };
    actions.showStatusBarMessage = [&calls](const QString &message, int timeoutMs) {
        calls.append(QStringLiteral("status:%1:%2").arg(message).arg(timeoutMs));
    };

    MainWindowDocumentController::SaveAllRequest request;
    request.documentPaths = {
        QStringLiteral("/tmp/a.th2"),
        QStringLiteral("/tmp/b.th2"),
        QStringLiteral("/tmp/c.th2")};

    const bool saved = MainWindowDocumentController::saveAll(request, actions);

    const QStringList expected = {
        QStringLiteral("dirty:/tmp/a.th2"),
        QStringLiteral("save:/tmp/a.th2"),
        QStringLiteral("attached:/tmp/a.th2"),
        QStringLiteral("title:/tmp/a.th2"),
        QStringLiteral("dirty:/tmp/b.th2"),
        QStringLiteral("save:/tmp/b.th2"),
        QStringLiteral("warning:Save All:permission denied")};
    if (!expect(!saved,
                "Save-all should return false when one document save fails.")) {
        return 1;
    }
    if (!expect(calls == expected,
                "Save-all failure flow changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runCloseAllStopsOnCancelAndStillPersistsTest()
{
    QStringList calls;

    MainWindowDocumentController::Actions actions;
    actions.confirmCloseTab = [&calls](int tabIndex) {
        calls.append(QStringLiteral("confirm_tab:%1").arg(tabIndex));
        return tabIndex != 2;
    };
    actions.closeTab = [&calls](int tabIndex) {
        calls.append(QStringLiteral("close_tab:%1").arg(tabIndex));
        return true;
    };
    actions.confirmCloseDocumentPath = [&calls](const QString &documentPath) {
        calls.append(QStringLiteral("confirm_detached:%1").arg(documentPath));
        return true;
    };
    actions.closeDetachedDocumentPath = [&calls](const QString &documentPath) {
        calls.append(QStringLiteral("close_detached:%1").arg(documentPath));
        return true;
    };
    actions.hasNoAttachedTabs = []() { return false; };
    actions.refreshDocumentStatusWidgets = [&calls]() { calls.append(QStringLiteral("refresh")); };
    actions.persistOpenDocuments = [&calls]() { calls.append(QStringLiteral("persist")); };
    actions.showStatusBarMessage = [&calls](const QString &message, int timeoutMs) {
        calls.append(QStringLiteral("status:%1:%2").arg(message).arg(timeoutMs));
    };

    MainWindowDocumentController::CloseAllRequest request;
    request.tabEntries = {
        {3, QStringLiteral("/tmp/c.th2")},
        {2, QStringLiteral("/tmp/b.th2")},
        {1, QStringLiteral("/tmp/a.th2")}};
    request.detachedDocumentPaths = {
        QStringLiteral("/tmp/detached.th2")};

    const bool closedAny = MainWindowDocumentController::closeAll(request, actions);

    const QStringList expected = {
        QStringLiteral("confirm_tab:3"),
        QStringLiteral("close_tab:3"),
        QStringLiteral("confirm_tab:2"),
        QStringLiteral("confirm_detached:/tmp/detached.th2"),
        QStringLiteral("close_detached:/tmp/detached.th2"),
        QStringLiteral("refresh"),
        QStringLiteral("persist"),
        QStringLiteral("status:Closed all open document tabs:2000")};
    if (!expect(closedAny,
                "Close-all should report closedAny=true when at least one document closes.")) {
        return 1;
    }
    if (!expect(calls == expected,
                "Close-all cancel flow changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runCloseTabSuccessEnsuresWelcomeTabTest()
{
    QStringList calls;

    MainWindowDocumentController::Actions actions;
    actions.confirmCloseTab = [&calls](int tabIndex) {
        calls.append(QStringLiteral("confirm_tab:%1").arg(tabIndex));
        return true;
    };
    actions.closeTab = [&calls](int tabIndex) {
        calls.append(QStringLiteral("close_tab:%1").arg(tabIndex));
        return true;
    };
    actions.hasNoAttachedTabs = [&calls]() {
        calls.append(QStringLiteral("has_no_tabs"));
        return true;
    };
    actions.ensureWelcomeTab = [&calls]() { calls.append(QStringLiteral("welcome")); };
    actions.refreshDocumentStatusWidgets = [&calls]() { calls.append(QStringLiteral("refresh")); };
    actions.persistOpenDocuments = [&calls]() { calls.append(QStringLiteral("persist")); };

    MainWindowDocumentController::CloseTabRequest request;
    request.tabIndex = 0;

    const bool closed = MainWindowDocumentController::closeTab(request, actions);

    const QStringList expected = {
        QStringLiteral("confirm_tab:0"),
        QStringLiteral("close_tab:0"),
        QStringLiteral("has_no_tabs"),
        QStringLiteral("welcome"),
        QStringLiteral("refresh"),
        QStringLiteral("persist")};
    if (!expect(closed,
                "Close-tab should return true on successful close.")) {
        return 1;
    }
    if (!expect(calls == expected,
                "Close-tab flow changed unexpectedly.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runSaveActiveWithoutDocumentShowsComingSoonTest() != 0) {
        return 1;
    }
    if (runSaveActiveFailureShowsWarningTest() != 0) {
        return 1;
    }
    if (runSaveActiveEmptyPathStillAttemptsSaveTest() != 0) {
        return 1;
    }
    if (runSaveAllStopsOnFailureTest() != 0) {
        return 1;
    }
    if (runCloseAllStopsOnCancelAndStillPersistsTest() != 0) {
        return 1;
    }
    if (runCloseTabSuccessEnsuresWelcomeTabTest() != 0) {
        return 1;
    }

    return 0;
}
