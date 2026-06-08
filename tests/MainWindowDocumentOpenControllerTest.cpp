#include "../src/app/MainWindowDocumentOpenController.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

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

int runOpenCurrentDocumentInMapPlanTest()
{
    const auto noActivePlan = MainWindowDocumentOpenController::planOpenCurrentDocumentInMap(false, QString());
    if (!expect(noActivePlan.action == MainWindowDocumentOpenController::OpenCurrentInMapAction::NoActiveDocument,
                "No-active-document map-open plan changed unexpectedly.")) {
        return 1;
    }

    const auto unsupportedPlan = MainWindowDocumentOpenController::planOpenCurrentDocumentInMap(
        true,
        QStringLiteral("/tmp/survey.th"));
    if (!expect(unsupportedPlan.action == MainWindowDocumentOpenController::OpenCurrentInMapAction::UnsupportedDocument,
                "Unsupported current-document map-open plan changed unexpectedly.")) {
        return 1;
    }

    const auto supportedPlan = MainWindowDocumentOpenController::planOpenCurrentDocumentInMap(
        true,
        QStringLiteral("/tmp/map.th2"));
    if (!expect(supportedPlan.action == MainWindowDocumentOpenController::OpenCurrentInMapAction::OpenMapDocument,
                "Supported current-document map-open plan changed unexpectedly.")) {
        return 1;
    }
    if (!expect(supportedPlan.documentPath == QStringLiteral("/tmp/map.th2"),
                "Map-open plan should preserve target document path.")) {
        return 1;
    }

    return 0;
}

int runPlanOpenTextTabTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(),
                "Temporary directory setup failed for text-open plan test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("survey.th"));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly),
                "Temporary text file creation failed for text-open plan test.")) {
        return 1;
    }
    file.write("survey");
    file.close();

    const QString canonicalPath = MainWindowDocumentOpenController::canonicalDocumentPath(filePath);

    const auto unsupportedPlan = MainWindowDocumentOpenController::planOpenTextTab(
        filePath,
        false,
        {});
    if (!expect(unsupportedPlan.action == MainWindowDocumentOpenController::OpenTextTabAction::UnsupportedDocument,
                "Unsupported text-open plan changed unexpectedly.")) {
        return 1;
    }
    if (!expect(unsupportedPlan.canonicalPath == canonicalPath,
                "Text-open plan should compute canonical path even for unsupported files.")) {
        return 1;
    }

    QList<MainWindowDocumentOpenController::IndexedDocumentPath> attachedTabs;
    attachedTabs.append({2, canonicalPath});
    const auto reusePlan = MainWindowDocumentOpenController::planOpenTextTab(
        filePath,
        true,
        attachedTabs);
    if (!expect(reusePlan.action == MainWindowDocumentOpenController::OpenTextTabAction::ReuseAttachedTab,
                "Text-open reuse plan changed unexpectedly.")) {
        return 1;
    }
    if (!expect(reusePlan.reuseTabIndex == 2,
                "Text-open reuse plan should preserve matched tab index.")) {
        return 1;
    }

    const auto newPlan = MainWindowDocumentOpenController::planOpenTextTab(
        filePath,
        true,
        {});
    if (!expect(newPlan.action == MainWindowDocumentOpenController::OpenTextTabAction::OpenNewTab,
                "Text-open new-tab plan changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runPlanOpenMapTabTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(),
                "Temporary directory setup failed for map-open plan test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("map.th2"));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly),
                "Temporary map file creation failed for map-open plan test.")) {
        return 1;
    }
    file.write("map");
    file.close();

    const QString canonicalPath = MainWindowDocumentOpenController::canonicalDocumentPath(filePath);

    QList<MainWindowDocumentOpenController::IndexedDocumentPath> attachedTabs;
    attachedTabs.append({1, canonicalPath});

    const auto detachedPlan = MainWindowDocumentOpenController::planOpenMapTab(
        filePath,
        QStringList{canonicalPath},
        attachedTabs);
    if (!expect(detachedPlan.action == MainWindowDocumentOpenController::OpenMapTabAction::FocusDetachedTab,
                "Map-open detached-tab plan changed unexpectedly.")) {
        return 1;
    }

    const auto reusePlan = MainWindowDocumentOpenController::planOpenMapTab(
        filePath,
        {},
        attachedTabs);
    if (!expect(reusePlan.action == MainWindowDocumentOpenController::OpenMapTabAction::ReuseAttachedTab,
                "Map-open reuse-tab plan changed unexpectedly.")) {
        return 1;
    }
    if (!expect(reusePlan.reuseTabIndex == 1,
                "Map-open reuse-tab plan should preserve matched tab index.")) {
        return 1;
    }

    const auto newPlan = MainWindowDocumentOpenController::planOpenMapTab(filePath,
                                                                           {},
                                                                           {});
    if (!expect(newPlan.action == MainWindowDocumentOpenController::OpenMapTabAction::OpenNewTab,
                "Map-open new-tab plan changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runPlanOpenProjectSearchResultTest()
{
    const auto mapPlan = MainWindowDocumentOpenController::planOpenProjectSearchResult(
        QStringLiteral("/tmp/project/maps/scrap.th2"));
    if (!expect(mapPlan.action
                    == MainWindowDocumentOpenController::OpenProjectSearchResultAction::OpenMapDocument,
                "Project-search .th2 result should open as a map document.")) {
        return 1;
    }
    if (!expect(mapPlan.canonicalPath.endsWith(QStringLiteral("scrap.th2")),
                "Project-search map plan should preserve the target document path.")) {
        return 1;
    }

    const auto sourcePlan = MainWindowDocumentOpenController::planOpenProjectSearchResult(
        QStringLiteral("/tmp/project/survey.th"));
    if (!expect(sourcePlan.action
                    == MainWindowDocumentOpenController::OpenProjectSearchResultAction::OpenTextDocument,
                "Project-search .th result should open as a text document.")) {
        return 1;
    }

    const auto configPlan = MainWindowDocumentOpenController::planOpenProjectSearchResult(
        QStringLiteral("/tmp/project/thconfig"));
    if (!expect(configPlan.action
                    == MainWindowDocumentOpenController::OpenProjectSearchResultAction::OpenTextDocument,
                "Project-search thconfig result should open as a text document.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runOpenCurrentDocumentInMapPlanTest() != 0) {
        return 1;
    }
    if (runPlanOpenTextTabTest() != 0) {
        return 1;
    }
    if (runPlanOpenMapTabTest() != 0) {
        return 1;
    }
    if (runPlanOpenProjectSearchResultTest() != 0) {
        return 1;
    }

    return 0;
}
