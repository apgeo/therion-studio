#include "../src/app/MainWindowSessionDocumentService.h"

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

int runRestoreEntriesBuildTest()
{
    const QStringList openDocumentPaths = {
        QString(),
        QStringLiteral("/tmp/survey.th"),
        QStringLiteral("/tmp/map.TH2"),
        QStringLiteral("/tmp/log.txt")};

    const std::vector<MainWindowSessionDocumentService::RestoreEntry> entries =
        MainWindowSessionDocumentService::buildRestoreEntries(openDocumentPaths);

    if (!expect(entries.size() == 3, "Restore entries should skip empty paths.")) {
        return 1;
    }

    if (!expect(entries.at(0).filePath == QStringLiteral("/tmp/survey.th")
                    && entries.at(0).target == MainWindowSessionDocumentService::RestoreTarget::TextEditor,
                "Therion source file should target the text editor restore path.")) {
        return 1;
    }

    if (!expect(entries.at(1).filePath == QStringLiteral("/tmp/map.TH2")
                    && entries.at(1).target == MainWindowSessionDocumentService::RestoreTarget::MapEditor,
                "TH2 file should target the map editor restore path.")) {
        return 1;
    }

    if (!expect(entries.at(2).filePath == QStringLiteral("/tmp/log.txt")
                    && entries.at(2).target == MainWindowSessionDocumentService::RestoreTarget::TextEditor,
                "Non-map file should target the text editor restore path.")) {
        return 1;
    }

    return 0;
}

int runOpenDocumentPathMergeTest()
{
    const QStringList tabDocumentPaths = {
        QStringLiteral("/tmp/a.th"),
        QString(),
        QStringLiteral("/tmp/b.th")};
    const QStringList detachedMapDocumentPaths = {
        QStringLiteral("/tmp/b.th"),
        QString(),
        QStringLiteral("/tmp/c.th2")};

    const QStringList merged = MainWindowSessionDocumentService::mergeOpenDocumentPaths(tabDocumentPaths,
                                                                                         detachedMapDocumentPaths);
    const QStringList expected = {
        QStringLiteral("/tmp/a.th"),
        QStringLiteral("/tmp/b.th"),
        QStringLiteral("/tmp/c.th2")};
    if (!expect(merged == expected,
                "Merged document paths should keep tab order and append unique detached map paths.")) {
        return 1;
    }

    return 0;
}

int runMapDocumentDetectionTest()
{
    if (!expect(MainWindowSessionDocumentService::isMapDocumentPath(QStringLiteral("/tmp/map.th2")),
                ".th2 extension should be treated as a map document.")) {
        return 1;
    }
    if (!expect(MainWindowSessionDocumentService::isMapDocumentPath(QStringLiteral("/tmp/map.TH2")),
                "Map document detection should be case-insensitive.")) {
        return 1;
    }
    if (!expect(!MainWindowSessionDocumentService::isMapDocumentPath(QStringLiteral("/tmp/survey.th")),
                "Non-.th2 extension should not be treated as a map document.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runRestoreEntriesBuildTest() != 0) {
        return 1;
    }
    if (runOpenDocumentPathMergeTest() != 0) {
        return 1;
    }
    if (runMapDocumentDetectionTest() != 0) {
        return 1;
    }

    return 0;
}
