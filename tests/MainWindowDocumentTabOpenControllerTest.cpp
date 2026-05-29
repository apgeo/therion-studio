#include "../src/app/MainWindowDocumentTabOpenController.h"

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

int runActivateExistingTabTest()
{
    QStringList calls;

    MainWindowDocumentTabOpenController::Actions actions;
    actions.setCurrentTabIndex = [&calls](int tabIndex) {
        calls.append(QStringLiteral("set:%1").arg(tabIndex));
    };
    actions.refreshDocumentStatusWidgets = [&calls]() {
        calls.append(QStringLiteral("refresh_status"));
    };
    actions.refreshWorkspaceModeSwitcher = [&calls]() {
        calls.append(QStringLiteral("refresh_workspace"));
    };

    MainWindowDocumentTabOpenController::ActivateExistingTabRequest request;
    request.tabIndex = 3;

    const bool activated = MainWindowDocumentTabOpenController::activateExistingTab(request, actions);
    const QStringList expected = {
        QStringLiteral("set:3"),
        QStringLiteral("refresh_status"),
        QStringLiteral("refresh_workspace")};

    if (!expect(activated,
                "Activating existing tab should return true for non-negative index.")) {
        return 1;
    }
    if (!expect(calls == expected,
                "Activate-existing-tab execution order changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runActivateExistingTabRejectsInvalidIndexTest()
{
    QStringList calls;

    MainWindowDocumentTabOpenController::Actions actions;
    actions.setCurrentTabIndex = [&calls](int tabIndex) {
        calls.append(QStringLiteral("set:%1").arg(tabIndex));
    };

    MainWindowDocumentTabOpenController::ActivateExistingTabRequest request;
    request.tabIndex = -1;

    const bool activated = MainWindowDocumentTabOpenController::activateExistingTab(request, actions);
    if (!expect(!activated,
                "Activating existing tab should return false for negative index.")) {
        return 1;
    }
    if (!expect(calls.isEmpty(),
                "Activate-existing-tab should not invoke actions for invalid index.")) {
        return 1;
    }

    return 0;
}

int runAttachNewTabOrderTest()
{
    QStringList calls;

    MainWindowDocumentTabOpenController::Actions actions;
    actions.removeWelcomeTabIfPresent = [&calls]() {
        calls.append(QStringLiteral("remove_welcome"));
    };
    actions.addTab = [&calls](const QString &tabTitle) {
        calls.append(QStringLiteral("add:%1").arg(tabTitle));
        return 2;
    };
    actions.setCurrentTabIndex = [&calls](int tabIndex) {
        calls.append(QStringLiteral("set:%1").arg(tabIndex));
    };
    actions.handleCurrentLineChanged = [&calls](const QString &path, int lineNumber) {
        calls.append(QStringLiteral("line:%1:%2").arg(path).arg(lineNumber));
    };
    actions.updateCurrentTabTitle = [&calls]() {
        calls.append(QStringLiteral("update_title"));
    };
    actions.refreshDocumentStatusWidgets = [&calls]() {
        calls.append(QStringLiteral("refresh_status"));
    };
    actions.refreshWorkspaceModeSwitcher = [&calls]() {
        calls.append(QStringLiteral("refresh_workspace"));
    };
    actions.persistOpenDocuments = [&calls]() {
        calls.append(QStringLiteral("persist"));
    };
    actions.appendConsoleLine = [&calls](const QString &line) {
        calls.append(QStringLiteral("console:%1").arg(line));
    };

    MainWindowDocumentTabOpenController::AttachNewTabRequest request;
    request.tabTitle = QStringLiteral("Map.th2");
    request.openedDocumentPath = QStringLiteral("/tmp/map.th2");
    request.currentLineNumber = 42;
    request.consoleOpenedLine = QStringLiteral("Opened /tmp/map.th2");

    const bool attached = MainWindowDocumentTabOpenController::attachNewTab(request, actions);
    const QStringList expected = {
        QStringLiteral("remove_welcome"),
        QStringLiteral("add:Map.th2"),
        QStringLiteral("set:2"),
        QStringLiteral("line:/tmp/map.th2:42"),
        QStringLiteral("update_title"),
        QStringLiteral("refresh_status"),
        QStringLiteral("refresh_workspace"),
        QStringLiteral("persist"),
        QStringLiteral("console:Opened /tmp/map.th2")};

    if (!expect(attached,
                "Attach-new-tab should return true when addTab returns a valid index.")) {
        return 1;
    }
    if (!expect(calls == expected,
                "Attach-new-tab execution order changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runAttachNewTabStopsWhenAddFailsTest()
{
    QStringList calls;

    MainWindowDocumentTabOpenController::Actions actions;
    actions.removeWelcomeTabIfPresent = [&calls]() {
        calls.append(QStringLiteral("remove_welcome"));
    };
    actions.addTab = [&calls](const QString &tabTitle) {
        calls.append(QStringLiteral("add:%1").arg(tabTitle));
        return -1;
    };
    actions.setCurrentTabIndex = [&calls](int tabIndex) {
        calls.append(QStringLiteral("set:%1").arg(tabIndex));
    };

    MainWindowDocumentTabOpenController::AttachNewTabRequest request;
    request.tabTitle = QStringLiteral("Text.th");

    const bool attached = MainWindowDocumentTabOpenController::attachNewTab(request, actions);
    const QStringList expected = {
        QStringLiteral("remove_welcome"),
        QStringLiteral("add:Text.th")};

    if (!expect(!attached,
                "Attach-new-tab should return false when addTab fails.")) {
        return 1;
    }
    if (!expect(calls == expected,
                "Attach-new-tab should stop immediately when addTab fails.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runActivateExistingTabTest() != 0) {
        return 1;
    }
    if (runActivateExistingTabRejectsInvalidIndexTest() != 0) {
        return 1;
    }
    if (runAttachNewTabOrderTest() != 0) {
        return 1;
    }
    if (runAttachNewTabStopsWhenAddFailsTest() != 0) {
        return 1;
    }

    return 0;
}
